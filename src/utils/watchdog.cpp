/**
 * Watchdog timer implementation.
 */

#include "html2ndi/utils/watchdog.h"
#include "html2ndi/utils/logger.h"

#include <cstdlib>

namespace html2ndi {

Watchdog::Watchdog(std::chrono::seconds timeout, TimeoutCallback callback)
    : timeout_(timeout)
    , callback_(std::move(callback)) {
}

Watchdog::~Watchdog() {
    stop();
}

void Watchdog::start() {
    if (running_) {
        return;
    }
    
    LOG_INFO("Starting watchdog timer (timeout: %llds)", 
             static_cast<long long>(timeout_.count()));
    
    running_ = true;
    heartbeat(); // Initial heartbeat
    
    thread_ = std::thread(&Watchdog::watchdog_thread, this);
}

void Watchdog::stop() {
    if (!running_) {
        return;
    }
    
    LOG_DEBUG("Stopping watchdog timer");
    running_ = false;
    
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Watchdog::heartbeat() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    last_heartbeat_ = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::chrono::milliseconds Watchdog::time_since_heartbeat() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::chrono::milliseconds(now_ms - last_heartbeat_.load());
}

void Watchdog::watchdog_thread() {
    LOG_DEBUG("Watchdog thread started");
    
    while (running_) {
        // Sleep for 1 second intervals to allow responsive shutdown
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (!running_) {
            break;
        }
        
        auto elapsed = time_since_heartbeat();
        auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_);
        
        if (elapsed > timeout_ms) {
            LOG_FATAL("Watchdog timeout! No heartbeat for %lldms (timeout: %lldms)",
                      static_cast<long long>(elapsed.count()),
                      static_cast<long long>(timeout_ms.count()));
            
            if (callback_) {
                // Call custom callback
                callback_();
            } else {
                // Default action: abort
                LOG_FATAL("Main loop hung - aborting process");
                std::abort();
            }
            
            // Stop after triggering
            running_ = false;
            break;
        }
    }
    
    LOG_DEBUG("Watchdog thread exited");
}

} // namespace html2ndi




