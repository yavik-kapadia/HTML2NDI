import Foundation
import AppKit

struct StreamConfig: Codable, Identifiable {
    var id: UUID
    var name: String
    var url: String
    var ndiName: String
    var ndiGroups: String  // NDI access groups (comma-separated), "public" for all
    var width: Int
    var height: Int
    var fps: Int
    var colorPreset: String
    var httpPort: Int
    var autoStart: Bool
    
    init(name: String? = nil, url: String = "about:blank", ndiName: String? = nil, ndiGroups: String = "public") {
        self.id = UUID()
        // Generate unique name if not provided
        let shortId = String(self.id.uuidString.prefix(8))
        self.name = (name?.isEmpty ?? true) ? "Stream-\(shortId)" : name!
        self.url = url
        self.ndiName = (ndiName?.isEmpty ?? true) ? "NDI-\(shortId)" : ndiName!
        self.ndiGroups = ndiGroups.isEmpty ? "public" : ndiGroups
        self.width = 1920
        self.height = 1080
        self.fps = 60
        self.colorPreset = "rec709"
        self.httpPort = 0 // Auto-assign
        self.autoStart = false
    }
}

class StreamInstance: ObservableObject, Identifiable {
    let id: UUID
    @Published var config: StreamConfig
    @Published var isRunning: Bool = false
    @Published var status: StreamStatus?
    @Published var actualFps: Double = 0
    @Published var connections: Int = 0
    @Published var crashCount: Int = 0
    @Published var isHealthy: Bool = true
    @Published var lastError: String = ""
    @Published var errorTime: Date? = nil
    
    // Extended stats
    @Published var onProgram: Bool = false
    @Published var onPreview: Bool = false
    @Published var framesSent: UInt64 = 0
    @Published var framesDropped: UInt64 = 0
    @Published var dropRate: Double = 0
    @Published var uptimeSeconds: Double = 0
    @Published var bandwidthMbps: Double = 0
    
    var process: Process?
    var httpPort: Int = 0
    private var statusTimer: Timer?
    private var healthCheckTimer: Timer?
    private var workerPathCache: String = ""
    
    // Auto-restart configuration
    private var intentionalStop: Bool = false
    private var lastRestartTime: Date?
    private var restartAttempts: Int = 0
    private let maxRestartAttempts: Int = 5
    private let baseRestartDelay: TimeInterval = 2.0
    private var lastFpsUpdateTime: Date = Date()
    private var stalledFrameCount: Int = 0
    
    init(config: StreamConfig) {
        self.id = config.id
        self.config = config
    }
    
    func start(workerPath: String) {
        guard !isRunning else { return }
        
        // Clear previous error
        lastError = ""
        errorTime = nil
        
        // Check if worker exists
        if !FileManager.default.fileExists(atPath: workerPath) {
            lastError = "Worker not found at: \(workerPath)"
            errorTime = Date()
            logError(lastError)
            return
        }
        
        workerPathCache = workerPath
        intentionalStop = false
        
        // Find available port
        httpPort = config.httpPort > 0 ? config.httpPort : findAvailablePort()
        
        // Each stream needs its own cache directory to avoid CEF singleton conflicts
        let cacheDir = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first!
            .appendingPathComponent("HTML2NDI")
            .appendingPathComponent(id.uuidString)
        try? FileManager.default.createDirectory(at: cacheDir, withIntermediateDirectories: true)
        
        let process = Process()
        process.executableURL = URL(fileURLWithPath: workerPath)
        
        // Use test card URL if no URL specified or blank
        let effectiveUrl = (config.url.isEmpty || config.url == "about:blank") 
            ? "http://localhost:\(httpPort)/testcard" 
            : config.url
        
        var args = [
            "--url", effectiveUrl,
            "--ndi-name", config.ndiName,
            "--width", String(config.width),
            "--height", String(config.height),
            "--fps", String(config.fps),
            "--http-port", String(httpPort),
            "--cache-path", cacheDir.path
        ]
        
        // Add NDI groups if not "public" (public means all groups)
        if !config.ndiGroups.isEmpty && config.ndiGroups.lowercased() != "public" {
            args.append(contentsOf: ["--ndi-groups", config.ndiGroups])
        }
        
        process.arguments = args
        
        // Capture output for error detection
        let outputPipe = Pipe()
        let errorPipe = Pipe()
        process.standardOutput = outputPipe
        process.standardError = errorPipe
        
        // Read stderr for error messages
        errorPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if !data.isEmpty, let output = String(data: data, encoding: .utf8) {
                DispatchQueue.main.async {
                    // Capture last significant error
                    let lines = output.components(separatedBy: .newlines)
                    for line in lines where line.contains("ERROR") || line.contains("error") || line.contains("failed") {
                        self?.lastError = line.trimmingCharacters(in: .whitespaces)
                        self?.errorTime = Date()
                    }
                }
            }
        }
        
