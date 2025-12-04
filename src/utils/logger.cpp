/**
 * Logger implementation.
 */

#include "html2ndi/utils/logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/types.h>
#endif

namespace html2ndi {

std::string get_default_log_directory() {
#ifdef __APPLE__
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    if (home) {
        std::string log_dir = std::string(home) + "/Library/Logs/HTML2NDI";
        std::filesystem::create_directories(log_dir);
        return log_dir;
    }
#endif
    return "";
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
#ifdef __APPLE__
    // Create os_log handle for Console.app integration
    os_log_ = os_log_create("com.html2ndi.worker", "general");
#endif
}

Logger::~Logger() {
    flush();
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::initialize(LogLevel level, const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    level_ = level;
    file_path_ = file_path;
    
    if (!file_path_.empty()) {
        // Create directory if needed
        std::filesystem::path path(file_path_);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        
        file_.open(file_path_, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Warning: Could not open log file: " << file_path_ << std::endl;
        }
    }
}

void Logger::set_level(LogLevel level) {
    level_ = level;
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (level < level_) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    vlog(level, format, args);
    va_end(args);
}

void Logger::vlog(LogLevel level, const char* format, va_list args) {
    if (level < level_) {
        return;
    }
    
    // Format message
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);
    
    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm;
    localtime_r(&time, &tm);
    
    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    // Level string
    const char* level_str;
    const char* color_start = "";
    const char* color_end = "\033[0m";
    
    switch (level) {
        case LogLevel::DEBUG:
            level_str = "DEBUG";
            color_start = "\033[36m"; // Cyan
            break;
        case LogLevel::INFO:
            level_str = "INFO ";
            color_start = "\033[32m"; // Green
            break;
        case LogLevel::WARNING:
            level_str = "WARN ";
            color_start = "\033[33m"; // Yellow
            break;
        case LogLevel::ERROR:
            level_str = "ERROR";
            color_start = "\033[31m"; // Red
            break;
        case LogLevel::FATAL:
            level_str = "FATAL";
            color_start = "\033[35m"; // Magenta
            break;
        default:
            level_str = "?????";
            color_start = "";
            color_end = "";
    }
    
    // Format log line
    std::ostringstream log_line;
    log_line << "[" << timestamp.str() << "] [" << level_str << "] " << message;
    
    std::string line = log_line.str();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Output to console with colors
        if (isatty(fileno(stderr))) {
            std::cerr << color_start << line << color_end << std::endl;
        } else {
            std::cerr << line << std::endl;
        }
        
        // Output to file without colors
        if (file_.is_open()) {
            file_ << line << std::endl;
            rotate_file_if_needed();
        }
        
        // Output to macOS unified logging (Console.app)
        log_to_os(level, message);
    }
}

void Logger::log_to_os(LogLevel level, const char* message) {
#ifdef __APPLE__
    if (!os_log_) return;
    
    os_log_type_t log_type;
    switch (level) {
        case LogLevel::DEBUG:
            log_type = OS_LOG_TYPE_DEBUG;
            break;
        case LogLevel::INFO:
            log_type = OS_LOG_TYPE_INFO;
            break;
        case LogLevel::WARNING:
            log_type = OS_LOG_TYPE_DEFAULT;
            break;
        case LogLevel::ERROR:
            log_type = OS_LOG_TYPE_ERROR;
            break;
        case LogLevel::FATAL:
            log_type = OS_LOG_TYPE_FAULT;
            break;
        default:
            log_type = OS_LOG_TYPE_DEFAULT;
    }
    
    os_log_with_type(os_log_, log_type, "%{public}s", message);
#else
    (void)level;
    (void)message;
#endif
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.flush();
    }
}

void Logger::write_to_file(const std::string& message) {
    if (file_.is_open()) {
        file_ << message << std::endl;
    }
}

void Logger::rotate_file_if_needed() {
    if (file_path_.empty() || !file_.is_open()) {
        return;
    }
    
    // Check file size
    file_.seekp(0, std::ios::end);
    auto size = file_.tellp();
    
    if (static_cast<size_t>(size) < max_file_size_) {
        return;
    }
    
    // Rotate files
    file_.close();
    
    std::filesystem::path base_path(file_path_);
    
    // Remove oldest file
    std::filesystem::path oldest = base_path;
    oldest += "." + std::to_string(max_files_);
    if (std::filesystem::exists(oldest)) {
        std::filesystem::remove(oldest);
    }
    
    // Rotate existing files
    for (int i = max_files_ - 1; i >= 1; --i) {
        std::filesystem::path old_path = base_path;
        old_path += "." + std::to_string(i);
        
        std::filesystem::path new_path = base_path;
        new_path += "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(old_path)) {
            std::filesystem::rename(old_path, new_path);
        }
    }
    
    // Rename current file
    std::filesystem::path rotated = base_path;
    rotated += ".1";
    std::filesystem::rename(base_path, rotated);
    
    // Open new file
    file_.open(file_path_, std::ios::app);
}

} // namespace html2ndi

