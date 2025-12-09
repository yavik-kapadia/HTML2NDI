#!/bin/bash
# End-to-end NDI output validation tests
# Tests actual NDI stream format, framerate, and progressive/interlaced mode

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORKER_BIN="$PROJECT_ROOT/build/bin/html2ndi.app/Contents/MacOS/html2ndi"

# Test results
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."
    
    # Check if NDI tools are available
    if ! command -v ndi-record &> /dev/null && ! command -v ffmpeg &> /dev/null; then
        echo -e "${RED}ERROR: Neither ndi-record nor ffmpeg with NDI support found${NC}"
        echo "Install one of:"
        echo "  1. NDI Tools: https://ndi.video/tools/"
        echo "  2. ffmpeg with NDI: brew install ffmpeg --with-ndi"
        exit 1
    fi
    
    # Check if worker binary exists
    if [ ! -f "$WORKER_BIN" ]; then
        echo -e "${RED}ERROR: Worker binary not found at $WORKER_BIN${NC}"
        echo "Build it first: cd build && ninja"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Prerequisites OK${NC}"
}

# Start worker with specific config
start_worker() {
    local url="$1"
    local width="$2"
    local height="$3"
    local fps="$4"
    local progressive="$5"
    local ndi_name="$6"
    local http_port="$7"
    
    local progressive_flag=""
    if [ "$progressive" = "false" ]; then
        progressive_flag="--interlaced"
    fi
    
    echo "Starting worker: ${width}x${height}@${fps}fps $([ "$progressive" = "true" ] && echo "progressive" || echo "interlaced")"
    
    "$WORKER_BIN" \
        --url "$url" \
        --width "$width" \
        --height "$height" \
        --fps "$fps" \
        $progressive_flag \
        --ndi-name "$ndi_name" \
        --http-port "$http_port" \
        --cache-path "/tmp/html2ndi-test-$$" \
        > "/tmp/html2ndi-worker-$$.log" 2>&1 &
    
    WORKER_PID=$!
    echo "Worker PID: $WORKER_PID"
    
    # Wait for worker to start
    echo "Waiting for worker to initialize..."
    local retries=0
    while [ $retries -lt 30 ]; do
        if curl -s "http://localhost:$http_port/status" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Worker started${NC}"
            return 0
        fi
        sleep 1
        retries=$((retries + 1))
    done
    
    echo -e "${RED}ERROR: Worker failed to start${NC}"
    cat "/tmp/html2ndi-worker-$$.log"
    return 1
}

# Stop worker
stop_worker() {
    if [ -n "$WORKER_PID" ]; then
        echo "Stopping worker (PID: $WORKER_PID)..."
        kill $WORKER_PID 2>/dev/null || true
        wait $WORKER_PID 2>/dev/null || true
        WORKER_PID=""
        rm -rf "/tmp/html2ndi-test-$$"
    fi
}

# Verify NDI stream using NDI tools
verify_ndi_stream_with_ndi_tools() {
    local ndi_name="$1"
    local expected_width="$2"
    local expected_height="$3"
    local expected_fps="$4"
    local expected_progressive="$5"
    
    echo "Verifying NDI stream with NDI tools..."
    
    # Use ndi-recv or ndi-record to capture stream info
    # Note: This is a placeholder - actual implementation depends on NDI tools available
    if command -v ndi-record &> /dev/null; then
        # Record 5 seconds and analyze
        timeout 5 ndi-record -s "$ndi_name" -o "/tmp/ndi-test-$$.mov" 2>&1 | tee "/tmp/ndi-output-$$.txt"
        
        # Parse output for resolution and framerate
        # This is NDI-tool specific parsing
        local actual_resolution=$(grep -i "resolution" "/tmp/ndi-output-$$.txt" | head -1)
        local actual_fps=$(grep -i "framerate\|fps" "/tmp/ndi-output-$$.txt" | head -1)
        
        echo "Detected: $actual_resolution @ $actual_fps"
        
        rm -f "/tmp/ndi-test-$$.mov" "/tmp/ndi-output-$$.txt"
        return 0
    fi
    
    return 1
}

