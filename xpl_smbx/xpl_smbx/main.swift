import NIOCore
import NIOPosix
import Dispatch


struct NetbiosSsnRequest {
    let serverNbName: [UInt8]
    let localNbName: [UInt8]
}

var savedNetbiosSsnRequestStore: [UInt32: NetbiosSsnRequest] = [:]
let dispatchQueueStore = DispatchQueue(label: "dq_store")


// MARK: - SMB & Netbios request routing
class RequestHandler: ChannelInboundHandler {
    public typealias InboundIn = ByteBuffer
    public typealias OutboundOut = ByteBuffer
    
    var lastNetbiosSsnRequest: NetbiosSsnRequest? = nil
    
    func channelRead(context: ChannelHandlerContext, data: NIOAny) {
        var request = unwrapInboundIn(data)
        
        if (request.readableBytes < 8) {
            print("[WARN] ignoring request with too small length \(request.readableBytes)")
            return
        }

        assert(SMB_SIGLEN == SMB2_SIGLEN)
        let signature = request.getBytes(at: 4, length: Int(SMB_SIGLEN))!
        let structSize = request.getInteger(at: 4 + Int(SMB_SIGLEN), endianness: .little, as: UInt16.self)!
        if signature == SMB2_SIGNATURE.utf8.map({ UInt8($0) }) || structSize == 64 {
            var body = request.getSlice(at: 4, length: request.readableBytes - 4)!
            handleSmbV2Request(context: context, request: &body)
        } else if request.getString(at: 4, length: Int(SMB_SIGLEN)) == "\u{fffd}SMB" {
            var body = request.getSlice(at: 4, length: request.readableBytes - 4)!
            handleSmbV1Request(context: context, request: &body)
        } else {
            handleNetbiosRequest(context: context, request: &request)
        }
    }
}

//MARK: - SMBv1 handling
extension RequestHandler {
    func handleSmbV1Request(context: ChannelHandlerContext, request: inout ByteBuffer) {
        guard request.readableBytes >= SMB_HDRLEN else {
            print("[WARN] ingoring request with size less than header length")
            return
        }

        let str = request.getString(at: 0, length: Int(SMB_SIGLEN))!
        guard str == "\u{fffd}SMB" else {
            print("[WARN] ignoring request with invalid smb signature")
            return
        }

        let cmd = request.getInteger(at: Int(SMB_SIGLEN), endianness: .little, as: UInt8.self)!
        let pidHigh = request.getInteger(at: 12, endianness: .little, as: UInt16.self)!
        let tid = request.getInteger(at: 24, endianness: .little, as: UInt16.self)!
        let pidLow = request.getInteger(at: 26, endianness: .little, as: UInt16.self)!
        let uid = request.getInteger(at: 28, endianness: .little, as: UInt16.self)!
        let mid = request.getInteger(at: 30, endianness: .little, as: UInt16.self)!

        print("[INFO] handling SMBv1 command", cmd)

        var response = context.channel.allocator.buffer(capacity: MemoryLayout<UInt32>.size + Int(SMB_HDRLEN) + 32)

        var status: Int32? = nil
        request.moveReaderIndex(to: Int(SMB_HDRLEN))
        response.moveWriterIndex(to: Int(SMB_HDRLEN) + MemoryLayout<UInt32>.size)
        switch Int32(cmd) {
        case SMB_COM_NEGOTIATE:
            status = handleCmdNegotiateV1(request: &request, response: &response)
        case SMB_COM_SESSION_SETUP_ANDX:
            status = handleCmdSsnSetup(request: &request, response: &response)
        case SMB_COM_TREE_CONNECT_ANDX:
            status = handleCmdTreeConnectAndX(request: &request, response: &response)
        case SMB_COM_TREE_DISCONNECT:
            status = handleCmdTreeDisconnect(request: &request, response: &response)
        case SMB_COM_LOGOFF_ANDX:
            status = handleCmdLogoff(request: &request, response: &response)
        case SMB_COM_TRANSACTION2:
            status = handleCmdTransaction2(request: &request, response: &response)
        case SMB_COM_NT_CREATE_ANDX:
            status = handleCmdNTCreateAndX(request: &request, response: &response)
        case xpl_SMBX_SMB_CMD_GET_LAST_NB_SSN_REQUEST:
            status = handleCustomCmdReadLastNbSsnRequest(request: &request, response: &response)
        case xpl_SMBX_SMB_CMD_GET_SAVED_NB_SSN_REQUEST:
            status = handleCustomCmdReadSavedRequest(request: &request, response: &response)
        default:
            print("[WARN] unsupported command", cmd)
        }

        guard let status = status else {
            print("[WARN] ignoring request with no response")
            return
        }

        let responseSize = response.writerIndex
        response.moveWriterIndex(to: 0)
        response.writeInteger(UInt32(responseSize - MemoryLayout<UInt32>.size), endianness: .big, as: UInt32.self)
        response.writeString(SMB_SIGNATURE)
        response.writeInteger(cmd, endianness: .little, as: UInt8.self)
        response.writeInteger(UInt32(bitPattern: status), endianness: .little, as: UInt32.self)
        response.writeInteger(0, endianness: .little, as: UInt8.self) // rpflags
        response.writeInteger(0, endianness: .little, as: UInt16.self) // rpflags2
        response.writeInteger(pidHigh, endianness: .little, as: UInt16.self)
        response.writeRepeatingByte(0, count: 8) // signature
        response.writeRepeatingByte(0, count: 2) // reserved bytes
        response.writeInteger(tid, endianness: .little, as: UInt16.self)
        response.writeInteger(pidLow, endianness: .little, as: UInt16.self)
        response.writeInteger(uid, endianness: .little, as: UInt16.self)
        response.writeInteger(mid, endianness: .little, as: UInt16.self)
        response.moveWriterIndex(to: responseSize)

        context.channel.writeAndFlush(response, promise: nil)
        print("[INFO] done handling command", cmd, "response size:", responseSize)
    }

