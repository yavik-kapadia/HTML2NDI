/**
 * CEF client handler implementation.
 */

#include "html2ndi/cef/cef_handler.h"
#include "html2ndi/utils/logger.h"

#include "include/wrapper/cef_helpers.h"

#include <chrono>
#include <thread>

namespace html2ndi {

CefHandler::CefHandler(int width, int height, FrameCallback callback, int target_fps)
    : width_(width)
    , height_(height)
    , frame_callback_(std::move(callback))
    , target_fps_(target_fps) {
    LOG_DEBUG("CefHandler created: %dx%d @ %dfps", width_, height_, target_fps_);
}

CefHandler::~CefHandler() {
    StopInvalidationTimer();
    LOG_DEBUG("CefHandler destroyed");
}

void CefHandler::StartInvalidationTimer() {
    if (invalidation_running_) {
        return;
    }
    
    invalidation_running_ = true;
    
    // Calculate frame duration in microseconds for more precise timing
    // Run invalidation at 2x the target rate to ensure frames are ready
    // when the frame pump requests them (optimal balance of CPU vs drops)
    int frame_duration_us = 1'000'000 / (target_fps_ * 2);
    
    LOG_DEBUG("Starting invalidation timer: %dus interval (~%d fps) for %d fps target", 
              frame_duration_us, 1'000'000 / frame_duration_us, target_fps_);
    
    invalidation_thread_ = std::thread([this, frame_duration_us]() {
        auto next_invalidate = std::chrono::steady_clock::now();
        
        while (invalidation_running_) {
            // Invalidate the browser view to force a repaint
            {
                std::lock_guard<std::mutex> lock(browser_mutex_);
                if (browser_) {
                    browser_->GetHost()->Invalidate(PET_VIEW);
                }
            }
            
            // Use precise timing with high-resolution clock
            next_invalidate += std::chrono::microseconds(frame_duration_us);
            
            auto now = std::chrono::steady_clock::now();
            if (next_invalidate > now) {
                std::this_thread::sleep_until(next_invalidate);
            } else {
                // If we're behind, reset timing to avoid spiral
                next_invalidate = now;
            }
        }
    });
}

void CefHandler::StopInvalidationTimer() {
    if (!invalidation_running_) {
        return;
    }
    
    invalidation_running_ = false;
    
    if (invalidation_thread_.joinable()) {
        invalidation_thread_.join();
    }
    
    LOG_DEBUG("Invalidation timer stopped");
}

// =============================================================================
// CefRenderHandler
// =============================================================================

void CefHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, width_, height_);
}

void CefHandler::OnPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
    
    // Only handle main view, not popups
    if (type != PET_VIEW) {
        return;
    }
    
    // Forward frame to callback
    if (frame_callback_) {
        frame_callback_(buffer, width, height);
    }
}

bool CefHandler::GetScreenInfo(
    CefRefPtr<CefBrowser> browser,
    CefScreenInfo& screen_info) {
    
    screen_info.device_scale_factor = 1.0f;
    screen_info.rect = CefRect(0, 0, width_, height_);
    screen_info.available_rect = screen_info.rect;
    screen_info.depth = 32;
    screen_info.depth_per_component = 8;
    screen_info.is_monochrome = false;
    
    return true;
}

// =============================================================================
// CefLifeSpanHandler
// =============================================================================

void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    
    {
        std::lock_guard<std::mutex> lock(browser_mutex_);
        browser_ = browser;
    }
    
    LOG_INFO("Browser created");
    
    // Start the invalidation timer to force continuous rendering
    StartInvalidationTimer();
}

bool CefHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    
    // Allow the close. The OnBeforeClose method will be called.
    return false;
}

void CefHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    
    std::lock_guard<std::mutex> lock(browser_mutex_);
    browser_ = nullptr;
    is_closed_ = true;
    
    LOG_INFO("Browser closed");
}

// =============================================================================
// CefLoadHandler
// =============================================================================

void CefHandler::OnLoadStart(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    TransitionType transition_type) {
    
    if (frame->IsMain()) {
        current_url_ = frame->GetURL().ToString();
        LOG_DEBUG("Load started: %s", current_url_.c_str());
    }
}

void CefHandler::OnLoadEnd(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode) {
    
    if (frame->IsMain()) {
        LOG_INFO("Page loaded: %s (status: %d)", 
                 current_url_.c_str(), httpStatusCode);
        
        // Inject helper to ensure video.play() works when called by external apps (NodeCG, etc.)
        // This doesn't auto-play videos, just ensures play() calls succeed
        const char* video_helper_script = R"(
            (function() {
                // Override video.play() to ensure it works in CEF
                const originalPlay = HTMLVideoElement.prototype.play;
                HTMLVideoElement.prototype.play = function() {
                    // Ensure video is muted for autoplay policy compliance
                    // (external apps can unmute after play starts)
                    const wasMuted = this.muted;
                    this.muted = true;
                    
                    return originalPlay.call(this).then(() => {
                        // Restore muted state after play starts if originally unmuted
                        // (allows external app to control audio)
                        if (!wasMuted) {
                            setTimeout(() => { this.muted = false; }, 100);
                        }
                    }).catch(e => {
                        console.warn('Video play() blocked:', e.message);
                        throw e;
                    });
                };
                console.log('HTML2NDI: Video playback helper installed');
            })();
        )";
        
        frame->ExecuteJavaScript(video_helper_script, frame->GetURL(), 0);
        LOG_DEBUG("Injected video playback helper for external control");
    }
}

