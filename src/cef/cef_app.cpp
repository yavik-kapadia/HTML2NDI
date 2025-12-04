/**
 * CEF Application implementation.
 */

#include "html2ndi/cef/cef_app.h"
#include "html2ndi/cef/cef_handler.h"
#include "html2ndi/utils/logger.h"

namespace html2ndi {

void CefAppImpl::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    
    // For off-screen rendering, software compositing is faster because it
    // avoids GPU→CPU readback latency. However, we can still use GPU for
    // certain operations like WebGL and video decoding.
    command_line->AppendSwitch("disable-gpu-compositing");
    
    // Enable GPU rasterization for complex CSS/SVG (runs on GPU, composites on CPU)
    command_line->AppendSwitch("enable-gpu-rasterization");
    
    // Use Metal for WebGL content
    command_line->AppendSwitchWithValue("use-angle", "metal");
    
    // Enable hardware video decoding for video elements
    command_line->AppendSwitch("enable-accelerated-video-decode");
    
    // Disable features we don't need
    command_line->AppendSwitch("disable-extensions");
    command_line->AppendSwitch("disable-plugins");
    command_line->AppendSwitch("disable-spell-checking");
    command_line->AppendSwitch("disable-popup-blocking");
    
    // Use mock keychain to avoid macOS Keychain permission prompts
    command_line->AppendSwitch("use-mock-keychain");
    
    // Video/Audio playback settings - aggressive autoplay
    command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
    command_line->AppendSwitch("enable-media-stream");
    command_line->AppendSwitch("allow-running-insecure-content");
    
    // Force enable all media features
    command_line->AppendSwitch("disable-gesture-requirement-for-media-playback");
    command_line->AppendSwitch("enable-features=PlatformHEVCDecoderSupport");
    command_line->AppendSwitchWithValue("disable-features", "AudioServiceOutOfProcess,MediaEngagementBypassAutoplayPolicies");
    
    // Disable CORS restrictions (useful for local development)
    command_line->AppendSwitch("disable-web-security");
    command_line->AppendSwitch("disable-site-isolation-trials");
    
    // Reduce logging noise
    command_line->AppendSwitchWithValue("log-severity", "warning");
    
    LOG_DEBUG("CEF command line configured for process: %s (hybrid GPU/CPU)", 
              process_type.ToString().c_str());
}

void CefAppImpl::OnContextInitialized() {
    LOG_DEBUG("CEF context initialized");
}

CefRefPtr<CefClient> CefAppImpl::GetDefaultClient() {
    return nullptr;
}

void CefAppImpl::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
    
    // Can inject JavaScript bindings here if needed
    LOG_DEBUG("V8 context created for frame: %s", 
              frame->GetURL().ToString().c_str());
}

} // namespace html2ndi

