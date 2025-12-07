# Genlock (Frame Synchronization)

## Overview

Genlock enables frame-accurate synchronization between multiple HTML2NDI streams. This is essential for professional broadcast environments where multiple video sources must maintain perfect timing alignment.

## What is Genlock?

Genlock (Generator Locking) is a technique where one video device (the master) provides a timing reference that other devices (slaves) synchronize to. This ensures:

- Frame-accurate alignment across multiple streams
- Consistent NDI timecode across all outputs
- Seamless switching between sources in production environments
- Proper multi-camera setups

## Architecture

HTML2NDI implements genlock using a master/slave topology:

### Master Mode
- Generates timing reference clock
- Broadcasts sync packets over UDP at each frame boundary
- All frame timing and NDI timecode derived from reference clock
- Always considers itself synchronized

### Slave Mode
- Receives sync packets from master
- Adjusts local clock to match master timing
- Uses exponential moving average for smooth synchronization
- Tracks sync quality metrics (offset, jitter, packet loss)

### Disabled Mode
- No synchronization
- Each stream uses independent local clock
- Default behavior (backward compatible)

## Quick Workflow

```
1. Configure via CLI
   ↓
   ./html2ndi --genlock master  (or slave)
   
2. Worker starts with genlock enabled
   ↓
   Master broadcasts sync packets
   Slaves receive and synchronize
   
3. Monitor via Dashboard
   ↓
   http://localhost:8080/
   View real-time sync status
   
4. Check Quality
   ↓
   Offset < 1ms? ✓ Good
   Jitter < 500μs? ✓ Stable
   Status: Synced? ✓ Working
```

## Configuration

### CLI Arguments

```bash
# Master stream
./html2ndi --url https://graphics1.local \
           --ndi-name "Camera 1" \
           --genlock master \
           --genlock-master 239.255.0.1:5960

# Slave streams
./html2ndi --url https://graphics2.local \
           --ndi-name "Camera 2" \
           --genlock slave \
           --genlock-master 239.255.0.1:5960

./html2ndi --url https://graphics3.local \
           --ndi-name "Camera 3" \
           --genlock slave \
           --genlock-master 239.255.0.1:5960
```

### Configuration Options

| Option | Description | Values | Default |
|--------|-------------|--------|---------|
| `--genlock` | Genlock mode | `disabled`, `master`, `slave` | `disabled` |
| `--genlock-master` | Master address (for slave mode) | `IP:PORT` | `127.0.0.1:5960` |

### Manager UI Configuration

In the HTML2NDI Manager, each stream can be configured with genlock settings:

1. **Genlock Mode**: Choose `disabled`, `master`, or `slave`
2. **Master Address**: Set the master's IP and port (for slave mode)

The configuration is persisted and applied when starting streams.

## Network Setup

### Unicast (Single Machine)
For streams on the same machine:
```bash
--genlock-master 127.0.0.1:5960
```

### Unicast (Multiple Machines)
For streams across a network:
```bash
# Master
--genlock-master 192.168.1.100:5960

# Slaves
--genlock-master 192.168.1.100:5960
```

### Multicast (Recommended for Production)
For efficient multi-slave deployments:
```bash
--genlock-master 239.255.0.1:5960
```

**Note**: Ensure your network switch supports multicast (IGMP snooping enabled).

## Monitoring Sync Quality

### Web Dashboard

Open the control panel in your browser:

```
http://localhost:8080/
```

The dashboard includes a **Genlock** card that displays:

- **Mode**: Current genlock mode (Disabled/Master/Slave)
  - Master: Displayed in accent color
  - Slave: Displayed in green when synced
  - Disabled: Displayed in gray
  
- **Status**: Synchronization state
  - "Synced" (green): Slave is receiving packets and synchronized
  - "Not Synced" (red): Slave is not synchronized or no packets received
  - "N/A": Genlock is disabled
  
- **Offset**: Time offset from master (slave mode only)
  - Green: < 1ms (excellent)
  - Yellow: 1-5ms (acceptable)
  - Red: > 5ms (poor sync)
  
- **Jitter**: Timing stability (slave mode only)
  - Green: < 500μs (excellent)
  - Yellow: 500μs - 1ms (acceptable)
  - Red: > 1ms (unstable)

The dashboard auto-refreshes every 2 seconds, providing real-time monitoring of sync quality.

**For detailed dashboard usage**, see [Dashboard Guide](genlock-dashboard-guide.md)

**Example Dashboard States:**

