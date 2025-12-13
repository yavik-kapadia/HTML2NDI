#pragma once

#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/ndi/genlock.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace html2ndi {

/**
 * Frame pump for managing frame timing and delivery.
 * Receives frames from CEF and delivers them to NDI at a consistent rate.
 */
class FramePump {
public:
    /**
     * Create a frame pump.
     * @param sender NDI sender to deliver frames to
     * @param target_fps Target framerate
     * @param progressive true for progressive, false for interlaced
     * @param genlock_clock Optional genlock clock for synchronization
     */
    FramePump(NdiSender* sender, int target_fps, bool progressive = true, std::shared_ptr<GenlockClock> genlock_clock = nullptr);
    ~FramePump();
    
    // Non-copyable, non-movable
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;
    
    /**
     * Start the frame pump thread.
     */
    void start();
    
    /**
     * Stop the frame pump thread.
     */
    void stop();
    
    /**
     * Submit a new frame from CEF.
     * Thread-safe.
     * @param data RGBA pixel data
     * @param width Frame width
     * @param height Frame height
     */
    void submit_frame(const void* data, int width, int height);
    
    /**
     * Set the target framerate.
     * @param fps Target frames per second
     */
    void set_target_fps(int fps);
    
    /**
     * Get the actual framerate.
     * @return Measured frames per second
     */
    float actual_fps() const;
    
    /**
     * Get the number of frames sent.
     */
    uint64_t frames_sent() const { return frames_sent_; }
    
    /**
     * Get the number of frames dropped (truly unavailable).
     */
    uint64_t frames_dropped() const { return frames_dropped_; }
    
    /**
     * Get the number of frames held (repeated during content changes).
     */
    uint64_t frames_held() const { return frames_held_; }
    
    /**
     * Get uptime in seconds.
     */
    double uptime_seconds() const;
    
    /**
     * Get drop rate (0.0 to 1.0).
     */
    double drop_rate() const;
    
    /**
     * Get estimated bandwidth in bytes per second.
     */
    uint64_t bandwidth_bytes_per_sec() const;
    
    /**
     * Get a copy of the current frame for thumbnail/preview.
     * @param out_data Output buffer (will be resized)
     * @param out_width Output width
     * @param out_height Output height
     * @return true if a frame was available
     */
    bool get_current_frame(std::vector<uint8_t>& out_data, int& out_width, int& out_height) const;
    
    /**
     * Set genlock clock for synchronization.
     * @param genlock_clock Genlock clock instance (nullptr to disable)
     */
    void set_genlock_clock(std::shared_ptr<GenlockClock> genlock_clock);
    
    /**
     * Get genlock synchronization status.
     * @return true if genlocked and synchronized
     */
    bool is_genlocked() const;

private:
    void pump_thread();
    void update_fps_counter();
    std::chrono::steady_clock::time_point get_current_time() const;
    
    NdiSender* sender_;
    std::shared_ptr<GenlockClock> genlock_clock_;
    
    std::atomic<int> target_fps_;
    std::atomic<bool> progressive_;
    std::chrono::nanoseconds frame_duration_;
    
    // Double buffering
    struct FrameBuffer {
        std::vector<uint8_t> data;
        int width{0};
        int height{0};
        bool ready{false};
    };
    
    FrameBuffer buffers_[2];
    std::atomic<int> write_buffer_{0};
    std::atomic<int> read_buffer_{1};
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    
    // Thread control
    std::thread thread_;
    std::atomic<bool> running_{false};
    
    // Statistics
    std::atomic<uint64_t> frames_sent_{0};
    std::atomic<uint64_t> frames_dropped_{0};
    std::atomic<uint64_t> frames_held_{0};
    std::chrono::steady_clock::time_point start_time_;
    
    // Current frame info for bandwidth calculation
    std::atomic<int> current_width_{0};
    std::atomic<int> current_height_{0};
    
    // FPS measurement
    std::chrono::steady_clock::time_point fps_start_;
    std::atomic<uint64_t> fps_frame_count_{0};
    std::atomic<float> measured_fps_{0.0f};
};

} // namespace html2ndi

