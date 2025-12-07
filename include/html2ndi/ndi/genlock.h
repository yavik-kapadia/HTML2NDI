#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace html2ndi {

/**
 * Genlock mode for synchronization.
 */
enum class GenlockMode {
    Disabled,   // No genlock, independent timing
    Master,     // This instance provides reference clock
    Slave       // This instance syncs to master clock
};

/**
 * Genlock clock for frame-accurate multi-stream synchronization.
 * 
 * Provides a shared time reference across multiple NDI streams to ensure
 * frame-accurate synchronization. Supports master/slave topology where
 * one stream acts as the timing reference and others sync to it.
 */
class GenlockClock {
public:
    /**
     * Create a genlock clock.
     * @param mode Genlock mode (master/slave/disabled)
     * @param master_address For slave mode, IP:port of master (default: 127.0.0.1:5960)
     * @param fps Target framerate for sync calculations
     */
    explicit GenlockClock(
        GenlockMode mode = GenlockMode::Disabled,
        const std::string& master_address = "127.0.0.1:5960",
        int fps = 60
    );
    
    ~GenlockClock();
    
    // Non-copyable, non-movable
    GenlockClock(const GenlockClock&) = delete;
    GenlockClock& operator=(const GenlockClock&) = delete;
    
    /**
     * Initialize the genlock clock.
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * Shutdown the genlock clock.
     */
    void shutdown();
    
    /**
     * Get the current genlock time.
     * For master mode: returns local clock
     * For slave mode: returns synchronized time from master
     * For disabled mode: returns local steady_clock
     * @return Synchronized time point
     */
    std::chrono::steady_clock::time_point now() const;
    
    /**
     * Calculate the next frame time based on genlock.
     * Ensures frame boundaries align across all genlocked streams.
     * @param current_time Current time point
     * @param frame_duration Duration of one frame
     * @return Next frame boundary time point
     */
    std::chrono::steady_clock::time_point next_frame_boundary(
        std::chrono::steady_clock::time_point current_time,
        std::chrono::nanoseconds frame_duration
    ) const;
    
    /**
     * Get NDI timecode for current time.
     * Generates frame-accurate timecode synchronized across streams.
     * @return NDI timecode in 100ns units
     */
    int64_t get_ndi_timecode() const;
    
    /**
     * Get sync status.
     */
    bool is_synchronized() const { return synchronized_; }
    
    /**
     * Get sync offset in microseconds (for slaves).
     * Positive means slave is ahead, negative means behind.
     */
    int64_t sync_offset_us() const { return sync_offset_us_; }
    
    /**
     * Get current mode.
     */
    GenlockMode mode() const { return mode_; }
    
    /**
     * Set new mode and reinitialize.
     */
    void set_mode(GenlockMode mode);
    
    /**
     * Set master address (for slave mode).
     */
    void set_master_address(const std::string& address);
    
    /**
     * Get statistics.
     */
    struct Stats {
        uint64_t sync_packets_sent{0};      // Master: packets sent
        uint64_t sync_packets_received{0};  // Slave: packets received
        uint64_t sync_failures{0};          // Slave: failed sync attempts
        int64_t avg_offset_us{0};           // Slave: average offset
        int64_t max_offset_us{0};           // Slave: max offset seen
        double jitter_us{0.0};              // Slave: timing jitter
    };
    
    Stats get_stats() const;

private:
    void master_thread();
    void slave_thread();
    void update_sync_offset(int64_t offset_us);
    
    GenlockMode mode_;
    std::string master_address_;
    int fps_;
    
    // Reference time for master mode
    std::chrono::steady_clock::time_point reference_time_;
    
    // Synchronization state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> synchronized_{false};
    std::atomic<int64_t> sync_offset_us_{0};
    
    // Sync thread
    std::thread sync_thread_;
    mutable std::mutex sync_mutex_;
    
    // UDP socket for sync packets
    int socket_fd_{-1};
    
    // Statistics
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> sync_failures_{0};
    std::atomic<int64_t> avg_offset_us_{0};
    std::atomic<int64_t> max_offset_us_{0};
    std::atomic<double> jitter_us_{0.0};
};

/**
 * Shared genlock clock instance.
 * Provides global access to genlock clock for all streams in the same process.
 */
class SharedGenlockClock {
public:
    /**
     * Get or create the shared genlock instance.
     */
    static std::shared_ptr<GenlockClock> instance();
    
    /**
     * Set the shared genlock instance.
     */
    static void set_instance(std::shared_ptr<GenlockClock> clock);
    
    /**
     * Clear the shared instance.
     */
    static void clear_instance();

private:
    static std::shared_ptr<GenlockClock> instance_;
    static std::mutex instance_mutex_;
};

} // namespace html2ndi