    func handleCmdNegotiateV1(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(17, endianness: .little, as: UInt8.self) // word count
        response.writeInteger(0, endianness: .little, as: UInt16.self) // dindex
        response.writeInteger(0, endianness: .little, as: UInt8.self)  // security mode
        response.writeInteger(1, endianness: .little, as: UInt16.self) // max mux
        response.writeInteger(1, endianness: .little, as: UInt16.self) // max sessions
        response.writeInteger(UInt32.max, endianness: .little, as: UInt32.self) // max tx
        response.writeInteger(0, endianness: .little, as: UInt32.self) // sv maxraw
        response.writeInteger(1, endianness: .little, as: UInt32.self) // sv skey
        response.writeInteger(UInt32(SMB_CAP_NT_SMBS | SMB_CAP_UNICODE), endianness: .little, as: UInt32.self) // sv caps
        response.writeRepeatingByte(0, count: 8)                       // stime
        response.writeInteger(0, endianness: .little, as: UInt16.self) // stimezone
        response.writeInteger(0, endianness: .little, as: UInt8.self)  // sb len
        response.writeInteger(0, endianness: .little, as: UInt16.self) // bc
        return NTSTATUS_SUCCESS
    }

    func handleCmdSsnSetup(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(3, endianness: .little, as: UInt8.self)  // word count
        response.writeInteger(0, endianness: .little, as: UInt8.self)  // secondary command
        response.writeInteger(0, endianness: .little, as: UInt8.self)  // mbz
        response.writeInteger(0, endianness: .little, as: UInt16.self) // andx offset
        response.writeInteger(0, endianness: .little, as: UInt16.self) // action
        response.writeInteger(0, endianness: .little, as: UInt16.self) // bc
        return NTSTATUS_SUCCESS;
    }
    
    func handleCmdTreeConnectAndX(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(3, endianness: .little, as: UInt8.self)   // word count
        response.writeInteger(0, endianness: .little, as: UInt8.self)   // andx command
        response.writeInteger(0, endianness: .little, as: UInt8.self)   // andx reserved
        response.writeInteger(0, endianness: .little, as: UInt16.self)  // andx offset
        response.writeInteger(0, endianness: .little, as: UInt16.self)  // optional support
        return NTSTATUS_SUCCESS
    }
    
    func handleCmdTreeDisconnect(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        return NTSTATUS_SUCCESS
    }

    func handleCmdLogoff(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        return NTSTATUS_SUCCESS
    }
    
