import SwiftUI

struct DashboardView: View {
    @ObservedObject private var manager = StreamManager.shared
    @State private var showingAddStream = false
    @State private var selectedStream: StreamInstance?
    @State private var searchText = ""
    
    var filteredStreams: [StreamInstance] {
        if searchText.isEmpty {
            return manager.streams
        }
        return manager.streams.filter { stream in
            stream.config.name.localizedCaseInsensitiveContains(searchText) ||
            stream.config.ndiName.localizedCaseInsensitiveContains(searchText) ||
            stream.config.url.localizedCaseInsensitiveContains(searchText)
        }
    }
    
    var body: some View {
        NavigationSplitView {
            // Sidebar - Stream List
            VStack(spacing: 0) {
                // Header with search
                VStack(spacing: 8) {
                    HStack {
                        Text("Streams")
                            .font(.headline)
                        Spacer()
                        Text("\(manager.streams.count)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 4)
                            .background(Color.accentColor.opacity(0.1))
                            .cornerRadius(8)
                    }
                    .padding(.horizontal)
                    .padding(.top, 12)
                    
                    TextField("Search streams...", text: $searchText)
                        .textFieldStyle(.roundedBorder)
                        .padding(.horizontal)
                }
                .padding(.bottom, 8)
                
                Divider()
                
                // Stream List
                List(selection: $selectedStream) {
                    ForEach(filteredStreams, id: \.id) { stream in
                        StreamSidebarRow(stream: stream)
                            .tag(stream)
                    }
                }
                .listStyle(.sidebar)
                
                Divider()
                
                // Bottom toolbar
                HStack(spacing: 12) {
                    Button(action: { showingAddStream = true }) {
                        Label("Add", systemImage: "plus")
                    }
                    .buttonStyle(.bordered)
                    
                    Spacer()
                    
                    Button(action: { manager.startAll() }) {
                        Label("Start All", systemImage: "play.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(manager.streams.allSatisfy { $0.isRunning })
                    
                    Button(action: { manager.stopAll() }) {
                        Label("Stop All", systemImage: "stop.fill")
                    }
                    .buttonStyle(.bordered)
                    .tint(.red)
                    .disabled(manager.streams.allSatisfy { !$0.isRunning })
                }
                .padding()
            }
            .frame(minWidth: 250)
        } detail: {
            // Detail View
            if let stream = selectedStream {
                StreamDetailView(stream: stream)
            } else {
                VStack(spacing: 20) {
                    Image(systemName: "tv.and.mediabox")
                        .font(.system(size: 64))
                        .foregroundColor(.secondary)
                    
                    Text("No Stream Selected")
                        .font(.title2)
                        .foregroundColor(.secondary)
                    
                    Text("Select a stream from the sidebar to view details and controls")
                        .font(.body)
                        .foregroundColor(.secondary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 40)
                    
                    Button(action: { showingAddStream = true }) {
                        Label("Add Your First Stream", systemImage: "plus.circle.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .sheet(isPresented: $showingAddStream) {
            AddStreamView()
        }
        .toolbar {
            ToolbarItem(placement: .navigation) {
                Button(action: {
                    if let url = URL(string: "http://localhost:8080") {
                        NSWorkspace.shared.open(url)
                    }
                }) {
                    Label("Open Web Dashboard", systemImage: "globe")
                }
            }
            
            ToolbarItem(placement: .automatic) {
                Button(action: { showingAddStream = true }) {
                    Label("Add Stream", systemImage: "plus")
                }
            }
        }
    }
}

struct StreamSidebarRow: View {
    @ObservedObject var stream: StreamInstance
    
    var statusColor: Color {
        if !stream.isRunning { return .secondary }
        if !stream.lastError.isEmpty { return .red }
        if !stream.isHealthy { return .orange }
        return .green
    }
    
    var body: some View {
        HStack(spacing: 12) {
            // Status indicator
            Circle()
                .fill(statusColor)
                .frame(width: 10, height: 10)
                .shadow(color: statusColor.opacity(0.5), radius: stream.isRunning ? 2 : 0)
            
            VStack(alignment: .leading, spacing: 4) {
                Text(stream.config.name)
                    .font(.body)
                    .lineLimit(1)
                
                HStack(spacing: 6) {
                    Image(systemName: "antenna.radiowaves.left.and.right")
                        .font(.caption2)
                    Text(stream.config.ndiName)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .lineLimit(1)
                }
            }
            
            Spacer()
            
            if stream.isRunning {
                VStack(alignment: .trailing, spacing: 2) {
                    Text(String(format: "%.1f fps", stream.actualFps))
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    if stream.connections > 0 {
                        HStack(spacing: 4) {
                            Image(systemName: "person.2.fill")
                                .font(.caption2)
                            Text("\(stream.connections)")
                        }
                        .font(.caption)
                        .foregroundColor(.blue)
                    }
                }
            }
        }
        .padding(.vertical, 4)
        .contextMenu {
            Button(action: {
                if stream.isRunning {
                    stream.stop()
                } else {
                    StreamManager.shared.startStream(stream)
                }
            }) {
                Label(stream.isRunning ? "Stop" : "Start", 
                      systemImage: stream.isRunning ? "stop.fill" : "play.fill")
            }
            
            Divider()
            
            Button(action: {
                if let url = URL(string: "http://localhost:\(stream.httpPort)") {
                    NSWorkspace.shared.open(url)
                }
            }) {
                Label("Open Control Panel", systemImage: "slider.horizontal.3")
            }
            .disabled(!stream.isRunning)
            
            Divider()
            
            Button(role: .destructive, action: {
                StreamManager.shared.removeStream(stream)
            }) {
                Label("Delete", systemImage: "trash")
            }
        }
    }
}

struct StreamDetailView: View {
    @ObservedObject var stream: StreamInstance
    @State private var editedName: String = ""
    @State private var editedURL: String = ""
    @State private var editedNdiName: String = ""
    
    init(stream: StreamInstance) {
        self.stream = stream
        _editedName = State(initialValue: stream.config.name)
        _editedURL = State(initialValue: stream.config.url)
        _editedNdiName = State(initialValue: stream.config.ndiName)
    }
    
    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Header
                VStack(spacing: 12) {
                    HStack {
                        Image(systemName: "tv.fill")
                            .font(.system(size: 48))
                            .foregroundColor(.accentColor)
                        
                        Spacer()
                        
                        StatusBadge(stream: stream)
                    }
                    
                    HStack {
                        Text(stream.config.name)
                            .font(.title)
                            .fontWeight(.bold)
                        
                        Spacer()
                        
                        // Control buttons
                        if stream.isRunning {
                            Button(action: { stream.stop() }) {
                                Label("Stop", systemImage: "stop.fill")
                            }
                            .buttonStyle(.bordered)
                            .tint(.red)
                        } else {
                            Button(action: { StreamManager.shared.startStream(stream) }) {
                                Label("Start", systemImage: "play.fill")
                            }
                            .buttonStyle(.borderedProminent)
                        }
                    }
                }
                .padding()
                .background(Color(.controlBackgroundColor))
                .cornerRadius(12)
                
                // Stats Grid
                if stream.isRunning {
                    LazyVGrid(columns: [
                        GridItem(.flexible()),
                        GridItem(.flexible()),
                        GridItem(.flexible())
                    ], spacing: 16) {
                        StatCard(title: "Frame Rate", value: String(format: "%.1f fps", stream.actualFps), icon: "film")
                        StatCard(title: "Connections", value: "\(stream.connections)", icon: "person.2.fill")
                        StatCard(title: "Resolution", value: "\(stream.config.width)×\(stream.config.height)", icon: "aspectratio")
                        StatCard(title: "Bandwidth", value: String(format: "%.1f Mbps", stream.bandwidthMbps), icon: "speedometer")
                        StatCard(title: "Frames Sent", value: "\(stream.framesSent)", icon: "arrow.up.circle")
                        StatCard(title: "Uptime", value: formatUptime(stream.uptimeSeconds), icon: "clock")
                    }
                }
                
                // Configuration
                GroupBox("Configuration") {
                    VStack(spacing: 16) {
                        LabeledTextField(label: "Stream Name", text: $editedName)
                        LabeledTextField(label: "NDI Source Name", text: $editedNdiName)
                        LabeledTextField(label: "Source URL", text: $editedURL)
                        
                        HStack(spacing: 12) {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Resolution")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                                Text("\(stream.config.width) × \(stream.config.height)")
                            }
                            
                            Spacer()
                            
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Frame Rate")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                                Text("\(stream.config.fps) fps")
                            }
                            
                            Spacer()
                            
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Color Preset")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                                Text(stream.config.colorPreset)
                            }
                        }
                        .padding(.vertical, 8)
                        
                        Button(action: saveChanges) {
                            Label("Save Changes", systemImage: "checkmark.circle.fill")
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(!hasChanges)
                    }
                    .padding()
                }
                
                // Actions
                GroupBox("Actions") {
                    VStack(spacing: 12) {
                        Button(action: {
                            if let url = URL(string: "http://localhost:\(stream.httpPort)") {
                                NSWorkspace.shared.open(url)
                            }
                        }) {
                            Label("Open Control Panel", systemImage: "slider.horizontal.3")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                        .disabled(!stream.isRunning)
                        
                        Button(role: .destructive, action: {
                            StreamManager.shared.removeStream(stream)
                        }) {
                            Label("Delete Stream", systemImage: "trash")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                    }
                    .padding()
                }
            }
            .padding()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
    
    private var hasChanges: Bool {
        editedName != stream.config.name ||
        editedURL != stream.config.url ||
        editedNdiName != stream.config.ndiName
    }
    
    private func saveChanges() {
        stream.config.name = editedName
        stream.config.url = editedURL
        stream.config.ndiName = editedNdiName
        
        if stream.isRunning {
            stream.setURL(editedURL)
        }
        
        StreamManager.shared.saveStreams()
    }
    
    private func formatUptime(_ seconds: Double) -> String {
        let hours = Int(seconds) / 3600
        let minutes = (Int(seconds) % 3600) / 60
        if hours > 0 {
            return "\(hours)h \(minutes)m"
        } else {
            return "\(minutes)m"
        }
    }
}

struct StatusBadge: View {
    @ObservedObject var stream: StreamInstance
    
    var statusText: String {
        if !stream.isRunning { return "Stopped" }
        if !stream.lastError.isEmpty { return "Error" }
        if !stream.isHealthy { return "Stalled" }
        return "Running"
    }
    
    var statusColor: Color {
        if !stream.isRunning { return .secondary }
        if !stream.lastError.isEmpty { return .red }
        if !stream.isHealthy { return .orange }
        return .green
    }
    
    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(statusColor)
                .frame(width: 8, height: 8)
            Text(statusText)
                .font(.caption)
                .fontWeight(.medium)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(statusColor.opacity(0.15))
        .cornerRadius(12)
    }
}

struct StatCard: View {
    let title: String
    let value: String
    let icon: String
    
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: icon)
                .font(.title2)
                .foregroundColor(.accentColor)
            
            Text(value)
                .font(.title3)
                .fontWeight(.semibold)
            
            Text(title)
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(.controlBackgroundColor))
        .cornerRadius(12)
    }
}

struct LabeledTextField: View {
    let label: String
    @Binding var text: String
    
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
            TextField(label, text: $text)
                .textFieldStyle(.roundedBorder)
        }
    }
}

