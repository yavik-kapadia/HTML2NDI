# Tractus.HtmlToNdi — Native macOS Port (CEF + NDI)  
**Cursor Project Specification**

## Overview
Create a **native macOS application** in **C++17/20** that loads an HTML page, renders it using **CEF in off-screen mode**, and publishes the resulting frames as **NDI video** via the **NDI Advanced SDK for macOS**.  
The application should run as a **headless daemon** controlled through CLI flags and a small HTTP API.

---

## Project Architecture
/src
  /app        — main entrypoint, config, CLI parsing
  /cef        — CEF wrapper, off-screen rendering, frame callbacks
  /ndi        — NDI initialization and frame sending
  /http       — lightweight control API (/seturl, /status)
  /utils      — logging, threading, helpers
/include
/cmake
/build

Use **CMake** to build and link against:
- CEF (OSR mode)
- NDI macOS SDK (`libndi.dylib`)

---

## Functional Requirements

### 1. HTML Rendering (CEF Off-Screen Mode)
- Run CEF with no visible window.
- Implement `OnPaint` callback:
  - Provide RGBA buffer for each new frame.
  - Forward frame buffers to NDI sender thread.
- Configurable options:
  - `--url`
  - `--width`, `--height`
  - `--fps`
- Disable VSync, control rendering rate manually.

### 2. NDI Video Output
- Initialize NDI with `NDIlib_send_create()`.
- Create and submit `NDIlib_video_frame_v2_t` frames:
  - RGBA pixel format
  - Matching CEF resolution
  - Configurable framerate (`frame_rate_N` / `frame_rate_D`)
- Gracefully handle dropped frames, reconnects, shutdown.

### 3. Optional Audio Pipeline (Phase 2)
- Capture PCM from WebAudio via CEF native extension or JS binding.
- Create `NDIlib_audio_frame_v3_t`.
- Sync audio with video timestamps when possible.

### 4. HTTP Control API
Implement JSON endpoints using `cpp-httplib`:

| Endpoint | Method | Description |
|---------|--------|-------------|
| `/status` | GET | Return JSON with current URL, fps, resolution, and NDI state |
| `/seturl` | POST | `{ "url": "…" }` — reload CEF with new target |
| `/reload` | POST | Manual page reload |
| `/shutdown` | POST | Clean shutdown |

### 5. macOS Build Requirements
- Build for `x86_64` and `arm64`.
- Produce a single executable: `htmltondi-macos`.
- Provide:
  - `LaunchAgent` plist for background operation
  - A `run.sh` helper script
- Include CMake scripts to:
  - Download/extract CEF
  - Link against NDI
  - Add RPATH correctly for `.dylib` files

---

## Operational Expectations
- Strong separation of concerns (CEF, NDI, HTTP, App).
- RAII for all resources (CEF lifecycle, NDI sender, threads).
- Avoid global state; use configuration structures.
- Logging:
  - stdout + rotating file logger
- Kill handling:
  - SIGTERM → graceful shutdown sequence
- Robustness:
  - Detect CEF subprocess crashes and automatically restart.

---

## Deliverables to Generate
1. **Complete folder structure** with placeholder `.cpp` / `.h` files.  
2. **CMakeLists.txt** with:
   - CEF inclusion
   - NDI linking
   - Build targets for macOS ARM + Intel  
3. Implementations for:
   - `CefApp` and off-screen renderer  
   - `NdiSender` wrapper  
   - `FramePump`  
   - `HttpServer`  
   - `Config` + CLI parsing  
4. Example test HTML file.  
5. Production-ready README including:
   - architecture diagrams  
   - CMake build instructions  
   - troubleshooting (CEF sandbox, Apple Silicon issues)  
   - NDI validation steps with Studio Monitor  
6. A minimal LaunchAgent plist for autostart.

---

## Coding Guidelines
- Prefer C++17 or C++20 features.
- Follow Google C++ Style unless otherwise noted.
- Use:
  - `std::thread`
  - `std::mutex`
  - `std::unique_ptr` / `std::shared_ptr`
  - `std::chrono`  
- Avoid external dependencies beyond:
  - CEF  
  - NDI SDK  
  - `cpp-httplib`  
- Keep each subsystem completely isolated.

---

## Stretch Goals (Optional)
- Hot-reload templates for local HTML rendering.
- FPS display overlay for diagnostics.
- Local MJPEG preview server.
- Load balancing to multiple NDI outputs.

---

## Final Output
Cursor should create a **ready-to-build macOS C++ project** following this spec, with compile-ready code, proper linking, and documentation.
