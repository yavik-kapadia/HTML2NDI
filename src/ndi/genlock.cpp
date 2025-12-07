/**
 * Genlock implementation for frame-accurate multi-stream synchronization.
 */

#include "html2ndi/ndi/genlock.h"
#include "html2ndi/utils/logger.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace html2ndi {

// Genlock sync packet structure
struct GenlockPacket {
    uint32_t magic{0x474E4C4B};  // 'GNLK'
    uint32_t version{1};
    int64_t timestamp_ns;        // Master timestamp in nanoseconds
    int64_t frame_number;        // Frame counter
    uint32_t fps;                // Framerate
    uint32_t checksum;           // Simple checksum
    
    uint32_t calculate_checksum() const {
        return magic ^ version ^ static_cast<uint32_t>(timestamp_ns) ^
               static_cast<uint32_t>(frame_number) ^ fps;
    }
    
    bool validate() const {
        return magic == 0x474E4C4B && checksum == calculate_checksum();
    }
};

GenlockClock::GenlockClock(
    GenlockMode mode,
    const std::string& master_address,
    int fps)
    : mode_(mode)
    , master_address_(master_address)
    , fps_(fps)
    , reference_time_(std::chrono::steady_clock::now()) {
}

GenlockClock::~GenlockClock() {
    shutdown();
}

bool GenlockClock::initialize() {
    if (initialized_) {
        return true;
    }
    
    if (mode_ == GenlockMode::Disabled) {
        LOG_DEBUG("Genlock disabled");
        initialized_ = true;
        return true;
    }
    
    LOG_INFO("Initializing genlock in %s mode", 
             mode_ == GenlockMode::Master ? "MASTER" : "SLAVE");
    
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create genlock socket: %s", strerror(errno));
        return false;
    }
    
    // Set socket options
    int reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LOG_DEBUG("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    if (mode_ == GenlockMode::Master) {
        // Master: bind to port for sending
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;  // Let system assign port
        
        if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            LOG_ERROR("Failed to bind genlock socket: %s", strerror(errno));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        reference_time_ = std::chrono::steady_clock::now();
        synchronized_ = true;  // Master is always synchronized
        
    } else {
        // Slave: bind to port for receiving
        size_t colon_pos = master_address_.find(':');
        int port = 5960;  // Default port
        if (colon_pos != std::string::npos) {
            port = std::stoi(master_address_.substr(colon_pos + 1));
        }
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            LOG_ERROR("Failed to bind genlock socket to port %d: %s", port, strerror(errno));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Set receive timeout
        struct timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    // Start sync thread
    running_ = true;
    if (mode_ == GenlockMode::Master) {
        sync_thread_ = std::thread(&GenlockClock::master_thread, this);
    } else {
        sync_thread_ = std::thread(&GenlockClock::slave_thread, this);
    }
    
    initialized_ = true;
    LOG_INFO("Genlock initialized successfully");
    
    return true;
}

void GenlockClock::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_DEBUG("Shutting down genlock...");
    
    running_ = false;
    
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    initialized_ = false;
    LOG_DEBUG("Genlock shutdown complete");
}

std::chrono::steady_clock::time_point GenlockClock::now() const {
    if (mode_ == GenlockMode::Disabled || !initialized_) {
        return std::chrono::steady_clock::now();
    }
    
    if (mode_ == GenlockMode::Master) {
        return std::chrono::steady_clock::now();
    }
    
    // Slave mode: apply offset
    auto local_time = std::chrono::steady_clock::now();
    auto offset = std::chrono::microseconds(sync_offset_us_.load());
    return local_time - offset;
}

std::chrono::steady_clock::time_point GenlockClock::next_frame_boundary(
    std::chrono::steady_clock::time_point current_time,
    std::chrono::nanoseconds frame_duration) const {
    
    if (mode_ == GenlockMode::Disabled || !initialized_) {
        return current_time + frame_duration;
    }
    
    // Calculate time since reference
    auto elapsed = current_time - reference_time_;
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    
    // Calculate current frame number
    int64_t current_frame = elapsed_ns / frame_duration.count();
    
    // Next frame boundary
    int64_t next_frame = current_frame + 1;
    int64_t next_boundary_ns = next_frame * frame_duration.count();
    
    return reference_time_ + std::chrono::nanoseconds(next_boundary_ns);
}

int64_t GenlockClock::get_ndi_timecode() const {
    if (mode_ == GenlockMode::Disabled || !initialized_) {
        // Use synthesized timecode (NDI constant value is 0x7FFFFFFFFFFFFFFF)
        return INT64_C(0x7FFFFFFFFFFFFFFF);
    }
    
    // Calculate timecode in 100ns units since reference
    auto current = now();
    auto elapsed = current - reference_time_;
    auto elapsed_100ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100;
    
    return elapsed_100ns;
}

void GenlockClock::set_mode(GenlockMode mode) {
    if (mode == mode_) {
        return;
    }
    
    bool was_initialized = initialized_;
    if (was_initialized) {
        shutdown();
    }
    
    mode_ = mode;
    synchronized_ = false;
    
    if (was_initialized && mode != GenlockMode::Disabled) {
        initialize();
    }
}

