/**
 * Frame pump implementation.
 * Manages frame timing and delivery between CEF and NDI.
 */

#include "html2ndi/ndi/frame_pump.h"
#include "html2ndi/utils/logger.h"

#include <algorithm>
#include <cstring>

namespace html2ndi {

FramePump::FramePump(NdiSender* sender, int target_fps, bool progressive, std::shared_ptr<GenlockClock> genlock_clock)
    : sender_(sender)
    , target_fps_(target_fps)
    , progressive_(progressive)
    , genlock_clock_(genlock_clock) {
    
    // Calculate frame duration
    frame_duration_ = std::chrono::nanoseconds(1'000'000'000 / target_fps);
    
    // Pre-allocate frame buffers to prevent fragmentation
    // Reserve for 4K resolution (3840 x 2160 x 4 bytes BGRA)
    size_t max_frame_size = 3840 * 2160 * 4;
    buffers_[0].data.reserve(max_frame_size);
    buffers_[1].data.reserve(max_frame_size);
    
    LOG_DEBUG("Frame buffers pre-allocated: %zu bytes each", max_frame_size);
}

FramePump::~FramePump() {
    stop();
}

void FramePump::start() {
    if (running_) {
        return;
    }
    
    LOG_DEBUG("Starting frame pump at %d fps%s", 
              target_fps_.load(),
              genlock_clock_ ? " (GENLOCKED)" : "");
    
    running_ = true;
    start_time_ = get_current_time();
    fps_start_ = start_time_;
    fps_frame_count_ = 0;
    frames_sent_ = 0;
    frames_dropped_ = 0;
    frames_held_ = 0;
    
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
    
    LOG_DEBUG("Frame pump stopped. Sent: %llu, Dropped: %llu, Held: %llu", 
              frames_sent_.load(), frames_dropped_.load(), frames_held_.load());
}

void FramePump::submit_frame(const void* data, int width, int height) {
    if (!running_) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Track current dimensions for bandwidth calculation
    current_width_ = width;
    current_height_ = height;
    
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
        
        auto memcpy_start = std::chrono::high_resolution_clock::now();
        std::memcpy(buffer.data.data(), data, size);
        auto memcpy_end = std::chrono::high_resolution_clock::now();
        
        // Track memcpy time (exponential moving average)
        auto memcpy_us = std::chrono::duration_cast<std::chrono::microseconds>(
            memcpy_end - memcpy_start).count();
        double current_avg = avg_memcpy_time_us_.load();
        avg_memcpy_time_us_ = 0.9 * current_avg + 0.1 * memcpy_us;
        
        buffer.width = width;
        buffer.height = height;
        buffer.ready = true;
    }
    
    // Swap buffers
    write_buffer_.store(1 - write_idx);
    read_buffer_.store(write_idx);
    
    // Notify pump thread
    buffer_cv_.notify_one();
    
    // Track total submit time
    auto end_time = std::chrono::high_resolution_clock::now();
    auto submit_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    double current_avg = avg_submit_time_us_.load();
    avg_submit_time_us_ = 0.9 * current_avg + 0.1 * submit_us;
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
    
    auto next_frame_time = get_current_time();
    int fps = target_fps_.load();
    auto frame_duration = std::chrono::nanoseconds(1'000'000'000 / fps);
    
    while (running_) {
        // Wait for next frame time
        auto now = get_current_time();
        if (now < next_frame_time) {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait_until(lock, next_frame_time, [this] {
                return !running_;
            });
            
            if (!running_) break;
        }
        
        // Get current frame rate and duration
        fps = target_fps_.load();
        frame_duration = std::chrono::nanoseconds(1'000'000'000 / fps);
        
        // Calculate next frame time
        if (genlock_clock_ && genlock_clock_->mode() != GenlockMode::Disabled) {
            // Use genlock for frame-accurate synchronization
            next_frame_time = genlock_clock_->next_frame_boundary(
                get_current_time(), frame_duration);
        } else {
            // Standard timing
            next_frame_time = get_current_time() + frame_duration;
        }
        
        // Get read buffer
        int read_idx = read_buffer_.load();
        FrameBuffer& buffer = buffers_[read_idx];
        
        // Check if we have a frame to send
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            
            // Determine which frame to send: new or held
            bool send_new_frame = buffer.ready;
            bool can_send_frame = send_new_frame || (!buffer.data.empty() && buffer.width > 0 && buffer.height > 0);
            
            if (!can_send_frame) {
                // Truly no frame available (startup condition before first frame)
                frames_dropped_++;
                continue;
            }
            
            // Track frame hold if repeating previous frame
            if (!send_new_frame) {
                frames_held_++;
            }
            
            // Send frame to NDI (new or repeated for seamless output)
            if (sender_) {
                auto saved_timecode = sender_->get_timecode_mode();
                
                if (genlock_clock_ && genlock_clock_->mode() != GenlockMode::Disabled) {
                    sender_->set_timecode(genlock_clock_->get_ndi_timecode());
                }
                
                sender_->send_video_frame(
                    buffer.data.data(),
                    buffer.width,
                    buffer.height,
                    fps,
                    1,
                    progressive_
                );
                
                sender_->set_timecode_mode(saved_timecode);
            }
            
            // Mark new frame as consumed
            if (send_new_frame) {
                buffer.ready = false;
            }
            // Note: Frame hold (repeating) is NOT counted as dropped
            // The stream continues seamlessly at target framerate
        }
        
        frames_sent_++;
        fps_frame_count_++;
        
        // Update FPS counter
        update_fps_counter();
    }
    
    LOG_DEBUG("Frame pump thread exited");
}

void FramePump::update_fps_counter() {
    auto now = get_current_time();
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

void FramePump::set_genlock_clock(std::shared_ptr<GenlockClock> genlock_clock) {
    genlock_clock_ = genlock_clock;
    if (genlock_clock_) {
        LOG_INFO("Frame pump genlock enabled");
    } else {
        LOG_INFO("Frame pump genlock disabled");
    }
}

bool FramePump::is_genlocked() const {
    return genlock_clock_ && 
           genlock_clock_->mode() != GenlockMode::Disabled &&
           genlock_clock_->is_synchronized();
}

std::chrono::steady_clock::time_point FramePump::get_current_time() const {
    if (genlock_clock_ && genlock_clock_->mode() != GenlockMode::Disabled) {
        return genlock_clock_->now();
    }
    return std::chrono::steady_clock::now();
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

double FramePump::uptime_seconds() const {
    if (!running_) {
        return 0.0;
    }
    auto now = get_current_time();
    auto elapsed = std::chrono::duration<double>(now - start_time_);
    return elapsed.count();
}

double FramePump::drop_rate() const {
    uint64_t sent = frames_sent_.load();
    uint64_t dropped = frames_dropped_.load();
    uint64_t total = sent + dropped;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(dropped) / static_cast<double>(total);
}

uint64_t FramePump::bandwidth_bytes_per_sec() const {
    int width = current_width_.load();
    int height = current_height_.load();
    int fps = target_fps_.load();
    
    // BGRA = 4 bytes per pixel
    return static_cast<uint64_t>(width) * height * 4 * fps;
}

size_t FramePump::current_buffer_size() const {
    int read_idx = read_buffer_.load();
    const FrameBuffer& buffer = buffers_[read_idx];
    return buffer.data.size();
}

} // namespace html2ndi

