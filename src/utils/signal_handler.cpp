/**
 * Signal handler implementation.
 */

#include "html2ndi/utils/signal_handler.h"
#include "html2ndi/utils/logger.h"

#include <csignal>
#include <condition_variable>
#include <mutex>
#include <unistd.h>

namespace html2ndi {

std::atomic<bool> SignalHandler::shutdown_requested_{false};
SignalHandler::ShutdownCallback SignalHandler::callback_;

namespace {
    std::mutex signal_mutex;
    std::condition_variable signal_cv;
}

void SignalHandler::install(ShutdownCallback callback) {
    callback_ = std::move(callback);
    shutdown_requested_ = false;
    
    // Install signal handlers
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // kill
    std::signal(SIGHUP, signal_handler);   // Terminal closed
    
    // Ignore SIGPIPE (broken pipe)
    std::signal(SIGPIPE, SIG_IGN);
    
    LOG_DEBUG("Signal handlers installed");
}

void SignalHandler::remove() {
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    std::signal(SIGHUP, SIG_DFL);
    std::signal(SIGPIPE, SIG_DFL);
    
    callback_ = nullptr;
    
    LOG_DEBUG("Signal handlers removed");
}

bool SignalHandler::shutdown_requested() {
    return shutdown_requested_;
}

void SignalHandler::wait_for_shutdown() {
    std::unique_lock<std::mutex> lock(signal_mutex);
    signal_cv.wait(lock, [] {
        return shutdown_requested_.load();
    });
}

void SignalHandler::signal_handler(int signal) {
    const char* signal_name;
    switch (signal) {
        case SIGINT:  signal_name = "SIGINT";  break;
        case SIGTERM: signal_name = "SIGTERM"; break;
        case SIGHUP:  signal_name = "SIGHUP";  break;
        default:      signal_name = "UNKNOWN"; break;
    }
    
    // Only handle first signal
    if (shutdown_requested_.exchange(true)) {
        // Second signal - force exit
        LOG_WARNING("Received second %s signal, forcing exit", signal_name);
        _exit(128 + signal);
    }
    
    LOG_INFO("Received %s signal", signal_name);
    
    // Call callback
    if (callback_) {
        callback_();
    }
    
    // Wake up any waiting threads
    signal_cv.notify_all();
}

} // namespace html2ndi

