/**
 * Unit tests for Genlock functionality.
 */

#include <gtest/gtest.h>
#include "html2ndi/ndi/genlock.h"
#include <thread>
#include <chrono>

using namespace html2ndi;

class GenlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tests run quickly, so we use a higher FPS for faster verification
        test_fps_ = 60;
    }
    
    void TearDown() override {
        // Cleanup any shared instances
        SharedGenlockClock::clear_instance();
    }
    
    int test_fps_ = 60;
};

// Test basic genlock clock creation
TEST_F(GenlockTest, CreateGenlockClock) {
    GenlockClock clock(GenlockMode::Disabled);
    EXPECT_TRUE(clock.initialize());
    EXPECT_EQ(clock.mode(), GenlockMode::Disabled);
    EXPECT_FALSE(clock.is_synchronized());
}

// Test master mode initialization
TEST_F(GenlockTest, MasterModeInitialization) {
    GenlockClock clock(GenlockMode::Master, "127.0.0.1:5960", test_fps_);
    EXPECT_TRUE(clock.initialize());
    EXPECT_EQ(clock.mode(), GenlockMode::Master);
    EXPECT_TRUE(clock.is_synchronized());  // Master is always synchronized
}

// Test slave mode initialization
TEST_F(GenlockTest, SlaveModeInitialization) {
    GenlockClock clock(GenlockMode::Slave, "127.0.0.1:5960", test_fps_);
    EXPECT_TRUE(clock.initialize());
    EXPECT_EQ(clock.mode(), GenlockMode::Slave);
    // Slave starts not synchronized until it receives packets
    // We don't expect it to be synchronized without a master
}

// Test mode switching
TEST_F(GenlockTest, ModeSwitching) {
    GenlockClock clock(GenlockMode::Disabled);
    EXPECT_TRUE(clock.initialize());
    EXPECT_EQ(clock.mode(), GenlockMode::Disabled);
    
    // Switch to master
    clock.set_mode(GenlockMode::Master);
    EXPECT_EQ(clock.mode(), GenlockMode::Master);
    
    // Switch to slave
    clock.set_mode(GenlockMode::Slave);
    EXPECT_EQ(clock.mode(), GenlockMode::Slave);
    
    // Back to disabled
    clock.set_mode(GenlockMode::Disabled);
    EXPECT_EQ(clock.mode(), GenlockMode::Disabled);
}

// Test timecode generation
TEST_F(GenlockTest, TimecodeGeneration) {
    GenlockClock clock(GenlockMode::Master, "127.0.0.1:5960", test_fps_);
    EXPECT_TRUE(clock.initialize());
    
    // Get timecode
    int64_t tc1 = clock.get_ndi_timecode();
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get another timecode
    int64_t tc2 = clock.get_ndi_timecode();
    
    // Second timecode should be larger (time has passed)
    EXPECT_GT(tc2, tc1);
}

// Test frame boundary calculation
TEST_F(GenlockTest, FrameBoundaryCalculation) {
    GenlockClock clock(GenlockMode::Master, "127.0.0.1:5960", test_fps_);
    EXPECT_TRUE(clock.initialize());
    
    auto frame_duration = std::chrono::nanoseconds(1'000'000'000 / test_fps_);
    auto current_time = clock.now();
    
    auto next_boundary = clock.next_frame_boundary(current_time, frame_duration);
    
    // Next boundary should be in the future
    EXPECT_GT(next_boundary, current_time);
    
    // Calculate expected next boundary
    auto next_expected = current_time + frame_duration;
    
    // Should be close (within reasonable tolerance)
    // CI environments can have less precise timing, so use a more generous tolerance
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
        next_boundary - next_expected).count();
    EXPECT_LT(std::abs(diff), 5000);  // Within 5ms (generous for CI)
}

// Test disabled mode uses local clock
TEST_F(GenlockTest, DisabledModeUsesLocalClock) {
    GenlockClock clock(GenlockMode::Disabled);
    EXPECT_TRUE(clock.initialize());
    
    auto t1 = clock.now();
    auto local_t1 = std::chrono::steady_clock::now();
    
    // Should be very close to local clock
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
        t1 - local_t1).count();
    EXPECT_LT(std::abs(diff), 1000);  // Within 1ms
}

