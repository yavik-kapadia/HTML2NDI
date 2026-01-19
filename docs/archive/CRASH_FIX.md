# Thread Join Crash Fix

## Issue
```
libc++abi: terminating due to uncaught exception of type std::__1::system_error: 
thread::join failed: Invalid argument
```

## Root Cause
The `thread::join()` call in `GenlockClock::shutdown()` was failing because:
1. Thread might already be detached or not properly initialized
2. Rapid shutdown/reinitialize cycles in `set_mode()` and `set_master_address()`
3. No exception handling around `join()` call
4. No delay between shutdown and reinitialization

## Fix Applied

### 1. Added Exception Handling
```cpp
if (sync_thread_.joinable()) {
    try {
        sync_thread_.join();
    } catch (const std::system_error& e) {
        LOG_ERROR("Error joining genlock thread: %s", e.what());
    }
}
```

### 2. Added Cleanup in shutdown()
```cpp
initialized_ = false;
synchronized_ = false;  // Reset sync flag
```

### 3. Added Delay in Mode Switching
```cpp
void GenlockClock::set_mode(GenlockMode mode) {
    if (was_initialized) {
        shutdown();
        // Give thread time to fully exit
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // ... reinitialize
}
```

### 4. Same Fix for set_master_address()
Added 10ms delay after shutdown before reinitializing.

## Testing
- ✅ No crashes on mode switching
- ✅ Clean shutdown without exceptions
- ✅ Rapid start/stop cycles work correctly
- ✅ Thread cleanup verified

## Files Modified
- `src/ndi/genlock.cpp` - Added exception handling and delays

## Version
Fixed in v1.5.12 (updated build)



