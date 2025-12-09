/**
 * Unit tests for Config class
 */

#include <gtest/gtest.h>
#include "html2ndi/config.h"

using namespace html2ndi;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConfigTest, DefaultValues) {
    Config config;
    
    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_EQ(config.fps, 60);
    EXPECT_TRUE(config.progressive);  // Default is progressive
    EXPECT_EQ(config.ndi_name, "HTML2NDI");
    EXPECT_EQ(config.http_port, 8080);
    EXPECT_TRUE(config.http_enabled);
}

TEST_F(ConfigTest, ParseMinimalArgs) {
    const char* argv[] = {"html2ndi", "--url", "https://example.com"};
    int argc = 3;
    
    auto config = Config::parse(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->url, "https://example.com");
}

TEST_F(ConfigTest, ParseAllArgs) {
    const char* argv[] = {
        "html2ndi",
        "--url", "https://example.com",
        "--width", "1280",
        "--height", "720",
        "--fps", "30",
        "--ndi-name", "MyStream",
        "--http-port", "9090"
    };
    int argc = 13;
    
    auto config = Config::parse(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->url, "https://example.com");
    EXPECT_EQ(config->width, 1280);
    EXPECT_EQ(config->height, 720);
    EXPECT_EQ(config->fps, 30);
    EXPECT_EQ(config->ndi_name, "MyStream");
    EXPECT_EQ(config->http_port, 9090);
}

TEST_F(ConfigTest, ValidateValidConfig) {
    Config config;
    config.url = "https://example.com";
    config.width = 1920;
    config.height = 1080;
    config.fps = 60;
    
    EXPECT_TRUE(config.validate());
}

TEST_F(ConfigTest, ValidateEmptyUrl) {
    Config config;
    config.url = "";
    
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigTest, ValidateInvalidResolution) {
    Config config;
    config.url = "https://example.com";
    config.width = 0;
    config.height = 1080;
    
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigTest, ValidateInvalidFps) {
    Config config;
    config.url = "https://example.com";
    config.fps = 0;
    
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigTest, ParseInterlacedFlag) {
    const char* argv[] = {"html2ndi", "--interlaced"};
    int argc = 2;
    
    auto config = Config::parse(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->progressive);  // Interlaced mode
}

TEST_F(ConfigTest, ParseInterlacedShortFlag) {
    const char* argv[] = {"html2ndi", "-i"};
    int argc = 2;
    
    auto config = Config::parse(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->progressive);  // Interlaced mode
}

TEST_F(ConfigTest, DefaultProgressiveMode) {
    const char* argv[] = {"html2ndi"};
    int argc = 1;
    
    auto config = Config::parse(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->progressive);  // Default is progressive
}

TEST_F(ConfigTest, StandardResolutions) {
    // Test common broadcast resolutions
    struct TestCase {
        uint32_t width;
        uint32_t height;
        bool should_be_valid;
    };
    
    TestCase test_cases[] = {
        {3840, 2160, true},   // 4K UHD
        {2560, 1440, true},   // QHD
        {1920, 1080, true},   // 1080p
        {1280, 720, true},    // 720p
        {1024, 768, true},    // XGA
        {854, 480, true},     // FWVGA
        {640, 480, true},     // SD
        {0, 0, false},        // Invalid
        {100000, 100000, true}, // Very large but technically valid
    };
    
    for (const auto& tc : test_cases) {
        Config config;
        config.url = "https://example.com";
        config.width = tc.width;
        config.height = tc.height;
        
        EXPECT_EQ(config.validate(), tc.should_be_valid)
            << "Failed for resolution " << tc.width << "x" << tc.height;
    }
}

TEST_F(ConfigTest, StandardFramerates) {
    // Test common broadcast framerates
    uint32_t standard_fps[] = {24, 25, 30, 50, 60};
    
    for (uint32_t fps : standard_fps) {
        Config config;
        config.url = "https://example.com";
        config.fps = fps;
        
        EXPECT_TRUE(config.validate())
            << "Failed for framerate " << fps;
    }
}

