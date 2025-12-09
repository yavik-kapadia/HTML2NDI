# End-to-End Integration Tests

Comprehensive integration tests that validate actual NDI output including format, framerate, and progressive/interlaced mode.

## Overview

These tests start real `html2ndi` worker processes and verify the NDI streams using:
- **HTTP API** - Verifies configuration and status
- **ffmpeg with NDI** - Captures and analyzes actual NDI streams
- **NDI Tools** - Official NDI validation tools (optional)

## Prerequisites

### 1. Build the Worker

```bash
cd /Users/yavik/HTML2NDI
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

### 2. Install ffmpeg with NDI Support

#### Option A: Homebrew (Recommended)

```bash
# Install ffmpeg with NDI plugin
brew tap homebrew-ffmpeg/ffmpeg
brew install homebrew-ffmpeg/ffmpeg/ffmpeg --with-libndi_newtek
```

#### Option B: Build from Source

```bash
# 1. Download NDI SDK
# Visit: https://ndi.video/download-ndi-sdk/
# Install to /Library/NDI SDK for Apple

# 2. Build ffmpeg with NDI
git clone https://git.ffmpeg.org/ffmpeg.git
cd ffmpeg
./configure --enable-libndi_newtek \
    --extra-cflags="-I/Library/NDI\ SDK\ for\ Apple/include" \
    --extra-ldflags="-L/Library/NDI\ SDK\ for\ Apple/lib/macOS"
make -j8
sudo make install
```

#### Verify ffmpeg NDI Support

```bash
ffmpeg -formats | grep ndi
# Should show:
#  E libndi_newtek    NewTek NDI output
```

### 3. Install NDI Tools (Optional)

Download from: https://ndi.video/tools/

Useful tools:
- **NDI Studio Monitor** - Visual verification
- **NDI Analysis** - Stream inspection
- **NDI Test Patterns** - Source testing

### 4. Install jq (for API parsing)

```bash
brew install jq bc
```

## Running Tests

### CI/CD Tests (Lightweight)

For continuous integration environments:

```bash
cd /Users/yavik/HTML2NDI
./tests/integration/test_ci.sh
```

This runs:
- Worker startup validation
- HTTP API endpoint tests
- Configuration validation
- **No NDI network required**

### Full Integration Tests (Requires NDI)

For local testing with NDI validation:

```bash
cd /Users/yavik/HTML2NDI
./tests/integration/test_ndi_output.sh
# or
./tests/integration/test_ndi_output.py
```

This runs:
- All CI tests above
- NDI stream capture with ffmpeg
- Resolution/framerate validation
- Progressive/interlaced detection
- **Requires NDI SDK and ffmpeg with NDI support**

### Test Output

```
=========================================
HTML2NDI End-to-End Integration Tests
=========================================

Checking prerequisites...
✓ Prerequisites OK

=========================================
Test 1: 1080p60 Progressive
=========================================
Starting worker: 1920x1080@60fps progressive
Worker PID: 12345
Waiting for worker to initialize...
✓ Worker started
Verifying stream via HTTP API...
API reports: 1920x1080 @ 60fps progressive=true
Verifying NDI stream with ffmpeg...
Detected: 1920x1080 @ 60.00fps
Scan type: progressive
Stopping worker (PID: 12345)...
✓ Test passed: 1080p60 Progressive

... (more tests) ...

=========================================
Test Summary
=========================================
Total tests run: 5
Tests passed: 5
Tests failed: 0

✓ All tests passed!
```

## Test Cases

The test suite runs the following scenarios:

| Test | Resolution | FPS | Scan Mode | Format ID |
|------|------------|-----|-----------|-----------|
| 1 | 1920×1080 | 60 | Progressive | 1080p60 |
| 2 | 1280×720 | 50 | Progressive | 720p50 |
| 3 | 1920×1080 | 30 | Interlaced | 1080i30 |
| 4 | 3840×2160 | 30 | Progressive | 4Kp30 |
| 5 | 1280×720 | 24 | Progressive | 720p24 |

## Verification Methods

### 1. HTTP API Verification

```bash
# Query worker status
curl http://localhost:8080/status | jq

# Expected response:
{
  "width": 1920,
  "height": 1080,
  "fps": 60,
  "progressive": true,
  "ndi_name": "HTML2NDI-Test-1",
  "ndi_connections": 0,
  "running": true
}
```

### 2. ffmpeg NDI Stream Analysis

```bash
# Capture stream metadata
ffmpeg -f libndi_newtek -i "HTML2NDI-Test-1" -t 2 -f null -