void GenlockClock::set_master_address(const std::string& address) {
    if (address == master_address_) {
        return;
    }
    
    bool was_initialized = initialized_ && mode_ == GenlockMode::Slave;
    if (was_initialized) {
        shutdown();
    }
    
    master_address_ = address;
    
    if (was_initialized) {
        initialize();
    }
}

GenlockClock::Stats GenlockClock::get_stats() const {
    Stats stats;
    stats.sync_packets_sent = packets_sent_;
    stats.sync_packets_received = packets_received_;
    stats.sync_failures = sync_failures_;
    stats.avg_offset_us = avg_offset_us_;
    stats.max_offset_us = max_offset_us_;
    stats.jitter_us = jitter_us_;
    return stats;
}

void GenlockClock::master_thread() {
    LOG_DEBUG("Genlock master thread started");
    
    // Parse master address for broadcast
    std::string ip_addr = "127.0.0.1";
    int port = 5960;
    
    size_t colon_pos = master_address_.find(':');
    if (colon_pos != std::string::npos) {
        ip_addr = master_address_.substr(0, colon_pos);
        port = std::stoi(master_address_.substr(colon_pos + 1));
    }
    
    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_addr.c_str(), &dest_addr.sin_addr);
    
    // Enable broadcast if using broadcast address
    if (ip_addr.find("255") != std::string::npos) {
        int broadcast = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    }
    
    int64_t frame_number = 0;
    auto frame_duration = std::chrono::nanoseconds(1'000'000'000 / fps_);
    auto next_send = std::chrono::steady_clock::now();
    
    while (running_) {
        // Wait until next frame boundary
        auto now = std::chrono::steady_clock::now();
        if (now < next_send) {
            std::this_thread::sleep_until(next_send);
        }
        
        // Prepare sync packet
        GenlockPacket packet;
        auto timestamp = std::chrono::steady_clock::now() - reference_time_;
        packet.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp).count();
        packet.frame_number = frame_number;
        packet.fps = fps_;
        packet.checksum = packet.calculate_checksum();
        
        // Send packet
        ssize_t sent = sendto(socket_fd_, &packet, sizeof(packet), 0,
                             (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent == sizeof(packet)) {
            packets_sent_++;
        } else if (sent < 0) {
            LOG_DEBUG("Failed to send genlock packet: %s", strerror(errno));
        }
        
        // Schedule next send
        frame_number++;
        next_send += frame_duration;
    }
    
    LOG_DEBUG("Genlock master thread exited");
}

void GenlockClock::slave_thread() {
    LOG_DEBUG("Genlock slave thread started");
    
    GenlockPacket packet;
    struct sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);
    
    std::vector<int64_t> offset_history;
    offset_history.reserve(100);
    
    while (running_) {
        // Receive sync packet
        ssize_t received = recvfrom(socket_fd_, &packet, sizeof(packet), 0,
                                   (struct sockaddr*)&src_addr, &src_len);
        
        if (received == sizeof(packet) && packet.validate()) {
            packets_received_++;
            
            // Calculate time offset
            auto local_now = std::chrono::steady_clock::now();
            auto master_time = reference_time_ + std::chrono::nanoseconds(packet.timestamp_ns);
            auto offset = std::chrono::duration_cast<std::chrono::microseconds>(
                local_now - master_time).count();
            
            // Update sync state
            update_sync_offset(offset);
            
            // Track offset history for jitter calculation
            offset_history.push_back(offset);
            if (offset_history.size() > 100) {
                offset_history.erase(offset_history.begin());
            }
            
            // Calculate jitter
            if (offset_history.size() > 1) {
                int64_t sum = 0;
                for (auto o : offset_history) sum += o;
                int64_t avg = sum / offset_history.size();
                
                int64_t variance_sum = 0;
                for (auto o : offset_history) {
                    int64_t diff = o - avg;
                    variance_sum += diff * diff;
                }
                double variance = static_cast<double>(variance_sum) / offset_history.size();
                jitter_us_ = std::sqrt(variance);
            }
            
            synchronized_ = true;
            
        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_DEBUG("Failed to receive genlock packet: %s", strerror(errno));
            sync_failures_++;
        }
        
        // Check for sync timeout
        if (!synchronized_ && packets_received_ == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    LOG_DEBUG("Genlock slave thread exited");
}

void GenlockClock::update_sync_offset(int64_t offset_us) {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    
    // Apply exponential moving average for smooth adjustment
    const double alpha = 0.1;  // Smoothing factor
    int64_t current_offset = sync_offset_us_.load();
    int64_t new_offset = static_cast<int64_t>(
        alpha * offset_us + (1.0 - alpha) * current_offset
    );
    
    sync_offset_us_ = new_offset;
    avg_offset_us_ = new_offset;
    
    // Track max offset
    int64_t abs_offset = std::abs(offset_us);
    int64_t current_max = max_offset_us_.load();
    if (abs_offset > current_max) {
        max_offset_us_ = abs_offset;
    }
}

// Shared instance implementation
std::shared_ptr<GenlockClock> SharedGenlockClock::instance_;
std::mutex SharedGenlockClock::instance_mutex_;

std::shared_ptr<GenlockClock> SharedGenlockClock::instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_shared<GenlockClock>();
    }
    return instance_;
}

void SharedGenlockClock::set_instance(std::shared_ptr<GenlockClock> clock) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_ = clock;
}

void SharedGenlockClock::clear_instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_.reset();
}

} // namespace html2ndi

