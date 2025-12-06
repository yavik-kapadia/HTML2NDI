#pragma once

#include "html2ndi/config.h"
#include "html2ndi/cef/cef_handler.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace html2ndi {

/**
 * CEF off-screen renderer.
 * Manages CEF initialization and browser lifecycle.
 */
class OffscreenRenderer {
public:
    /**
     * Create an off-screen renderer.
     * @param config Application configuration
     * @param frame_callback Callback for rendered frames
     */
    OffscreenRenderer(const Config& config, FrameCallback frame_callback);
    ~OffscreenRenderer();
    
    // Non-copyable, non-movable
    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;
    
    /**
     * Initialize CEF and create browser.
     * Must be called from the main thread.
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * Run the CEF message loop.
     * Blocks until shutdown() is called.
     */
    void run_message_loop();
    
    /**
     * Perform a single iteration of the message loop.
     * Returns immediately.
     */
    void do_message_loop_work();
    
    /**
     * Shutdown CEF.
     * Must be called from the main thread.
     */
    void shutdown();
    
    /**
     * Navigate to a URL.
     * Thread-safe.
     */
    void load_url(const std::string& url);
    
    /**
     * Reload the current page.
     * Thread-safe.
     */
    void reload();
    
    /**
     * Execute JavaScript in the page.
     * Thread-safe.
     */
    void execute_javascript(const std::string& code);
    
    /**
     * Resize the viewport.
     * Thread-safe.
     */
    void resize(int width, int height);
    
    /**
     * Get the current URL.
     */
    std::string current_url() const;
    
    /**
     * Check if CEF is initialized.
     */
    bool is_initialized() const { return initialized_; }

private:
    Config config_;
    FrameCallback frame_callback_;
    
    CefRefPtr<CefHandler> handler_;
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_requested_{false};
};

} // namespace html2ndi

