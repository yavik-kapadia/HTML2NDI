import Foundation

class ManagerServer {
    static let shared = ManagerServer()
    
    private var serverSocket: Int32 = -1
    private let port: UInt16 = 8080
    private var isRunning = false
    private let queue = DispatchQueue(label: "manager-server", qos: .utility)
    
    func start() {
        queue.async { [weak self] in
            self?.runServer()
        }
    }
    
    func stop() {
        isRunning = false
        if serverSocket >= 0 {
            Darwin.close(serverSocket)
            serverSocket = -1
        }
    }
    
    private func runServer() {
        // Create socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0)
        guard serverSocket >= 0 else {
            print("Failed to create socket")
            return
        }
        
        // Set socket options
        var yes: Int32 = 1
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, socklen_t(MemoryLayout<Int32>.size))
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &yes, socklen_t(MemoryLayout<Int32>.size))
        
        // Bind
        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        addr.sin_addr.s_addr = INADDR_ANY
        
        let bindResult = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(serverSocket, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        
        guard bindResult == 0 else {
            print("Failed to bind to port \(port): \(String(cString: strerror(errno)))")
            Darwin.close(serverSocket)
            return
        }
        
        // Listen
        guard listen(serverSocket, 10) == 0 else {
            print("Failed to listen")
            Darwin.close(serverSocket)
            return
        }
        
        print("Manager server listening on port \(port)")
        isRunning = true
        
        // Accept loop
        while isRunning {
            var clientAddr = sockaddr_in()
            var clientAddrLen = socklen_t(MemoryLayout<sockaddr_in>.size)
            
            let clientSocket = withUnsafeMutablePointer(to: &clientAddr) {
                $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                    accept(serverSocket, $0, &clientAddrLen)
                }
            }
            
            guard clientSocket >= 0 else { continue }
            
            // Handle connection in background
            DispatchQueue.global().async { [weak self] in
                self?.handleClient(clientSocket)
            }
        }
    }
    
    private func handleClient(_ socket: Int32) {
        defer { Darwin.close(socket) }
        
        // Read request
        var buffer = [UInt8](repeating: 0, count: 65536)
        let bytesRead = recv(socket, &buffer, buffer.count, 0)
        guard bytesRead > 0 else { return }
        
        let request = String(bytes: buffer[0..<bytesRead], encoding: .utf8) ?? ""
        let response = handleRequest(request)
        
        // Send response
        let responseData = Array(response.utf8)
        _ = send(socket, responseData, responseData.count, 0)
    }
    
    private func handleRequest(_ request: String) -> String {
        let lines = request.split(separator: "\r\n")
        guard let firstLine = lines.first else { return notFoundResponse() }
        
        let parts = firstLine.split(separator: " ")
        guard parts.count >= 2 else { return notFoundResponse() }
        
        let method = String(parts[0])
        let path = String(parts[1])
        
        // Extract body for POST requests
        var body = ""
        if let bodyIndex = request.range(of: "\r\n\r\n") {
            body = String(request[bodyIndex.upperBound...])
        }
        
        switch (method, path) {
        case ("GET", "/"):
            return htmlResponse(dashboardHTML())
        case ("GET", "/api/streams"):
            return jsonResponse(getStreamsJSON())
        case ("POST", "/api/streams"):
            return jsonResponse(addStream(body))
        case ("DELETE", _) where path.hasPrefix("/api/streams/"):
            let id = String(path.dropFirst("/api/streams/".count))
            return jsonResponse(deleteStream(id))
        case ("POST", _) where path.hasPrefix("/api/streams/") && path.hasSuffix("/start"):
            let id = String(path.dropFirst("/api/streams/".count).dropLast("/start".count))
            return jsonResponse(startStream(id))
        case ("POST", _) where path.hasPrefix("/api/streams/") && path.hasSuffix("/stop"):
            let id = String(path.dropFirst("/api/streams/".count).dropLast("/stop".count))
            return jsonResponse(stopStream(id))
        case ("PUT", _) where path.hasPrefix("/api/streams/"):
            let id = String(path.dropFirst("/api/streams/".count))
            return jsonResponse(updateStream(id, body))
        case ("POST", "/api/start-all"):
            return jsonResponse(startAll())
        case ("POST", "/api/stop-all"):
            return jsonResponse(stopAll())
        default:
            return notFoundResponse()
        }
    }
    
    // MARK: - API Handlers
    
    private func getStreamsJSON() -> String {
        let streams = StreamManager.shared.streams.map { stream -> [String: Any] in
            return [
                "id": stream.id.uuidString,
                "name": stream.config.name,
                "url": stream.config.url,
                "ndiName": stream.config.ndiName,
                "width": stream.config.width,
                "height": stream.config.height,
                "fps": stream.config.fps,
                "colorPreset": stream.config.colorPreset,
                "autoStart": stream.config.autoStart,
                "isRunning": stream.isRunning,
                "httpPort": stream.httpPort,
                "actualFps": stream.actualFps,
                "connections": stream.connections
            ]
        }
        
        if let data = try? JSONSerialization.data(withJSONObject: streams),
           let json = String(data: data, encoding: .utf8) {
            return json
        }
        return "[]"
    }
    
    private func addStream(_ body: String) -> String {
        guard let data = body.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return "{\"error\": \"Invalid JSON\"}"
        }
        
        var config = StreamConfig()
        config.name = json["name"] as? String ?? "New Stream"
        config.url = json["url"] as? String ?? "about:blank"
        config.ndiName = json["ndiName"] as? String ?? "HTML2NDI"
        config.width = json["width"] as? Int ?? 1920
        config.height = json["height"] as? Int ?? 1080
        config.fps = json["fps"] as? Int ?? 60
        config.colorPreset = json["colorPreset"] as? String ?? "rec709"
        config.autoStart = json["autoStart"] as? Bool ?? false
        
        DispatchQueue.main.async {
            StreamManager.shared.addStream(config)
        }
        
        return "{\"success\": true, \"id\": \"\(config.id.uuidString)\"}"
    }
    
    private func deleteStream(_ id: String) -> String {
        guard let uuid = UUID(uuidString: id),
              let stream = StreamManager.shared.streams.first(where: { $0.id == uuid }) else {
            return "{\"error\": \"Stream not found\"}"
        }
        
        DispatchQueue.main.async {
            StreamManager.shared.removeStream(stream)
        }
        
        return "{\"success\": true}"
    }
    
    private func startStream(_ id: String) -> String {
        guard let uuid = UUID(uuidString: id),
              let stream = StreamManager.shared.streams.first(where: { $0.id == uuid }) else {
            return "{\"error\": \"Stream not found\"}"
        }
        
        DispatchQueue.main.async {
            StreamManager.shared.startStream(stream)
        }
        
        return "{\"success\": true}"
    }
    
    private func stopStream(_ id: String) -> String {
        guard let uuid = UUID(uuidString: id),
              let stream = StreamManager.shared.streams.first(where: { $0.id == uuid }) else {
            return "{\"error\": \"Stream not found\"}"
        }
        
        DispatchQueue.main.async {
            StreamManager.shared.stopStream(stream)
        }
        
        return "{\"success\": true}"
    }
    
    private func updateStream(_ id: String, _ body: String) -> String {
        guard let uuid = UUID(uuidString: id),
              let stream = StreamManager.shared.streams.first(where: { $0.id == uuid }),
              let data = body.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return "{\"error\": \"Stream not found or invalid JSON\"}"
        }
        
        DispatchQueue.main.async {
            if let name = json["name"] as? String { stream.config.name = name }
            if let url = json["url"] as? String { 
                stream.config.url = url
                if stream.isRunning { stream.setURL(url) }
            }
            if let ndiName = json["ndiName"] as? String { stream.config.ndiName = ndiName }
            if let width = json["width"] as? Int { stream.config.width = width }
            if let height = json["height"] as? Int { stream.config.height = height }
            if let fps = json["fps"] as? Int { stream.config.fps = fps }
            if let colorPreset = json["colorPreset"] as? String { 
                stream.config.colorPreset = colorPreset
                if stream.isRunning { stream.applyColorPreset() }
            }
            if let autoStart = json["autoStart"] as? Bool { stream.config.autoStart = autoStart }
            
            StreamManager.shared.saveStreams()
        }
        
        return "{\"success\": true}"
    }
    
    private func startAll() -> String {
        DispatchQueue.main.async {
            StreamManager.shared.startAll()
        }
        return "{\"success\": true}"
    }
    
    private func stopAll() -> String {
        DispatchQueue.main.async {
            StreamManager.shared.stopAll()
        }
        return "{\"success\": true}"
    }
    
    // MARK: - Response Helpers
    
    private func htmlResponse(_ body: String) -> String {
        return """
        HTTP/1.1 200 OK\r
        Content-Type: text/html; charset=utf-8\r
        Content-Length: \(body.utf8.count)\r
        Access-Control-Allow-Origin: *\r
        Connection: close\r
        \r
        \(body)
        """
    }
    
    private func jsonResponse(_ body: String) -> String {
        return """
        HTTP/1.1 200 OK\r
        Content-Type: application/json\r
        Content-Length: \(body.utf8.count)\r
        Access-Control-Allow-Origin: *\r
        Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r
        Access-Control-Allow-Headers: Content-Type\r
        Connection: close\r
        \r
        \(body)
        """
    }
    
    private func notFoundResponse() -> String {
        let body = "{\"error\": \"Not Found\"}"
        return """
        HTTP/1.1 404 Not Found\r
        Content-Type: application/json\r
        Content-Length: \(body.utf8.count)\r
        Connection: close\r
        \r
        \(body)
        """
    }
    
    // MARK: - Dashboard HTML
    
    private func dashboardHTML() -> String {
        return """
        <!DOCTYPE html>
        <html>
        <head>
            <title>HTML2NDI Manager</title>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
                :root{--bg:#0a0a0f;--card:#12121a;--input:#1a1a24;--accent:#6366f1;--glow:rgba(99,102,241,.3);--ok:#22c55e;--err:#ef4444;--text:#e4e4e7;--dim:#71717a;--border:#27272a}
                *{box-sizing:border-box;margin:0;padding:0}
                body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;background-image:radial-gradient(ellipse at top,rgba(99,102,241,.1)0,transparent 50%)}
                .c{max-width:1400px;margin:0 auto;padding:2rem}
                header{display:flex;align-items:center;justify-content:space-between;margin-bottom:2rem;padding-bottom:1.5rem;border-bottom:1px solid var(--border)}
                .logo{display:flex;align-items:center;gap:.75rem}
                .logo-icon{width:48px;height:48px;background:linear-gradient(135deg,var(--accent),#8b5cf6);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:1.5rem;font-weight:700;box-shadow:0 4px 20px var(--glow)}
                h1{font-size:1.75rem;font-weight:600;background:linear-gradient(135deg,#fff,#a1a1aa);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
                .actions{display:flex;gap:.75rem}
                button{padding:.75rem 1.25rem;border:none;border-radius:10px;font-family:inherit;font-size:.9rem;font-weight:500;cursor:pointer;transition:all .2s}
                .btn-p{background:linear-gradient(135deg,var(--accent),#8b5cf6);color:#fff;box-shadow:0 4px 15px var(--glow)}
                .btn-p:hover{transform:translateY(-1px);box-shadow:0 6px 20px var(--glow)}
                .btn-s{background:var(--input);color:var(--text);border:1px solid var(--border)}
                .btn-s:hover{background:var(--border)}
                .btn-d{background:var(--err);color:#fff}
                .btn-sm{padding:.5rem .75rem;font-size:.8rem}
                .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(400px,1fr));gap:1.5rem}
                .card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:1.5rem;transition:transform .2s,box-shadow .2s}
                .card:hover{transform:translateY(-2px);box-shadow:0 8px 30px rgba(0,0,0,.3)}
                .card-h{display:flex;align-items:center;justify-content:space-between;margin-bottom:1rem}
                .status{display:flex;align-items:center;gap:.5rem}
                .dot{width:10px;height:10px;border-radius:50%}
                .dot.on{background:var(--ok);box-shadow:0 0 10px var(--ok)}
                .dot.off{background:var(--dim)}
                .ndi-name{font-size:1.25rem;font-weight:600}
                .stats{display:grid;grid-template-columns:repeat(3,1fr);gap:.75rem;margin-bottom:1rem}
                .stat{background:var(--input);padding:.75rem;border-radius:8px;text-align:center}
                .stat-l{font-size:.7rem;color:var(--dim);text-transform:uppercase}
                .stat-v{font-family:ui-monospace,monospace;font-size:1rem;font-weight:600}
                .url-row{display:flex;gap:.5rem;margin-bottom:1rem}
                input,select{flex:1;padding:.6rem .75rem;background:var(--input);border:1px solid var(--border);border-radius:8px;color:var(--text);font-family:ui-monospace,monospace;font-size:.85rem}
                input:focus,select:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 3px var(--glow)}
                .card-actions{display:flex;gap:.5rem;justify-content:flex-end}
                .modal{position:fixed;inset:0;background:rgba(0,0,0,.7);display:none;align-items:center;justify-content:center;z-index:100}
                .modal.show{display:flex}
                .modal-c{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:2rem;width:100%;max-width:500px}
                .modal-h{font-size:1.25rem;font-weight:600;margin-bottom:1.5rem}
                .form-g{margin-bottom:1rem}
                .form-g label{display:block;font-size:.85rem;color:var(--dim);margin-bottom:.5rem}
                .form-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:.75rem}
                .toast{position:fixed;bottom:2rem;right:2rem;padding:1rem 1.5rem;background:var(--card);border:1px solid var(--ok);border-radius:12px;transform:translateY(100px);opacity:0;transition:all .3s;z-index:200}
                .toast.show{transform:translateY(0);opacity:1}
            </style>
        </head>
        <body>
            <div class="c">
                <header>
                    <div class="logo"><div class="logo-icon">N</div><div><h1>HTML2NDI Manager</h1><div style="font-size:.85rem;color:var(--dim)">Multi-Stream Dashboard</div></div></div>
                    <div class="actions">
                        <button class="btn-s" onclick="stopAll()">Stop All</button>
                        <button class="btn-s" onclick="startAll()">Start All</button>
                        <button class="btn-p" onclick="showAddModal()">+ Add Stream</button>
                    </div>
                </header>
                <div class="grid" id="streams"></div>
            </div>
            
            <div class="modal" id="addModal">
                <div class="modal-c">
                    <div class="modal-h">Add New Stream</div>
                    <div class="form-g"><label>Stream Name</label><input id="newName" placeholder="My Graphics"></div>
                    <div class="form-g"><label>Source URL</label><input id="newUrl" placeholder="http://localhost:9090/graphics.html"></div>
                    <div class="form-g"><label>NDI Source Name</label><input id="newNdi" placeholder="Graphics"></div>
                    <div class="form-row">
                        <div class="form-g"><label>Width</label><input id="newW" type="number" value="1920"></div>
                        <div class="form-g"><label>Height</label><input id="newH" type="number" value="1080"></div>
                        <div class="form-g"><label>FPS</label><input id="newFps" type="number" value="60"></div>
                    </div>
                    <div class="form-g"><label>Color Preset</label><select id="newColor"><option value="rec709">Rec. 709</option><option value="rec2020">Rec. 2020</option><option value="srgb">sRGB</option><option value="rec601">Rec. 601</option></select></div>
                    <div class="form-g"><label><input type="checkbox" id="newAuto"> Auto-start on launch</label></div>
                    <div style="display:flex;gap:.75rem;justify-content:flex-end;margin-top:1.5rem">
                        <button class="btn-s" onclick="hideAddModal()">Cancel</button>
                        <button class="btn-p" onclick="addStream()">Add Stream</button>
                    </div>
                </div>
            </div>
            
            <div class="toast" id="toast"></div>
            
            <script>
            let streams = [];
            
            async function load() {
                const r = await fetch('/api/streams');
                streams = await r.json();
                render();
            }
            
            function render() {
                const el = document.getElementById('streams');
                el.innerHTML = streams.map(s => `
                    <div class="card">
                        <div class="card-h">
                            <div class="status"><div class="dot ${s.isRunning?'on':'off'}"></div><span class="ndi-name">${s.ndiName}</span></div>
                            <span style="color:var(--dim);font-size:.85rem">${s.width}x${s.height} @ ${s.fps}fps</span>
                        </div>
                        <div class="stats">
                            <div class="stat"><div class="stat-l">Status</div><div class="stat-v" style="color:${s.isRunning?'var(--ok)':'var(--dim)'}">${s.isRunning?'Running':'Stopped'}</div></div>
                            <div class="stat"><div class="stat-l">FPS</div><div class="stat-v">${s.actualFps?.toFixed(1)||'-'}</div></div>
                            <div class="stat"><div class="stat-l">Connections</div><div class="stat-v">${s.connections||0}</div></div>
                        </div>
                        <div class="url-row">
                            <input value="${s.url}" onchange="updateUrl('${s.id}',this.value)" placeholder="URL">
                            <button class="btn-s btn-sm" onclick="openControl('${s.httpPort}')">Control</button>
                        </div>
                        <div class="card-actions">
                            <button class="btn-s btn-sm" onclick="del('${s.id}')">Delete</button>
                            ${s.isRunning
                                ? `<button class="btn-d btn-sm" onclick="stop('${s.id}')">Stop</button>`
                                : `<button class="btn-p btn-sm" onclick="start('${s.id}')">Start</button>`
                            }
                        </div>
                    </div>
                `).join('');
            }
            
            async function start(id) { await fetch('/api/streams/'+id+'/start',{method:'POST'}); toast('Stream started'); load(); }
            async function stop(id) { await fetch('/api/streams/'+id+'/stop',{method:'POST'}); toast('Stream stopped'); load(); }
            async function del(id) { if(!confirm('Delete this stream?'))return; await fetch('/api/streams/'+id,{method:'DELETE'}); toast('Stream deleted'); load(); }
            async function startAll() { await fetch('/api/start-all',{method:'POST'}); toast('All streams started'); load(); }
            async function stopAll() { await fetch('/api/stop-all',{method:'POST'}); toast('All streams stopped'); load(); }
            
            async function updateUrl(id,url) {
                await fetch('/api/streams/'+id,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({url})});
                toast('URL updated');
            }
            
            function openControl(port) { if(port>0) window.open('http://localhost:'+port,'_blank'); else toast('Stream not running'); }
            
            function showAddModal() { document.getElementById('addModal').classList.add('show'); }
            function hideAddModal() { document.getElementById('addModal').classList.remove('show'); }
            
            async function addStream() {
                const data = {
                    name: document.getElementById('newName').value || 'New Stream',
                    url: document.getElementById('newUrl').value || 'about:blank',
                    ndiName: document.getElementById('newNdi').value || 'HTML2NDI',
                    width: parseInt(document.getElementById('newW').value) || 1920,
                    height: parseInt(document.getElementById('newH').value) || 1080,
                    fps: parseInt(document.getElementById('newFps').value) || 60,
                    colorPreset: document.getElementById('newColor').value,
                    autoStart: document.getElementById('newAuto').checked
                };
                await fetch('/api/streams',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
                hideAddModal();
                toast('Stream added');
                load();
            }
            
            function toast(msg) { const t=document.getElementById('toast'); t.textContent=msg; t.classList.add('show'); setTimeout(()=>t.classList.remove('show'),3000); }
            
            load();
            setInterval(load, 2000);
            </script>
        </body>
        </html>
        """
    }
}

