import XCTest
@testable import HTML2NDI_Manager
import SwiftUI

@MainActor
final class DashboardViewTests: XCTestCase {
    var streamManager: StreamManager!
    
    override func setUp() async throws {
        streamManager = StreamManager.shared
        // Clear existing streams for clean test environment
        streamManager.streams.removeAll()
    }
    
    override func tearDown() async throws {
        streamManager.streams.removeAll()
    }
    
    // MARK: - Stream Filtering Tests
    
    func testStreamFiltering() {
        // Add test streams
        let stream1 = StreamConfig(name: "Camera 1", url: "http://example.com", ndiName: "NDI Camera 1")
        let stream2 = StreamConfig(name: "Graphics", url: "http://graphics.local", ndiName: "NDI Graphics")
        let stream3 = StreamConfig(name: "Camera 2", url: "http://camera2.local", ndiName: "NDI Camera 2")
        
        streamManager.addStream(stream1)
        streamManager.addStream(stream2)
        streamManager.addStream(stream3)
        
        XCTAssertEqual(streamManager.streams.count, 3, "Should have 3 streams")
        
        // Test name filtering
        let cameraStreams = streamManager.streams.filter { $0.config.name.contains("Camera") }
        XCTAssertEqual(cameraStreams.count, 2, "Should find 2 camera streams")
        
        // Test NDI name filtering
        let ndiFiltered = streamManager.streams.filter { $0.config.ndiName.contains("Graphics") }
        XCTAssertEqual(ndiFiltered.count, 1, "Should find 1 graphics stream")
    }
    
    // MARK: - Genlock Configuration Tests
    
    func testGenlockMasterConfiguration() {
        var config = StreamConfig(name: "Master Stream", url: "http://test.com", ndiName: "Master")
        config.genlockMode = "master"
        
        let stream = StreamInstance(config: config)
        
        XCTAssertEqual(stream.config.genlockMode, "master", "Genlock mode should be master")
        XCTAssertTrue(stream.config.genlockMasterAddr.contains(":"), "Should have default master address with port")
    }
    
    func testGenlockSlaveConfiguration() {
        var config = StreamConfig(name: "Slave Stream", url: "http://test.com", ndiName: "Slave")
        config.genlockMode = "slave"
        config.genlockMasterAddr = "192.168.1.100:5960"
        
        let stream = StreamInstance(config: config)
        
        XCTAssertEqual(stream.config.genlockMode, "slave", "Genlock mode should be slave")
        XCTAssertEqual(stream.config.genlockMasterAddr, "192.168.1.100:5960", "Should have custom master address")
    }
    
    func testGenlockDisabledByDefault() {
        let config = StreamConfig(name: "Test Stream", url: "http://test.com", ndiName: "Test")
        let stream = StreamInstance(config: config)
        
        XCTAssertEqual(stream.config.genlockMode, "disabled", "Genlock should be disabled by default")
    }
    
    func testGenlockModeTransitions() {
        var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        let stream = StreamInstance(config: config)
        
        // Test disabled -> master
        stream.config.genlockMode = "master"
        XCTAssertEqual(stream.config.genlockMode, "master")
        
        // Test master -> slave
        stream.config.genlockMode = "slave"
        stream.config.genlockMasterAddr = "127.0.0.1:5960"
        XCTAssertEqual(stream.config.genlockMode, "slave")
        
        // Test slave -> disabled
        stream.config.genlockMode = "disabled"
        XCTAssertEqual(stream.config.genlockMode, "disabled")
    }
    
    // MARK: - Stream Instance Tests
    
    func testStreamInstanceHashable() {
        let config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        let stream1 = StreamInstance(config: config)
        let stream2 = StreamInstance(config: config)
        
        // Different instances should have different IDs
        XCTAssertNotEqual(stream1.id, stream2.id, "Different instances should have unique IDs")
        XCTAssertNotEqual(stream1, stream2, "Different instances should not be equal")
        
        // Same instance should equal itself
        XCTAssertEqual(stream1, stream1, "Instance should equal itself")
        
        // Hash values should be consistent
        XCTAssertEqual(stream1.hashValue, stream1.hashValue, "Hash should be consistent")
    }
    
