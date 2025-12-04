# HTML2NDI

**Render HTML pages as NDI video output on macOS**

HTML2NDI is a native macOS application that loads HTML pages using Chromium Embedded Framework (CEF) in off-screen mode and publishes the rendered frames as NDI video. It includes a menu bar manager app for controlling multiple streams.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Environment Setup](#environment-setup)
- [Building the Project](#building-the-project)
- [Running the Application](#running-the-application)
- [Web Dashboard](#web-dashboard)
- [HTTP Control API](#http-control-api)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

HTML2NDI consists of two components:

1. **html2ndi** (C++) - Worker process that renders HTML to NDI video
2. **HTML2NDI Manager** (Swift) - Menu bar app for managing multiple streams

### Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        HTML2NDI Manager (Swift)                         │
│                     Menu Bar App + Web Dashboard                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐      │
│   │  html2ndi       │   │  html2ndi       │   │  html2ndi       │      │
│   │  Worker #1      │   │  Worker #2      │   │  Worker #N      │      │
│   │                 │   │                 │   │                 │      │
│   │  CEF → NDI      │   │  CEF → NDI      │   │  CEF → NDI      │      │
│   └────────┬────────┘   └────────┬────────┘   └────────┬────────┘      │
│            │                     │                     │                │
│            ▼                     ▼                     ▼                │
│      NDI Source #1         NDI Source #2         NDI Source #N         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Features

- Multi-stream management via native macOS menu bar app
- Web-based configuration dashboard
- Live preview thumbnails for running streams
- Configurable color space (Rec. 709, Rec. 2020, sRGB, Rec. 601)
- Editable stream names and NDI source names
- Auto-generated unique identifiers
- HTTP control API per stream
- 1920x1080 @ 60fps default resolution
- Graceful shutdown and error handling
- macOS unified logging (Console.app + file logs)

---

## Requirements

### System Requirements

| Requirement | Version |
|-------------|---------|
| macOS | 13.0 (Ventura) or later |
| Architecture | Apple Silicon (arm64) |
| Xcode Command Line Tools | Latest |

### Build Tools

| Tool | Version | Installation |
|------|---------|--------------|
| CMake | 3.21+ | `brew install cmake` |
| Ninja | Latest | `brew install ninja` |
| Swift | 5.9+ | Included with Xcode |
| librsvg | Latest | `brew install librsvg` (for icon generation) |

### Required SDKs

| SDK | Source |
|-----|--------|
| NDI SDK for macOS | https://ndi.video/download-ndi-sdk/ |
| CEF (Chromium Embedded Framework) | Auto-downloaded during build |

---

## Environment Setup

### Step 1: Install Homebrew (if not installed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Step 2: Install Build Tools

```bash
brew install cmake ninja librsvg
```

### Step 3: Install Xcode Command Line Tools

```bash
xcode-select --install
```

### Step 4: Install NDI SDK

1. Download the NDI SDK from https://ndi.video/download-ndi-sdk/
2. Run the installer package
3. The SDK installs to `/Library/NDI SDK for Apple/`

Verify the installation:

```bash
ls "/Library/NDI SDK for Apple/lib/macOS/"
# Should show: libndi.dylib
```

### Step 5: Clone the Repository

```bash
git clone https://github.com/yourusername/HTML2NDI.git
cd HTML2NDI
```

---

## Building the Project

### Quick Build (Recommended)

Use the provided build script to build everything:

```bash
cd manager
./build.sh
```

This will:
1. Build the C++ html2ndi worker
2. Build the Swift manager app
3. Create the complete app bundle

### Manual Build

#### Build the C++ Worker

```bash
# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build (first build downloads CEF ~300MB)
ninja
```

The first build will:
- Download CEF automatically (~300MB)
- Build the CEF wrapper library
- Compile the html2ndi worker
- Create the .app bundle with helper processes

#### Build the Swift Manager

```bash
cd manager

# Build with Swift Package Manager
swift build -c release

# Create the app bundle
mkdir -p "build/HTML2NDI Manager.app/Contents/MacOS"
mkdir -p "build/HTML2NDI Manager.app/Contents/Resources"

# Copy executable
cp ".build/release/HTML2NDI Manager" "build/HTML2NDI Manager.app/Contents/MacOS/"

# Copy Info.plist
cp "HTML2NDI Manager/Info.plist" "build/HTML2NDI Manager.app/Contents/"

# Copy icon
cp "Resources/AppIcon.icns" "build/HTML2NDI Manager.app/Contents/Resources/"

# Copy worker app
cp -R "../build/bin/html2ndi.app" "build/HTML2NDI Manager.app/Contents/Resources/"
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug, Release) |
| `CMAKE_OSX_ARCHITECTURES` | arm64 | Target architecture |
| `NDI_SDK_DIR` | auto | Custom NDI SDK path |

Example with options:

```bash
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNDI_SDK_DIR=/custom/path/to/ndi
```

### Verify the Build

```bash
# Check that the app bundle exists
ls -la build/bin/html2ndi.app/Contents/MacOS/html2ndi

# Check helper apps exist
ls build/bin/html2ndi.app/Contents/Frameworks/

# Check manager app
ls manager/build/"HTML2NDI Manager.app"/Contents/MacOS/
```

---

## Running the Application

### Option 1: Using the Manager App (Recommended)

Launch the manager app:

```bash
open manager/build/"HTML2NDI Manager.app"
```

Or from the terminal:

```bash
manager/build/"HTML2NDI Manager.app"/Contents/MacOS/"HTML2NDI Manager"
```

The manager will:
- Appear in the menu bar
- Start the web dashboard at http://localhost:8080
- Auto-start any streams configured with auto-start enabled

### Option 2: Using the CLI Wrapper

For single-stream usage:

```bash
./html2ndi --url "https://example.com" --ndi-name "My Stream"
```

### Option 3: Direct Worker Execution

```bash
./build/bin/html2ndi.app/Contents/MacOS/html2ndi \
  --url "https://example.com" \
  --ndi-name "My Stream" \
  --width 1920 \
  --height 1080 \
  --fps 60 \
  --http-port 8080
```

### Command Line Options

```
HTML Rendering:
  --url <url>              URL to render (required)
  --width <pixels>         Frame width (default: 1920)
  --height <pixels>        Frame height (default: 1080)
  --fps <rate>             Target framerate (default: 60)

NDI Output:
  --ndi-name <name>        NDI source name (default: HTML2NDI)

HTTP API:
  --http-port <port>       HTTP control port (default: 8080)

Advanced:
  --cache-path <path>      CEF cache directory (required for multi-instance)
  --verbose                Enable debug logging
```

---

## Web Dashboard

Access the dashboard at http://localhost:8080 when the manager is running.

### Dashboard Features

- View all configured streams
- Start/stop individual streams or all at once
- Edit stream name, NDI source name, and URL
- View live preview thumbnails
- Monitor FPS and connection count
- Add new streams with custom settings
- Delete streams

### Stream Configuration

Each stream can be configured with:

| Setting | Description | Default |
|---------|-------------|---------|
| Stream Name | Internal identifier | Auto-generated UUID |
| NDI Source Name | Name visible in NDI receivers | Auto-generated |
| URL | HTML page to render | about:blank |
| Width | Frame width in pixels | 1920 |
| Height | Frame height in pixels | 1080 |
| FPS | Target framerate | 60 |
| Color Preset | Color space setting | Rec. 709 |
| Auto-start | Start on manager launch | false |

---

## HTTP Control API

Each worker exposes a REST API on its configured port.

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/status` | GET | Current state (URL, FPS, connections, color settings) |
| `/seturl` | POST | Navigate to new URL: `{"url": "..."}` |
| `/reload` | POST | Reload current page |
| `/shutdown` | POST | Graceful shutdown |
| `/thumbnail` | GET | JPEG preview (params: `width`, `quality`) |
| `/color` | GET | Current color space settings |
| `/color` | POST | Set color space: `{"preset": "rec709"}` |

### Examples

```bash
# Get status
curl http://localhost:8080/status

# Load new URL
curl -X POST http://localhost:8080/seturl \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}'

# Get thumbnail
curl http://localhost:8080/thumbnail?width=320 -o preview.jpg

# Set color space
curl -X POST http://localhost:8080/color \
  -H "Content-Type: application/json" \
  -d '{"preset": "rec709"}'

# Reload page
curl -X POST http://localhost:8080/reload

# Shutdown
curl -X POST http://localhost:8080/shutdown
```

### Color Space Presets

| Preset | Color Space | Gamma | Range |
|--------|-------------|-------|-------|
| `rec709` | Rec. 709 | Rec. 709 | Full |
| `rec2020` | Rec. 2020 | Rec. 2020 | Full |
| `srgb` | sRGB | sRGB | Full |
| `rec601` | Rec. 601 | Rec. 601 | Full |

---

## Configuration

### Configuration Storage

Stream configurations are stored in:

```
~/Library/Application Support/HTML2NDI/streams.json
```

### CEF Cache

Each stream uses a unique cache directory:

```
~/Library/Caches/HTML2NDI/<stream-uuid>/
```

### Logs

Logs are stored in `~/Library/Logs/HTML2NDI/` and also sent to macOS unified logging (Console.app).

| Component | Log File | Console.app Subsystem |
|-----------|----------|----------------------|
| Manager | `~/Library/Logs/HTML2NDI/manager.log` | `com.html2ndi.manager` |
| Worker | `~/Library/Logs/HTML2NDI/html2ndi.log` | `com.html2ndi.worker` |

**View logs in Terminal:**

```bash
# Manager logs
tail -f ~/Library/Logs/HTML2NDI/manager.log

# Worker logs
tail -f ~/Library/Logs/HTML2NDI/html2ndi.log
```

**View logs in Console.app:**

1. Open Console.app
2. Click "Start streaming"
3. Filter by `com.html2ndi`

Log files automatically rotate at 10MB (keeps 5 rotated files).

---

## Troubleshooting

### CEF Fails to Initialize

**Symptoms:** App crashes on startup, "Failed to initialize CEF" error

**Solutions:**
1. Ensure the app bundle structure is complete:
   ```bash
   ls build/bin/html2ndi.app/Contents/Frameworks/
   # Should show: Chromium Embedded Framework.framework and helper apps
   ```

2. Check helper apps exist:
   ```bash
   ls "build/bin/html2ndi.app/Contents/Frameworks/html2ndi Helper.app"
   ```

3. Rebuild from clean:
   ```bash
   rm -rf build && mkdir build && cd build
   cmake .. -G Ninja && ninja
   ```

### NDI Source Not Visible

**Symptoms:** Stream runs but doesn't appear in NDI Studio Monitor

**Solutions:**
1. Verify NDI SDK is installed:
   ```bash
   ls "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"
   ```

2. Check firewall settings:
   - System Settings > Network > Firewall
   - Allow incoming connections for HTML2NDI

3. Verify NDI library is linked:
   ```bash
   otool -L build/bin/html2ndi.app/Contents/MacOS/html2ndi | grep ndi
   ```

### Multiple Streams Fail to Start

**Symptoms:** First stream works, additional streams crash

**Solutions:**
1. Ensure unique cache paths (manager handles this automatically)
2. Ensure unique NDI names (manager auto-generates if duplicate)
3. Check available ports (each stream needs a unique HTTP port)

### Keychain Permission Prompts

**Symptoms:** macOS asks for keychain access repeatedly

**Solution:** This should not happen with recent builds. If it does:
1. The `--use-mock-keychain` flag is automatically added
2. Rebuild the worker if using an old build

### High CPU Usage

**Solutions:**
1. Lower framerate: `--fps 30`
2. Lower resolution: `--width 1280 --height 720`
3. Use simpler HTML content
4. Disable unused streams

### Preview Thumbnails Not Loading

**Symptoms:** Dashboard shows broken image icons

**Solutions:**
1. Wait a few seconds after starting (first frame takes time)
2. Check worker is running: `curl http://localhost:<port>/status`
3. Verify thumbnail endpoint: `curl http://localhost:<port>/thumbnail -o test.jpg`

---

## Project Structure

```
HTML2NDI/
├── .github/
│   └── workflows/
│       ├── build.yml           # CI build workflow
│       └── release.yml         # Release workflow
├── cmake/
│   ├── CEF.cmake               # CEF download and configuration
│   └── NDI.cmake               # NDI SDK detection
├── include/html2ndi/
│   ├── application.h           # Main application class
│   ├── config.h                # Configuration structure
│   ├── cef/                    # CEF wrapper headers
│   ├── ndi/                    # NDI wrapper headers
│   ├── http/                   # HTTP server headers
│   └── utils/                  # Utility headers
├── src/
│   ├── app/                    # Application entry and config
│   ├── cef/                    # CEF implementation
│   ├── ndi/                    # NDI implementation
│   ├── http/                   # HTTP server implementation
│   └── utils/                  # Utilities implementation
├── manager/
│   ├── HTML2NDI Manager/       # Swift source files
│   │   ├── HTML2NDIManagerApp.swift
│   │   ├── StreamManager.swift
│   │   ├── ManagerServer.swift
│   │   ├── MenuBarView.swift
│   │   ├── Logger.swift        # macOS unified logging
│   │   └── Info.plist
│   ├── Resources/
│   │   └── AppIcon.icns
│   ├── Package.swift
│   └── build.sh
├── resources/
│   ├── Info.plist              # Worker app Info.plist
│   ├── helper-Info.plist       # Helper app template
│   ├── control-panel.html      # Worker control panel
│   └── test.html               # Test page
├── CMakeLists.txt              # Main CMake configuration
├── AGENTS.md                   # Project specification
├── README.md                   # This file
├── AppIcon.icns                # App icon
└── html2ndi                    # CLI wrapper script
```

---

## License

MIT License - See LICENSE file for details.

---

## Acknowledgments

- [Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef)
- [NDI SDK](https://ndi.video/)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [nlohmann/json](https://github.com/nlohmann/json)
- [stb_image_write](https://github.com/nothings/stb)
