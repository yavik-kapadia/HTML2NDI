import SwiftUI

struct MenuBarView: View {
    @ObservedObject var manager = StreamManager.shared
    @State private var showingAddStream = false
    
    var body: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Image(systemName: "tv.fill")
                    .foregroundColor(.accentColor)
                Text("HTML2NDI")
                    .font(.headline)
                Spacer()
                Text("\(manager.runningCount)/\(manager.streams.count)")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(Color(NSColor.controlBackgroundColor))
            
            Divider()
            
            // Streams List
            ScrollView {
                LazyVStack(spacing: 8) {
                    ForEach(manager.streams) { stream in
                        StreamRowView(stream: stream)
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
            }
            .frame(maxHeight: 200)
            
            Divider()
            
            // Actions
            VStack(spacing: 4) {
                Button(action: openDashboard) {
                    Label("Open Dashboard", systemImage: "globe")
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .buttonStyle(.plain)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(Color.clear)
                .contentShape(Rectangle())
                
                Divider().padding(.horizontal, 8)
                
                HStack(spacing: 12) {
                    Button("Start All") {
                        manager.startAll()
                    }
                    .buttonStyle(.bordered)
                    
                    Button("Stop All") {
                        manager.stopAll()
                    }
                    .buttonStyle(.bordered)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                
                Divider().padding(.horizontal, 8)
                
                Button(action: { NSApp.terminate(nil) }) {
                    Label("Quit HTML2NDI", systemImage: "power")
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .foregroundColor(.red)
                }
                .buttonStyle(.plain)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
            }
            .padding(.bottom, 8)
        }
        .frame(width: 320)
    }
    
    func openDashboard() {
        if let url = URL(string: "http://localhost:8080") {
            NSWorkspace.shared.open(url)
        }
    }
}

struct StreamRowView: View {
    @ObservedObject var stream: StreamInstance
    @ObservedObject var manager = StreamManager.shared
    
    var body: some View {
        HStack(spacing: 10) {
            // Status indicator
            Circle()
                .fill(stream.isRunning ? Color.green : Color.gray)
                .frame(width: 8, height: 8)
            
            VStack(alignment: .leading, spacing: 2) {
                Text(stream.config.ndiName)
                    .font(.system(size: 13, weight: .medium))
                
                if stream.isRunning {
                    Text("\(stream.connections) conn • \(String(format: "%.1f", stream.actualFps)) fps")
                        .font(.system(size: 10))
                        .foregroundColor(.secondary)
                } else {
                    Text("Stopped")
                        .font(.system(size: 10))
                        .foregroundColor(.secondary)
                }
            }
            
            Spacer()
            
            // Play/Stop button
            Button(action: {
                if stream.isRunning {
                    manager.stopStream(stream)
                } else {
                    manager.startStream(stream)
                }
            }) {
                Image(systemName: stream.isRunning ? "stop.fill" : "play.fill")
                    .foregroundColor(stream.isRunning ? .red : .green)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(Color(NSColor.controlBackgroundColor).opacity(0.5))
        .cornerRadius(8)
    }
}



