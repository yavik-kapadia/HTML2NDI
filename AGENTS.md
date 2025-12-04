# HTML2NDI — Native macOS Application (CEF + NDI)
**Project Specification**

## Overview
A **native macOS application** that loads HTML pages, renders them using **CEF in off-screen mode**, and publishes the resulting frames as **NDI video** via the **NDI Advanced SDK for macOS**.

The project consists of two components:
1. **html2ndi** — C++17/20 worker that renders HTML to NDI (runs as .app bundle)
2. **HTML2NDI Manager** — Swift/SwiftUI menu bar app for managing multiple streams

---

## Project Architecture

```
/src
  /app        — main entrypoint, config, CLI parsing
  /cef        — CEF wrapper, off-screen rendering, frame callbacks
  /ndi        — NDI initialization and frame sending
  /http       — lightweight control API
  /utils      — logging, threading, image encoding, helpers
/include      — header files
/cmake        — CMake modules for CEF and NDI
/manager      — Swift menu bar manager app
  /HTML2NDI Manager  — SwiftUI source files
/resources    — Info.plist, HTML templates, icons
/.github/workflows — CI/CD for builds and releases
```

Build system: **CMake** + **Swift Package Manager**

Dependencies:
- CEF (Chromium Embedded Framework) — off-screen rendering
- NDI macOS SDK (`libndi.dylib`) — video output
- `cpp-httplib` — HTTP server (header-only)
- `nlohmann/json` — JSON parsing (header-only)
- `stb_image_write` — JPEG encoding for thumbnails

---

## Functional Requirements

### 1. HTML Rendering (CEF Off-Screen Mode)
- Run CEF with no visible window in .app bundle structure
- Implement `OnPaint` callback for BGRA frame capture
- Forward frame buffers to NDI sender thread
- CLI options:
  - `--url` — source URL
  - `--width`, `--height` — resolution (default: 1920x1080)
  - `--fps` — target framerate (default: 60)
  - `--ndi-name` — NDI source name
  - `--http-port` — control API port
  - `--cache-path` — unique CEF cache directory

### 2. NDI Video Output
- Initialize NDI with `NDIlib_send_create()`
- Submit `NDIlib_video_frame_v2_t` frames with configurable:
  - Color space (Rec. 709, Rec. 2020, sRGB, Rec. 601)
  - Gamma mode
  - Color range (full/limited)
- Handle dropped frames gracefully

### 3. HTTP Control API
Endpoints provided by each worker instance:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/status` | GET | Current URL, FPS, resolution, NDI connections, color settings |
| `/seturl` | POST | `{ "url": "…" }` — navigate to new URL |
| `/reload` | POST | Reload current page |
| `/shutdown` | POST | Graceful shutdown |
| `/thumbnail` | GET | JPEG preview image (`?width=320&quality=75`) |
| `/color` | GET/POST | Get or set color space settings |

### 4. Multi-Stream Manager (Swift)
Menu bar application that:
- Manages multiple html2ndi worker processes
- Provides web-based dashboard at `http://localhost:8080`
- Supports stream operations:
  - Add/delete streams
  - Start/stop individual or all streams
  - Edit stream name, NDI name, URL
  - Configure resolution, FPS, color preset
- Live preview thumbnails for running streams
- Auto-generates unique names if not provided
- Persists configuration to `~/Library/Application Support/HTML2NDI/`

### 5. macOS Build Requirements
- Build for Apple Silicon (`arm64`)
- Produce .app bundle: `html2ndi.app`
- Manager app: `HTML2NDI Manager.app`
- CEF helper processes in bundle (GPU, Renderer, Plugin, Alerts)
- CMake scripts for:
  - CEF download and extraction
  - NDI SDK linking
  - Proper RPATH for dylibs
  - Helper app bundle generation

---

## Operational Expectations
- Strong separation of concerns (CEF, NDI, HTTP, App)
- RAII for all resources
- Thread-safe frame pump with double buffering
- Logging to stdout and rotating file
- SIGTERM handling for graceful shutdown
- Unique CEF cache paths for multi-instance support
- Mock keychain to avoid macOS permission prompts

---

## Files Structure

### Core Source Files
- `src/app/main.cpp` — entry point
- `src/app/application.cpp` — main coordinator
- `src/cef/offscreen_renderer.cpp` — CEF initialization
- `src/cef/cef_handler.cpp` — browser event handling
- `src/ndi/ndi_sender.cpp` — NDI output
- `src/ndi/frame_pump.cpp` — frame timing/delivery
- `src/http/http_server.cpp` — control API
- `src/utils/image_encode.cpp` — JPEG thumbnail encoding

### Manager App (Swift)
- `manager/HTML2NDI Manager/HTML2NDIManagerApp.swift` — app entry
- `manager/HTML2NDI Manager/StreamManager.swift` — process management
- `manager/HTML2NDI Manager/ManagerServer.swift` — dashboard HTTP server
- `manager/HTML2NDI Manager/MenuBarView.swift` — menu bar UI

---

## Build & Run

### Prerequisites
```bash
brew install cmake ninja librsvg
# Install NDI SDK from https://ndi.video/download-ndi-sdk/
```

### Build Worker
```bash
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

### Build Manager
```bash
cd manager
swift build -c release
./build.sh  # Creates complete app bundle
```

### Run
Launch `HTML2NDI Manager.app` from Applications or:
```bash
./manager/build/"HTML2NDI Manager.app"/Contents/MacOS/"HTML2NDI Manager"
```

Open dashboard: `http://localhost:8080`

---

## CI/CD (GitHub Actions)

### Workflows
- `build.yml` — runs on push to main, builds and tests
- `release.yml` — creates releases on version tags (`v*`)

### Creating a Release
```bash
git tag v1.3.0
git push origin v1.3.0
```

Produces:
- `HTML2NDI-Manager-{version}-arm64.dmg`
- `HTML2NDI-Manager-{version}-arm64.zip`

---

## Coding Guidelines
- C++17/20 for worker, Swift 5.9+ for manager
- Google C++ Style for C++ code
- Use `std::thread`, `std::mutex`, smart pointers, `std::chrono`
- Minimal external dependencies
- Each subsystem isolated and testable

---

## Implemented Features (Beyond Original Spec)
- Multi-stream management via native menu bar app
- Web-based configuration dashboard
- Live preview thumbnails (JPEG)
- Configurable color space presets
- Editable stream/NDI names
- Auto-generated unique identifiers
- GitHub Actions CI/CD
- Custom app icon
- Process auto-restart with exponential backoff
- Health checks for stalled streams
- CEF render process crash recovery
- Watchdog timer for main loop hang detection
- macOS unified logging (os_log)
- Prometheus metrics endpoint (/metrics) for Grafana
- NDI tally state (program/preview)
- Enhanced per-stream analytics (frames, bandwidth, uptime)

## Future Enhancements
- Audio pipeline (WebAudio → NDI audio)
- Intel x86_64 support
- Code signing and notarization
- App Store distribution
- Multiple NDI output formats


## AGENT INSTRUCTIONS
- Always write tests.
- All docs and md files should be stored in /docs except readme.md and AGENTS.md
- You are an expert in C++ and Swift, take on the harder route and only choose the easiest path last.
- Be concise with your documentation.
- The commit should pass all tests before you merge.
- Always run the linter before before you merge.
- Do not hard code colors
- Avoid Emojis
- Increment versioning