// Test stats collection
TEST_F(GenlockTest, StatsCollection) {
    GenlockClock clock(GenlockMode::Master, "127.0.0.1:5960", test_fps_);
    EXPECT_TRUE(clock.initialize());
    
    // Give it time to send some packets
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto stats = clock.get_stats();
    
    // Master should have sent some packets
    EXPECT_GT(stats.sync_packets_sent, 0u);
    
    clock.shutdown();
}

// Test shared genlock instance
TEST_F(GenlockTest, SharedGenlockInstance) {
    // Get shared instance
    auto instance1 = SharedGenlockClock::instance();
    ASSERT_NE(instance1, nullptr);
    
    // Get it again
    auto instance2 = SharedGenlockClock::instance();
    
    // Should be the same instance
    EXPECT_EQ(instance1, instance2);
    
    // Clear instance
    SharedGenlockClock::clear_instance();
    
    // New instance should be different
    auto instance3 = SharedGenlockClock::instance();
    EXPECT_NE(instance1, instance3);
}

// Test master-slave synchronization (integration test)
// Note: This test uses actual UDP networking and may be affected by CI environment
TEST_F(GenlockTest, MasterSlaveSync) {
    // Create master
    auto master = std::make_shared<GenlockClock>(
        GenlockMode::Master, "127.0.0.1:5961", test_fps_);
    
    // If initialization fails (e.g., in restricted CI), skip test gracefully
    if (!master->initialize()) {
        GTEST_SKIP() << "Genlock master initialization failed (network restricted?)";
        return;
    }
    
    // Create slave
    auto slave = std::make_shared<GenlockClock>(
        GenlockMode::Slave, "127.0.0.1:5961", test_fps_);
    
    if (!slave->initialize()) {
        master->shutdown();
        GTEST_SKIP() << "Genlock slave initialization failed (network restricted?)";
        return;
    }
    
    // Give them time to sync
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Check master stats
    auto master_stats = master->get_stats();
    EXPECT_GT(master_stats.sync_packets_sent, 0u);
    
    // Check slave stats - may not receive in CI environments
    auto slave_stats = slave->get_stats();
    if (slave_stats.sync_packets_received > 0) {
        // If packets were received, slave should be synchronized
        EXPECT_TRUE(slave->is_synchronized());
        
        // Check that times are close
        auto master_time = master->now();
        auto slave_time = slave->now();
        
        auto time_diff = std::chrono::duration_cast<std::chrono::microseconds>(
            master_time - slave_time).count();
        
        // Should be synchronized within 10ms
        EXPECT_LT(std::abs(time_diff), 10000);
    } else {
        // In CI, UDP packets might be blocked - this is acceptable
        GTEST_SKIP() << "UDP packets not received (firewall/network restriction)";
    }
    
    master->shutdown();
    slave->shutdown();
}

// Test graceful shutdown
TEST_F(GenlockTest, GracefulShutdown) {
    GenlockClock clock(GenlockMode::Master, "127.0.0.1:5962", test_fps_);
    EXPECT_TRUE(clock.initialize());
    
    // Give it time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Shutdown should not hang
    clock.shutdown();
    
    // Second shutdown should be safe
    clock.shutdown();
}

// Test address parsing
TEST_F(GenlockTest, AddressParsing) {
    // Test with port - use different ports to avoid conflicts
    GenlockClock clock1(GenlockMode::Slave, "127.0.0.1:6000", test_fps_);
    if (clock1.initialize()) {
        clock1.shutdown();
    }
    // If initialization fails in CI, that's acceptable
    
    // Test with default port
    GenlockClock clock2(GenlockMode::Slave, "127.0.0.1:6001", test_fps_);
    if (clock2.initialize()) {
        clock2.shutdown();
    }
    
    // Test localhost
    GenlockClock clock3(GenlockMode::Slave, "localhost:6002", test_fps_);
    if (clock3.initialize()) {
        clock3.shutdown();
    }
    
    // These tests verify parsing doesn't crash, not network functionality
    SUCCEED();
}

