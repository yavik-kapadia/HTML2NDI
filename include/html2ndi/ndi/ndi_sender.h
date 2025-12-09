#pragma once

#include "html2ndi/config.h"
#include <Processing.NDI.Lib.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace html2ndi {

/**
 * Color space configuration for NDI output.
 */
enum class ColorSpace {
    Rec709,      // BT.709 (HD standard)
    Rec2020,     // BT.2020 (UHD/HDR)
    sRGB,        // sRGB (web standard)
    Rec601       // BT.601 (SD legacy)
};

enum class GammaMode {
    BT709,       // Standard broadcast gamma
    BT2020,      // HDR gamma
    sRGB,        // sRGB gamma (~2.2)
    Linear       // Linear (1.0)
};

enum class ColorRange {
    Full,        // 0-255 (PC/web)
    Limited      // 16-235 (broadcast)
};

/**
 * NDI sender wrapper.
 * Handles NDI initialization and frame transmission.
 */
class NdiSender {
public:
    /**
     * Create an NDI sender.
     * @param name NDI source name
     * @param groups NDI groups (comma-separated, empty for all)
     */
    explicit NdiSender(const std::string& name, const std::string& groups = "");
    ~NdiSender();
    
    // Non-copyable, non-movable
    NdiSender(const NdiSender&) = delete;
    NdiSender& operator=(const NdiSender&) = delete;
    NdiSender(NdiSender&&) = delete;
    NdiSender& operator=(NdiSender&&) = delete;
    
    /**
     * Initialize the NDI sender.
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * Shutdown the NDI sender.
     */
    void shutdown();
    
    /**
     * Send a video frame.
     * @param data RGBA pixel data
     * @param width Frame width
     * @param height Frame height
     * @param frame_rate_n Framerate numerator
     * @param frame_rate_d Framerate denominator
     * @param progressive true for progressive, false for interlaced
     */
    void send_video_frame(
        const uint8_t* data,
        int width,
        int height,
        int frame_rate_n,
        int frame_rate_d,
        bool progressive = true);
    
    /**
     * Send an audio frame.
     * @param data Audio sample data (float, interleaved)
     * @param sample_rate Sample rate
     * @param channels Number of channels
     * @param samples Number of samples per channel
     */
    void send_audio_frame(
        const float* data,
        int sample_rate,
        int channels,
        int samples);
    
    /**
     * Get the number of connected receivers.
     * @param timeout_ms Timeout in milliseconds
     * @return Number of connections
     */
    int get_connection_count(uint32_t timeout_ms = 0) const;
    
    /**
     * Tally state structure.
     */
    struct TallyState {
        bool on_program{false};  // Source is on air/program output
        bool on_preview{false};  // Source is on preview output
    };
    
    /**
     * Get the current tally state.
     * @param timeout_ms Timeout in milliseconds (0 for immediate)
     * @return Tally state
     */
    TallyState get_tally(uint32_t timeout_ms = 0) const;
    
    /**
     * Get the full NDI source name (includes machine name).
     */
    std::string get_source_name() const;
    
    /**
     * Check if NDI is initialized.
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * Get the NDI source name.
     */
    const std::string& name() const { return name_; }
    
    /**
     * Set color space settings.
     */
    void set_color_space(ColorSpace cs) { color_space_ = cs; update_metadata(); }
    void set_gamma_mode(GammaMode gm) { gamma_mode_ = gm; update_metadata(); }
    void set_color_range(ColorRange cr) { color_range_ = cr; update_metadata(); }
    
    /**
     * Get current color settings.
     */
    ColorSpace color_space() const { return color_space_; }
    GammaMode gamma_mode() const { return gamma_mode_; }
    ColorRange color_range() const { return color_range_; }
    
    /**
     * Get string representation of current settings.
     */
    std::string color_space_name() const;
    std::string gamma_mode_name() const;
    std::string color_range_name() const;

    /**
     * Set explicit timecode for next frame.
     * @param timecode NDI timecode in 100ns units (or NDIlib_send_timecode_synthesize)
     */
    void set_timecode(int64_t timecode) { next_timecode_ = timecode; }
    
    /**
     * Get current timecode mode.
     */
    int64_t get_timecode_mode() const { return next_timecode_; }
    
    /**
     * Set timecode mode (synthesized vs explicit).
     * @param mode NDIlib_send_timecode_synthesize for auto, or explicit value
     */
    void set_timecode_mode(int64_t mode) { next_timecode_ = mode; }

private:
    void update_metadata();
    std::string name_;
    std::string groups_;
    
    NDIlib_send_instance_t sender_{nullptr};
    std::atomic<bool> initialized_{false};
    
    mutable std::mutex send_mutex_;
    
    // Video frame buffer (reused to avoid allocations)
    std::vector<uint8_t> video_buffer_;
    NDIlib_video_frame_v2_t video_frame_{};
    
    // Audio frame buffer
    std::vector<float> audio_buffer_;
    NDIlib_audio_frame_v3_t audio_frame_{};
    
    // Color space settings
    ColorSpace color_space_{ColorSpace::Rec709};
    GammaMode gamma_mode_{GammaMode::BT709};
    ColorRange color_range_{ColorRange::Full};
    std::string color_metadata_;
    
    // Timecode control
    std::atomic<int64_t> next_timecode_{NDIlib_send_timecode_synthesize};
};

} // namespace html2ndi

