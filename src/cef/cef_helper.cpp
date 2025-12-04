/**
 * CEF helper subprocess.
 * Required for CEF's multi-process architecture.
 */

#include "include/cef_app.h"
#include "include/cef_version.h"

#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Minimal CEF app for subprocess.
 */
class HelperApp : public CefApp {
public:
    HelperApp() = default;
    
private:
    IMPLEMENT_REFCOUNTING(HelperApp);
    DISALLOW_COPY_AND_ASSIGN(HelperApp);
};

int main(int argc, char* argv[]) {
    // Get the path to the helper executable
    char helper_path[PATH_MAX];
    uint32_t path_size = sizeof(helper_path);
    if (_NSGetExecutablePath(helper_path, &path_size) != 0) {
        fprintf(stderr, "Failed to get helper executable path\n");
        return 1;
    }
    
    // Resolve to absolute path
    char* resolved_path = realpath(helper_path, NULL);
    if (!resolved_path) {
        fprintf(stderr, "Failed to resolve helper path\n");
        return 1;
    }
    
    // Get the directory containing the helper
    char* dir = dirname(resolved_path);
    
    // Construct framework path: ../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework
    char framework_path[PATH_MAX];
    snprintf(framework_path, sizeof(framework_path), 
             "%s/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework", 
             dir);
    
    // Try to load the framework
    void* library = dlopen(framework_path, RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "Failed to load CEF framework from: %s\n", framework_path);
        fprintf(stderr, "Error: %s\n", dlerror());
        free(resolved_path);
        return 1;
    }
    
    free(resolved_path);
    
    // Provide CEF with command-line arguments
    CefMainArgs main_args(argc, argv);
    
    // Create the helper application
    CefRefPtr<HelperApp> app = new HelperApp();
    
    // Execute the sub-process logic
    int result = CefExecuteProcess(main_args, app, nullptr);
    
    // Cleanup
    dlclose(library);
    
    return result;
}