```
┌─────────────────────────────────┐
│ Genlock                         │
├─────────────────────────────────┤
│ Mode: Master (purple)           │
│ Status: N/A                     │
│ Offset: N/A                     │
│ Jitter: N/A                     │
│                                 │
│ Note: Genlock mode is           │
│ configured via CLI arguments.   │
│ Restart required to change.     │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│ Genlock                         │
├─────────────────────────────────┤
│ Mode: Slave (green)             │
│ Status: Synced (green)          │
│ Offset: -125μs (green)          │
│ Jitter: 85.3μs (green)          │
│                                 │
│ Note: Genlock mode is           │
│ configured via CLI arguments.   │
│ Restart required to change.     │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│ Genlock                         │
├─────────────────────────────────┤
│ Mode: Slave (gray)              │
│ Status: Not Synced (red)        │
│ Offset: N/A                     │
│ Jitter: N/A                     │
│                                 │
│ Note: Genlock mode is           │
│ configured via CLI arguments.   │
│ Restart required to change.     │
└─────────────────────────────────┘
```

### HTTP API

Query genlock status via HTTP API:

```bash
curl http://localhost:8080/genlock
```

Response:
```json
{
  "mode": "slave",
  "synchronized": true,
  "offset_us": -125,
  "available": true,
  "stats": {
    "packets_sent": 0,
    "packets_received": 3842,
    "sync_failures": 2,
    "avg_offset_us": -125,
    "max_offset_us": 2450,
    "jitter_us": 85.3
  }
}
```

### Status Metrics

| Metric | Description | Good Value | Dashboard Color |
|--------|-------------|------------|-----------------|
| `synchronized` | Slave is receiving packets | `true` | Green |
| `offset_us` | Time offset from master (μs) | < 1000 | Green < 1ms, Yellow 1-5ms, Red > 5ms |
| `jitter_us` | Timing stability (μs) | < 500 | Green < 500μs, Yellow 500μs-1ms, Red > 1ms |
| `sync_failures` | Failed sync attempts | Low count | N/A |
| `packets_received` | Total packets received | Increasing | N/A |

### Interpreting Offset

- **Positive offset**: Slave is ahead of master
- **Negative offset**: Slave is behind master
- **Target**: Keep within ±1ms (±1000μs)

High offset or jitter indicates:
- Network congestion
- CPU overload
- Incorrect master address
- Firewall blocking UDP

## Performance Considerations

### CPU Impact
- **Master**: Minimal overhead (< 1% CPU)
- **Slave**: < 2% CPU overhead for sync calculations

### Network Bandwidth
- Sync packets: ~100 bytes per frame
- 60 FPS: ~6 KB/s per master
- Negligible compared to NDI video bandwidth

### Latency
- Sync convergence: < 1 second
- Frame alignment accuracy: Sub-millisecond
- No added video latency (timing only)

## Best Practices

### 1. Designate One Master
- Only one stream should be in master mode
- All others should be slaves pointing to that master
- Master should be the most critical/stable stream

### 2. Use Multicast for Scale
- Unicast requires master to send packets to each slave
- Multicast sends one packet received by all slaves
- Critical for 3+ streams

### 3. Match Frame Rates
- All streams should use the same FPS
- Genlock requires consistent frame timing
- Mismatched rates will cause drift

### 4. Monitor Sync Quality
- Check offset and jitter regularly
- High jitter (> 1ms) indicates network issues
- Set up alerting for `synchronized: false`

### 5. Network Configuration
- Use dedicated VLAN for sync traffic if possible
- Enable QoS/traffic prioritization
- Ensure low network latency (< 10ms RTT)

### 6. Firewall Rules
Ensure UDP port is open:
```bash
# macOS
sudo pfctl -f /etc/pf.conf

# Add to pf.conf if needed:
pass in proto udp to port 5960
```

## Troubleshooting

### Slave Not Synchronizing

**Symptoms**: `synchronized: false`, no packets received

**Solutions**:
1. Check master is running and in master mode
2. Verify master address and port
3. Check firewall rules
4. Test network connectivity:
   ```bash
   nc -u <master-ip> 5960
   ```
5. Check for port conflicts

### High Jitter / Unstable Sync

**Symptoms**: `jitter_us > 1000`, erratic offset

**Solutions**:
1. Reduce network load
2. Check for CPU overload on slave
3. Use wired network instead of WiFi
4. Enable QoS on network switches
5. Move to multicast if using unicast

### Offset Drift

**Symptoms**: Offset steadily increasing

**Solutions**:
1. Check system clocks (NTP sync)
2. Verify same FPS on all streams
3. Ensure master is stable
4. Check for dropped frames

### Packets Not Received

**Symptoms**: `packets_received: 0`

**Solutions**:
1. Verify UDP port is open
2. Check multicast routing (if using multicast)
3. Test with unicast to localhost first
4. Disable firewall temporarily to test
5. Use tcpdump/Wireshark to verify packets

## Advanced Topics

### Custom Sync Port

To avoid conflicts, use a custom port:
```bash
--genlock-master 192.168.1.100:7000
```

### Multiple Master Groups

Run independent genlock groups:
```bash
# Group A: port 5960
# Group B: port 5961
```

Each group has its own master and slaves.

