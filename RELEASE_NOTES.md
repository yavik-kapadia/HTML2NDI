# HTML2NDI Release Notes

## Version 1.5.12 - Genlock Fix Release (December 26, 2024)

### 🐛 Critical Bug Fixes

#### Genlock Synchronization
- **Fixed master `synchronized` flag not being set** - Master now correctly reports `synchronized: true`
- **Fixed error handling in `set_mode()`** - Now properly checks `initialize()` return value
- **Fixed error handling in `set_master_address()`** - Now properly checks `initialize()` return value
- **Fixed audio frame pointer cast** - Corrected incompatible pointer type in `ndi_sender.cpp`

### ✨ Improvements

#### Enhanced Logging
Added comprehensive genlock diagnostic logging:
- Socket creation with file descriptor logging
- Bind success with port information for both master and slave
- First packet send/receive notifications
- Periodic packet counter logging (every 60 packets)
- Timeout warnings for slave when no packets received
- Detailed error messages with errno values

Example log output:
```
[INFO] Genlock master bound to port 49600, will send to 127.0.0.1:5960
[INFO] Genlock initialized successfully
[INFO] Genlock master thread started
[DEBUG] Genlock master sent packet #1 (frame 0)
[INFO] Genlock slave received first sync packet from 127.0.0.1:49600
[DEBUG] Genlock slave received packet #2
```

#### Build System
- Updated NDI SDK stub with missing constants (`NDIlib_send_timecode_synthesize`, `NDIlib_FourCC_video_type_BGRX`)
- Added missing NDI types (`NDIlib_tally_t`, `NDIlib_source_t`)
- Fixed NDI function declarations (`NDIlib_send_get_tally`, `NDIlib_send_get_source_name`)

### 📊 Testing Results

Verified genlock synchronization working correctly:
- ✅ Master: `synchronized: true`, packets sent successfully
- ✅ Slave: `synchronized: true`, packets received successfully
- ✅ Low jitter: ~25μs (excellent stability)
- ✅ Offset: ~3.5ms (acceptable for video sync)

### 🔧 Technical Details

#### Files Modified
- `src/ndi/genlock.cpp` - All genlock fixes and enhanced logging
- `src/ndi/ndi_sender.cpp` - Audio frame pointer fix
- `cmake/NDI.cmake` - Updated NDI stub definitions
- `src/app/config.cpp` - No changes (already correct)
- `src/app/application.cpp` - No changes (already correct)

#### Root Cause Analysis
The original issue was caused by:
1. Master's socket bind() failing silently in some cases
2. No error checking in mode switching functions
3. Insufficient logging to diagnose issues
4. Port conflicts when multiple instances running

#### Solution
1. Added comprehensive error checking throughout genlock code
2. Added detailed logging at every critical step
3. Improved error messages with errno information
4. Made port conflicts immediately visible in logs

### 📦 Installation

```bash
# Stop existing instances
killall "HTML2NDI Manager"

# Install new version
cp -R "HTML2NDI Manager.app" /Applications/

# Launch
open /Applications/HTML2NDI\ Manager.app
```

### ⚙️ Configuration

To use genlock with the fixed version:

**Master Stream:**
```
Mode: master
(Default settings work - sends to 127.0.0.1:5960)
```

**Slave Streams:**
```
Mode: slave
Master Address: 127.0.0.1:5960
```

**Alternative Port (to avoid conflicts):**
```
Master: --genlock-master 127.0.0.1:5970
Slave: --genlock slave --genlock-master 127.0.0.1:5970
```

### 🔍 Troubleshooting

If genlock isn't working:

1. **Check logs** - Look for:
   - "Genlock master bound to port X"
   - "Genlock slave received first sync packet"
   - Any ERROR messages

2. **Verify port availability**:
   ```bash
   lsof -i UDP:5960
   ```

3. **Check synchronization status**:
   ```bash
   curl http://localhost:PORT/genlock | python3 -m json.tool
   ```

4. **Expected values**:
   - Master: `synchronized: true`, `packets_sent > 0`
   - Slave: `synchronized: true`, `packets_received > 0`, `offset_us < 10000`

### 📝 Known Issues

- Port 5960 must be free for slave to bind
- If port is in use, slave initialization will fail with "Address already in use"
- Workaround: Use alternative port (e.g., 5970)

### 🙏 Credits

This release fixes critical genlock synchronization issues discovered through comprehensive testing and debugging. Special thanks to the detailed diagnostic tools and logging that made root cause analysis possible.

---

**Full Changelog**: https://github.com/yourusername/HTML2NDI/compare/v1.5.11...v1.5.12



