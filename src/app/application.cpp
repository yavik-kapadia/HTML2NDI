/**
 * Main application implementation.
 * Coordinates all subsystems.
 */

#include "html2ndi/application.h"
#include "html2ndi/cef/offscreen_renderer.h"
#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/ndi/frame_pump.h"
#include "html2ndi/http/http_server.h"
#include "html2ndi/utils/logger.h"
#include "html2ndi/utils/image_encode.h"
#include "html2ndi/utils/watchdog.h"

#include <chrono>
#include <thread>

namespace html2ndi {

Application::Application(Config config)
    : config_(std::move(config))
    , current_url_(new std::string(config_.url)) {
}

Application::~Application() {
    shutdown();
    
    // Cleanup URL storage
    delete current_url_.load();
}

bool Application::initialize() {
    LOG_DEBUG("Initializing application...");
    
    // Initialize NDI sender
    LOG_DEBUG("Creating NDI sender: %s", config_.ndi_name.c_str());
    ndi_sender_ = std::make_unique<NdiSender>(config_.ndi_name, config_.ndi_groups);
    if (!ndi_sender_->initialize()) {
        LOG_ERROR("Failed to initialize NDI sender");
        return false;
    }
    
    // Create frame pump
    LOG_DEBUG("Creating frame pump at %d fps", config_.fps);
    frame_pump_ = std::make_unique<FramePump>(ndi_sender_.get(), config_.fps);
    
    // Create CEF renderer with frame callback
    LOG_DEBUG("Creating CEF renderer %dx%d", config_.width, config_.height);
    renderer_ = std::make_unique<OffscreenRenderer>(
        config_,
        [this](const void* buffer, int width, int height) {
            // Forward frame to pump
            if (frame_pump_) {
                frame_pump_->submit_frame(buffer, width, height);
            }
        }
    );
    
    if (!renderer_->initialize()) {
        LOG_ERROR("Failed to initialize CEF renderer");
        return false;
    }
    
    // Start frame pump
    frame_pump_->start();
    
    // Load initial URL
    renderer_->load_url(config_.url);
    
    // Start HTTP server if enabled
    if (config_.http_enabled) {
        LOG_DEBUG("Starting HTTP server on %s:%d", 
                  config_.http_host.c_str(), config_.http_port);
        http_server_ = std::make_unique<HttpServer>(
            this, config_.http_host, config_.http_port);
        
        if (!http_server_->start()) {
            LOG_WARNING("Failed to start HTTP server (continuing without it)");
            http_server_.reset();
        }
    }
    
    // Start watchdog timer (30 second timeout for main loop hang detection)
    watchdog_ = std::make_unique<Watchdog>(
        std::chrono::seconds(30),
        [this]() {
            LOG_FATAL("Watchdog detected main loop hang - forcing shutdown");
            // Force shutdown - will cause process to exit, triggering auto-restart
            // if managed by the StreamManager
            std::abort();
        }
    );
    watchdog_->start();
    
    return true;
}

int Application::run() {
    LOG_DEBUG("Entering main loop");
    
    // Run CEF message loop
    // This blocks until shutdown is requested
    while (!shutdown_requested_) {
        // Signal watchdog that main loop is alive
        if (watchdog_) {
            watchdog_->heartbeat();
        }
        
        renderer_->do_message_loop_work();
        
        // Update actual FPS from frame pump
        if (frame_pump_) {
            actual_fps_ = frame_pump_->actual_fps();
        }
        
        // Small sleep to prevent spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    LOG_DEBUG("Exiting main loop");
    return 0;
}

void Application::shutdown() {
    if (shutdown_requested_.exchange(true)) {
        return; // Already shutting down
    }
    
    LOG_INFO("Shutting down application...");
    
    // Stop watchdog first to prevent false alarms during shutdown
    if (watchdog_) {
        LOG_DEBUG("Stopping watchdog");
        watchdog_->stop();
        watchdog_.reset();
    }
    
    // Stop HTTP server
    if (http_server_) {
        LOG_DEBUG("Stopping HTTP server");
        http_server_->stop();
        http_server_.reset();
    }
    
    // Stop frame pump
    if (frame_pump_) {
        LOG_DEBUG("Stopping frame pump");
        frame_pump_->stop();
    }
    
    // Shutdown CEF
    if (renderer_) {
        LOG_DEBUG("Shutting down CEF");
        renderer_->shutdown();
    }
    
    // Shutdown NDI
    if (ndi_sender_) {
        LOG_DEBUG("Shutting down NDI sender");
        ndi_sender_->shutdown();
    }
    
    LOG_INFO("Shutdown complete");
}

bool Application::is_shutting_down() const {
    return shutdown_requested_;
}

void Application::set_url(const std::string& url) {
    if (renderer_) {
        LOG_INFO("Loading URL: %s", url.c_str());
        renderer_->load_url(url);
        
        // Update stored URL
        auto* new_url = new std::string(url);
        auto* old_url = current_url_.exchange(new_url);
        delete old_url;
    }
}

void Application::reload() {
    if (renderer_) {
        LOG_INFO("Reloading page");
        renderer_->reload();
    }
}

std::string Application::current_url() const {
    if (renderer_) {
        return renderer_->current_url();
    }
    return config_.url;
}

int Application::ndi_connection_count() const {
    if (ndi_sender_) {
        return ndi_sender_->get_connection_count();
    }
    return 0;
}

float Application::current_fps() const {
    return actual_fps_;
}

bool Application::get_thumbnail(std::vector<uint8_t>& out_jpeg, int width, int quality) {
    if (!frame_pump_) {
        return false;
    }
    
    std::vector<uint8_t> frame_data;
    int frame_width, frame_height;
    
    if (!frame_pump_->get_current_frame(frame_data, frame_width, frame_height)) {
        return false;
    }
    
    if (width > 0 && width < frame_width) {
        return encode_jpeg_scaled(frame_data.data(), frame_width, frame_height, 
                                  width, quality, out_jpeg);
    } else {
        return encode_jpeg(frame_data.data(), frame_width, frame_height, 
                          quality, out_jpeg);
    }
}

} // namespace html2ndi