    func handleCmdTransaction2(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        let wc = request.readInteger(endianness: .little, as: UInt8.self)
        let totpcount = request.readInteger(endianness: .little, as: UInt16.self)!
        let totdcount = request.readInteger(endianness: .little, as: UInt16.self)!
        let t2Maxpcount = request.readInteger(endianness: .little, as: UInt16.self)!
        let t2Maxdcount = request.readInteger(endianness: .little, as: UInt16.self)!
        let t2Maxscount = request.readInteger(endianness: .little, as: UInt8.self)!
        _ = request.readInteger(endianness: .little, as: UInt8.self)! // reserved
        _ = request.readInteger(endianness: .little, as: UInt16.self)! // flags
        _ = request.readInteger(endianness: .little, as: UInt32.self)! // timeout
        _ = request.readInteger(endianness: .little, as: UInt16.self)! // reserved 2
        let txpcountp = request.readInteger(endianness: .little, as: UInt16.self)!
        let txpoffsetp = request.readInteger(endianness: .little, as: UInt16.self)!
        let txdcountp = request.readInteger(endianness: .little, as: UInt16.self)!
        let txdoffsetp = request.readInteger(endianness: .little, as: UInt16.self)!
        let t2Setupcount = request.readInteger(endianness: .little, as: UInt8.self)!
        
        if t2Setupcount != 1 {
            print("[WARN] received smbv1 transaction2 with \(t2Setupcount) setup count")
            return Int32(NTSTATUS_NOT_CAPABLE)
        }
        
        _ = request.readInteger(endianness: .little, as: UInt8.self)! // reserved
        let t2Setupdata = request.readInteger(endianness: .little, as: UInt16.self)!
        
        var dbuf = ByteBuffer()
        var pbuf = ByteBuffer()
        var status: Int32!
        switch Int32(t2Setupdata) {
        case SMB_TRANS2_QUERY_FS_INFORMATION:
            status = handleT2QueryFSInformation(request: &request, dbuf: &dbuf, pbuf: &pbuf)
        case SMB_TRANS2_QUERY_PATH_INFORMATION:
            status = handleT2QueryPathInformation(request: &request, dbuf: &dbuf, pbuf: &pbuf)
        default:
            print("[WARN] received unknown setup data \(t2Setupdata)")
            return NTSTATUS_INVALID_SMB
        }
        
        response.writeInteger(10, endianness: .little, as: UInt8.self) // wc
        response.writeInteger(pbuf.writerIndex > 0 ? 1 : 0, endianness: .little, as: UInt16.self) // totpcount
        response.writeInteger(dbuf.writerIndex > 0 ? 1 : 0, endianness: .little, as: UInt16.self) // totdcount
        response.writeInteger(0, endianness: .little, as: UInt16.self) // reserved
        response.writeInteger(UInt16(pbuf.writerIndex), endianness: .little, as: UInt16.self) // pcount
        response.writeInteger(0, endianness: .little, as: UInt16.self) // poff
        response.writeInteger(0, endianness: .little, as: UInt16.self) // pdisp
        response.writeInteger(UInt16(dbuf.writerIndex), endianness: .little, as: UInt16.self) // dcount
        response.writeInteger(0, endianness: .little, as: UInt16.self) // doff
        response.writeInteger(0, endianness: .little, as: UInt16.self) // ddisp
        
        response.writeInteger(0, endianness: .little, as: UInt8.self) // wc
        response.writeInteger(0, endianness: .little, as: UInt8.self) // reserved
        response.writeInteger(0, endianness: .little, as: UInt16.self) // bc
        
        response.writeBuffer(&dbuf)
        response.writeBuffer(&pbuf)
        
        return status
    }
    
    func handleCmdNTCreateAndX(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        return NTSTATUS_SUCCESS
    }
    
    func handleCustomCmdReadLastNbSsnRequest(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        guard let lastRequest = lastNetbiosSsnRequest else {
            return Int32(NTSTATUS_NOT_FOUND)
        }
        
        response.writeInteger(UInt32(lastRequest.serverNbName.count), endianness: .little, as: UInt32.self)
        response.writeInteger(UInt32(lastRequest.localNbName.count), endianness: .little, as: UInt32.self)
        response.writeBytes(lastRequest.serverNbName)
        response.writeBytes(lastRequest.localNbName)
        
        return NTSTATUS_SUCCESS
    }
    
