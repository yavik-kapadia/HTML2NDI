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
        
        // Actually try to bind to the ports we'll use to test permission
        DispatchQueue.global().async { [weak self] in
            self?.testActualNetworkAccess()
        }
    }
    
    /// Test actual network access by binding to ports the app will use
    private func testActualNetworkAccess() {
        var hasPermission = false
        
        // Test 1: Try to bind to port 8080 (web dashboard)
        let socket1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
        if socket1 >= 0 {
            var addr = sockaddr_in()
            addr.sin_family = sa_family_t(AF_INET)
            addr.sin_port = UInt16(8080).bigEndian
            addr.sin_addr.s_addr = INADDR_ANY
            
            let bindResult = withUnsafePointer(to: &addr) { ptr in
                ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                    bind(socket1, sockaddrPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
            
            close(socket1)
            
            if bindResult == 0 {
                hasPermission = true
            }
        }
        
        // Test 2: Try UDP on port 5960 (NDI discovery) - this is most important
        if !hasPermission {
            let socket2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
            if socket2 >= 0 {
                var addr = sockaddr_in()
                addr.sin_family = sa_family_t(AF_INET)
                addr.sin_port = UInt16(5960).bigEndian
                addr.sin_addr.s_addr = INADDR_ANY
                
                let bindResult = withUnsafePointer(to: &addr) { ptr in
                    ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                        bind(socket2, sockaddrPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
                    }
                }
                
                close(socket2)
                
                if bindResult == 0 {
                    hasPermission = true
                }
            }
        }
        
        // Wait a moment for any permission dialogs
        Thread.sleep(forTimeInterval: 0.5)
        
        DispatchQueue.main.async { [weak self] in
            self?.hasNetworkPermission = hasPermission
            self?.permissionStatus = hasPermission ? .granted : .denied
            
            if !hasPermission {
                self?.showPermissionAlert = true
            }
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


