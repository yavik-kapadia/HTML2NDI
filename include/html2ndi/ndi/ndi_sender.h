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
     */
    void send_video_frame(
        const uint8_t* data,
        int width,
        int height,
        int frame_rate_n,
        int frame_rate_d);
    
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
     * Check if NDI is initialized.
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * Get the NDI source name.
     */
    const std::string& name() const { return name_; }

private:
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
};

} // namespace html2ndi

