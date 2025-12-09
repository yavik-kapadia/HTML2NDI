import SwiftUI
import AppKit

@main
struct HTML2NDIManagerApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    var body: some Scene {
        WindowGroup {
            DashboardView()
                .frame(minWidth: 800, minHeight: 600)
        }
        .windowStyle(.titleBar)
        .windowToolbarStyle(.unified)
        .commands {
            CommandGroup(after: .appInfo) {
                Button("Open Web Dashboard") {
                    appDelegate.openDashboard()
                }
                .keyboardShortcut("d", modifiers: .command)
            }
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    var statusItem: NSStatusItem?
    var streamManager = StreamManager.shared
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Show in dock (we have a window now!)
        NSApp.setActivationPolicy(.regular)
        
        // Create menu bar item for quick access
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        
        if let button = statusItem?.button {
            button.image = NSImage(systemSymbolName: "tv.fill", accessibilityDescription: "HTML2NDI")
            button.action = #selector(showMainWindow)
            button.target = self
        }
        
        // Create menu bar menu
        let menu = NSMenu()
        
        menu.addItem(NSMenuItem(title: "Show Dashboard", action: #selector(showMainWindow), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Open Web Dashboard", action: #selector(openDashboard), keyEquivalent: "d"))
        menu.addItem(NSMenuItem.separator())
        
        let startAllItem = NSMenuItem(title: "Start All Streams", action: #selector(startAllStreams), keyEquivalent: "")
        let stopAllItem = NSMenuItem(title: "Stop All Streams", action: #selector(stopAllStreams), keyEquivalent: "")
        menu.addItem(startAllItem)
        menu.addItem(stopAllItem)
        
        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        
        statusItem?.menu = menu
        
        // Load saved streams
        streamManager.loadStreams()
        
        // Start the manager HTTP server
        ManagerServer.shared.start()
        
        logInfo("HTML2NDI Manager started")
        logInfo("Dashboard: http://localhost:8080")
    }
    
    func applicationWillTerminate(_ notification: Notification) {
        streamManager.stopAll()
        ManagerServer.shared.stop()
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        // Don't quit when window is closed - stay in menu bar
        return false
    }
    
    @objc func showMainWindow() {
        NSApp.activate(ignoringOtherApps: true)
        for window in NSApplication.shared.windows {
            if window.title.contains("HTML2NDI") || window.isVisible {
                window.makeKeyAndOrderFront(nil)
                return
            }
        }
    }
    
    @objc func openDashboard() {
        if let url = URL(string: "http://localhost:8080") {
            NSWorkspace.shared.open(url)
        }
    }
    
    @objc func startAllStreams() {
        streamManager.startAll()
    }
    
    @objc func stopAllStreams() {
        streamManager.stopAll()
    }
}