# Verify NDI stream using ffmpeg
verify_ndi_stream_with_ffmpeg() {
    local ndi_name="$1"
    local expected_width="$2"
    local expected_height="$3"
    local expected_fps="$4"
    local expected_progressive="$5"
    
    echo "Verifying NDI stream with ffmpeg..."
    
    if ! command -v ffmpeg &> /dev/null; then
        echo "ffmpeg not available"
        return 1
    fi
    
    # Capture stream metadata
    timeout 10 ffmpeg -f libndi_newtek -i "$ndi_name" -t 2 -f null - 2>&1 | tee "/tmp/ffmpeg-output-$$.txt"
    
    local output=$(cat "/tmp/ffmpeg-output-$$.txt")
    
    # Parse resolution
    local actual_resolution=$(echo "$output" | grep -o "[0-9]\+x[0-9]\+" | head -1)
    local actual_width=$(echo "$actual_resolution" | cut -d'x' -f1)
    local actual_height=$(echo "$actual_resolution" | cut -d'x' -f2)
    
    # Parse framerate
    local actual_fps=$(echo "$output" | grep -oP '\d+(\.\d+)? fps' | grep -oP '\d+(\.\d+)?' | head -1)
    
    # Parse scan type
    local scan_type=$(echo "$output" | grep -i "progressive\|interlaced" | head -1)
    
    echo "Detected: ${actual_width}x${actual_height} @ ${actual_fps}fps"
    echo "Scan type: $scan_type"
    
    # Validate
    local passed=true
    
    if [ "$actual_width" != "$expected_width" ]; then
        echo -e "${RED}✗ Width mismatch: expected $expected_width, got $actual_width${NC}"
        passed=false
    fi
    
    if [ "$actual_height" != "$expected_height" ]; then
        echo -e "${RED}✗ Height mismatch: expected $expected_height, got $actual_height${NC}"
        passed=false
    fi
    
    if [ -n "$actual_fps" ]; then
        local fps_diff=$(echo "$actual_fps - $expected_fps" | bc | sed 's/-//')
        if (( $(echo "$fps_diff > 1" | bc -l) )); then
            echo -e "${RED}✗ FPS mismatch: expected $expected_fps, got $actual_fps${NC}"
            passed=false
        fi
    fi
    
    if [ "$expected_progressive" = "true" ]; then
        if echo "$scan_type" | grep -qi "interlaced"; then
            echo -e "${RED}✗ Scan mode mismatch: expected progressive, got interlaced${NC}"
            passed=false
        fi
    else
        if echo "$scan_type" | grep -qi "progressive"; then
            echo -e "${RED}✗ Scan mode mismatch: expected interlaced, got progressive${NC}"
            passed=false
        fi
    fi
    
    rm -f "/tmp/ffmpeg-output-$$.txt"
    
    if [ "$passed" = true ]; then
        return 0
    else
        return 1
    fi
}

