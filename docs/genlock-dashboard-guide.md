# Genlock Dashboard Guide

## Overview

The HTML2NDI web control panel includes real-time genlock monitoring with color-coded quality indicators.

## Accessing the Dashboard

Open your browser and navigate to:
```
http://localhost:<http-port>/
```

Where `<http-port>` is the port configured for your stream (default: 8080).

## Genlock Card Layout

The Genlock card appears in the main dashboard grid and displays four key metrics:

```
┌──────────────────────────────────────────┐
│ G  Genlock                               │
├──────────────────────────────────────────┤
│                                          │
│  ┌─────────────┐  ┌──────────────┐      │
│  │ Mode        │  │ Status       │      │
│  │ Master      │  │ N/A          │      │
│  └─────────────┘  └──────────────┘      │
│                                          │
│  ┌─────────────┐  ┌──────────────┐      │
│  │ Offset      │  │ Jitter       │      │
│  │ N/A         │  │ N/A          │      │
│  └─────────────┘  └──────────────┘      │
│                                          │
│  ┌────────────────────────────────────┐ │
│  │ Note: Genlock mode is configured   │ │
│  │ via CLI arguments. Restart         │ │
│  │ required to change mode.           │ │
│  └────────────────────────────────────┘ │
└──────────────────────────────────────────┘
```

## Metrics Explained

### 1. Mode

**What it shows:** Current genlock mode

**Possible values:**
- `Disabled` - Genlock not configured (gray)
- `Master` - This stream is the timing reference (purple/blue)
- `Slave` - This stream syncs to a master (green when synced)

**Color coding:**
- Master: Accent color (purple/blue gradient)
- Slave: Green (indicates slave mode)
- Disabled: Gray/dim

### 2. Status

**What it shows:** Current synchronization state

