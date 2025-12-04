#pragma once

#include "html2ndi/ndi/ndi_sender.h"

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
     */
    FramePump(NdiSender* sender, int target_fps);
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
     * Get the number of frames dropped.
     */
    uint64_t frames_dropped() const { return frames_dropped_; }

private:
    void pump_thread();
    void update_fps_counter();
    
    NdiSender* sender_;
    
    std::atomic<int> target_fps_;
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
    
    // FPS measurement
    std::chrono::steady_clock::time_point fps_start_;
    std::atomic<uint64_t> fps_frame_count_{0};
    std::atomic<float> measured_fps_{0.0f};
};

} // namespace html2ndi

