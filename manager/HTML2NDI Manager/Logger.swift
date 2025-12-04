import Foundation
import os.log

/// Unified logging for HTML2NDI Manager
/// Logs to Console.app and ~/Library/Logs/HTML2NDI/manager.log
final class AppLogger {
    static let shared = AppLogger()
    
    private let osLog: OSLog
    private var fileHandle: FileHandle?
    private var logFileURL: URL?
    private let dateFormatter: DateFormatter
    private let queue = DispatchQueue(label: "com.html2ndi.manager.logger")
    
    private init() {
        osLog = OSLog(subsystem: "com.html2ndi.manager", category: "general")
        
        dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyy-MM-dd HH:mm:ss.SSS"
        
        // Create log directory and file
        let logDir = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first!
            .appendingPathComponent("Logs/HTML2NDI", isDirectory: true)
        
        do {
            try FileManager.default.createDirectory(at: logDir, withIntermediateDirectories: true)
            let logFile = logDir.appendingPathComponent("manager.log")
            logFileURL = logFile
            
            // Create file if it doesn't exist
            if !FileManager.default.fileExists(atPath: logFile.path) {
                FileManager.default.createFile(atPath: logFile.path, contents: nil)
            }
            
            fileHandle = try FileHandle(forWritingTo: logFile)
            fileHandle?.seekToEndOfFile()
            
            os_log("Logger initialized, log file: %{public}@", log: osLog, type: .info, logFile.path)
        } catch {
            logFileURL = nil
            fileHandle = nil
            os_log("Failed to create log file: %{public}@", log: osLog, type: .error, error.localizedDescription)
        }
    }
    
    deinit {
        try? fileHandle?.close()
    }
    
    func debug(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
        log(message, level: .debug, file: file, function: function, line: line)
    }
    
    func info(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
        log(message, level: .info, file: file, function: function, line: line)
    }
    
    func warning(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
        log(message, level: .default, file: file, function: function, line: line)
    }
    
    func error(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
        log(message, level: .error, file: file, function: function, line: line)
    }
    
    private func log(_ message: String, level: OSLogType, file: String, function: String, line: Int) {
        let filename = (file as NSString).lastPathComponent
        let timestamp = dateFormatter.string(from: Date())
        let levelStr: String
        
        switch level {
        case .debug: levelStr = "DEBUG"
        case .info: levelStr = "INFO "
        case .default: levelStr = "WARN "
        case .error: levelStr = "ERROR"
        case .fault: levelStr = "FATAL"
        default: levelStr = "?????"
        }
        
        let logLine = "[\(timestamp)] [\(levelStr)] [\(filename):\(line)] \(message)\n"
        
        // Log to Console.app
        os_log("%{public}@", log: osLog, type: level, message)
        
        // Log to file
        queue.async { [weak self] in
            guard let data = logLine.data(using: .utf8) else { return }
            self?.fileHandle?.write(data)
            
            // Rotate file if needed (10MB max)
            self?.rotateIfNeeded()
        }
        
        // Also print for development
        #if DEBUG
        print(logLine, terminator: "")
        #endif
    }
    
    private func rotateIfNeeded() {
        guard let url = logFileURL else { return }
        
        do {
            let attrs = try FileManager.default.attributesOfItem(atPath: url.path)
            let size = attrs[.size] as? UInt64 ?? 0
            
            if size > 10 * 1024 * 1024 { // 10MB
                try? fileHandle?.close()
                
                // Rotate: remove old, rename current
                let rotatedURL = url.deletingPathExtension().appendingPathExtension("1.log")
                try? FileManager.default.removeItem(at: rotatedURL)
                try FileManager.default.moveItem(at: url, to: rotatedURL)
                
                // Create new file
                FileManager.default.createFile(atPath: url.path, contents: nil)
            }
        } catch {
            os_log("Log rotation failed: %{public}@", log: osLog, type: .error, error.localizedDescription)
        }
    }
}

// Global convenience functions
func logDebug(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
    AppLogger.shared.debug(message, file: file, function: function, line: line)
}

func logInfo(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
    AppLogger.shared.info(message, file: file, function: function, line: line)
}

func logWarning(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
    AppLogger.shared.warning(message, file: file, function: function, line: line)
}

func logError(_ message: String, file: String = #file, function: String = #function, line: Int = #line) {
    AppLogger.shared.error(message, file: file, function: function, line: line)
}

