#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace html2ndi {

/**
 * Application configuration structure.
 * Populated from CLI arguments and/or config file.
 */
struct Config {
    // HTML rendering settings
    std::string url = "about:blank";
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    
    // NDI settings
    std::string ndi_name = "HTML2NDI";
    std::string ndi_groups = "";
    bool ndi_clock_video = true;
    bool ndi_clock_audio = true;
    
    // HTTP server settings
    bool http_enabled = true;
    std::string http_host = "127.0.0.1";
    uint16_t http_port = 8080;
    
    // CEF settings
    std::string cef_cache_path = "";
    bool cef_disable_gpu = false;
    std::string cef_user_agent = "";
    int cef_log_severity = 2; // WARNING
    
    // Application settings
    std::string log_file = "";
    int log_level = 1; // INFO
    bool daemon_mode = false;
    
    // Parse command line arguments
    static std::optional<Config> parse(int argc, char* argv[]);
    
    // Print help message
    static void print_help(const char* program_name);
    
    // Print version
    static void print_version();
    
    // Validate configuration
    bool validate() const;
};

} // namespace html2ndi