# Output shows:
# Stream #0:0: Video: rawvideo (UYVY / 0x59565955), uyvy422, 1920x1080, 60 fps
# frame_format_type=progressive
```

### 3. NDI Studio Monitor (Visual)

1. Open NDI Studio Monitor
2. Select source: `HTML2NDI-Test-1`
3. Right-click → Properties
4. Verify:
   - Resolution: 1920×1080
   - Frame rate: 60.00 fps
   - Scan mode: Progressive

## Manual Testing

### Test Progressive Mode

```bash
# Start worker with progressive scan
./build/bin/html2ndi.app/Contents/MacOS/html2ndi \
    --url about:blank \
    --width 1920 \
    --height 1080 \
    --fps 60 \
    --ndi-name "Test-Progressive" \
    --http-port 8080

# Verify with ffmpeg
ffmpeg -f libndi_newtek -i "Test-Progressive" -t 5 \
    -vf "idet,metadata=mode=print" -f null -

# Look for: "frame_format_type=progressive"
```

### Test Interlaced Mode

```bash
# Start worker with interlaced scan
./build/bin/html2ndi.app/Contents/MacOS/html2ndi \
    --url about:blank \
    --width 1920 \
    --height 1080 \
    --fps 30 \
    --interlaced \
    --ndi-name "Test-Interlaced" \
    --http-port 8081

# Verify with ffmpeg
ffmpeg -f libndi_newtek -i "Test-Interlaced" -t 5 \
    -vf "idet,metadata=mode=print" -f null -

# Look for: "frame_format_type=interleaved" or interlaced fields
```

## Troubleshooting

### Error: "libndi_newtek not found"

ffmpeg doesn't have NDI support. Reinstall with NDI:

```bash
brew reinstall homebrew-ffmpeg/ffmpeg/ffmpeg --with-libndi_newtek
```

### Error: "No NDI sources found"

1. Check if NDI is initialized:
   ```bash
   # Look for NDI discovery packets
   sudo tcpdump -i any port 5960 or port 5961
   ```

2. Verify worker is running:
   ```bash
   curl http://localhost:8080/status
   ```

3. Check firewall settings (allow ports 5960-5970)

### Error: "Worker failed to start"

Check worker logs:

```bash
cat /tmp/html2ndi-worker-*.log
```

Common issues:
- NDI SDK not installed
- CEF cache conflicts
- Port already in use

### Tests Pass but NDI Not Visible

1. Check NDI discovery:
   ```bash
   # List NDI sources on network
   ffmpeg -f libndi_newtek -find_sources 1 -i dummy
   ```

2. Verify NDI name:
   ```bash
   curl http://localhost:8080/status | jq -r '.ndi_name'
   ```

3. Check network interface (NDI uses mDNS):
   ```bash
   dns-sd -B _ndi._tcp
   ```

## CI/CD Integration

### GitHub Actions

```yaml
- name: Run Integration Tests
  run: |
    # Install dependencies
    brew install ffmpeg jq bc
    brew tap homebrew-ffmpeg/ffmpeg
    brew install homebrew-ffmpeg/ffmpeg/ffmpeg --with-libndi_newtek
    
    # Run tests
    ./tests/integration/test_ndi_output.sh
```

### Docker

For containerized testing, use:
- `newtek/ndi-sdk` base image
- Mount NDI SDK
- Expose ports 5960-5970

## Advanced Testing

### Performance Testing

```bash
# Test sustained framerate
ffmpeg -f libndi_newtek -i "HTML2NDI-Test-1" \
    -t 60 -f null - 2>&1 | grep "fps="

# Should consistently show target FPS (e.g., 60 fps)
```

### Latency Testing

```bash
# Measure glass-to-glass latency
# 1. Display timestamp overlay in source HTML
# 2. Capture NDI stream
# 3. Compare timestamp with system clock
```

### Network Load Testing

```bash
# Simulate multiple receivers
for i in {1..5}; do
    ffmpeg -f libndi_newtek -i "HTML2NDI-Test-1" \
        -t 60 -f null - &
done

# Monitor bandwidth
nettop -p html2ndi
```

## API Testing

Test HTTP endpoints:

```bash
# Get status
curl http://localhost:8080/status | jq

# Change URL
curl -X POST http://localhost:8080/seturl \
    -H "Content-Type: application/json" \
    -d '{"url": "https://example.com"}'

# Reload page
curl -X POST http://localhost:8080/reload

# Get thumbnail
curl http://localhost:8080/thumbnail?width=320 -o thumb.jpg
```

## Contributing

To add new tests:

1. Add test case to `test_ndi_output.sh`:
   ```bash
   run_test "Test Name" width height fps progressive
   ```

2. Document expected behavior in this README

3. Run full test suite:
   ```bash
   ./tests/integration/test_ndi_output.sh
   ```

## See Also

- [Unit Tests](../cpp/) - C++ unit tests
- [Web Tests](../web/) - Browser-based UI tests
- [NDI Documentation](https://ndi.video/documentation/)
- [ffmpeg NDI Plugin](https://github.com/Palakis/obs-ndi)

