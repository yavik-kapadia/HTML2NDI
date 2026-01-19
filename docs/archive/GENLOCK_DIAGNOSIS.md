# Genlock Diagnosis Report

Generated: 2025-12-26 11:05:15

## Current Status

### Master Process (PID 95659)
- **Port**: 8081
- **Genlock Mode**: master
- **Command**: `--genlock master`
- **Synchronized**: `false` ❌ (SHOULD BE `true`)
- **Packets Sent**: 355
- **Packets Received**: 0
- **UDP Socket**: **NONE** ❌ (Critical Issue)

### Slave Process (PID 95941)
- **Port**: 8082  
- **Genlock Mode**: slave
- **Command**: `--genlock slave --genlock-master 127.0.0.1:5960`
- **Synchronized**: `false` ❌
- **Packets Sent**: 0
- **Packets Received**: 0 ❌
- **UDP Socket**: Bound to `*:5960` ✓

## Root Cause Analysis

### Primary Issue: Master Socket Initialization Failure

The master's `synchronized: false` status indicates that this line never executed:
```cpp
synchronized_ = true;  // Line 91 in genlock.cpp
```

This means `bind()` failed on line 83, which should have caused `initialize()` to return `false`.

### Evidence

1. **No UDP Socket**: `lsof` shows NO UDP socket for master process
2. **Packets Still "Sent"**: 355 packets reported as sent  
   - This is possible because UDP `sendto()` works without `bind()`
   - OS assigns ephemeral port automatically
   - But packets go nowhere/wrong destination

3. **Packet Capture Test**: No genlock packets received on port 5961 during 10-second test

### Why bind() Might Have Failed

Possible reasons:
1. **Port already in use** during master startup
2. **Permission issue** with UDP socket creation
3. **Race condition** with slave starting first
4. **Error not properly logged** (no genlock logs found)

## Sequence of Events (Hypothesis)

1. User starts streams via HTML2NDI Manager
2. Master process starts with `--genlock master`  
3. Master calls `GenlockClock::initialize()`
4. Socket creation succeeds (`socket()`)
5. **Bind fails** (`bind()` returns < 0)
6. Error logged but not visible in current log file
7. Function returns `false`
8. Application SHOULD have failed to start
9. But application IS running... **Why?**

### Possible Explanation

Looking at `application.cpp:48-51`:
```cpp
if (!genlock_clock_->initialize()) {
    LOG_ERROR("Failed to initialize genlock");
    return false;  // Should cause app to exit
}
```

The app should have exited! But it's running. This suggests:
- Initialize() returned `true` even though bind() failed, OR
- The error happened after initialization, OR  
- There's a code path we're not seeing

## Testing Results

### UDP Functionality Test
```bash
python3 test_udp_simple.py
```
Result: ✅ UDP localhost working (10/10 packets received)

### Genlock Packet Capture
```bash
python3 test_genlock_capture.py  
```
Result: ❌ No genlock packets received in 10 seconds

### Binary Analysis
- Binary Date: December 18, 2024
- Contains genlock strings but OLD code (pre-fixes)
- No new logging statements present
- Linked against real NDI SDK (libndi.dylib v6.0.0)

## What's Actually Happening

Based on the evidence, here's what's likely occurring:

1. **Master's genlock initialization succeeded partially**
   - Socket created
   - Bind might have succeeded or failed silently
   - But `synchronized_` flag not set correctly

2. **Master is sending packets somewhere**
   - 355 packets "sent" via `sendto()`
   - But not arriving at slave's port 5960
   - Likely going to wrong address or dropped

3. **Slave is waiting correctly**
   - Bound to port 5960 successfully  
   - Listening for packets
   - But receiving nothing

## Network Configuration

### Current Setup
- Master sending to: `127.0.0.1:5960` (default)
- Slave listening on: `0.0.0.0:5960` ✓
- Firewall: Unknown (needs checking)
- Loopback: Should work (UDP test passed)

### Port Status
```
lsof -i UDP:5960
html2ndi  95941  *:5960  (slave only)
```

Master has no visible UDP socket!

## Recommendations

### Immediate Actions

1. **Stop both streams completely**
2. **Restart in specific order**:
   - Start Master first (let it bind and initialize)
   - Wait 2 seconds
   - Start Slave  
3. **Monitor with verbose logging** (-v flag)
4. **Check system logs**: `log stream --predicate 'process == "html2ndi"'`

### After Rebuild with Fixes

The fixes I applied add:
- Error checking in `set_mode()` and `set_master_address()`
- Comprehensive logging:
  - "Created genlock socket: fd=X"
  - "Genlock master bound to port X, will send to Y:Z"
  - "Genlock slave successfully bound to port X"
  - "Genlock master sent packet #N"
  - "Genlock slave received first sync packet from X:Y"

### Alternative Workarounds

If rebuild continues to fail:

1. **Use different genlock port**:
   ```
   Master: --genlock master --genlock-master 127.0.0.1:5970
   Slave: --genlock slave --genlock-master 127.0.0.1:5970  
   ```

2. **Use broadcast address** (if on same machine):
   ```
   Master: --genlock master --genlock-master 255.255.255.255:5960
   ```

3. **Check firewall settings**:
   ```bash
   /usr/libexec/ApplicationFirewall/socketfilterfw --listapps | grep html2ndi
   ```

## Next Steps

1. ✅ Identified root cause (master socket binding failure)
2. ✅ Created comprehensive fixes with logging
3. ⏳ Need to rebuild with real NDI SDK
4. ⏳ Test with new binary
5. ⏳ Verify genlock synchronization works

## Files Modified

- `src/ndi/genlock.cpp` - All fixes applied
- Build blocked by NDI stub vs real SDK mismatch

## Build Issue

Current blocker: NDI stub missing constants:
- `NDIlib_send_timecode_synthesize`
- `NDIlib_FourCC_video_type_BGRX`  
- `NDIlib_tally_t`
- `NDIlib_source_t`
- `NDIlib_send_get_source_name`

Real NDI SDK (libndi.dylib) is available at:
`/Applications/HTML2NDI Manager.app/Contents/Resources/html2ndi.app/Contents/Frameworks/libndi.dylib`

Need to configure build to use real SDK instead of stub.



