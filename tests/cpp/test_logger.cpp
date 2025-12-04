/**
 * Unit tests for Logger class
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include "html2ndi/utils/logger.h"

using namespace html2ndi;

class LoggerTest : public ::testing::Test {
protected:
    std::string test_log_path;
    
    void SetUp() override {
        // Create temp directory for test logs
        test_log_path = std::filesystem::temp_directory_path() / "html2ndi_test.log";
        // Remove if exists from previous test
        std::filesystem::remove(test_log_path);
    }
    
    void TearDown() override {
        std::filesystem::remove(test_log_path);
    }
};

TEST_F(LoggerTest, InitializesWithoutFile) {
    // Should not crash when initialized without a file path
    Logger::instance().initialize(LogLevel::INFO, "");
    Logger::instance().log(LogLevel::INFO, "Test message without file");
}

TEST_F(LoggerTest, WritesToFile) {
    Logger::instance().initialize(LogLevel::INFO, test_log_path);
    Logger::instance().log(LogLevel::INFO, "Test log message");
    Logger::instance().flush();
    
    // Check file exists and contains message
    std::ifstream file(test_log_path);
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Test log message") != std::string::npos);
}

TEST_F(LoggerTest, RespectsLogLevel) {
    Logger::instance().initialize(LogLevel::WARNING, test_log_path);
    
    // These should not be logged
    Logger::instance().log(LogLevel::DEBUG, "Debug message");
    Logger::instance().log(LogLevel::INFO, "Info message");
    
    // This should be logged
    Logger::instance().log(LogLevel::WARNING, "Warning message");
    Logger::instance().log(LogLevel::ERROR, "Error message");
    
    Logger::instance().flush();
    
    std::ifstream file(test_log_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
}

TEST_F(LoggerTest, IncludesTimestamp) {
    Logger::instance().initialize(LogLevel::INFO, test_log_path);
    Logger::instance().log(LogLevel::INFO, "Timestamped message");
    Logger::instance().flush();
    
    std::ifstream file(test_log_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Check for timestamp pattern [YYYY-MM-DD HH:MM:SS.mmm]
    EXPECT_TRUE(content.find("[20") != std::string::npos);
}

TEST_F(LoggerTest, ThreadSafety) {
    Logger::instance().initialize(LogLevel::INFO, test_log_path);
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 100; ++j) {
                Logger::instance().log(LogLevel::INFO, "Thread %d message %d", i, j);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    Logger::instance().flush();
    
    // Should not crash and file should exist
    EXPECT_TRUE(std::filesystem::exists(test_log_path));
}

TEST_F(LoggerTest, GetDefaultLogDirectory) {
    std::string log_dir = get_default_log_directory();
    
    // Should return a non-empty path on macOS
    EXPECT_FALSE(log_dir.empty());
    EXPECT_TRUE(log_dir.find("Library/Logs/HTML2NDI") != std::string::npos);
}

