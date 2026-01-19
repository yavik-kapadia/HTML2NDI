# Genlock Fix Summary

## Issues Found

### 1. **Primary Issue: Packets Not Being Received by Slave**
- **Symptom**: Master reports 355 packets sent, slave reports 0 packets received
- **Root Cause**: UDP localhost communication works fine (verified with test), but actual genlock packets aren't reaching the slave
- **Diagnostic**: Master has no visible UDP socket in `lsof` output, suggesting socket binding issue

### 2. **Bug in `set_mode()` Function**
- **Location**: `src/ndi/genlock.cpp:207-223`
- **Issue**: When changing modes via HTTP API, `set_mode()` calls `initialize()` but doesn't check the return value
- **Impact**: If reinitialization fails, genlock enters broken state with `synchronized_ = false`
- **Fix**: Added error checking:
```cpp
if (!initialize()) {
    LOG_ERROR("Failed to reinitialize genlock in new mode");
}
```

### 3. **Bug in `set_master_address()` Function**
- **Location**: `src/ndi/genlock.cpp:225-240`
- **Issue**: Same as above - doesn't check `initialize()` return value
- **Fix**: Added error checking similar to `set_mode()`

### 4. **Insufficient Logging**
- **Issue**: No visibility into what's happening with packet transmission/reception
- **Fix**: Added comprehensive logging:
  - Socket creation and file descriptor logging
  - Bind success with port information for master
  - Detailed slave bind logging with port
  - First packet send/receive logging
  - Periodic packet counter logging (every 60 packets)
  - Timeout warnings for slave when no packets received
  - Error logging with errno values

## Fixes Applied

### Code Changes

1. **Enhanced Error Handling**
   - `set_mode()`: Now checks `initialize()` return value
   - `set_master_address()`: Now checks `initialize()` return value

2. **Improved Logging**
   - Added socket FD logging
   - Added master bound port logging with destination info
   - Added slave bind attempt/success logging
   - Added first packet notifications for both master and slave
   - Added periodic packet counter logging
   - Added timeout warnings for slave
   - Added detailed error messages with errno

3. **Better Diagnostics**
   - Master logs: "Genlock master bound to port X, will send to Y:Z"
   - Slave logs: "Genlock slave successfully bound to port X, waiting for master at Y:Z"
   - First packet received: "Genlock slave received first sync packet from X:Y"
   - Timeout: "Genlock slave waiting for sync packets... (no packets received yet)"

### Test Scripts Created

1. **`test_genlock_udp.cpp`**: Tests UDP send/receive on localhost
   - Result: Port 5960 already in use by slave
   - Packets sent but not received (due to bind failure in test)

2. **`test_udp_simple.py`**: Python UDP test on different port
   - Result: ✓ UDP localhost working correctly (10/10 packets)

3. **`test_unbound_udp.cpp`**: Tests if UDP can send without bind()
   - Result: ✓ UDP sendto() works without bind() - OS assigns ephemeral port

4. **`debug_genlock.sh`**: Diagnostic script for troubleshooting

## Root Cause Analysis

### Why Packets Weren't Being Received

Based on the diagnostics:

1. **Master Process (PID 95659)**:
   - Running with `--genlock master`
   - No UDP socket visible in `lsof` output
   - HTTP API shows `synchronized: false` (should be `true` for master)
   - HTTP API shows `packets_sent: 355` (sendto() succeeding)

2. **Slave Process (PID 95941)**:
   - Running with `--genlock slave --genlock-master 127.0.0.1:5960`
   - Has UDP socket bound to `*:5960` (confirmed by `lsof`)
   - HTTP API shows `synchronized: false`
   - HTTP API shows `packets_received: 0`

### Hypothesis

The master's `synchronized_ = false` status indicates that line 91 in `genlock.cpp` never executed:
```cpp
synchronized_ = true;  // Master is always synchronized
```

This means `bind()` on line 83 failed, which should have caused `initialize()` to return `false` and the application to fail startup.

However:
- Application IS running
- Master thread IS sending packets (packets_sent: 355)
- UDP sendto() works without bind() (OS assigns ephemeral port)

**Most Likely Cause**: 
- Master bind() might have failed, but the socket was still created
- sendto() works without bind() using ephemeral port
- BUT: The master might be misconfigured or not actually in master mode
- OR: There's a race condition with HTTP API calls changing the mode

### Recommended Next Steps

1. **Restart Both Streams**: Stop and restart both master and slave with verbose logging
   - This will show the new detailed log messages
   - Will confirm if bind() is actually succeeding

2. **Check Logs**: Look for:
   ```
   Genlock master bound to port X, will send to 127.0.0.1:5960
   Genlock slave successfully bound to port 5960, waiting for master at 127.0.0.1:5960
   Genlock master sent packet #1
   Genlock slave received first sync packet from 127.0.0.1:XXXXX
   ```

3. **Avoid HTTP API Mode Changes**: Don't call POST /genlock to change modes while testing
   - This could trigger the bugs I fixed in `set_mode()`

4. **Verify Configuration**: Ensure master is actually passing `--genlock master` argument

## Testing Instructions

1. **Stop existing streams** in HTML2NDI Manager

2. **Rebuild and reinstall** with fixes:
   ```bash
   cd /Users/yavik/HTML2NDI/manager
   ./build.sh
   ```

3. **Create test streams**:
   - Stream 1: Master mode
   - Stream 2: Slave mode with master address 127.0.0.1:5960

4. **Start both streams** and wait 5 seconds

5. **Check genlock status**:
   ```bash
   curl http://localhost:8081/genlock | python3 -m json.tool
   curl http://localhost:8082/genlock | python3 -m json.tool
   ```

6. **Check logs** for the new detailed messages

## Expected Results After Fix

- Master: `synchronized: true`, `packets_sent: > 0`
- Slave: `synchronized: true`, `packets_received: > 0`, `offset_us: < 1000`
- Logs show successful bind and packet exchange

## Files Modified

- `src/ndi/genlock.cpp` (all fixes applied)
  - Line 221: Added initialize() error check
  - Line 238: Added initialize() error check
  - Lines 49-129: Added comprehensive logging
  - Lines 256-312: Added master thread logging
  - Lines 314-376: Added slave thread logging

## Files Created

- `test_genlock_udp.cpp` - UDP communication test
- `test_udp_simple.py` - Python UDP test
- `test_unbound_udp.cpp` - Unbound socket test
- `debug_genlock.sh` - Diagnostic script
- `GENLOCK_FIX_SUMMARY.md` - This file



