#pragma once

#include "html2ndi/config.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace html2ndi {

// Forward declarations
class OffscreenRenderer;
class NdiSender;
class FramePump;
class HttpServer;
class Logger;
class Watchdog;

/**
 * Main application class.
 * Coordinates all subsystems: CEF renderer, NDI sender, HTTP server.
 */
class Application {
public:
    explicit Application(Config config);
    ~Application();
    
    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    
    /**
     * Initialize all subsystems.
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * Run the main event loop.
     * Blocks until shutdown is requested.
     * @return exit code
     */
    int run();
    
    /**
     * Request graceful shutdown.
     * Can be called from any thread.
     */
    void shutdown();
    
    /**
     * Check if shutdown has been requested.
     */
    bool is_shutting_down() const;
    
    /**
     * Navigate to a new URL.
     * @param url The URL to load
     */
    void set_url(const std::string& url);
    
    /**
     * Reload the current page.
     */
    void reload();
    
    /**
     * Get the current configuration.
     */
    const Config& config() const { return config_; }
    
    /**
     * Get current URL.
     */
    std::string current_url() const;
    
    /**
     * Get NDI connection count.
     */
    int ndi_connection_count() const;
    
    /**
     * Get current FPS (actual rendered frames per second).
     */
    float current_fps() const;
    
    /**
     * Get NDI sender for configuration.
     */
    NdiSender* ndi_sender() { return ndi_sender_.get(); }
    
    /**
     * Get a JPEG thumbnail of the current frame.
     * @param out_jpeg Output JPEG data
     * @param width Target width (0 for original)
     * @param quality JPEG quality (1-100)
     * @return true if thumbnail was generated
     */
    bool get_thumbnail(std::vector<uint8_t>& out_jpeg, int width = 320, int quality = 75);

private:
    Config config_;
    std::atomic<bool> shutdown_requested_{false};
    
    std::unique_ptr<OffscreenRenderer> renderer_;
    std::unique_ptr<NdiSender> ndi_sender_;
    std::unique_ptr<FramePump> frame_pump_;
    std::unique_ptr<HttpServer> http_server_;
    std::unique_ptr<Watchdog> watchdog_;
    
    // Actual measured FPS
    std::atomic<float> actual_fps_{0.0f};
    std::atomic<std::string*> current_url_;
};

} // namespace html2ndi

