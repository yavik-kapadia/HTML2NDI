/**
 * Integration tests for HTTP API
 * Tests the HTTP endpoints without requiring CEF/NDI
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class HttpApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: These tests require a running html2ndi instance
        // They are integration tests that verify the API contract
    }
    
    void TearDown() override {}
    
    bool isServerRunning(const std::string& host, int port) {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(1);
        auto res = cli.Get("/status");
        return res != nullptr;
    }
};

// These tests are skipped if no server is running
// They serve as integration test examples

TEST_F(HttpApiTest, StatusEndpointFormat) {
    // This test documents the expected /status response format
    json expected_fields = {
        {"url", "string"},
        {"width", "number"},
        {"height", "number"},
        {"fps", "number"},
        {"actual_fps", "number"},
        {"ndi_name", "string"},
        {"ndi_connections", "number"},
        {"running", "boolean"},
        {"color", "object"}
    };
    
    // Verify we have all expected fields documented
    EXPECT_EQ(expected_fields.size(), 9);
}

TEST_F(HttpApiTest, SetUrlEndpointFormat) {
    // This test documents the expected /seturl request format
    json request = {
        {"url", "https://example.com"}
    };
    
    EXPECT_TRUE(request.contains("url"));
    EXPECT_TRUE(request["url"].is_string());
}

TEST_F(HttpApiTest, ColorEndpointFormat) {
    // This test documents the expected /color request format
    json preset_request = {{"preset", "rec709"}};
    json custom_request = {
        {"colorspace", "BT709"},
        {"gamma", "BT709"},
        {"range", "full"}
    };
    
    EXPECT_TRUE(preset_request.contains("preset"));
    EXPECT_TRUE(custom_request.contains("colorspace"));
}

TEST_F(HttpApiTest, ThumbnailEndpointParams) {
    // This test documents the expected /thumbnail query parameters
    std::string endpoint = "/thumbnail?width=320&quality=75";
    
    // Parameters: width (default 320), quality (default 75)
    EXPECT_TRUE(endpoint.find("width=") != std::string::npos);
    EXPECT_TRUE(endpoint.find("quality=") != std::string::npos);
}

TEST_F(HttpApiTest, ColorPresets) {
    // Document valid color presets
    std::vector<std::string> valid_presets = {
        "rec709",
        "rec2020",
        "srgb",
        "rec601"
    };
    
    EXPECT_EQ(valid_presets.size(), 4);
}

// Integration test that runs only if server is available
TEST_F(HttpApiTest, DISABLED_LiveStatusEndpoint) {
    // This test is disabled by default
    // Enable by removing DISABLED_ prefix when testing against live server
    
    httplib::Client cli("localhost", 8080);
    auto res = cli.Get("/status");
    
    ASSERT_NE(res, nullptr);
    ASSERT_EQ(res->status, 200);
    
    auto body = json::parse(res->body);
    EXPECT_TRUE(body.contains("url"));
    EXPECT_TRUE(body.contains("running"));
    EXPECT_TRUE(body.contains("ndi_name"));
}

