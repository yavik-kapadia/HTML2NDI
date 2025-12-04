import SwiftUI
import AppKit

@main
struct HTML2NDIManagerApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    var body: some Scene {
        Settings {
            SettingsView()
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    var statusItem: NSStatusItem?
    var popover: NSPopover?
    var streamManager = StreamManager.shared
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Hide dock icon - menu bar only
        NSApp.setActivationPolicy(.accessory)
        
        // Create status bar item
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        
        if let button = statusItem?.button {
            button.image = NSImage(systemSymbolName: "tv.fill", accessibilityDescription: "HTML2NDI")
            button.action = #selector(togglePopover)
            button.target = self
        }
        
        // Create popover
        popover = NSPopover()
        popover?.contentSize = NSSize(width: 320, height: 400)
        popover?.behavior = .transient
        popover?.contentViewController = NSHostingController(rootView: MenuBarView())
        
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
    
    @objc func togglePopover() {
        if let button = statusItem?.button {
            if popover?.isShown == true {
                popover?.performClose(nil)
            } else {
                popover?.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
                NSApp.activate(ignoringOtherApps: true)
            }
        }
    }
    
    func openDashboard() {
        if let url = URL(string: "http://localhost:8080") {
            NSWorkspace.shared.open(url)
        }
    }
}

