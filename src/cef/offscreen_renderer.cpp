/**
 * CEF off-screen renderer implementation.
 */

#include "html2ndi/cef/offscreen_renderer.h"
#include "html2ndi/cef/cef_app.h"
#include "html2ndi/utils/logger.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_library_loader.h"

#include <filesystem>
#include <mach-o/dyld.h>

namespace html2ndi {

OffscreenRenderer::OffscreenRenderer(const Config& config, FrameCallback frame_callback)
    : config_(config)
    , frame_callback_(std::move(frame_callback)) {
}

OffscreenRenderer::~OffscreenRenderer() {
    shutdown();
}

bool OffscreenRenderer::initialize() {
    LOG_DEBUG("Initializing CEF...");
    
    // Load the CEF framework library at runtime (required on macOS)
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInMain()) {
        LOG_ERROR("Failed to load CEF framework library");
        return false;
    }
    LOG_DEBUG("CEF framework loaded");
    
    // Get executable path
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        LOG_ERROR("Failed to get executable path");
        return false;
    }
    
    std::filesystem::path exe_path(path);
    std::filesystem::path exe_dir = exe_path.parent_path(); // Contents/MacOS
    std::filesystem::path contents_dir = exe_dir.parent_path(); // Contents
    std::filesystem::path bundle_dir = contents_dir.parent_path(); // HTML2NDI.app
    std::filesystem::path frameworks_dir = contents_dir / "Frameworks";
    
    // Helper app bundle is in Contents/Frameworks/html2ndi Helper.app
    std::filesystem::path helper_path = frameworks_dir / "html2ndi Helper.app" / "Contents" / "MacOS" / "html2ndi Helper";
    
    // CEF settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = true;
    settings.command_line_args_disabled = false;
    settings.remote_debugging_port = config_.devtools_port;
    settings.log_severity = static_cast<cef_log_severity_t>(config_.cef_log_severity);
    
    if (config_.devtools_port > 0) {
        LOG_INFO("CEF DevTools enabled on port %d", config_.devtools_port);
    }
    
    // Only set main_bundle_path - let CefScopedLibraryLoader handle the rest
    CefString(&settings.main_bundle_path).FromString(bundle_dir.string());
    
    LOG_DEBUG("Bundle path: %s", bundle_dir.string().c_str());
    LOG_DEBUG("Helper path: %s", helper_path.string().c_str());
    
    // Set subprocess path
    CefString(&settings.browser_subprocess_path).FromString(helper_path.string());
    
    // Set cache path if specified (both cache_path and root_cache_path for multi-instance support)
    if (!config_.cef_cache_path.empty()) {
        CefString(&settings.cache_path).FromString(config_.cef_cache_path);
        CefString(&settings.root_cache_path).FromString(config_.cef_cache_path);
    }
    
    // Set user agent if specified
    if (!config_.cef_user_agent.empty()) {
        CefString(&settings.user_agent).FromString(config_.cef_user_agent);
    }
    
    // Initialize CEF
    CefRefPtr<CefAppImpl> app = new CefAppImpl();
    
    CefMainArgs main_args;
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        LOG_ERROR("CefInitialize failed");
        return false;
    }
    
    LOG_DEBUG("CEF initialized");
    
    // Create browser handler with target FPS for continuous invalidation
    handler_ = new CefHandler(
        config_.width, 
        config_.height, 
        frame_callback_,
        config_.fps
    );
    
    // Browser window info (off-screen)
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);
    
    // Browser settings
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = config_.fps;
    browser_settings.background_color = CefColorSetARGB(255, 0, 0, 0);
    
    // JavaScript settings
    browser_settings.javascript = STATE_ENABLED;
    browser_settings.javascript_access_clipboard = STATE_DISABLED;
    browser_settings.javascript_dom_paste = STATE_DISABLED;
    
    // Media settings
    browser_settings.webgl = STATE_ENABLED;
    browser_settings.remote_fonts = STATE_ENABLED;
    
    // Create browser
    LOG_DEBUG("Creating browser window...");
    if (!CefBrowserHost::CreateBrowser(
            window_info,
            handler_,
            config_.url,
            browser_settings,
            nullptr,
            nullptr)) {
        LOG_ERROR("Failed to create browser");
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("CEF renderer initialized");
    
    return true;
}

void OffscreenRenderer::run_message_loop() {
    CefRunMessageLoop();
}

void OffscreenRenderer::do_message_loop_work() {
    CefDoMessageLoopWork();
}

void OffscreenRenderer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    shutdown_requested_ = true;
    
    LOG_DEBUG("Shutting down CEF...");
    
    // Close browser
    if (handler_) {
        auto browser = handler_->GetBrowser();
        if (browser) {
            browser->GetHost()->CloseBrowser(true);
            
            // Wait for browser to close
            while (!handler_->IsBrowserClosed()) {
                CefDoMessageLoopWork();
            }
        }
        handler_ = nullptr;
    }
    
    // Shutdown CEF
    CefShutdown();
    
    initialized_ = false;
    LOG_DEBUG("CEF shutdown complete");
}

void OffscreenRenderer::load_url(const std::string& url) {
    if (!initialized_ || !handler_) {
        return;
    }
    
    auto browser = handler_->GetBrowser();
    if (browser) {
        browser->GetMainFrame()->LoadURL(url);
    }
}

void OffscreenRenderer::reload() {
    if (!initialized_ || !handler_) {
        return;
    }
    
    auto browser = handler_->GetBrowser();
    if (browser) {
        browser->Reload();
    }
}

void OffscreenRenderer::execute_javascript(const std::string& code) {
    if (!initialized_ || !handler_) {
        return;
    }
    
    auto browser = handler_->GetBrowser();
    if (browser) {
        browser->GetMainFrame()->ExecuteJavaScript(code, "", 0);
    }
}

void OffscreenRenderer::resize(int width, int height) {
    if (handler_) {
        handler_->Resize(width, height);
    }
}

std::string OffscreenRenderer::current_url() const {
    if (handler_) {
        return handler_->GetCurrentUrl();
    }
    return config_.url;
}

} // namespace html2ndi

