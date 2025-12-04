#pragma once

#include <cstdarg>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace html2ndi {

/**
 * Log severity levels.
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

/**
 * Simple thread-safe logger.
 * Outputs to console and optionally to a rotating file.
 */
class Logger {
public:
    /**
     * Get the singleton instance.
     */
    static Logger& instance();
    
    /**
     * Initialize the logger.
     * @param level Minimum log level
     * @param file_path Optional log file path
     */
    void initialize(LogLevel level, const std::string& file_path = "");
    
    /**
     * Set the minimum log level.
     */
    void set_level(LogLevel level);
    
    /**
     * Log a message.
     * @param level Log level
     * @param format Printf-style format string
     */
    void log(LogLevel level, const char* format, ...);
    
    /**
     * Log a message with va_list.
     */
    void vlog(LogLevel level, const char* format, va_list args);
    
    /**
     * Flush any buffered output.
     */
    void flush();

private:
    Logger() = default;
    ~Logger();
    
    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void write_to_file(const std::string& message);
    void rotate_file_if_needed();
    
    LogLevel level_{LogLevel::INFO};
    std::string file_path_;
    std::ofstream file_;
    std::mutex mutex_;
    
    size_t max_file_size_{10 * 1024 * 1024}; // 10MB
    int max_files_{5};
};

// Convenience macros
#define LOG_DEBUG(fmt, ...) \
    html2ndi::Logger::instance().log(html2ndi::LogLevel::DEBUG, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    html2ndi::Logger::instance().log(html2ndi::LogLevel::INFO, fmt, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) \
    html2ndi::Logger::instance().log(html2ndi::LogLevel::WARNING, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    html2ndi::Logger::instance().log(html2ndi::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    html2ndi::Logger::instance().log(html2ndi::LogLevel::FATAL, fmt, ##__VA_ARGS__)

} // namespace html2ndi