struct AddStreamView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var url = ""
    @State private var ndiName = ""
    @State private var width = 1920
    @State private var height = 1080
    @State private var fps = 60
    @State private var autoStart = false
    
    var body: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Text("Add New Stream")
                    .font(.title2)
                    .fontWeight(.bold)
                
                Spacer()
                
                Button(action: { dismiss() }) {
                    Image(systemName: "xmark.circle.fill")
                        .font(.title2)
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            }
            .padding()
            
            Divider()
            
            // Form
            Form {
                Section("Basic Information") {
                    TextField("Stream Name", text: $name, prompt: Text("My Graphics"))
                    TextField("Source URL", text: $url, prompt: Text("https://example.com or leave blank for test card"))
                    TextField("NDI Source Name", text: $ndiName, prompt: Text("Graphics 1"))
                }
                
                Section("Video Settings") {
                    HStack {
                        Text("Width")
                        Spacer()
                        TextField("Width", value: $width, format: .number)
                            .frame(width: 100)
                            .multilineTextAlignment(.trailing)
                    }
                    
                    HStack {
                        Text("Height")
                        Spacer()
                        TextField("Height", value: $height, format: .number)
                            .frame(width: 100)
                            .multilineTextAlignment(.trailing)
                    }
                    
                    HStack {
                        Text("Frame Rate")
                        Spacer()
                        TextField("FPS", value: $fps, format: .number)
                            .frame(width: 100)
                            .multilineTextAlignment(.trailing)
                        Text("fps")
                    }
                }
                
                Section {
                    Toggle("Auto-start on launch", isOn: $autoStart)
                }
            }
            .formStyle(.grouped)
            
            Divider()
            
            // Footer
            HStack(spacing: 12) {
                Button("Cancel") {
                    dismiss()
                }
                .buttonStyle(.bordered)
                
                Spacer()
                
                Button("Add Stream") {
                    addStream()
                }
                .buttonStyle(.borderedProminent)
                .disabled(ndiName.isEmpty)
            }
            .padding()
        }
        .frame(width: 500, height: 600)
    }
    
    private func addStream() {
        var config = StreamConfig(
            name: name.isEmpty ? "Stream \(StreamManager.shared.streams.count + 1)" : name,
            url: url,
            ndiName: ndiName
        )
        config.width = width
        config.height = height
        config.fps = fps
        config.autoStart = autoStart
        
        StreamManager.shared.addStream(config)
        dismiss()
    }
}

