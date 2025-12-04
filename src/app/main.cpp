/**
 * HTML2NDI - HTML to NDI Video Renderer
 * 
 * Main entry point for the application.
 * Parses command line arguments, initializes subsystems, and runs the main loop.
 */

#include "html2ndi/application.h"
#include "html2ndi/config.h"
#include "html2ndi/utils/logger.h"
#include "html2ndi/utils/signal_handler.h"

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace html2ndi;

int main(int argc, char* argv[]) {
    // Parse command line arguments
    auto config = Config::parse(argc, argv);
    if (!config) {
        return EXIT_FAILURE;
    }
    
    // Validate configuration
    if (!config->validate()) {
        std::cerr << "Invalid configuration" << std::endl;
        return EXIT_FAILURE;
    }
    
    // Initialize logger with default log directory if not specified
    LogLevel log_level = static_cast<LogLevel>(config->log_level);
    std::string log_file = config->log_file;
    if (log_file.empty()) {
        // Default to ~/Library/Logs/HTML2NDI/html2ndi.log
        std::string log_dir = get_default_log_directory();
        if (!log_dir.empty()) {
            log_file = log_dir + "/html2ndi.log";
        }
    }
    Logger::instance().initialize(log_level, log_file);
    
    LOG_INFO("HTML2NDI starting...");
    if (!log_file.empty()) {
        LOG_INFO("Log file: %s", log_file.c_str());
    }
    LOG_INFO("URL: %s", config->url.c_str());
    LOG_INFO("Resolution: %dx%d @ %d fps", config->width, config->height, config->fps);
    LOG_INFO("NDI Source: %s", config->ndi_name.c_str());
    
    // Create application
    auto app = std::make_unique<Application>(*config);
    
    // Install signal handlers
    SignalHandler::install([&app]() {
        LOG_INFO("Shutdown signal received");
        app->shutdown();
    });
    
    // Initialize
    if (!app->initialize()) {
        LOG_FATAL("Failed to initialize application");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Application initialized successfully");
    
    if (config->http_enabled) {
        LOG_INFO("HTTP API available at http://%s:%d", 
                 config->http_host.c_str(), config->http_port);
    }
    
    // Run main loop
    int exit_code = app->run();
    
    // Cleanup
    SignalHandler::remove();
    
    LOG_INFO("HTML2NDI shutting down with exit code %d", exit_code);
    Logger::instance().flush();
    
    return exit_code;
}

