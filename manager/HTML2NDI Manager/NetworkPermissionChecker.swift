//
//  NetworkPermissionChecker.swift
//  HTML2NDI Manager
//
//  Checks and requests local network permissions for NDI streaming
//

import Foundation
import Network
import SwiftUI

class NetworkPermissionChecker: ObservableObject {
    @Published var hasNetworkPermission: Bool = false
    @Published var permissionStatus: PermissionStatus = .unknown
    @Published var showPermissionAlert: Bool = false
    
    enum PermissionStatus {
        case unknown
        case checking
        case granted
        case denied
        case restricted
    }
    
    private var monitor: NWPathMonitor?
    private let queue = DispatchQueue(label: "com.html2ndi.network.monitor")
    
    init() {
        checkNetworkPermission()
    }
    
    /// Check if local network permission is granted
    func checkNetworkPermission() {
        permissionStatus = .checking
        
        // Create a network path monitor
        monitor = NWPathMonitor()
        
        monitor?.pathUpdateHandler = { [weak self] path in
            DispatchQueue.main.async {
                // If we can see the network path, we have permission
                if path.status == .satisfied {
                    self?.hasNetworkPermission = true
                    self?.permissionStatus = .granted
                } else if path.status == .unsatisfied {
                    // Could be denied or no network
                    self?.hasNetworkPermission = false
                    self?.permissionStatus = .denied
                }
                
                // Stop monitoring after initial check
                self?.monitor?.cancel()
            }
        }
        
        monitor?.start(queue: queue)
        
        // Also try to bind a socket to force permission prompt
        DispatchQueue.global().async { [weak self] in
            self?.triggerPermissionPrompt()
        }
    }
    
    /// Trigger the system permission prompt by attempting network access
    private func triggerPermissionPrompt() {
        // Create a simple UDP socket to trigger the local network permission prompt
        let socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard socket >= 0 else { return }
        
        // Try to bind to any available port (this triggers the permission prompt)
        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = 0  // Any port
        addr.sin_addr.s_addr = INADDR_ANY
        
        withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                let _ = bind(socket, sockaddrPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        
        close(socket)
        
        // Wait a moment and check again
        Thread.sleep(forTimeInterval: 1.0)
        DispatchQueue.main.async { [weak self] in
            self?.recheckPermission()
        }
    }
    
    /// Recheck permission after a delay
    private func recheckPermission() {
        // If still denied after prompt, show alert
        if permissionStatus == .denied {
            showPermissionAlert = true
        }
    }
    
    /// Show alert to guide user to System Settings
    func showPermissionDeniedAlert() -> Alert {
        Alert(
            title: Text("Local Network Access Required"),
            message: Text("""
                HTML2NDI Manager needs local network access to:
                • Communicate with worker processes
                • Stream NDI video over your local network
                • Discover NDI sources
                
                Please enable local network access in System Settings.
                """),
            primaryButton: .default(Text("Open System Settings")) {
                self.openSystemSettings()
            },
            secondaryButton: .cancel()
        )
    }
    
    /// Open System Settings to the Privacy & Security > Local Network section
    func openSystemSettings() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_LocalNetwork") {
            NSWorkspace.shared.open(url)
        }
    }
    
    /// Test network connectivity to a specific address
    func testNetworkConnection(host: String, port: Int, completion: @escaping (Bool) -> Void) {
        let connection = NWConnection(
            host: NWEndpoint.Host(host),
            port: NWEndpoint.Port(integerLiteral: UInt16(port)),
            using: .tcp
        )
        
        connection.stateUpdateHandler = { state in
            switch state {
            case .ready:
                completion(true)
                connection.cancel()
            case .failed, .cancelled:
                completion(false)
            default:
                break
            }
        }
        
        connection.start(queue: queue)
        
        // Timeout after 5 seconds
        DispatchQueue.global().asyncAfter(deadline: .now() + 5) {
            connection.cancel()
            completion(false)
        }
    }
}

// Extension to check NDI-specific network access
extension NetworkPermissionChecker {
    /// Check if NDI port (5960) is accessible
    func checkNDIAccess(completion: @escaping (Bool) -> Void) {
        // NDI uses UDP port 5960 for discovery
        let socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard socket >= 0 else {
            completion(false)
            return
        }
        
        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = UInt16(5960).bigEndian
        addr.sin_addr.s_addr = INADDR_ANY
        
        let result = withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                bind(socket, sockaddrPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        
        close(socket)
        completion(result == 0)
    }
    
    /// Get user-friendly permission status message
    var statusMessage: String {
        switch permissionStatus {
        case .unknown:
            return "Checking network permissions..."
        case .checking:
            return "Checking network permissions..."
        case .granted:
            return "Local network access granted ✓"
        case .denied:
            return "Local network access denied - NDI streaming will not work"
        case .restricted:
            return "Local network access restricted by system policy"
        }
    }
    
    /// Color for status indicator
    var statusColor: Color {
        switch permissionStatus {
        case .granted:
            return .green
        case .denied, .restricted:
            return .red
        case .checking, .unknown:
            return .orange
        }
    }
}


