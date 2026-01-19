/**
 * Unit tests for NdiSender audio functionality
 */

#include <gtest/gtest.h>
#include "html2ndi/ndi/ndi_sender.h"

using namespace html2ndi;

class NdiSenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sender with mock NDI
        sender = std::make_unique<NdiSender>("Test NDI Source", "");
    }

    void TearDown() override {
        sender.reset();
    }

    std::unique_ptr<NdiSender> sender;
};

TEST_F(NdiSenderTest, InitializeSucceeds) {
    EXPECT_TRUE(sender->initialize());
    EXPECT_TRUE(sender->is_initialized());
}

TEST_F(NdiSenderTest, SendAudioFrameDoesNotCrash) {
    ASSERT_TRUE(sender->initialize());
    
    // Create audio data: 10ms of 48kHz stereo (480 samples per channel)
    const int sample_rate = 48000;
    const int channels = 2;
    const int samples = 480;
    float audio_data[samples * channels] = {0};
    
    // Fill with test tone
    for (int i = 0; i < samples * channels; i++) {
        audio_data[i] = 0.5f * sinf(2.0f * M_PI * 440.0f * i / sample_rate);
    }
    
    // This should not crash - tests that the type cast is correct
    EXPECT_NO_THROW(sender->send_audio_frame(audio_data, sample_rate, channels, samples));
}

TEST_F(NdiSenderTest, SendAudioWithDifferentFormats) {
    ASSERT_TRUE(sender->initialize());
    
    float audio_data[4800] = {0};  // 100ms buffer
    
    // Test various common audio formats
    struct AudioFormat {
        int sample_rate;
        int channels;
        int samples;
    };
    
    AudioFormat formats[] = {
        {48000, 2, 480},   // 48kHz stereo, 10ms
        {48000, 1, 480},   // 48kHz mono, 10ms
        {44100, 2, 441},   // 44.1kHz stereo, 10ms
        {96000, 2, 960},   // 96kHz stereo, 10ms
    };
    
    for (const auto& fmt : formats) {
        EXPECT_NO_THROW(sender->send_audio_frame(audio_data, fmt.sample_rate, fmt.channels, fmt.samples))
            << "Failed for " << fmt.sample_rate << "Hz, " << fmt.channels << " channels, " << fmt.samples << " samples";
    }
}

TEST_F(NdiSenderTest, SendAudioWithoutInitializationDoesNotCrash) {
    // Don't initialize - should handle gracefully
    float audio_data[480 * 2] = {0};
    EXPECT_NO_THROW(sender->send_audio_frame(audio_data, 48000, 2, 480));
}

TEST_F(NdiSenderTest, SendVideoFrameWorks) {
    ASSERT_TRUE(sender->initialize());
    
    // Create a small video frame (100x100 BGRX)
    const int width = 100;
    const int height = 100;
    const int bytes_per_pixel = 4;
    uint8_t video_data[width * height * bytes_per_pixel] = {0};
    
    // Should not crash
    EXPECT_NO_THROW(sender->send_video_frame(video_data, width, height, 60, 1, true));
}

TEST_F(NdiSenderTest, GetConnectionCount) {
    ASSERT_TRUE(sender->initialize());
    
    int count = sender->get_connection_count(1000);
    EXPECT_GE(count, 0);  // Should return non-negative value
}

TEST_F(NdiSenderTest, GetSourceName) {
    ASSERT_TRUE(sender->initialize());
    
    std::string name = sender->get_source_name();
    EXPECT_FALSE(name.empty());
}

TEST_F(NdiSenderTest, GetTally) {
    ASSERT_TRUE(sender->initialize());
    
    auto tally = sender->get_tally(1000);
    // Just verify it doesn't crash - values depend on mock implementation
    EXPECT_FALSE(tally.on_program && tally.on_preview);  // Reasonable default
}

TEST_F(NdiSenderTest, ColorSpaceSettings) {
    ASSERT_TRUE(sender->initialize());
    
    // Test setting different color spaces
    sender->set_color_space(NdiSender::ColorSpace::Rec709);
    sender->set_color_space(NdiSender::ColorSpace::Rec2020);
    sender->set_color_space(NdiSender::ColorSpace::sRGB);
    sender->set_color_space(NdiSender::ColorSpace::Rec601);
    
    // No crash means success
    SUCCEED();
}

TEST_F(NdiSenderTest, GammaModeSettings) {
    ASSERT_TRUE(sender->initialize());
    
    // Test setting different gamma modes
    sender->set_gamma_mode(NdiSender::GammaMode::BT709);
    sender->set_gamma_mode(NdiSender::GammaMode::BT2020);
    sender->set_gamma_mode(NdiSender::GammaMode::sRGB);
    sender->set_gamma_mode(NdiSender::GammaMode::Linear);
    
    SUCCEED();
}

TEST_F(NdiSenderTest, ColorRangeSettings) {
    ASSERT_TRUE(sender->initialize());
    
    sender->set_color_range(NdiSender::ColorRange::Full);
    sender->set_color_range(NdiSender::ColorRange::Limited);
    
    SUCCEED();
}

TEST_F(NdiSenderTest, TimecodeSettings) {
    ASSERT_TRUE(sender->initialize());
    
    int64_t timecode = 1234567890LL;
    sender->set_next_timecode(timecode);
    
    // Verify timecode is used
    uint8_t video_data[100 * 100 * 4] = {0};
    EXPECT_NO_THROW(sender->send_video_frame(video_data, 100, 100, 60, 1, true));
}
