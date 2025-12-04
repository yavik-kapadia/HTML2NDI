/**
 * Frame pump implementation.
 * Manages frame timing and delivery between CEF and NDI.
 */

#include "html2ndi/ndi/frame_pump.h"
#include "html2ndi/utils/logger.h"

#include <algorithm>
#include <cstring>

namespace html2ndi {

FramePump::FramePump(NdiSender* sender, int target_fps)
    : sender_(sender)
    , target_fps_(target_fps) {
    
    // Calculate frame duration
    frame_duration_ = std::chrono::nanoseconds(1'000'000'000 / target_fps);
}

FramePump::~FramePump() {
    stop();
}

void FramePump::start() {
    if (running_) {
        return;
    }
    
    LOG_DEBUG("Starting frame pump at %d fps", target_fps_.load());
    
    running_ = true;
    fps_start_ = std::chrono::steady_clock::now();
    fps_frame_count_ = 0;
    
    thread_ = std::thread(&FramePump::pump_thread, this);
}

void FramePump::stop() {
    if (!running_) {
        return;
    }
    
    LOG_DEBUG("Stopping frame pump...");
    
    running_ = false;
    buffer_cv_.notify_all();
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    LOG_DEBUG("Frame pump stopped. Sent: %llu, Dropped: %llu", 
              frames_sent_.load(), frames_dropped_.load());
}

void FramePump::submit_frame(const void* data, int width, int height) {
    if (!running_) {
        return;
    }
    
    // Get write buffer
    int write_idx = write_buffer_.load();
    FrameBuffer& buffer = buffers_[write_idx];
    
    // Copy frame data
    size_t size = width * height * 4; // RGBA
    
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        if (buffer.data.size() != size) {
            buffer.data.resize(size);
        }
        
        std::memcpy(buffer.data.data(), data, size);
        buffer.width = width;
        buffer.height = height;
        buffer.ready = true;
    }
    
    // Swap buffers
    write_buffer_.store(1 - write_idx);
    read_buffer_.store(write_idx);
    
    // Notify pump thread
    buffer_cv_.notify_one();
}

void FramePump::set_target_fps(int fps) {
    target_fps_ = fps;
    frame_duration_ = std::chrono::nanoseconds(1'000'000'000 / fps);
}

float FramePump::actual_fps() const {
    return measured_fps_.load();
}

void FramePump::pump_thread() {
    LOG_DEBUG("Frame pump thread started");
    
    auto next_frame_time = std::chrono::steady_clock::now();
    int fps = target_fps_.load();
    
    while (running_) {
        // Wait for next frame time
        auto now = std::chrono::steady_clock::now();
        if (now < next_frame_time) {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait_until(lock, next_frame_time, [this] {
                return !running_;
            });
            
            if (!running_) break;
        }
        
        // Get current frame rate
        fps = target_fps_.load();
        
        // Calculate next frame time
        next_frame_time = std::chrono::steady_clock::now() + 
                          std::chrono::nanoseconds(1'000'000'000 / fps);
        
        // Get read buffer
        int read_idx = read_buffer_.load();
        FrameBuffer& buffer = buffers_[read_idx];
        
        // Check if we have a frame to send
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            
            if (!buffer.ready) {
                // No new frame, count as dropped
                frames_dropped_++;
                continue;
            }
            
            // Send frame to NDI
            if (sender_) {
                sender_->send_video_frame(
                    buffer.data.data(),
                    buffer.width,
                    buffer.height,
                    fps,
                    1
                );
            }
            
            buffer.ready = false;
        }
        
        frames_sent_++;
        fps_frame_count_++;
        
        // Update FPS counter
        update_fps_counter();
    }
    
    LOG_DEBUG("Frame pump thread exited");
}

void FramePump::update_fps_counter() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - fps_start_).count();
    
    if (elapsed >= 1000) {
        // Calculate FPS
        float fps = static_cast<float>(fps_frame_count_.load()) * 1000.0f / 
                    static_cast<float>(elapsed);
        measured_fps_ = fps;
        
        // Reset counter
        fps_start_ = now;
        fps_frame_count_ = 0;
    }
}

bool FramePump::get_current_frame(std::vector<uint8_t>& out_data, int& out_width, int& out_height) const {
    // Get the most recently written buffer
    int read_idx = read_buffer_.load();
    const FrameBuffer& buffer = buffers_[read_idx];
    
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(buffer_mutex_));
    
    if (buffer.data.empty() || buffer.width == 0 || buffer.height == 0) {
        return false;
    }
    
    out_data = buffer.data;
    out_width = buffer.width;
    out_height = buffer.height;
    return true;
}

} // namespace html2ndi

