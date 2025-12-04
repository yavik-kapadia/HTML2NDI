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
    
    // Disable GPU when running headless
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
    
    // Off-screen rendering optimizations
    command_line->AppendSwitch("enable-begin-frame-scheduling");
    command_line->AppendSwitch("disable-surfaces");
    
    // Disable features we don't need
    command_line->AppendSwitch("disable-extensions");
    command_line->AppendSwitch("disable-plugins");
    command_line->AppendSwitch("disable-spell-checking");
    command_line->AppendSwitch("disable-popup-blocking");
    
    // Use mock keychain to avoid macOS Keychain permission prompts
    command_line->AppendSwitch("use-mock-keychain");
    
    // Audio capture for potential NDI audio
    command_line->AppendSwitch("autoplay-policy=no-user-gesture-required");
    
    // Reduce logging noise
    command_line->AppendSwitchWithValue("log-severity", "warning");
    
    LOG_DEBUG("CEF command line configured for process: %s", 
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