    func testStreamStatus() {
        let config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        let stream = StreamInstance(config: config)
        
        XCTAssertFalse(stream.isRunning, "Stream should not be running initially")
        XCTAssertTrue(stream.isHealthy, "Stream should be healthy initially")
        XCTAssertTrue(stream.lastError.isEmpty, "Should have no error initially")
        XCTAssertEqual(stream.actualFps, 0, "FPS should be 0 when not running")
        XCTAssertEqual(stream.connections, 0, "Should have 0 connections initially")
    }
    
    // MARK: - Stream Configuration Tests
    
    func testStreamConfigurationDefaults() {
        let config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        
        XCTAssertEqual(config.width, 1920, "Default width should be 1920")
        XCTAssertEqual(config.height, 1080, "Default height should be 1080")
        XCTAssertEqual(config.fps, 60, "Default FPS should be 60")
        XCTAssertTrue(config.progressive, "Default should be progressive scan")
        XCTAssertEqual(config.colorPreset, "rec709", "Default color preset should be rec709")
        XCTAssertFalse(config.autoStart, "Auto-start should be false by default")
        XCTAssertEqual(config.genlockMode, "disabled", "Genlock should be disabled by default")
    }
    
    func testStreamConfigurationCustomValues() {
        var config = StreamConfig(name: "4K Stream", url: "http://4k.test", ndiName: "4K NDI")
        config.width = 3840
        config.height = 2160
        config.fps = 30
        config.colorPreset = "rec2020"
        config.autoStart = true
        config.genlockMode = "master"
        
        XCTAssertEqual(config.width, 3840)
        XCTAssertEqual(config.height, 2160)
        XCTAssertEqual(config.fps, 30)
        XCTAssertEqual(config.colorPreset, "rec2020")
        XCTAssertTrue(config.autoStart)
        XCTAssertEqual(config.genlockMode, "master")
    }
    
    // MARK: - Stream Manager Tests
    
    func testAddStream() {
        let config = StreamConfig(name: "New Stream", url: "http://new.com", ndiName: "New NDI")
        
        streamManager.addStream(config)
        
        XCTAssertEqual(streamManager.streams.count, 1, "Should have 1 stream after adding")
        XCTAssertEqual(streamManager.streams.first?.config.name, "New Stream")
    }
    
    func testRemoveStream() {
        let config = StreamConfig(name: "Temporary", url: "http://temp.com", ndiName: "Temp")
        streamManager.addStream(config)
        
        XCTAssertEqual(streamManager.streams.count, 1)
        
        if let stream = streamManager.streams.first {
            streamManager.removeStream(stream)
        }
        
        XCTAssertEqual(streamManager.streams.count, 0, "Should have 0 streams after removal")
    }
    
    func testUniqueStreamIDs() {
        let config1 = StreamConfig(name: "Stream 1", url: "http://1.com", ndiName: "NDI 1")
        let config2 = StreamConfig(name: "Stream 2", url: "http://2.com", ndiName: "NDI 2")
        
        streamManager.addStream(config1)
        streamManager.addStream(config2)
        
        let ids = Set(streamManager.streams.map { $0.id })
        XCTAssertEqual(ids.count, 2, "All stream IDs should be unique")
    }
    
    // MARK: - Genlock Master Address Validation
    
    func testGenlockMasterAddressFormat() {
        let validAddresses = [
            "127.0.0.1:5960",
            "192.168.1.100:5960",
            "10.0.0.1:6000",
            "localhost:5960"
        ]
        
        for address in validAddresses {
            XCTAssertTrue(address.contains(":"), "Address should contain port separator: \(address)")
            let components = address.split(separator: ":")
            XCTAssertEqual(components.count, 2, "Address should have host:port format: \(address)")
        }
    }
    