        process.terminationHandler = { [weak self] proc in
            // Stop reading
            errorPipe.fileHandleForReading.readabilityHandler = nil
            outputPipe.fileHandleForReading.readabilityHandler = nil
            
            DispatchQueue.main.async {
                self?.handleProcessTermination(exitCode: proc.terminationStatus)
            }
        }
        
        do {
            try process.run()
            self.process = process
            isRunning = true
            isHealthy = true
            stalledFrameCount = 0
            
            // Apply color preset and start monitoring
            DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in
                self?.applyColorPreset()
                self?.startStatusPolling()
                self?.startHealthCheck()
            }
            
            logInfo("Started stream '\(config.name)' on port \(httpPort)")
        } catch {
            lastError = "Failed to start: \(error.localizedDescription)"
            errorTime = Date()
            logError("Failed to start stream: \(error)")
        }
    }
    
    private func handleProcessTermination(exitCode: Int32) {
        isRunning = false
        statusTimer?.invalidate()
        healthCheckTimer?.invalidate()
        
        // Check if this was an intentional stop
        if intentionalStop {
            logInfo("Stream '\(config.name)' stopped intentionally")
            restartAttempts = 0
            lastError = ""
            errorTime = nil
            return
        }
        
        // Process crashed - log and attempt restart
        crashCount += 1
        
        // Set error message based on exit code
        if lastError.isEmpty {
            switch exitCode {
            case -1:
                lastError = "Process died unexpectedly"
            case 1:
                lastError = "Initialization failed (check NDI/CEF)"
            case 2:
                lastError = "Invalid configuration"
            case 9, 15:
                lastError = "Process was killed"
            case 127:
                lastError = "Worker executable not found"
            case 139:
                lastError = "Segmentation fault (crash)"
            default:
                lastError = "Exited with code \(exitCode)"
            }
            errorTime = Date()
        }
        
        logWarning("Stream '\(config.name)' terminated unexpectedly (exit: \(exitCode), crashes: \(crashCount))")
        
        // Check if we should auto-restart
        guard config.autoStart || restartAttempts > 0 else {
            logInfo("Auto-restart disabled for '\(config.name)'")
            return
        }
        
        // Check restart limits
        if restartAttempts >= maxRestartAttempts {
            logError("Stream '\(config.name)' exceeded max restart attempts (\(maxRestartAttempts))")
            restartAttempts = 0
            return
        }
        
        // Calculate delay with exponential backoff
        let delay = baseRestartDelay * pow(2.0, Double(restartAttempts))
        restartAttempts += 1
        
        logInfo("Restarting stream '\(config.name)' in \(Int(delay))s (attempt \(restartAttempts)/\(maxRestartAttempts))")
        
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self = self, !self.intentionalStop, !self.isRunning else { return }
            self.start(workerPath: self.workerPathCache)
        }
    }
    
    func stop() {
        logInfo("Stopping stream '\(config.name)'")
        intentionalStop = true
        restartAttempts = 0
        statusTimer?.invalidate()
        healthCheckTimer?.invalidate()
        process?.terminate()
        process = nil
        isRunning = false
    }
    
    private func startHealthCheck() {
        healthCheckTimer = Timer.scheduledTimer(withTimeInterval: 10.0, repeats: true) { [weak self] _ in
            self?.checkHealth()
        }
    }
    
    private func checkHealth() {
        guard isRunning else { return }
        
        // Check if FPS has been zero for too long (stalled stream)
        if actualFps < 0.5 {
            stalledFrameCount += 1
            if stalledFrameCount >= 3 { // 30 seconds of no frames
                isHealthy = false
                logWarning("Stream '\(config.name)' appears stalled (FPS: \(actualFps), stalled checks: \(stalledFrameCount))")
            }
        } else {
            stalledFrameCount = 0
            isHealthy = true
        }
        
        // Check if process is still responding
        if let proc = process, !proc.isRunning {
            logWarning("Stream '\(config.name)' process died unexpectedly")
            handleProcessTermination(exitCode: -1)
        }
    }
    
    func resetRestartCounter() {
        restartAttempts = 0
        crashCount = 0
    }
    
    func applyColorPreset() {
        guard httpPort > 0 else { return }
        
        let url = URL(string: "http://localhost:\(httpPort)/color")!
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONEncoder().encode(["preset": config.colorPreset])
        
        URLSession.shared.dataTask(with: request).resume()
    }
    
    func setURL(_ newURL: String) {
        guard httpPort > 0 else { return }
        
        config.url = newURL
        
        let url = URL(string: "http://localhost:\(httpPort)/seturl")!
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONEncoder().encode(["url": newURL])
        
        URLSession.shared.dataTask(with: request).resume()
    }
    
    private func startStatusPolling() {
        statusTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.fetchStatus()
        }
    }
    
    private func fetchStatus() {
        guard httpPort > 0, isRunning else { return }
        
        let url = URL(string: "http://localhost:\(httpPort)/status")!
        URLSession.shared.dataTask(with: url) { [weak self] data, _, _ in
            guard let data = data,
                  let status = try? JSONDecoder().decode(StreamStatus.self, from: data) else { return }
            
            DispatchQueue.main.async {
                self?.status = status
                self?.actualFps = status.actual_fps
                self?.connections = status.ndi_connections
                
                // Update tally state
                if let tally = status.tally {
                    self?.onProgram = tally.on_program
                    self?.onPreview = tally.on_preview
                }
                
                // Update frame statistics
                if let stats = status.stats {
                    self?.framesSent = stats.frames_sent
                    self?.framesDropped = stats.frames_dropped
                    self?.dropRate = stats.drop_rate
                    self?.uptimeSeconds = stats.uptime_seconds
                    self?.bandwidthMbps = stats.bandwidth_mbps
                }
            }
        }.resume()
    }
    
    private func findAvailablePort() -> Int {
        // Start from 8081 and find available port
        for port in 8081...8199 {
            if isPortAvailable(port) {
                return port
            }
        }
        return 8081
    }
    
    private func isPortAvailable(_ port: Int) -> Bool {
        let socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
        guard socketFD != -1 else { return false }
        defer { close(socketFD) }
        
        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = in_port_t(port).bigEndian
        addr.sin_addr.s_addr = INADDR_ANY
        
        let result = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(socketFD, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        
        return result == 0
    }
}