### Genlock with Color Space

Genlock synchronizes timing only. Color space settings are independent:
```bash
--genlock slave \
--genlock-master 192.168.1.100:5960
# Color space configured separately via HTTP API or presets
```

### Integration with PTP (Future)

While not currently implemented, genlock could integrate with IEEE 1588 PTP for even tighter synchronization across network infrastructure.

## API Reference

### C++ API

```cpp
#include "html2ndi/ndi/genlock.h"

// Create genlock clock
auto genlock = std::make_shared<GenlockClock>(
    GenlockMode::Master,
    "239.255.0.1:5960",
    60  // FPS
);

// Initialize
genlock->initialize();

// Get synchronized time
auto time = genlock->now();

// Get NDI timecode
int64_t tc = genlock->get_ndi_timecode();

// Calculate next frame boundary
auto next = genlock->next_frame_boundary(
    time,
    std::chrono::nanoseconds(1'000'000'000 / 60)
);

// Check sync status
bool synced = genlock->is_synchronized();

// Get statistics
auto stats = genlock->get_stats();

// Shutdown
genlock->shutdown();
```

### HTTP API Endpoints

#### GET /genlock
Get current genlock status

**Response**:
```json
{
  "mode": "master|slave|disabled",
  "synchronized": true|false,
  "offset_us": 0,
  "stats": { ... }
}
```

#### POST /genlock
Configure genlock (not yet implemented - requires restart)

**Request**:
```json
{
  "mode": "master|slave|disabled",
  "master_address": "IP:PORT"
}
```

**Response**:
```json
{
  "error": "Genlock configuration requires restart",
  "hint": "Restart with --genlock flag"
}
```

## Examples

### Basic Two-Stream Setup

```bash
# Terminal 1: Master
./html2ndi.app/Contents/MacOS/html2ndi \
  --url https://graphics1.local \
  --ndi-name "Graphics 1" \
  --genlock master \
  --http-port 8080

# Terminal 2: Slave
./html2ndi.app/Contents/MacOS/html2ndi \
  --url https://graphics2.local \
  --ndi-name "Graphics 2" \
  --genlock slave \
  --genlock-master 127.0.0.1:5960 \
  --http-port 8081
```

### Multi-Stream Production Setup

```bash
# Master: Main program output
./html2ndi --url https://program.local \
           --ndi-name "Program" \
           --genlock master \
           --genlock-master 239.255.0.1:5960

# Slave 1: Preview
./html2ndi --url https://preview.local \
           --ndi-name "Preview" \
           --genlock slave \
           --genlock-master 239.255.0.1:5960

# Slave 2: Lower third
./html2ndi --url https://lower-third.local \
           --ndi-name "Lower Third" \
           --genlock slave \
           --genlock-master 239.255.0.1:5960

# Slave 3: Bug/logo
./html2ndi --url https://bug.local \
           --ndi-name "Bug" \
           --genlock slave \
           --genlock-master 239.255.0.1:5960
```

## Technical Implementation

### Sync Packet Format

```cpp
struct GenlockPacket {
    uint32_t magic;          // 'GNLK' (0x474E4C4B)
    uint32_t version;        // Protocol version (1)
    int64_t timestamp_ns;    // Master timestamp (nanoseconds)
    int64_t frame_number;    // Frame counter
    uint32_t fps;            // Framerate
    uint32_t checksum;       // Simple XOR checksum
};
```

### Synchronization Algorithm

1. **Master**: Broadcasts packet at each frame boundary
2. **Slave**: Receives packet and calculates offset
3. **Offset**: `slave_local_time - master_timestamp`
4. **Smoothing**: Apply exponential moving average (α=0.1)
5. **Adjustment**: Adjust all timing calculations by offset

### NDI Timecode Generation

```cpp
// Calculate elapsed time from reference
auto elapsed = now() - reference_time;

// Convert to 100ns units for NDI
int64_t timecode = elapsed.count() / 100;
```

All genlocked streams use the same reference time, ensuring identical timecodes.

## Limitations

1. **No Dynamic Reconfiguration**: Changing genlock mode requires restart
2. **UDP Only**: No TCP fallback (by design for low latency)
3. **No Authentication**: Sync packets are unencrypted (use VLANs for security)
4. **Single Master**: No master failover or redundancy
5. **Same-FPS Only**: All streams must use identical frame rate

## Future Enhancements

- PTP integration for sub-microsecond sync
- Master election for automatic failover
- Encrypted sync packets
- Dynamic mode switching via HTTP API
- Mixed frame rate support with ratio calculations
- Sync quality alerts and logging

## See Also

- [NDI Timecode Specification](https://ndi.video/documentation/)
- [IEEE 1588 PTP](https://en.wikipedia.org/wiki/Precision_Time_Protocol)
- [SMPTE Timecode](https://en.wikipedia.org/wiki/SMPTE_timecode)

