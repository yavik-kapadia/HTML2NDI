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
    
    /**
     * Frame statistics structure.
     */
    struct FrameStats {
        uint64_t frames_sent{0};
        uint64_t frames_dropped{0};
        double drop_rate{0.0};
        double uptime_seconds{0.0};
        uint64_t bandwidth_bytes_per_sec{0};
    };
    
    /**
     * Get frame statistics.
     */
    FrameStats get_frame_stats() const;
    
    /**
     * Execute JavaScript in the browser.
     * @param code JavaScript code to execute
     */
    void execute_javascript(const std::string& code);
    
    /**
     * Console message structure.
     */
    struct ConsoleMessage {
        std::string level;
        std::string message;
        std::string source;
        int line;
        int64_t timestamp;
    };
    
    /**
     * Get console messages from the browser.
     * @param max_count Maximum number of messages (0 = all)
     * @param clear Whether to clear messages after retrieval
     * @return Vector of console messages
     */
    std::vector<ConsoleMessage> get_console_messages(size_t max_count = 0, bool clear = false);
    
    /**
     * Clear console messages.
     */
    void clear_console_messages();
    
    /**
     * Get console message count.
     */
    size_t get_console_message_count() const;

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