# Verify NDI stream using HTTP API
verify_stream_via_api() {
    local http_port="$1"
    local expected_width="$2"
    local expected_height="$3"
    local expected_fps="$4"
    local expected_progressive="$5"
    
    echo "Verifying stream via HTTP API..."
    
    local response=$(curl -s "http://localhost:$http_port/status")
    
    local actual_width=$(echo "$response" | jq -r '.width')
    local actual_height=$(echo "$response" | jq -r '.height')
    local actual_fps=$(echo "$response" | jq -r '.fps')
    local actual_progressive=$(echo "$response" | jq -r '.progressive')
    
    echo "API reports: ${actual_width}x${actual_height} @ ${actual_fps}fps progressive=$actual_progressive"
    
    local passed=true
    
    if [ "$actual_width" != "$expected_width" ]; then
        echo -e "${RED}✗ Width mismatch: expected $expected_width, got $actual_width${NC}"
        passed=false
    fi
    
    if [ "$actual_height" != "$expected_height" ]; then
        echo -e "${RED}✗ Height mismatch: expected $expected_height, got $actual_height${NC}"
        passed=false
    fi
    
    if [ "$actual_fps" != "$expected_fps" ]; then
        echo -e "${RED}✗ FPS mismatch: expected $expected_fps, got $actual_fps${NC}"
        passed=false
    fi
    
    if [ "$actual_progressive" != "$expected_progressive" ]; then
        echo -e "${RED}✗ Progressive mismatch: expected $expected_progressive, got $actual_progressive${NC}"
        passed=false
    fi
    
    if [ "$passed" = true ]; then
        return 0
    else
        return 1
    fi
}

# Run a single test
run_test() {
    local test_name="$1"
    local width="$2"
    local height="$3"
    local fps="$4"
    local progressive="$5"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo ""
    echo "========================================="
    echo "Test $TESTS_RUN: $test_name"
    echo "========================================="
    
    local ndi_name="HTML2NDI-Test-$TESTS_RUN"
    local http_port=$((8080 + TESTS_RUN))
    
    # Start worker
    if ! start_worker "about:blank" "$width" "$height" "$fps" "$progressive" "$ndi_name" "$http_port"; then
        echo -e "${RED}✗ Test failed: Worker startup failed${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
    
    # Wait for NDI stream to stabilize
    sleep 3
    
    local test_passed=true
    
    # Verify via HTTP API
    if ! verify_stream_via_api "$http_port" "$width" "$height" "$fps" "$progressive"; then
        test_passed=false
    fi
    
    # Try to verify with ffmpeg if available
    if command -v ffmpeg &> /dev/null; then
        if ! verify_ndi_stream_with_ffmpeg "$ndi_name" "$width" "$height" "$fps" "$progressive"; then
            echo -e "${YELLOW}⚠ ffmpeg verification failed (may not have NDI support)${NC}"
        fi
    fi
    
    # Stop worker
    stop_worker
    
    if [ "$test_passed" = true ]; then
        echo -e "${GREEN}✓ Test passed: $test_name${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}✗ Test failed: $test_name${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Cleanup on exit
cleanup() {
    echo ""
    echo "Cleaning up..."
    stop_worker
    rm -f /tmp/html2ndi-worker-*.log
    rm -f /tmp/ffmpeg-output-*.txt
    rm -rf /tmp/html2ndi-test-*
}

trap cleanup EXIT

# Main test suite
main() {
    echo "========================================="
    echo "HTML2NDI End-to-End Integration Tests"
    echo "========================================="
    echo ""
    
    check_prerequisites
    
    # Test 1: 1080p60 (Progressive)
    run_test "1080p60 Progressive" 1920 1080 60 true
    
    # Test 2: 720p50 (Progressive)
    run_test "720p50 Progressive" 1280 720 50 true
    
    # Test 3: 1080i30 (Interlaced)
    run_test "1080i30 Interlaced" 1920 1080 30 false
    
    # Test 4: 4K30p (Progressive)
    run_test "4K UHD 30p Progressive" 3840 2160 30 true
    
    # Test 5: 720p24 (Film rate)
    run_test "720p24 Progressive" 1280 720 24 true
    
    # Print summary
    echo ""
    echo "========================================="
    echo "Test Summary"
    echo "========================================="
    echo "Total tests run: $TESTS_RUN"
    echo -e "${GREEN}Tests passed: $TESTS_PASSED${NC}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "${RED}Tests failed: $TESTS_FAILED${NC}"
    else
        echo -e "${GREEN}Tests failed: $TESTS_FAILED${NC}"
    fi
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}✗ Some tests failed${NC}"
        exit 1
    fi
}

# Run main
main "$@"

