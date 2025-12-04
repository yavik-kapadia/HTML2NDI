# HTML2NDI

**Render HTML pages as NDI video output on macOS**

HTML2NDI is a native macOS application that loads an HTML page using Chromium Embedded Framework (CEF) in off-screen mode and publishes the rendered frames as NDI video. It runs as a headless daemon controllable via CLI flags and a REST API.

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         HTML2NDI                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────┐     ┌──────────────┐     ┌──────────────┐    │
│   │   CEF       │     │  Frame Pump  │     │  NDI Sender  │    │
│   │  Renderer   │────▶│  (Buffering) │────▶│  (libndi)    │────┼──▶ NDI Network
│   │  (OSR)      │     │              │     │              │    │
│   └─────────────┘     └──────────────┘     └──────────────┘    │
│         ▲                                                       │
│         │                                                       │
│   ┌─────┴─────────────────────────────────────────────────┐    │
│   │                    HTTP Control API                    │    │
│   │   /status  /seturl  /reload  /shutdown                │    │
│   └───────────────────────────────────────────────────────┘    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Components

| Module | Description |
|--------|-------------|
| **CEF Renderer** | Off-screen Chromium browser that renders HTML/CSS/JS |
| **Frame Pump** | Double-buffered frame delivery with timing control |
| **NDI Sender** | NDI SDK wrapper for video frame transmission |
| **HTTP Server** | REST API for runtime control |
| **Application** | Coordination layer and lifecycle management |

---

## 📋 Requirements

### Build Requirements
- macOS 11.0 (Big Sur) or later
- Xcode Command Line Tools
- CMake 3.21+
- C++20 compatible compiler (Apple Clang 13+)

### Runtime Requirements
- [NDI SDK for macOS](https://ndi.video/for-developers/ndi-sdk/) (download required)
- NDI Tools (optional, for testing with Studio Monitor)

---

## 🔧 Building

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/HTML2NDI.git
cd HTML2NDI
```

### 2. Install NDI SDK

Download the NDI SDK from [ndi.video](https://ndi.video/for-developers/ndi-sdk/) and install it:

```bash
# After downloading, the SDK is typically installed to:
# /Library/NDI SDK for Apple/

# Verify installation
ls "/Library/NDI SDK for Apple/lib/macOS/"
# Should show: libndi.dylib
```

### 3. Build

```bash
# Create build directory
mkdir build && cd build

# Configure (downloads CEF automatically)
cmake ..

# Build
cmake --build . -j$(sysctl -n hw.ncpu)
```

> **Note:** The first build will download CEF (~300MB) and build the wrapper library. This may take several minutes.

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_UNIVERSAL` | ON | Build universal binary (x86_64 + arm64) |
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug, Release, RelWithDebInfo) |
| `NDI_SDK_DIR` | auto | Path to NDI SDK if not in standard location |

```bash
# Example: Debug build with custom NDI path
cmake -DCMAKE_BUILD_TYPE=Debug -DNDI_SDK_DIR=/path/to/ndi ..
```

---

## 🚀 Usage

### Basic Usage

```bash
# Render a webpage
./build/bin/html2ndi --url https://example.com

# With custom resolution and framerate
./build/bin/html2ndi --url https://mysite.com --width 1280 --height 720 --fps 30

# Use the built-in test page
./build/bin/html2ndi --url file://$(pwd)/resources/test.html
```

### Command Line Options

```
HTML Rendering Options:
  -u, --url <url>           URL to load (default: about:blank)
  -w, --width <pixels>      Frame width (default: 1920)
  -h, --height <pixels>     Frame height (default: 1080)
  -f, --fps <rate>          Target framerate (default: 60)

NDI Options:
  -n, --ndi-name <name>     NDI source name (default: HTML2NDI)
  -g, --ndi-groups <groups> NDI groups, comma-separated (default: all)
  --no-clock-video          Disable video clock timing
  --no-clock-audio          Disable audio clock timing

HTTP API Options:
  --http-host <host>        HTTP server bind address (default: 127.0.0.1)
  -p, --http-port <port>    HTTP server port (default: 8080)
  --no-http                 Disable HTTP server

Application Options:
  -l, --log-file <path>     Log file path
  -v, --verbose             Enable verbose logging (DEBUG level)
  -q, --quiet               Quiet mode (ERROR level only)
  -d, --daemon              Run as daemon (detach from terminal)
  --version                 Print version and exit
  --help                    Show help message
```

### Using the Helper Script

```bash
# Make executable
chmod +x scripts/run.sh

# Run with test page
./scripts/run.sh --test

# Run with custom settings
./scripts/run.sh --url https://mysite.com --fps 30
```

---

## 🌐 HTTP Control API

When running, HTML2NDI exposes a REST API for control:

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | HTML info page |
| `/status` | GET | JSON status (URL, resolution, FPS, connections) |
| `/seturl` | POST | Load new URL: `{"url": "https://..."}` |
| `/reload` | POST | Reload current page |
| `/shutdown` | POST | Graceful shutdown |