    func testGenlockDefaultPort() {
        let config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        let stream = StreamInstance(config: config)
        
        XCTAssertTrue(stream.config.genlockMasterAddr.contains("5960"), "Should use default port 5960")
    }
    
    // MARK: - Resolution Preset Tests
    
    func testStandardResolutions() {
        let standardResolutions = [
            (width: 3840, height: 2160, name: "4K UHD"),
            (width: 2560, height: 1440, name: "QHD"),
            (width: 1920, height: 1080, name: "1080p"),
            (width: 1280, height: 720, name: "720p"),
            (width: 1024, height: 768, name: "XGA"),
            (width: 854, height: 480, name: "FWVGA"),
            (width: 640, height: 480, name: "SD")
        ]
        
        for resolution in standardResolutions {
            var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
            config.width = resolution.width
            config.height = resolution.height
            
            XCTAssertEqual(config.width, resolution.width, "Width should match for \(resolution.name)")
            XCTAssertEqual(config.height, resolution.height, "Height should match for \(resolution.name)")
            XCTAssertGreaterThan(config.width, 0, "Width should be positive for \(resolution.name)")
            XCTAssertGreaterThan(config.height, 0, "Height should be positive for \(resolution.name)")
        }
    }
    
    func testStandardFramerates() {
        let standardFramerates = [24, 25, 30, 50, 60]
        
        for fps in standardFramerates {
            var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
            config.fps = fps
            
            XCTAssertEqual(config.fps, fps, "FPS should match \(fps)")
            XCTAssertGreaterThan(config.fps, 0, "FPS should be positive")
            XCTAssertLessThanOrEqual(config.fps, 120, "FPS should be reasonable")
        }
    }
    
    func testProgressiveScanMode() {
        var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        config.progressive = true
        
        XCTAssertTrue(config.progressive, "Should be progressive scan")
        
        let stream = StreamInstance(config: config)
        XCTAssertTrue(stream.config.progressive, "Stream should maintain progressive setting")
    }
    
    func testInterlacedScanMode() {
        var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        config.progressive = false
        
        XCTAssertFalse(config.progressive, "Should be interlaced scan")
        
        let stream = StreamInstance(config: config)
        XCTAssertFalse(stream.config.progressive, "Stream should maintain interlaced setting")
    }
    
    func testScanModeToggle() {
        var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        
        // Start progressive (default)
        XCTAssertTrue(config.progressive)
        
        // Toggle to interlaced
        config.progressive = false
        XCTAssertFalse(config.progressive)
        
        // Toggle back to progressive
        config.progressive = true
        XCTAssertTrue(config.progressive)
    }
    
    func testFormatCombinations() {
        // Test common broadcast format combinations
        let formats = [
            (width: 1920, height: 1080, fps: 60, progressive: true, name: "1080p60"),
            (width: 1920, height: 1080, fps: 30, progressive: false, name: "1080i30"),
            (width: 1280, height: 720, fps: 50, progressive: true, name: "720p50"),
            (width: 3840, height: 2160, fps: 30, progressive: true, name: "4Kp30"),
            (width: 1920, height: 1080, fps: 25, progressive: false, name: "1080i25")
        ]
        
        for format in formats {
            var config = StreamConfig(name: format.name, url: "http://test.com", ndiName: format.name)
            config.width = format.width
            config.height = format.height
            config.fps = format.fps
            config.progressive = format.progressive
            
            XCTAssertEqual(config.width, format.width, "\(format.name): Width mismatch")
            XCTAssertEqual(config.height, format.height, "\(format.name): Height mismatch")
            XCTAssertEqual(config.fps, format.fps, "\(format.name): FPS mismatch")
            XCTAssertEqual(config.progressive, format.progressive, "\(format.name): Scan mode mismatch")
        }
    }
    
