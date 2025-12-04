#pragma once

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_render_process_handler.h"

namespace html2ndi {

/**
 * CEF Application class.
 * Implements browser and render process handlers.
 */
class CefAppImpl : public CefApp,
                   public CefBrowserProcessHandler,
                   public CefRenderProcessHandler {
public:
    CefAppImpl() = default;
    
    // CefApp methods
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }
    
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override;
    
    // CefBrowserProcessHandler methods
    void OnContextInitialized() override;
    
    CefRefPtr<CefClient> GetDefaultClient() override;
    
    // CefRenderProcessHandler methods
    void OnContextCreated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override;

private:
    IMPLEMENT_REFCOUNTING(CefAppImpl);
    DISALLOW_COPY_AND_ASSIGN(CefAppImpl);
};

} // namespace html2ndi

