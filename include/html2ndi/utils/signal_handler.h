#pragma once

#include <atomic>
#include <functional>

namespace html2ndi {

/**
 * Signal handler for graceful shutdown.
 * Handles SIGTERM, SIGINT, etc.
 */
class SignalHandler {
public:
    using ShutdownCallback = std::function<void()>;
    
    /**
     * Install signal handlers.
     * @param callback Function to call on shutdown signal
     */
    static void install(ShutdownCallback callback);
    
    /**
     * Remove signal handlers.
     */
    static void remove();
    
    /**
     * Check if shutdown was requested.
     */
    static bool shutdown_requested();
    
    /**
     * Wait for shutdown signal.
     * Blocks until a signal is received.
     */
    static void wait_for_shutdown();

private:
    static void signal_handler(int signal);
    
    static std::atomic<bool> shutdown_requested_;
    static ShutdownCallback callback_;
};

} // namespace html2ndi