void CefHandler::OnLoadError(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    ErrorCode errorCode,
    const CefString& errorText,
    const CefString& failedUrl) {
    
    // Ignore aborted loads (e.g., navigating away before load completes)
    if (errorCode == ERR_ABORTED) {
        return;
    }
    
    LOG_ERROR("Load error for %s: %s (code: %d)",
              failedUrl.ToString().c_str(),
              errorText.ToString().c_str(),
              errorCode);
    
    // Optionally display an error page
    if (frame->IsMain()) {
        // Display simple error page
        std::string error_url = 
            "data:text/html,"
            "<html><body style='background:#111;color:#fff;font-family:sans-serif;"
            "display:flex;justify-content:center;align-items:center;height:100vh;'>"
            "<div style='text-align:center;'>"
            "<h1>Load Error</h1>"
            "<p>" + errorText.ToString() + "</p>"
            "<p style='color:#888;'>" + failedUrl.ToString() + "</p>"
            "</div></body></html>";
        
        frame->LoadURL(error_url);
    }
}

// =============================================================================
// CefDisplayHandler
// =============================================================================

void CefHandler::OnTitleChange(
    CefRefPtr<CefBrowser> browser,
    const CefString& title) {
    
    current_title_ = title.ToString();
    LOG_DEBUG("Title changed: %s", current_title_.c_str());
}

bool CefHandler::OnConsoleMessage(
    CefRefPtr<CefBrowser> browser,
    cef_log_severity_t level,
    const CefString& message,
    const CefString& source,
    int line) {
    
    const char* level_str = "INFO";
    switch (level) {
        case LOGSEVERITY_DEBUG: level_str = "DEBUG"; break;
        case LOGSEVERITY_INFO: level_str = "INFO"; break;
        case LOGSEVERITY_WARNING: level_str = "WARN"; break;
        case LOGSEVERITY_ERROR: level_str = "ERROR"; break;
        case LOGSEVERITY_FATAL: level_str = "FATAL"; break;
        default: break;
    }
    
    LOG_DEBUG("[JS:%s] %s (%s:%d)", 
              level_str,
              message.ToString().c_str(),
              source.ToString().c_str(),
              line);
    
    // Store in console buffer
    {
        std::lock_guard<std::mutex> lock(console_mutex_);
        
        ConsoleMessage msg;
        msg.level = level_str;
        msg.message = message.ToString();
        msg.source = source.ToString();
        msg.line = line;
        msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        console_messages_.push_back(std::move(msg));
        
        // Keep buffer bounded
        while (console_messages_.size() > kMaxConsoleMessages) {
            console_messages_.pop_front();
        }
    }
    
    return false; // Don't suppress
}

// =============================================================================
// CefRequestHandler
// =============================================================================

void CefHandler::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser> browser,
    TerminationStatus status) {
    
    const char* status_str = "unknown";
    switch (status) {
        case TS_ABNORMAL_TERMINATION:
            status_str = "abnormal termination";
            break;
        case TS_PROCESS_WAS_KILLED:
            status_str = "process killed";
            break;
        case TS_PROCESS_CRASHED:
            status_str = "process crashed";
            break;
        case TS_PROCESS_OOM:
            status_str = "out of memory";
            break;
    }
    
    LOG_ERROR("Render process terminated: %s", status_str);
    
    // Attempt recovery by reloading the page
    // Don't reload on abnormal termination as it might cause a loop
    if (status != TS_ABNORMAL_TERMINATION) {
        LOG_INFO("Attempting to recover by reloading page...");
        
        // Small delay before reload to avoid rapid crash loops
        std::thread([browser]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (browser) {
                browser->Reload();
            }
        }).detach();
    } else {
        LOG_ERROR("Cannot recover from render process failure, manual restart required");
    }
}

// =============================================================================
// Public methods
// =============================================================================

void CefHandler::SetBrowser(CefRefPtr<CefBrowser> browser) {
    std::lock_guard<std::mutex> lock(browser_mutex_);
    browser_ = browser;
}

CefRefPtr<CefBrowser> CefHandler::GetBrowser() const {
    std::lock_guard<std::mutex> lock(browser_mutex_);
    return browser_;
}

std::string CefHandler::GetCurrentUrl() const {
    std::lock_guard<std::mutex> lock(browser_mutex_);
    if (browser_) {
        return browser_->GetMainFrame()->GetURL().ToString();
    }
    return current_url_;
}

void CefHandler::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    
    std::lock_guard<std::mutex> lock(browser_mutex_);
    if (browser_) {
        browser_->GetHost()->WasResized();
    }
}

std::vector<CefHandler::ConsoleMessage> CefHandler::GetConsoleMessages(size_t max_count, bool clear) {
    std::lock_guard<std::mutex> lock(console_mutex_);
    
    std::vector<ConsoleMessage> result;
    
    if (max_count == 0 || max_count >= console_messages_.size()) {
        result.assign(console_messages_.begin(), console_messages_.end());
        if (clear) {
            console_messages_.clear();
        }
    } else {
        // Return last N messages
        auto start = console_messages_.end() - static_cast<ptrdiff_t>(max_count);
        result.assign(start, console_messages_.end());
        if (clear) {
            console_messages_.erase(start, console_messages_.end());
        }
    }
    
    return result;
}

void CefHandler::ClearConsoleMessages() {
    std::lock_guard<std::mutex> lock(console_mutex_);
    console_messages_.clear();
}

size_t CefHandler::GetConsoleMessageCount() const {
    std::lock_guard<std::mutex> lock(console_mutex_);
    return console_messages_.size();
}

} // namespace html2ndi