    func testVideoFormatIdentifiers() {
        // Test that format identifiers can be constructed correctly
        let testCases = [
            (width: 1920, height: 1080, fps: 60, progressive: true, expected: "1080p60"),
            (width: 1920, height: 1080, fps: 30, progressive: false, expected: "1080i30"),
            (width: 1280, height: 720, fps: 50, progressive: true, expected: "720p50"),
            (width: 3840, height: 2160, fps: 30, progressive: true, expected: "4Kp30")
        ]
        
        for testCase in testCases {
            let formatName = testCase.height == 2160 ? "4K" : String(testCase.height)
            let scanMode = testCase.progressive ? "p" : "i"
            let identifier = "\(formatName)\(scanMode)\(testCase.fps)"
            
            XCTAssertEqual(identifier, testCase.expected, 
                          "Format identifier mismatch for \(testCase.width)×\(testCase.height)")
        }
    }
    
    func testResolutionAspectRatios() {
        let resolutions = [
            (width: 1920, height: 1080, ratio: 16.0/9.0, name: "16:9"),
            (width: 1280, height: 720, ratio: 16.0/9.0, name: "16:9"),
            (width: 3840, height: 2160, ratio: 16.0/9.0, name: "16:9"),
            (width: 1024, height: 768, ratio: 4.0/3.0, name: "4:3"),
            (width: 640, height: 480, ratio: 4.0/3.0, name: "4:3")
        ]
        
        for resolution in resolutions {
            let calculatedRatio = Double(resolution.width) / Double(resolution.height)
            let difference = abs(calculatedRatio - resolution.ratio)
            
            XCTAssertLessThan(difference, 0.01, 
                             "\(resolution.width)×\(resolution.height) should have \(resolution.name) aspect ratio")
        }
    }
    
    // MARK: - Performance Tests
    
    func testStreamListPerformance() {
        // Add many streams
        for i in 0..<100 {
            let config = StreamConfig(
                name: "Stream \(i)",
                url: "http://stream\(i).com",
                ndiName: "NDI \(i)"
            )
            streamManager.addStream(config)
        }
        
        measure {
            // Test filtering performance
            _ = streamManager.streams.filter { $0.config.name.contains("5") }
        }
        
        XCTAssertEqual(streamManager.streams.count, 100, "Should have 100 streams")
    }
}

// MARK: - Integration Tests

@MainActor
final class GenlockIntegrationTests: XCTestCase {
    
    func testMasterSlaveSetup() {
        // Setup master stream
        var masterConfig = StreamConfig(name: "Master", url: "http://master.com", ndiName: "NDI Master")
        masterConfig.genlockMode = "master"
        let master = StreamInstance(config: masterConfig)
        
        // Setup slave streams
        var slave1Config = StreamConfig(name: "Slave 1", url: "http://slave1.com", ndiName: "NDI Slave 1")
        slave1Config.genlockMode = "slave"
        slave1Config.genlockMasterAddr = "127.0.0.1:5960"
        let slave1 = StreamInstance(config: slave1Config)
        
        var slave2Config = StreamConfig(name: "Slave 2", url: "http://slave2.com", ndiName: "NDI Slave 2")
        slave2Config.genlockMode = "slave"
        slave2Config.genlockMasterAddr = "127.0.0.1:5960"
        let slave2 = StreamInstance(config: slave2Config)
        
        // Verify configuration
        XCTAssertEqual(master.config.genlockMode, "master")
        XCTAssertEqual(slave1.config.genlockMode, "slave")
        XCTAssertEqual(slave2.config.genlockMode, "slave")
        XCTAssertEqual(slave1.config.genlockMasterAddr, slave2.config.genlockMasterAddr, 
                       "Both slaves should point to same master")
    }
    
    func testGenlockConfiguration() {
        var config = StreamConfig(name: "Test", url: "http://test.com", ndiName: "Test")
        
        // Test each mode
        let modes = ["disabled", "master", "slave"]
        for mode in modes {
            config.genlockMode = mode
            let stream = StreamInstance(config: config)
            XCTAssertEqual(stream.config.genlockMode, mode, "Should configure \(mode) mode correctly")
        }
    }
}

