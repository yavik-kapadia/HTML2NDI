/**
 * CEF helper subprocess.
 * Required for CEF's multi-process architecture on macOS.
 * 
 * This follows the official CEF pattern for helper apps:
 * - Uses CefScopedLibraryLoader to load the CEF framework at runtime
 * - Must be built as separate .app bundles inside Contents/Frameworks
 */

#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"

// Entry point function for sub-processes.
int main(int argc, char* argv[]) {
    // Load the CEF framework library at runtime instead of linking directly
    // as required by the macOS sandbox implementation.
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInHelper()) {
        return 1;
    }

    // Provide CEF with command-line arguments.
    CefMainArgs main_args(argc, argv);

    // Execute the sub-process.
    return CefExecuteProcess(main_args, nullptr, nullptr);
}