**Possible values:**
- `N/A` - Mode is disabled or master (masters don't sync to anything)
- `Synced` - Slave is synchronized and receiving packets
- `Not Synced` - Slave is not synchronized (no packets or lost connection)

**Color coding:**
- Synced: Green ✓
- Not Synced: Red ✗

### 3. Offset

**What it shows:** Time difference between slave and master (microseconds)

**Format:** `±XXXμs`
- Positive (+): Slave is ahead of master
- Negative (−): Slave is behind master

**Color coding:**
- Green: < 1,000μs (< 1ms) - Excellent sync
- Yellow: 1,000-5,000μs (1-5ms) - Acceptable
- Red: > 5,000μs (> 5ms) - Poor sync, investigate

**What's good?**
- ±100μs: Exceptional
- ±500μs: Excellent
- ±1000μs: Good
- ±5000μs: Marginal
- > 5000μs: Problem

### 4. Jitter

**What it shows:** Timing stability variance (microseconds)

Lower is better - indicates how stable the synchronization is.

**Color coding:**
- Green: < 500μs - Stable, excellent network
- Yellow: 500-1000μs - Acceptable
- Red: > 1000μs - Unstable, check network/CPU

**What's good?**
- < 100μs: Exceptional (wired network, no load)
- < 500μs: Excellent (production ready)
- < 1000μs: Good (acceptable for most use)
- > 1000μs: Poor (network congestion or CPU issues)

## Visual Examples

### Example 1: Master Stream

```
┌──────────────────────────────────┐
│ Mode:   Master                   │ ← Purple/blue color
│ Status: N/A                      │
│ Offset: N/A                      │
│ Jitter: N/A                      │
└──────────────────────────────────┘
```

**Interpretation:** This stream is acting as the timing master. It doesn't sync to anything, so offset and jitter are not applicable.

### Example 2: Healthy Slave

```
┌──────────────────────────────────┐
│ Mode:   Slave                    │ ← Green color
│ Status: Synced                   │ ← Green checkmark
│ Offset: -125μs                   │ ← Green (excellent)
│ Jitter: 85.3μs                   │ ← Green (stable)
└──────────────────────────────────┘
```

**Interpretation:** Perfect! Slave is synced with excellent offset and low jitter. Production ready.

### Example 3: Acceptable Sync

```
┌──────────────────────────────────┐
│ Mode:   Slave                    │ ← Green color
│ Status: Synced                   │ ← Green checkmark
│ Offset: +2,450μs                 │ ← Yellow (acceptable)
│ Jitter: 650.2μs                  │ ← Yellow (acceptable)
└──────────────────────────────────┘
```

**Interpretation:** Synced and working, but offset and jitter are higher than ideal. Usable for production but monitor for improvement.

### Example 4: Problem State

```
┌──────────────────────────────────┐
│ Mode:   Slave                    │ ← Gray color
│ Status: Not Synced               │ ← Red X
│ Offset: N/A                      │
│ Jitter: N/A                      │
└──────────────────────────────────┘
```

**Interpretation:** Slave mode configured but not receiving packets. Check:
1. Master is running
2. Network connectivity
3. Firewall settings
4. Correct master address

### Example 5: Poor Sync Quality

```
┌──────────────────────────────────┐
│ Mode:   Slave                    │ ← Green color
│ Status: Synced                   │ ← Green checkmark
│ Offset: +12,850μs                │ ← Red (poor)
│ Jitter: 3,420.5μs                │ ← Red (unstable)
└──────────────────────────────────┘
```

**Interpretation:** Receiving packets but sync quality is poor. Likely causes:
- Network congestion
- CPU overload on slave
- WiFi instead of wired connection
- Packet loss

## Monitoring Best Practices

### 1. Check Immediately After Start

After launching streams, verify sync within 5-10 seconds:
- Status should change from "Not Synced" to "Synced"
- Offset should stabilize to a consistent value
- Jitter should settle below 500μs

### 2. Monitor During Production

Keep dashboard open during live production:
- Watch for "Not Synced" status (connection lost)
- Monitor offset drift (should stay consistent)
- Check jitter spikes (network issues)

### 3. Set Up External Monitoring

For critical deployments:
- Poll `/status` endpoint every few seconds
- Alert if `genlock.synchronized == false`
- Alert if `genlock.stats.jitter_us > 1000`
- Alert if `abs(genlock.offset_us) > 5000`

### 4. Regular Health Checks

Even if everything looks good:
- Check offset hasn't drifted over time
- Verify jitter remains low
- Monitor sync_failures count (via `/genlock` endpoint)

## Troubleshooting Using Dashboard

### Slave Shows "Not Synced"

**Steps:**
1. Check master stream is running
2. Verify network connectivity (`ping <master-ip>`)
3. Check firewall allows UDP port 5960
4. Verify master address in CLI args is correct
5. Look at slave terminal output for errors

### Offset Gradually Increasing

**Steps:**
1. Check system clocks (use NTP)
2. Verify same FPS on all streams
3. Check for CPU throttling
4. Restart both master and slave

### High Jitter

**Steps:**
1. Switch to wired connection (if on WiFi)
2. Check network switch load
3. Enable QoS on network
4. Reduce other network traffic
5. Check CPU usage on slave

### Offset in Yellow Zone

**Steps:**
1. Acceptable for many uses, but optimize if possible
2. Check network latency (`ping -c 100 <master-ip>`)
3. Reduce network hops between master/slave
4. Use multicast if multiple slaves

## Dashboard Refresh Rate

- Status updates every 2 seconds automatically
- No manual refresh needed
- Colors update in real-time based on latest values

## Integration with Production Workflows

### Multi-Camera Setup

Monitor all streams from multiple browser tabs:
```
Tab 1: http://cam1-server:8080/  (Master)
Tab 2: http://cam2-server:8081/  (Slave)
Tab 3: http://cam3-server:8082/  (Slave)
```

Quick glance shows sync health across all cameras.

### Graphics Overlay

```
Main Program:  http://localhost:8080/  (Master)
Lower Third:   http://localhost:8081/  (Slave) 
Bug/Logo:      http://localhost:8082/  (Slave)
```

Ensure all graphics elements are frame-synced to main output.

### Automated Monitoring

Script to check all streams:
```bash
#!/bin/bash
for port in 8080 8081 8082; do
  echo "Stream on port $port:"
  curl -s http://localhost:$port/genlock | jq '.synchronized'
done
```

## Advanced: Browser Console Inspection

Open browser DevTools (F12) and check Network tab:
- Should see `/status` requests every 2 seconds
- Response should include genlock data
- Check for any 404 or 500 errors

View raw genlock data:
```javascript
fetch('/genlock')
  .then(r => r.json())
  .then(d => console.log(d));
```

## Accessibility

- Color coding is supplemented with text ("Synced"/"Not Synced")
- All metrics have clear labels
- Works on mobile browsers (responsive design)
- No JavaScript required for basic page load

## See Also

- [Genlock User Guide](genlock.md) - Complete configuration guide
- [Genlock Implementation](genlock-implementation-summary.md) - Technical details
- [Troubleshooting](genlock.md#troubleshooting) - Detailed troubleshooting

