/**
 * Configuration parsing and validation.
 */

#include "html2ndi/config.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <regex>

namespace html2ndi {

namespace {

const char* VERSION = "1.5.2";

void print_arg(const char* short_opt, const char* long_opt, 
               const char* arg, const char* desc) {
    std::cout << "  ";
    if (short_opt) {
        std::cout << short_opt;
        if (long_opt) std::cout << ", ";
    }
    if (long_opt) {
        std::cout << long_opt;
    }
    if (arg) {
        std::cout << " " << arg;
    }
    std::cout << std::endl;
    std::cout << "        " << desc << std::endl;
}

} // anonymous namespace

void Config::print_help(const char* program_name) {
    std::cout << "HTML2NDI - Render HTML pages as NDI video output" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "HTML Rendering Options:" << std::endl;
    print_arg("-u", "--url", "<url>", "URL to load (default: about:blank)");
    print_arg("-w", "--width", "<pixels>", "Frame width (default: 1920)");
    print_arg("-h", "--height", "<pixels>", "Frame height (default: 1080)");
    print_arg("-f", "--fps", "<rate>", "Target framerate (default: 60)");
    print_arg("-i", "--interlaced", nullptr, "Use interlaced mode (default: progressive)");
    
    std::cout << std::endl;
    std::cout << "NDI Options:" << std::endl;
    print_arg("-n", "--ndi-name", "<name>", "NDI source name (default: HTML2NDI)");
    print_arg("-g", "--ndi-groups", "<groups>", "NDI groups, comma-separated (default: all)");
    print_arg(nullptr, "--no-clock-video", nullptr, "Disable video clock timing");
    print_arg(nullptr, "--no-clock-audio", nullptr, "Disable audio clock timing");
    
    std::cout << std::endl;
    std::cout << "Genlock Options:" << std::endl;
    print_arg(nullptr, "--genlock", "<mode>", "Genlock mode: disabled, master, slave (default: disabled)");
    print_arg(nullptr, "--genlock-master", "<addr>", "Master address for slave mode (default: 127.0.0.1:5960)");
    
    std::cout << std::endl;
    std::cout << "HTTP API Options:" << std::endl;
    print_arg(nullptr, "--http-host", "<host>", "HTTP server bind address (default: 127.0.0.1)");
    print_arg("-p", "--http-port", "<port>", "HTTP server port (default: 8080)");
    print_arg(nullptr, "--no-http", nullptr, "Disable HTTP server");
    
    std::cout << std::endl;
    std::cout << "CEF Options:" << std::endl;
    print_arg(nullptr, "--cache-path", "<path>", "Browser cache directory");
    print_arg(nullptr, "--disable-gpu", nullptr, "Disable GPU acceleration");
    print_arg(nullptr, "--user-agent", "<ua>", "Custom user agent string");
    
    std::cout << std::endl;
    std::cout << "Application Options:" << std::endl;
    print_arg("-l", "--log-file", "<path>", "Log file path");
    print_arg("-v", "--verbose", nullptr, "Enable verbose logging (DEBUG level)");
    print_arg("-q", "--quiet", nullptr, "Quiet mode (ERROR level only)");
    print_arg("-d", "--daemon", nullptr, "Run as daemon (detach from terminal)");
    print_arg(nullptr, "--version", nullptr, "Print version and exit");
    print_arg(nullptr, "--help", nullptr, "Show this help message");
    
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --url https://example.com" << std::endl;
    std::cout << "  " << program_name << " -u file:///path/to/page.html -w 1280 -h 720 -f 30" << std::endl;
    std::cout << "  " << program_name << " --ndi-name \"My Source\" --http-port 9000" << std::endl;
}

void Config::print_version() {
    std::cout << "HTML2NDI version " << VERSION << std::endl;
    std::cout << "Copyright (c) 2024" << std::endl;
}

std::optional<Config> Config::parse(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        auto get_value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a value" << std::endl;
                return "";
            }
            return argv[++i];
        };
        
        auto get_int = [&]() -> int {
            std::string val = get_value();
            if (val.empty()) return -1;
            try {
                return std::stoi(val);
            } catch (...) {
                std::cerr << "Error: Invalid integer for " << arg << ": " << val << std::endl;
                return -1;
            }
        };
        
        // Help and version
        if (arg == "--help") {
            Config::print_help(argv[0]);
            return std::nullopt;
        }
        if (arg == "--version") {
            Config::print_version();
            return std::nullopt;
        }
        
        // HTML options
        if (arg == "-u" || arg == "--url") {
            config.url = get_value();
            if (config.url.empty()) return std::nullopt;
        }
        else if (arg == "-w" || arg == "--width") {
            int val = get_int();
            if (val <= 0) return std::nullopt;
            config.width = static_cast<uint32_t>(val);
        }
        else if (arg == "-h" || arg == "--height") {
            int val = get_int();
            if (val <= 0) return std::nullopt;
            config.height = static_cast<uint32_t>(val);
        }
        else if (arg == "-f" || arg == "--fps") {
            int val = get_int();
            if (val <= 0) return std::nullopt;
            config.fps = static_cast<uint32_t>(val);
        }
        else if (arg == "-i" || arg == "--interlaced") {
            config.progressive = false;
        }
        
        // NDI options
        else if (arg == "-n" || arg == "--ndi-name") {
            config.ndi_name = get_value();
            if (config.ndi_name.empty()) return std::nullopt;
        }
        else if (arg == "-g" || arg == "--ndi-groups") {
            config.ndi_groups = get_value();
        }
        else if (arg == "--no-clock-video") {
            config.ndi_clock_video = false;
        }
        else if (arg == "--no-clock-audio") {
            config.ndi_clock_audio = false;
        }
        
        // Genlock options
        else if (arg == "--genlock") {
            config.genlock_mode = get_value();
            if (config.genlock_mode.empty()) return std::nullopt;
            std::transform(config.genlock_mode.begin(), config.genlock_mode.end(),
                          config.genlock_mode.begin(), ::tolower);
        }
        else if (arg == "--genlock-master") {
            config.genlock_master_addr = get_value();
            if (config.genlock_master_addr.empty()) return std::nullopt;
        }
        
        // HTTP options
        else if (arg == "--http-host") {
            config.http_host = get_value();
            if (config.http_host.empty()) return std::nullopt;
        }
        else if (arg == "-p" || arg == "--http-port") {
            int val = get_int();
            if (val <= 0 || val > 65535) return std::nullopt;
            config.http_port = static_cast<uint16_t>(val);
        }
        else if (arg == "--no-http") {
            config.http_enabled = false;
        }
        
        // CEF options
        else if (arg == "--cache-path") {
            config.cef_cache_path = get_value();
        }
        else if (arg == "--disable-gpu") {
            config.cef_disable_gpu = true;
        }
        else if (arg == "--user-agent") {
            config.cef_user_agent = get_value();
        }
        
        // Application options
        else if (arg == "-l" || arg == "--log-file") {
            config.log_file = get_value();
        }
        else if (arg == "-v" || arg == "--verbose") {
            config.log_level = 0; // DEBUG
        }
        else if (arg == "-q" || arg == "--quiet") {
            config.log_level = 3; // ERROR
        }
        else if (arg == "-d" || arg == "--daemon") {
            config.daemon_mode = true;
        }
        
        // Unknown option
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return std::nullopt;
        }
        else {
            // Treat as URL if no URL specified
            if (config.url == "about:blank") {
                config.url = arg;
            } else {
                std::cerr << "Error: Unexpected argument: " << arg << std::endl;
                return std::nullopt;
            }
        }
    }
    
    return config;
}