    func handleCustomCmdReadSavedRequest(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        guard let length = request.readInteger(endianness: .little, as: UInt8.self) else {
            return NTSTATUS_INVALID_SMB
        }
        
        if length != 2 {
            return NTSTATUS_INVALID_SMB
        }
        
        guard let key = request.readInteger(endianness: .little, as: UInt32.self) else {
            return NTSTATUS_INVALID_SMB
        }
        
        var savedRequest: NetbiosSsnRequest? = nil
        dispatchQueueStore.sync {
            if let value = savedNetbiosSsnRequestStore.removeValue(forKey: key) {
                savedRequest = value
            }
        }
        
        guard let savedRequest = savedRequest else {
            return Int32(bitPattern: NTSTATUS_NOT_FOUND)
        }

        response.writeInteger(UInt32(savedRequest.serverNbName.count), endianness: .little, as: UInt32.self)
        response.writeInteger(UInt32(savedRequest.localNbName.count), endianness: .little, as: UInt32.self)
        response.writeBytes(savedRequest.serverNbName)
        response.writeBytes(savedRequest.localNbName)
        
        return NTSTATUS_SUCCESS
    }
}


// MARK: - SMBv2 handling
extension RequestHandler {
    func handleSmbV2Request(context: ChannelHandlerContext, request: inout ByteBuffer) {
        guard request.readableBytes >= SMB2_HDRLEN else {
            print("[WARN] ingoring request with size less than header length")
            return
        }

        let str = request.readString(length: Int(SMB2_SIGLEN))!
        guard str == "\u{fffd}SMB" else {
            print("[WARN] ignoring request with invalid smb signature")
            return
        }

        let hdrLength = request.readInteger(endianness: .little, as: UInt16.self)!
        guard hdrLength == SMB2_HDRLEN else {
            print("[WARN] ignoring request with invalid smb2 hdr length")
            return
        }

        let creditCharge = request.readInteger(endianness: .little, as: UInt16.self)!
        _ = request.readInteger(endianness: .little, as: UInt32.self)! // status
        let cmd = request.readInteger(endianness: .little, as: UInt16.self)!
        _ = request.readInteger(endianness: .little, as: UInt16.self)! // credit req
        _ = request.readInteger(endianness: .little, as: UInt32.self)! // flags
        _ = request.readInteger(endianness: .little, as: UInt32.self)! // next cmd
        let messageID = request.readInteger(endianness: .little, as: UInt64.self)!
        let asyncID = request.readInteger(endianness: .little, as: UInt64.self)!
        let sessionID = request.readInteger(endianness: .little, as: UInt64.self)!
        let signature = request.readBytes(length: 16)!

        print("[INFO] handling SMBv2 command", cmd)

        var response = context.channel.allocator.buffer(capacity: MemoryLayout<UInt32>.size + Int(SMB2_HDRLEN) + 32)

        var responseStatus: Int32? = nil
        request.moveReaderIndex(to: Int(SMB2_HDRLEN))
        response.moveWriterIndex(to: Int(SMB2_HDRLEN) + MemoryLayout<UInt32>.size)
        switch Int32(cmd) {
        case SMB2_NEGOTIATE:
            responseStatus = handleCmdNegotiateV2(request: &request, response: &response)
        case SMB2_TREE_CONNECT:
            responseStatus = handleCmdTreeConnectV2(request: &request, response: &response)
        case SMB2_TREE_DISCONNECT:
            responseStatus = handleCmdTreeDisconnectV2(request: &request, response: &response)
        default:
            print("[WARN] unsupported command", cmd)
        }

        guard let responseStatus = responseStatus else {
            print("[WARN] ignoring request with no response")
            return
        }

        let responseSize = response.writerIndex
        response.moveWriterIndex(to: 0)
        response.writeInteger(UInt32(responseSize - MemoryLayout<UInt32>.size), endianness: .big, as: UInt32.self)
        response.writeString(SMB2_SIGNATURE)
        response.writeInteger(UInt16(SMB2_HDRLEN), endianness: .little, as: UInt16.self)
        response.writeInteger(creditCharge, endianness: .little, as: UInt16.self)
        response.writeInteger(UInt32(responseStatus), endianness: .little, as: UInt32.self)
        response.writeInteger(cmd, endianness: .little, as: UInt16.self)
        response.writeInteger(256, endianness: .little, as: UInt16.self)  // credits granted
        response.writeInteger(0, endianness: .little, as: UInt32.self)  // rsp flags
        response.writeInteger(0, endianness: .little, as: UInt32.self)  // next cmd offset
        response.writeInteger(messageID, endianness: .little, as: UInt64.self)
        response.writeInteger(asyncID, endianness: .little, as: UInt64.self)
        response.writeInteger(sessionID, endianness: .little, as: UInt64.self)
        response.writeBytes(signature)
        response.moveWriterIndex(to: responseSize)

        context.channel.writeAndFlush(response, promise: nil)
        print("[INFO] done handling command", cmd, "response size:", responseSize)
    }

