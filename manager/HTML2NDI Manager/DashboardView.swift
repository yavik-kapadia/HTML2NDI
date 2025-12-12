import SwiftUI

struct DashboardView: View {
    @ObservedObject private var manager = StreamManager.shared
    @StateObject private var networkChecker = NetworkPermissionChecker()
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
                    
                    // Network Permission Status (always visible)
                    VStack(spacing: 6) {
                        HStack(spacing: 8) {
                            Image(systemName: networkChecker.permissionStatus == .denied ? "exclamationmark.triangle.fill" : 
                                            networkChecker.permissionStatus == .granted ? "checkmark.circle.fill" : "network")
                                .foregroundColor(networkChecker.statusColor)
                            
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Network Permissions")
                                    .font(.caption)
                                    .fontWeight(.medium)
                                Text(networkChecker.permissionStatus == .granted ? "Local network access enabled" : networkChecker.statusMessage)
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                            }
                            
                            Spacer()
                            
                            Menu {
                                Button(action: { networkChecker.checkNetworkPermission() }) {
                                    Label("Recheck", systemImage: "arrow.clockwise")
                                }
                                
                                if networkChecker.permissionStatus != .granted {
                                    Divider()
                                    Button(action: { networkChecker.openSystemSettings() }) {
                                        Label("Open Settings", systemImage: "gear")
                                    }
                                }
                            } label: {
                                Image(systemName: "ellipsis.circle")
                                    .foregroundColor(.secondary)
                            }
                            .menuStyle(.borderlessButton)
                            .fixedSize()
                        }
                    }
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
                    .background(networkChecker.statusColor.opacity(0.1))
                    .cornerRadius(8)
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
                                HStack(spacing: 4) {
                                    Text("Resolution")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                    Image(systemName: "info.circle")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                        .help("Resolution is set at stream creation and cannot be changed at runtime. Delete and recreate the stream to use a different resolution.")
                                }
                                Text("\(stream.config.width) × \(stream.config.height)\(stream.config.progressive ? "p" : "i")")
                            }
                            
                            Spacer()
                            
                            VStack(alignment: .leading, spacing: 4) {
                                HStack(spacing: 4) {
                                    Text("Frame Rate")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                    Image(systemName: "info.circle")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                        .help("Frame rate is set at stream creation and cannot be changed at runtime. Delete and recreate the stream to use a different frame rate.")
                                }
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
                        
                        Divider()
                        
                        // Genlock Configuration
                        VStack(alignment: .leading, spacing: 12) {
                            Text("Genlock Synchronization")
                                .font(.headline)
                            
                            Picker("Mode", selection: Binding(
                                get: { stream.config.genlockMode },
                                set: { newMode in
                                    stream.config.genlockMode = newMode
                                    if stream.isRunning {
                                        applyGenlockChange(stream: stream, mode: newMode, address: stream.config.genlockMasterAddr)
                                    }
                                }
                            )) {
                                Text("Disabled").tag("disabled")
                                Text("Master").tag("master")
                                Text("Slave").tag("slave")
                            }
                            .pickerStyle(.segmented)
                            
                            if stream.config.genlockMode == "slave" {
                                LabeledTextField(
                                    label: "Master Address",
                                    text: Binding(
                                        get: { stream.config.genlockMasterAddr },
                                        set: { newAddr in
                                            stream.config.genlockMasterAddr = newAddr
                                            if stream.isRunning {
                                                applyGenlockChange(stream: stream, mode: stream.config.genlockMode, address: newAddr)
                                            }
                                        }
                                    )
                                )
                            }
                            
                            Text(genlockDescription(for: stream.config.genlockMode))
                                .font(.caption)
                                .foregroundColor(.secondary)
                                .padding(.horizontal, 12)
                                .padding(.vertical, 8)
                                .background(Color.accentColor.opacity(0.1))
                                .cornerRadius(8)
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
    
    private func applyGenlockChange(stream: StreamInstance, mode: String, address: String) {
        guard stream.isRunning, stream.httpPort > 0 else { return }
        
        var body: [String: Any] = ["mode": mode]
        if mode == "slave" && !address.isEmpty {
            body["master_address"] = address
        }
        
        guard let jsonData = try? JSONSerialization.data(withJSONObject: body),
              let url = URL(string: "http://localhost:\(stream.httpPort)/genlock") else {
            return
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.httpBody = jsonData
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                logError("Genlock update failed: \(error.localizedDescription)")
            } else {
                logInfo("Genlock updated to \(mode)")
                StreamManager.shared.saveStreams()
            }
        }.resume()
    }
    
    private func genlockDescription(for mode: String) -> String {
        switch mode {
        case "master":
            return "⏱ This stream provides timing for all slave streams"
        case "slave":
            return "🔗 Syncs frame timing with the master stream"
        default:
            return "⏸ Independent frame timing (no synchronization)"
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

// MARK: - Standard Video Formats

struct VideoFormat: Identifiable, Hashable {
    let id = UUID()
    let width: Int
    let height: Int
    let name: String
    
    // Hash based on content, not UUID
    func hash(into hasher: inout Hasher) {
        hasher.combine(width)
        hasher.combine(height)
        hasher.combine(name)
    }
    
    // Compare based on content, not UUID
    static func == (lhs: VideoFormat, rhs: VideoFormat) -> Bool {
        return lhs.width == rhs.width && lhs.height == rhs.height && lhs.name == rhs.name
    }
    
    var displayName: String { "\(name) (\(width)×\(height))" }
}

let standardFormats: [VideoFormat] = [
    VideoFormat(width: 3840, height: 2160, name: "4K UHD"),
    VideoFormat(width: 2560, height: 1440, name: "QHD"),
    VideoFormat(width: 1920, height: 1080, name: "1080p"),
    VideoFormat(width: 1280, height: 720, name: "720p"),
    VideoFormat(width: 1024, height: 768, name: "XGA"),
    VideoFormat(width: 854, height: 480, name: "FWVGA"),
    VideoFormat(width: 640, height: 480, name: "SD")
]

let standardFramerates = [24, 25, 30, 50, 60]

struct AddStreamView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var url = ""
    @State private var ndiName = ""
    @State private var selectedFormat = standardFormats[2] // Default: 1080p
    @State private var fps = 60
    @State private var progressive = true
    @State private var autoStart = false
    @State private var genlockMode = "disabled"
    @State private var genlockMasterAddr = "127.0.0.1:5960"
    
    var formatDescription: String {
        "\(selectedFormat.name)\(progressive ? "p" : "i")\(fps)"
    }
    
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
                    Picker("Resolution", selection: $selectedFormat) {
                        ForEach(standardFormats) { format in
                            Text(format.displayName).tag(format)
                        }
                    }
                    
                    Picker("Scan Mode", selection: $progressive) {
                        Text("Progressive (p)").tag(true)
                        Text("Interlaced (i)").tag(false)
                    }
                    .pickerStyle(.segmented)
                    
                    Picker("Frame Rate", selection: $fps) {
                        ForEach(standardFramerates, id: \.self) { rate in
                            Text("\(rate) fps").tag(rate)
                        }
                    }
                    
                    HStack {
                        Text("Format")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Spacer()
                        Text(formatDescription)
                            .font(.system(.body, design: .monospaced))
                            .fontWeight(.semibold)
                            .foregroundColor(.accentColor)
                    }
                    .padding(.vertical, 4)
                }
                
                Section("Genlock Synchronization") {
                    Picker("Mode", selection: $genlockMode) {
                        Text("Disabled").tag("disabled")
                        Text("Master").tag("master")
                        Text("Slave").tag("slave")
                    }
                    
                    if genlockMode == "slave" {
                        TextField("Master Address", text: $genlockMasterAddr, prompt: Text("127.0.0.1:5960"))
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
        config.width = selectedFormat.width
        config.height = selectedFormat.height
        config.fps = fps
        config.progressive = progressive
        config.autoStart = autoStart
        config.genlockMode = genlockMode
        config.genlockMasterAddr = genlockMasterAddr
        
        StreamManager.shared.addStream(config)
        dismiss()
    }
}

// MARK: - Previews

#if DEBUG
struct DashboardView_Previews: PreviewProvider {
    static var previews: some View {
        DashboardView()
    }
}
#endif

