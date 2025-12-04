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