    func handleCmdNegotiateV2(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(65, endianness: .little, as: UInt16.self) // struct size
        response.writeInteger(0, endianness: .little, as: UInt16.self)  // security mode
        response.writeInteger(UInt16(SMB2_DIALECT_0300), endianness: .little, as: UInt16.self) // dialects
        response.writeInteger(0, endianness: .little, as: UInt16.self) // negotiation context count
        response.writeRepeatingByte(1, count: 16)                      // server GUID
        response.writeInteger(UInt32(SMB2_GLOBAL_CAP_MULTI_CHANNEL), endianness: .little, as: UInt32.self) // server capabilities
        response.writeInteger(UInt32.max, endianness: .little, as: UInt32.self) // sv max tx rx
        response.writeInteger(UInt32.max, endianness: .little, as: UInt32.self) // max read
        response.writeInteger(UInt32.max, endianness: .little, as: UInt32.self) // max write
        response.writeRepeatingByte(0, count: 8)                       // system time
        response.writeRepeatingByte(0, count: 8)                       // start time
        response.writeInteger(0, endianness: .little, as: UInt16.self) // security buffer offset
        response.writeInteger(0, endianness: .little, as: UInt16.self) // security buffer len
        response.writeInteger(0, endianness: .little, as: UInt32.self) // negotiation context offset
        return NTSTATUS_SUCCESS
    }
    
    func handleCmdTreeConnectV2(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(16, endianness: .little, as: UInt16.self) // struct size
        response.writeInteger(0, endianness: .little, as: UInt8.self)   // share type
        response.writeInteger(0, endianness: .little, as: UInt8.self)   // reserved
        response.writeInteger(0, endianness: .little, as: UInt32.self)  // share flags
        response.writeInteger(0, endianness: .little, as: UInt32.self)  // share capabilities
        response.writeInteger(0, endianness: .little, as: UInt32.self)  // share max access rights
        return NTSTATUS_SUCCESS
    }
    
    func handleCmdTreeDisconnectV2(request: inout ByteBuffer, response: inout ByteBuffer) -> Int32 {
        response.writeInteger(4, endianness: .little, as: UInt16.self)  // struct size
        return NTSTATUS_SUCCESS
    }
}

// MARK: - SMBv1 T2 Handling
extension RequestHandler {
    func handleT2QueryFSInformation(request: inout ByteBuffer, dbuf: inout ByteBuffer, pbuf: inout ByteBuffer) -> Int32 {
        dbuf.writeInteger(8, endianness: .little, as: UInt64.self) // # allocated units
        dbuf.writeInteger(8, endianness: .little, as: UInt64.self) // # free units
        dbuf.writeInteger(32, endianness: .little, as: UInt32.self) // # sectors / unit
        dbuf.writeInteger(32, endianness: .little, as: UInt32.self) // # bytes / sector
        return NTSTATUS_SUCCESS
    }
    