struct StreamStatus: Codable {
    let url: String
    let width: Int
    let height: Int
    let fps: Int
    let actual_fps: Double
    let ndi_name: String
    let ndi_connections: Int
    let running: Bool
    let color: ColorStatus?
    let tally: TallyStatus?
    let stats: FrameStats?
}

struct ColorStatus: Codable {
    let colorspace: String
    let gamma: String
    let range: String
}

struct TallyStatus: Codable {
    let on_program: Bool
    let on_preview: Bool
}

struct FrameStats: Codable {
    let frames_sent: UInt64
    let frames_dropped: UInt64
    let drop_rate: Double
    let uptime_seconds: Double
    let bandwidth_mbps: Double
}

class StreamManager: ObservableObject {
    static let shared = StreamManager()
    
    @Published var streams: [StreamInstance] = []
    
    private var workerPath: String {
        // Worker is bundled inside the app
        if let bundlePath = Bundle.main.resourcePath {
            return bundlePath + "/html2ndi.app/Contents/MacOS/html2ndi"
        }
        // Fallback to development path
        return FileManager.default.currentDirectoryPath + "/build/bin/html2ndi.app/Contents/MacOS/html2ndi"
    }
    
    private var configPath: URL {
        let appSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        let dir = appSupport.appendingPathComponent("HTML2NDI", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("streams.json")
    }
    
    func loadStreams() {
        logInfo("Loading streams from: \(configPath.path)")
        
        guard let data = try? Data(contentsOf: configPath),
              let configs = try? JSONDecoder().decode([StreamConfig].self, from: data) else {
            logInfo("No saved streams found, creating default")
            // Create default stream with test card (empty URL)
            addStream(StreamConfig(name: "Default Stream", url: "", ndiName: "HTML2NDI"))
            return
        }
        
        streams = configs.map { StreamInstance(config: $0) }
        logInfo("Loaded \(streams.count) stream(s)")
        
        // Auto-start streams
        for stream in streams where stream.config.autoStart {
            logInfo("Auto-starting stream '\(stream.config.name)'")
            stream.start(workerPath: workerPath)
        }
    }
    
    func saveStreams() {
        let configs = streams.map { $0.config }
        if let data = try? JSONEncoder().encode(configs) {
            do {
                try data.write(to: configPath)
                logDebug("Saved \(configs.count) stream(s) to config")
            } catch {
                logError("Failed to save streams: \(error)")
            }
        }
    }
    
    func addStream(_ config: StreamConfig = StreamConfig()) {
        logInfo("Adding stream '\(config.name)' with NDI name '\(config.ndiName)'")
        let instance = StreamInstance(config: config)
        streams.append(instance)
        saveStreams()
    }
    
    func removeStream(_ stream: StreamInstance) {
        logInfo("Removing stream '\(stream.config.name)'")
        stream.stop()
        streams.removeAll { $0.id == stream.id }
        saveStreams()
    }
    
    func startStream(_ stream: StreamInstance) {
        stream.start(workerPath: workerPath)
    }
    
    func stopStream(_ stream: StreamInstance) {
        stream.stop()
    }
    
    func startAll() {
        for stream in streams {
            stream.start(workerPath: workerPath)
        }
    }
    
    func stopAll() {
        for stream in streams {
            stream.stop()
        }
    }
    
    var runningCount: Int {
        streams.filter { $0.isRunning }.count
    }
}

