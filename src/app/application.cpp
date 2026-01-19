/**
 * Main application implementation.
 * Coordinates all subsystems.
 */

#include "html2ndi/application.h"
#include "html2ndi/cef/offscreen_renderer.h"
#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/ndi/frame_pump.h"
#include "html2ndi/ndi/genlock.h"
#include "html2ndi/http/http_server.h"
#include "html2ndi/utils/logger.h"
#include "html2ndi/utils/image_encode.h"

#include <chrono>
#include <thread>

namespace html2ndi {

Application::Application(Config config)
    : config_(std::move(config))
    , current_url_(new std::string(config_.url))
    , start_time_(std::chrono::steady_clock::now())
    , last_reload_time_(std::chrono::steady_clock::now())
    , last_gc_time_(std::chrono::steady_clock::now()) {
}

Application::~Application() {
    shutdown();
    
    // Cleanup URL storage
    delete current_url_.load();
}

bool Application::initialize() {
    LOG_DEBUG("Initializing application...");
    
    // Initialize genlock if enabled
    if (config_.genlock_mode != "disabled") {
        GenlockMode mode = GenlockMode::Disabled;
        if (config_.genlock_mode == "master") {
            mode = GenlockMode::Master;
        } else if (config_.genlock_mode == "slave") {
            mode = GenlockMode::Slave;
        }
        
        LOG_INFO("Initializing genlock in %s mode", config_.genlock_mode.c_str());
        genlock_clock_ = std::make_shared<GenlockClock>(
            mode, config_.genlock_master_addr, config_.fps);
        
        if (!genlock_clock_->initialize()) {
            LOG_ERROR("Failed to initialize genlock");
            return false;
        }
    }
    
    // Initialize NDI sender
    LOG_DEBUG("Creating NDI sender: %s", config_.ndi_name.c_str());
    ndi_sender_ = std::make_unique<NdiSender>(config_.ndi_name, config_.ndi_groups);
    if (!ndi_sender_->initialize()) {
        LOG_ERROR("Failed to initialize NDI sender");
        return false;
    }
    
    // Create frame pump with genlock
    LOG_DEBUG("Creating frame pump at %d fps (%s)", config_.fps, config_.progressive ? "progressive" : "interlaced");
    frame_pump_ = std::make_unique<FramePump>(ndi_sender_.get(), config_.fps, config_.progressive, genlock_clock_);
    
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
    
    return true;
}

int Application::run() {
    LOG_DEBUG("Entering main loop");
    
    // Run CEF message loop
    // This blocks until shutdown is requested
    while (!shutdown_requested_) {
        renderer_->do_message_loop_work();
        
        // Update actual FPS from frame pump
        if (frame_pump_) {
            actual_fps_ = frame_pump_->actual_fps();
        }
        
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        
        // Performance monitoring and recovery (after initial startup period)
        if (uptime > kWatchdogStartupDelay) {
            // Check for frame rate degradation
            float target_fps = static_cast<float>(config_.fps);
            float fps_threshold = target_fps * kFpsThresholdRatio;
            
            if (actual_fps_ < fps_threshold && actual_fps_ > 0.1f) {
                degradation_count_++;
                
                if (degradation_count_ >= kDegradationCheckCount) {
                    LOG_WARNING("Frame rate degradation detected: %.1f fps (target: %.0f fps). Triggering recovery...", 
                               actual_fps_.load(), target_fps);
                    
                    // Try JavaScript GC first
                    renderer_->execute_javascript("if (window.gc) window.gc();");
                    renderer_->notify_memory_pressure();
                    
                    // If still degraded after minimum interval, reload page
                    auto since_reload = std::chrono::duration_cast<std::chrono::seconds>(
                        now - last_reload_time_).count();
                    
                    if (since_reload > kMinReloadInterval) {
                        LOG_WARNING("Reloading page to recover from performance degradation");
                        renderer_->reload();
                        last_reload_time_ = now;
                    }
                    
                    degradation_count_ = 0;
                }
            } else {
                degradation_count_ = 0;
            }
            
            // Periodic JavaScript garbage collection
            auto since_gc = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_gc_time_).count();
            
            if (since_gc >= kGarbageCollectionInterval) {
                LOG_DEBUG("Triggering periodic JavaScript garbage collection");
                renderer_->execute_javascript("if (window.gc) window.gc();");
                renderer_->notify_memory_pressure();
                last_gc_time_ = now;
            }
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
    
    // Stop HTTP server first
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