bool Config::validate() const {
    // Validate resolution
    if (width < 16 || width > 7680) {
        std::cerr << "Error: Width must be between 16 and 7680" << std::endl;
        return false;
    }
    if (height < 16 || height > 4320) {
        std::cerr << "Error: Height must be between 16 and 4320" << std::endl;
        return false;
    }
    
    // Validate FPS
    if (fps < 1 || fps > 240) {
        std::cerr << "Error: FPS must be between 1 and 240" << std::endl;
        return false;
    }
    
    // Validate NDI name
    if (ndi_name.empty()) {
        std::cerr << "Error: NDI name cannot be empty" << std::endl;
        return false;
    }
    
    // Validate HTTP port
    if (http_enabled && http_port == 0) {
        std::cerr << "Error: Invalid HTTP port" << std::endl;
        return false;
    }
    
    // Basic URL validation
    if (url.empty()) {
        std::cerr << "Error: URL cannot be empty" << std::endl;
        return false;
    }
    
    // Validate genlock mode
    if (genlock_mode != "disabled" && genlock_mode != "master" && genlock_mode != "slave") {
        std::cerr << "Error: Genlock mode must be 'disabled', 'master', or 'slave'" << std::endl;
        return false;
    }
    
    // Validate genlock master address for slave mode
    if (genlock_mode == "slave" && genlock_master_addr.empty()) {
        std::cerr << "Error: Genlock master address required for slave mode" << std::endl;
        return false;
    }
    
    return true;
}

} // namespace html2ndi

