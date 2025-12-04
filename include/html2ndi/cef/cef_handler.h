#pragma once

#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_request_handler.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace html2ndi {

/**
 * Frame callback type.
 * Called when a new frame is rendered.
 * @param buffer RGBA pixel data
 * @param width Frame width
 * @param height Frame height
 */
using FrameCallback = std::function<void(const void* buffer, int width, int height)>;

/**
 * CEF client handler.
 * Handles rendering callbacks and browser lifecycle.
 */
class CefHandler : public CefClient,
                   public CefRenderHandler,
                   public CefLifeSpanHandler,
                   public CefLoadHandler,
                   public CefDisplayHandler,
                   public CefRequestHandler {
public:
    /**
     * Create a new CEF handler.
     * @param width Viewport width
     * @param height Viewport height
     * @param callback Frame callback for rendered frames
     * @param target_fps Target frame rate for continuous invalidation
     */
    CefHandler(int width, int height, FrameCallback callback, int target_fps = 60);
    ~CefHandler() override;
    
    // CefClient methods
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    
    // CefRenderHandler methods
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    
    void OnPaint(
        CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int width,
        int height) override;
    
    bool GetScreenInfo(
        CefRefPtr<CefBrowser> browser,
        CefScreenInfo& screen_info) override;
    
    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    
    // CefLoadHandler methods
    void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transition_type) override;
    
    void OnLoadEnd(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int httpStatusCode) override;
    
    void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedUrl) override;
    
    // CefDisplayHandler methods
    void OnTitleChange(
        CefRefPtr<CefBrowser> browser,
        const CefString& title) override;
    
    bool OnConsoleMessage(
        CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString& message,
        const CefString& source,
        int line) override;
    
    // CefRequestHandler methods
    void OnRenderProcessTerminated(
        CefRefPtr<CefBrowser> browser,
        TerminationStatus status) override;
    
    // Browser control
    void SetBrowser(CefRefPtr<CefBrowser> browser);
    CefRefPtr<CefBrowser> GetBrowser() const;
    bool IsBrowserClosed() const { return is_closed_; }
    
    // Get current URL
    std::string GetCurrentUrl() const;
    
    // Resize viewport
    void Resize(int width, int height);
    
    /**
     * Console message structure for retrieval.
     */
    struct ConsoleMessage {
        std::string level;
        std::string message;
        std::string source;
        int line;
        int64_t timestamp;
    };
    
    /**
     * Get captured console messages.
     * @param max_count Maximum number of messages to return (0 = all)
     * @param clear Whether to clear messages after retrieval
     * @return Vector of console messages
     */
    std::vector<ConsoleMessage> GetConsoleMessages(size_t max_count = 0, bool clear = false);
    
    /**
     * Clear all captured console messages.
     */
    void ClearConsoleMessages();
    
    /**
     * Get console message count.
     */
    size_t GetConsoleMessageCount() const;

private:
    // Start the invalidation timer to force continuous rendering
    void StartInvalidationTimer();
    void StopInvalidationTimer();
    
    int width_;
    int height_;
    int target_fps_;
    FrameCallback frame_callback_;
    
    mutable std::mutex browser_mutex_;
    CefRefPtr<CefBrowser> browser_;
    std::atomic<bool> is_closed_{false};
    
    std::string current_url_;
    std::string current_title_;
    
    // Invalidation timer for continuous rendering
    std::thread invalidation_thread_;
    std::atomic<bool> invalidation_running_{false};
    
    // Console message buffer (circular, max 1000 messages)
    static constexpr size_t kMaxConsoleMessages = 1000;
    mutable std::mutex console_mutex_;
    std::deque<ConsoleMessage> console_messages_;
    
    IMPLEMENT_REFCOUNTING(CefHandler);
    DISALLOW_COPY_AND_ASSIGN(CefHandler);
};

} // namespace html2ndi

