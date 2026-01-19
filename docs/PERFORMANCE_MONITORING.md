# Performance Monitoring and Troubleshooting Guide

## Overview

This document describes the performance monitoring and automatic recovery features added to HTML2NDI to address potential stuttering issues after extended runtime.

## Diagnostic Features

### 1. Enhanced Status Endpoint

The `/status` HTTP endpoint now includes comprehensive performance metrics:

```json
{
  "performance": {
    "buffer_size_bytes": 8294400,
    "avg_submit_time_us": 125.3,
    "avg_memcpy_time_us": 89.7,
    "uptime_seconds": 3600.5,
    "bandwidth_mbps": 497.6
  },
  "memory": {
    "resident_size_mb": 245.3,
    "virtual_size_mb": 1024.8
  }
}
```

**Metrics Explained:**
- `buffer_size_bytes`: Current frame buffer size
- `avg_submit_time_us`: Average time to submit frame from CEF to frame pump
- `avg_memcpy_time_us`: Average memory copy time
- `uptime_seconds`: Time since stream started
- `bandwidth_mbps`: Estimated bandwidth usage
- `resident_size_mb`: Physical memory usage (RSS)
- `virtual_size_mb`: Virtual memory usage

### 2. CEF Paint Timing

Track CEF rendering performance via handler metrics:
- Paint callback frequency
- Average interval between OnPaint calls

Access via Application API:
```cpp
auto handler = renderer->handler();
double paint_interval = handler->avg_paint_interval_ms();
uint64_t paint_count = handler->paint_count();
```

## Automatic Recovery Features

### 1. Frame Rate Watchdog

**Trigger**: When actual FPS drops below 50% of target for 5 consecutive seconds (after 60s uptime)

**Actions**:
1. Trigger JavaScript garbage collection
2. Send memory pressure notification to CEF
3. If degradation persists for 30+ seconds, reload the page

**Configuration**: Automatic, no configuration needed

**Logging**:
```
[WARNING] Frame rate degradation detected: 28.3 fps (target: 60 fps). Triggering recovery...
[WARNING] Reloading page to recover from performance degradation
```

### 2. Periodic Garbage Collection

**Trigger**: Every 5 minutes

**Actions**:
- Execute `window.gc()` if available in JavaScript context
- Send memory pressure notification to CEF

**Purpose**: Prevent gradual memory accumulation in rendered web pages

### 3. Pre-allocated Frame Buffers

**Implementation**: Frame buffers are pre-allocated at startup for 4K resolution

**Benefit**: Prevents memory fragmentation from repeated allocations/deallocations

**Memory Cost**: ~33 MB per stream (2 buffers × 3840 × 2160 × 4 bytes)

### 4. CEF Memory Management

**Settings Applied**:
- Plugins disabled (prevents plugin memory leaks)
- Image loading enabled but managed
- Memory pressure notifications enabled

## Monitoring Commands

### Real-time FPS Monitoring
```bash
watch -n 1 'curl -s http://localhost:8080/status | jq ".actual_fps, .performance.uptime_seconds"'
```

### Memory Growth Tracking
```bash
while true; do
    curl -s http://localhost:8080/status | jq '{
        uptime: .performance.uptime_seconds,
        rss_mb: .memory.resident_size_mb,
        fps: .actual_fps
    }'
    sleep 60
done
```

### Frame Processing Performance
```bash
curl -s http://localhost:8080/status | jq '.performance | {
    submit_us: .avg_submit_time_us,
    memcpy_us: .avg_memcpy_time_us,
    buffer_mb: (.buffer_size_bytes / 1024 / 1024)
}'
```

## Troubleshooting Workflow

### Step 1: Establish Baseline

Run stream for 15 minutes and monitor:

```bash
# Terminal 1: Monitor memory
while true; do
    ps aux | grep html2ndi | grep -v grep | awk '{print $3, $4, $6}'
    sleep 60
done

# Terminal 2: Monitor performance
watch -n 5 'curl -s http://localhost:8080/status | jq ".actual_fps, .memory.resident_size_mb"'
```

**Expected Results**:
- Memory should stabilize after initial page load
- FPS should remain constant at target
- RSS growth < 10 MB over 15 minutes = normal

### Step 2: Identify Root Cause

#### Test A: Static Content
```bash
./html2ndi --url "about:blank" --ndi-name "Test-Static"
```
**If NO stuttering**: Issue is in rendered web content

#### Test B: Simple Animation
```bash
./html2ndi --url "http://localhost:8080/testcard" --ndi-name "Test-Animated"
```
**If stuttering occurs**: Issue is in CEF or frame pipeline

#### Test C: Complex JavaScript
Test with actual production content
**If stuttering occurs**: JavaScript memory leak in page

### Step 3: Apply Targeted Fix

Based on test results:

**JavaScript Memory Leak** (most common):
- Automatic recovery should handle this
- Consider optimizing the rendered page
- Add explicit cleanup in page JavaScript

**CEF Memory Growth**:
- Increase GC frequency (modify `last_gc_time_` check in application.cpp)
- Enable CEF debug logging: `--cef-log-severity 0`

**Frame Pipeline Issue**:
- Check `avg_submit_time_us` and `avg_memcpy_time_us` metrics
- If increasing over time, may indicate system-level issue

## Performance Thresholds

### Normal Operation
- **FPS**: Within 5% of target
- **Memory growth**: < 1 MB/minute after stabilization
- **Submit time**: < 500 microseconds
- **Memcpy time**: < 200 microseconds

### Warning Signs
- **FPS**: Drops below 80% of target
- **Memory growth**: > 5 MB/minute sustained
- **Submit time**: > 1000 microseconds
- **Memcpy time**: > 500 microseconds

### Critical (Auto-recovery triggers)
- **FPS**: Below 50% of target for 5+ seconds
- **Memory**: RSS > 2 GB
- **Recovery actions**: GC → Memory pressure → Page reload

## Advanced Diagnostics

### Enable CEF Verbose Logging
```bash
./html2ndi --url "your-url" --cef-log-severity 0
```

### Monitor System-Wide
```bash
# macOS Activity Monitor
open -a "Activity Monitor"

# Command line
top -pid $(pgrep html2ndi)
```

### Analyze Memory with Instruments
```bash
instruments -t "Allocations" -D trace.trace ./html2ndi --url "your-url"
```

## Known Limitations

1. **JavaScript GC**: Only works if page exposes `window.gc()` (not standard)
2. **Memory pressure**: CEF may not respond immediately to notifications
3. **Page reload**: Causes brief interruption in NDI output (1-2 frames)
4. **Metrics overhead**: Performance tracking adds ~0.1% CPU overhead

## Future Enhancements

Potential improvements not yet implemented:

1. **Configurable thresholds**: Allow user to set FPS threshold and recovery timing
2. **Memory limits**: Hard limit on RSS with automatic restart
3. **Metrics export**: Prometheus endpoint for Grafana dashboards
4. **Page refresh scheduling**: Scheduled reloads at specific intervals
5. **CEF process isolation**: Separate renderer process with crash recovery

## Support

If stuttering persists after these fixes:

1. Collect diagnostics: Save `/status` output every minute for 20 minutes
2. Enable CEF logging and save output
3. Monitor system resources (CPU, memory, disk I/O)
4. Test with minimal HTML page to isolate issue
5. Check for macOS-specific issues (permissions, power management)