    func handleT2QueryPathInformation(request: inout ByteBuffer, dbuf: inout ByteBuffer, pbuf: inout ByteBuffer) -> Int32 {
        dumpRequest(&request)
        let wc = request.readInteger(endianness: .little, as: UInt8.self)!
        _ = request.readInteger(endianness: .little, as: UInt32.self)!
        let infoLevel = request.readInteger(endianness: .little, as: UInt16.self)!
        
        switch Int32(infoLevel) {
        case SMB_QFILEINFO_ALL_INFO:
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // creation time
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // access time
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // write time
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // change time
            dbuf.writeInteger(0, endianness: .little, as: UInt32.self) // attributes
            dbuf.writeInteger(0, endianness: .little, as: UInt32.self) // pad
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // allocation size
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // file size
            dbuf.writeInteger(0, endianness: .little, as: UInt32.self) // # hard links
            dbuf.writeInteger(0, endianness: .little, as: UInt64.self) // creation time
            dbuf.writeInteger(0, endianness: .little, as: UInt8.self) // delete pending
            dbuf.writeInteger(0, endianness: .little, as: UInt8.self) // dir / file
            dbuf.writeInteger(0, endianness: .little, as: UInt16.self) // unknown
            dbuf.writeInteger(0, endianness: .little, as: UInt32.self) // xattrs
            dbuf.writeInteger(0, endianness: .little, as: UInt8.self) // delete pending
        default:
            print("[WARN] ignoring t2 query path request with unknown infoLevel \(infoLevel)")
        }
        return NTSTATUS_SUCCESS
    }
}

// MARK: - Debug
extension RequestHandler {
    private func dumpRequest(_ request: inout ByteBuffer) {
        for i in 0..<request.readableBytes {
            let byte = request.getInteger(at: i + request.readerIndex, endianness: .little, as: UInt8.self)!
            let hex = String(byte, radix: 16, uppercase: false)
            let terminator = (i + 1) % 8 == 0 ? "\n" : " "
            let pad = byte > 15 ? "" : "0"
            print("\(pad)\(hex)", terminator: terminator)
        }
        if request.readableBytes % 8 != 0 {
            print("")
        }
    }
}


// MARK: - Netbios handling
extension RequestHandler {
    private func parseNetbiosName(_ data: inout ByteBuffer) throws -> [UInt8] {
        enum NetbiosNameDecodeError: Error {
            case noLength
            case insufficientData(expected: Int, got: Int)
        }
        
        var bytes = ByteBuffer()
        while true {
            guard let len = data.readInteger(endianness: .little, as: UInt8.self) else {
                throw NetbiosNameDecodeError.noLength
            }
            
            bytes.writeInteger(len, endianness: .little, as: UInt8.self)
            guard let sequence = data.readBytes(length: Int(len)) else {
                throw NetbiosNameDecodeError.insufficientData(expected: Int(len), got: data.readableBytes - data.readerIndex)
            }
            
            bytes.writeBytes(sequence)
            
            if (len == 0) {
                break
            }
        }
        
        return bytes.readBytes(length: bytes.readableBytes)!
    }
    
    func handleNetbiosSsnRequest(request: inout ByteBuffer, response: inout ByteBuffer) -> UInt8 {
        guard let serverNbName = try? parseNetbiosName(&request) else {
            print("[WARN] got netbios ssn request with invalid paddr")
            return UInt8(NB_SSN_NEGRESP)
        }
        
        guard let localNbName = try? parseNetbiosName(&request) else {
            print("[WARN] got netbios ssn request with invalid laddr")
            return UInt8(NB_SSN_NEGRESP)
        }
        
        let ssnRequest = NetbiosSsnRequest(serverNbName: serverNbName, localNbName: localNbName)
        lastNetbiosSsnRequest = ssnRequest
        
        // Check and process xpl_smbx_nb_laddr_cmd if it is embedded in laddr
        if localNbName.count < MemoryLayout<xpl_smbx_nb_laddr_cmd>.size + 1 {
            return UInt8(NB_SSN_POSRESP)
        }
        
        var laddrCmd = xpl_smbx_nb_laddr_cmd()
        _ = localNbName.withUnsafeBufferPointer {
            memcpy(&laddrCmd, $0.baseAddress! + 1 /* skip first byte containing length of segment */, MemoryLayout.size(ofValue: laddrCmd))
        }
        
        if laddrCmd.magic != xpl_SMBX_NB_LADDR_CMD_MAGIC {
            return UInt8(NB_SSN_POSRESP)
        }
        
        if laddrCmd.flags & UInt16(xpl_SMBX_NB_LADDR_CMD_FLAG_SAVE) != 0 {
            dispatchQueueStore.sync {
                savedNetbiosSsnRequestStore[UInt32(laddrCmd.key)] = ssnRequest
            }
        }
        
        if laddrCmd.flags & UInt16(xpl_SMBX_NB_LADDR_CMD_FLAG_FAIL) != 0 {
            return UInt8(NB_SSN_NEGRESP)
        } else {
            return UInt8(NB_SSN_POSRESP)
        }
    }
    