### Examples

```bash
# Get current status
curl http://localhost:8080/status

# Load a new URL
curl -X POST http://localhost:8080/seturl \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}'

# Reload page
curl -X POST http://localhost:8080/reload

# Shutdown
curl -X POST http://localhost:8080/shutdown
```

---

## 🔄 Running as a Service

### Using launchd (Recommended)

1. Install the LaunchAgent:

```bash
cp resources/com.html2ndi.plist ~/Library/LaunchAgents/

# Edit the plist to configure your settings
nano ~/Library/LaunchAgents/com.html2ndi.plist
```

2. Load the agent:

```bash
launchctl load ~/Library/LaunchAgents/com.html2ndi.plist
```

3. Control the service:

```bash
# Stop
launchctl stop com.html2ndi

# Start
launchctl start com.html2ndi

# Unload (disable)
launchctl unload ~/Library/LaunchAgents/com.html2ndi.plist
```

### Logs

When running as a service, logs are written to:
- `/var/log/html2ndi/html2ndi.log` - Application log
- `/var/log/html2ndi/stdout.log` - Standard output
- `/var/log/html2ndi/stderr.log` - Standard error

---

## 🧪 Testing with NDI Tools

1. Download [NDI Tools](https://ndi.video/tools/) and install **NDI Studio Monitor**

2. Start HTML2NDI:
```bash
./build/bin/html2ndi --url file://$(pwd)/resources/test.html
```

3. Open NDI Studio Monitor - you should see "HTML2NDI" as an available source

4. The test page shows:
   - Current time
   - Frame counter
   - FPS measurement
   - Uptime

---

## 🐛 Troubleshooting

### CEF Sandbox Issues

On macOS, CEF subprocess may fail with sandbox errors. The application disables the sandbox by default (`no_sandbox = true`).

If you encounter issues:
```bash
# Check if helper executable exists
ls -la build/bin/html2ndi_helper

# Ensure it's executable
chmod +x build/bin/html2ndi_helper
```

### Apple Silicon (M1/M2/M3)

The build system automatically detects architecture. If you encounter issues:

```bash
# Force arm64 build
cmake -DCMAKE_OSX_ARCHITECTURES=arm64 ..

# Or force x86_64 (Rosetta)
cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 ..
```

### NDI Not Found

If NDI devices don't appear:

1. Check NDI SDK installation:
```bash
ls "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"
```

2. Verify the library is loaded:
```bash
# Check linked libraries
otool -L build/bin/html2ndi | grep ndi
```

3. Ensure NDI is allowed through firewall (System Preferences → Security & Privacy → Firewall)

### High CPU Usage

CEF rendering can be CPU-intensive. To reduce usage:

```bash
# Lower framerate
./html2ndi --url https://example.com --fps 30

# Lower resolution  
./html2ndi --url https://example.com --width 1280 --height 720
```

### Memory Issues

For long-running instances with dynamic content:

```bash
# Use a cache directory to persist browser data
./html2ndi --cache-path /tmp/html2ndi-cache --url https://example.com
```

---

## 📁 Project Structure

```
HTML2NDI/
├── CMakeLists.txt          # Main CMake configuration
├── cmake/
│   ├── CEF.cmake           # CEF download and configuration
│   └── NDI.cmake           # NDI SDK detection
├── include/html2ndi/
│   ├── application.h       # Main application class
│   ├── config.h            # Configuration structure
│   ├── cef/
│   │   ├── cef_app.h       # CEF application
│   │   ├── cef_handler.h   # Browser event handler
│   │   └── offscreen_renderer.h
│   ├── ndi/
│   │   ├── ndi_sender.h    # NDI wrapper
│   │   └── frame_pump.h    # Frame timing
│   ├── http/
│   │   └── http_server.h   # HTTP API
│   └── utils/
│       ├── logger.h        # Logging
│       └── signal_handler.h
├── src/
│   ├── app/
│   │   ├── main.cpp        # Entry point
│   │   ├── config.cpp      # CLI parsing
│   │   └── application.cpp
│   ├── cef/
│   │   ├── cef_app.cpp
│   │   ├── cef_handler.cpp
│   │   ├── cef_helper.cpp  # Subprocess
│   │   └── offscreen_renderer.cpp
│   ├── ndi/
│   │   ├── ndi_sender.cpp
│   │   └── frame_pump.cpp
│   ├── http/
│   │   └── http_server.cpp
│   └── utils/
│       ├── logger.cpp
│       └── signal_handler.cpp
├── resources/
│   ├── test.html           # Test page
│   └── com.html2ndi.plist  # LaunchAgent
└── scripts/
    ├── run.sh              # Runner script
    ├── install.sh          # Installation script
    └── uninstall.sh        # Uninstallation script
```

---

## 📄 License

MIT License - See LICENSE file for details.

---

## 🙏 Acknowledgments

- [Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef)
- [NDI SDK](https://ndi.video/)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [nlohmann/json](https://github.com/nlohmann/json)

