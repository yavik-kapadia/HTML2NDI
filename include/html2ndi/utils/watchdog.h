#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace html2ndi {

/**
 * Watchdog timer to detect main loop hangs.
 * 
 * The main loop must call heartbeat() periodically.
 * If no heartbeat is received within the timeout period,
 * the watchdog triggers an action (default: abort).
 */
class Watchdog {
public:
    using TimeoutCallback = std::function<void()>;
    
    /**
     * Create a watchdog timer.
     * @param timeout Maximum time between heartbeats before triggering
     * @param callback Optional callback on timeout (default: abort)
     */
    explicit Watchdog(
        std::chrono::seconds timeout,
        TimeoutCallback callback = nullptr);
    
    ~Watchdog();
    
    // Non-copyable
    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;
    
    /**
     * Start the watchdog timer.
     */
    void start();
    
    /**
     * Stop the watchdog timer.
     */
    void stop();
    
    /**
     * Signal that the main loop is still alive.
     * Must be called periodically to prevent timeout.
     */
    void heartbeat();
    
    /**
     * Check if watchdog is running.
     */
    bool is_running() const { return running_; }
    
    /**
     * Get time since last heartbeat.
     */
    std::chrono::milliseconds time_since_heartbeat() const;

private:
    void watchdog_thread();
    
    std::chrono::seconds timeout_;
    TimeoutCallback callback_;
    
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> last_heartbeat_{0};
    std::thread thread_;
};

} // namespace html2ndi