    func handleNetbiosRequest(context: ChannelHandlerContext, request: inout ByteBuffer) {
        let header = request.readInteger(endianness: .big, as: UInt32.self)!
        let length = header & 0xffffff
        let requestCode = header >> 24
        print("[INFO] handling netbios request of type: \(requestCode) and length: \(length)")
        
        var body = request.getSlice(at: MemoryLayout<UInt32>.size, length: Int(length))!
        
        var response = context.channel.allocator.buffer(capacity: MemoryLayout<UInt32>.size + 8)
        response.moveWriterIndex(to: 4)
        var responseCode: UInt8? = nil
        
        switch (requestCode) {
        case UInt32(NB_SSN_REQUEST):
            responseCode = handleNetbiosSsnRequest(request: &body, response: &response)
        default:
            print("[WARN] invalid netbios request of type \(requestCode)")
        }
        
        guard let rpCode = responseCode else {
            print("[WARN] ignoring unknown netbios command")
            return
        }
        
        let responseBodySize = response.writerIndex - 4
        assert(responseBodySize <= 0xffffff)
        response.moveWriterIndex(to: 0)
        let responseHeader = UInt32(rpCode) << 24 | UInt32(responseBodySize)
        response.writeInteger(responseHeader, endianness: .big, as: UInt32.self)
        
        context.channel.writeAndFlush(response, promise: nil)
        print("[INFO] done handling netbios request of type \(requestCode)")
    }
}


// MARK: - length prefixed message buffering
final class MessageReader: ByteToMessageDecoder {
    public typealias InboundIn = ByteBuffer
    public typealias InboundOut = ByteBuffer

    public func decode(context: ChannelHandlerContext, buffer: inout ByteBuffer) throws -> DecodingState {
        guard var length = buffer.getInteger(at: 0, endianness: .big, as: UInt32.self) else {
            return .needMoreData
        }
                
        /*
         Netbios requests have bits 24-31 containing command type and
         bits 0-23 containing length of data. For simplicity, we use only
         bits 0-23 for finding length of both SMB and netbios requests,
         assuming that SMB requests will not have length greater than 2^24 - 1.
         */
        length = length & 0xffffff
        
        guard let data = buffer.getSlice(at: 0, length: Int(length) + MemoryLayout<UInt32>.size) else {
            return .needMoreData
        }

        context.fireChannelRead(self.wrapInboundOut(data))
        buffer.moveReaderIndex(forwardBy: Int(length) + MemoryLayout<UInt32>.size)
        return .continue
    }
}

// MARK: - Server setup
let arguments = CommandLine.arguments
let host = xpl_SMBX_HOST
let port = Int(xpl_SMBX_PORT)

var limit = rlimit(rlim_cur: 0, rlim_max: 0)
getrlimit(RLIMIT_NOFILE, &limit)
limit.rlim_cur = limit.rlim_max
setrlimit(RLIMIT_NOFILE, &limit)

let group = MultiThreadedEventLoopGroup(numberOfThreads: System.coreCount)
let bootstrap = ServerBootstrap(group: group)
    .serverChannelOption(ChannelOptions.backlog, value: 256)
    .serverChannelOption(ChannelOptions.socketOption(.so_reuseaddr), value: 1)
    .childChannelInitializer { channel in
        print("[INFO] new connection from", channel.remoteAddress!)
        return channel.pipeline.addHandler(ByteToMessageHandler(MessageReader())).flatMap { v in
            channel.pipeline.addHandler(RequestHandler())
        }
    }
    .childChannelOption(ChannelOptions.socketOption(.so_reuseaddr), value: 1)
    .childChannelOption(ChannelOptions.maxMessagesPerRead, value: 16)
    .childChannelOption(ChannelOptions.recvAllocator, value: AdaptiveRecvByteBufferAllocator())
defer {
    try! group.syncShutdownGracefully()
}

let channel = try bootstrap.bind(host: host, port: port).wait()
guard let localAddress = channel.localAddress else {
    fatalError("Unable to bind to specified \(host):\(port)")
}

print("SMB server started running on \(localAddress)")
try channel.closeFuture.wait()
print("SMB server stopped")
