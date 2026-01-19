#!/bin/bash
# Performance monitoring test script
# Tests for memory leaks and performance degradation over time

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
WORKER="$BUILD_DIR/bin/html2ndi.app/Contents/MacOS/html2ndi"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test configuration
TEST_DURATION_MINUTES=15
SAMPLE_INTERVAL_SECONDS=30
HTTP_PORT=8090

echo "HTML2NDI Performance Test"
echo "=========================="
echo "Duration: $TEST_DURATION_MINUTES minutes"
echo "Sample interval: $SAMPLE_INTERVAL_SECONDS seconds"
echo ""

# Check if worker exists
if [ ! -f "$WORKER" ]; then
    echo -e "${RED}Error: Worker not found at $WORKER${NC}"
    echo "Please build the project first: cd build && ninja"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$PROJECT_ROOT/test-results/performance-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUTPUT_DIR"

echo "Output directory: $OUTPUT_DIR"
echo ""

# Start worker with test card
echo "Starting html2ndi worker..."
"$WORKER" \
    --url "http://localhost:$HTTP_PORT/testcard" \
    --ndi-name "Performance-Test" \
    --width 1920 \
    --height 1080 \
    --fps 60 \
    --http-port $HTTP_PORT \
    --cache-path "/tmp/html2ndi-perf-test" \
    > "$OUTPUT_DIR/worker.log" 2>&1 &

WORKER_PID=$!
echo "Worker PID: $WORKER_PID"

# Wait for worker to start
sleep 5

# Check if worker is running
if ! kill -0 $WORKER_PID 2>/dev/null; then
    echo -e "${RED}Error: Worker failed to start${NC}"
    cat "$OUTPUT_DIR/worker.log"
    exit 1
fi

echo -e "${GREEN}Worker started successfully${NC}"
echo ""

# Monitoring function
monitor_performance() {
    local sample_num=$1
    local timestamp=$(date +%s)
    
    # Get process stats
    local ps_output=$(ps -p $WORKER_PID -o %cpu,%mem,rss,vsz 2>/dev/null || echo "0 0 0 0")
    local cpu=$(echo $ps_output | awk '{print $1}')
    local mem_pct=$(echo $ps_output | awk '{print $2}')
    local rss=$(echo $ps_output | awk '{print $3}')
    local vsz=$(echo $ps_output | awk '{print $4}')
    
    # Get HTTP status
    local status_json=$(curl -s http://localhost:$HTTP_PORT/status 2>/dev/null || echo "{}")
    local actual_fps=$(echo $status_json | jq -r '.actual_fps // 0' 2>/dev/null || echo "0")
    local frames_sent=$(echo $status_json | jq -r '.frames.sent // 0' 2>/dev/null || echo "0")
    local frames_dropped=$(echo $status_json | jq -r '.frames.dropped // 0' 2>/dev/null || echo "0")
    local buffer_size=$(echo $status_json | jq -r '.performance.buffer_size_bytes // 0' 2>/dev/null || echo "0")
    local submit_time=$(echo $status_json | jq -r '.performance.avg_submit_time_us // 0' 2>/dev/null || echo "0")
    local memcpy_time=$(echo $status_json | jq -r '.performance.avg_memcpy_time_us // 0' 2>/dev/null || echo "0")
    local resident_mb=$(echo $status_json | jq -r '.memory.resident_size_mb // 0' 2>/dev/null || echo "0")
    
    # Write to CSV
    echo "$timestamp,$sample_num,$cpu,$mem_pct,$rss,$vsz,$actual_fps,$frames_sent,$frames_dropped,$buffer_size,$submit_time,$memcpy_time,$resident_mb" >> "$OUTPUT_DIR/metrics.csv"
    
    # Display progress
    printf "[%3d] FPS: %5.1f | RSS: %7d KB | CPU: %5.1f%% | Submit: %6.1f us | Memcpy: %6.1f us\n" \
        $sample_num "$actual_fps" $rss "$cpu" "$submit_time" "$memcpy_time"
}

# Create CSV header
echo "timestamp,sample,cpu_pct,mem_pct,rss_kb,vsz_kb,actual_fps,frames_sent,frames_dropped,buffer_bytes,submit_us,memcpy_us,resident_mb" > "$OUTPUT_DIR/metrics.csv"

# Monitor loop
echo "Monitoring performance..."
echo ""

TOTAL_SAMPLES=$((TEST_DURATION_MINUTES * 60 / SAMPLE_INTERVAL_SECONDS))
for i in $(seq 1 $TOTAL_SAMPLES); do
    if ! kill -0 $WORKER_PID 2>/dev/null; then
        echo -e "${RED}Error: Worker process died${NC}"
        break
    fi
    
    monitor_performance $i
    sleep $SAMPLE_INTERVAL_SECONDS
done

echo ""
echo "Stopping worker..."
kill $WORKER_PID 2>/dev/null || true
wait $WORKER_PID 2>/dev/null || true

# Analyze results
echo ""
echo "Analyzing results..."
echo ""

# Calculate statistics
INITIAL_RSS=$(head -2 "$OUTPUT_DIR/metrics.csv" | tail -1 | cut -d',' -f5)
FINAL_RSS=$(tail -1 "$OUTPUT_DIR/metrics.csv" | cut -d',' -f5)
RSS_GROWTH=$((FINAL_RSS - INITIAL_RSS))
RSS_GROWTH_MB=$((RSS_GROWTH / 1024))

AVG_FPS=$(awk -F',' 'NR>1 {sum+=$7; count++} END {printf "%.1f", sum/count}' "$OUTPUT_DIR/metrics.csv")
MIN_FPS=$(awk -F',' 'NR>1 {if(min==""){min=$7}; if($7<min){min=$7}} END {printf "%.1f", min}' "$OUTPUT_DIR/metrics.csv")
MAX_FPS=$(awk -F',' 'NR>1 {if(max==""){max=$7}; if($7>max){max=$7}} END {printf "%.1f", max}' "$OUTPUT_DIR/metrics.csv")

TOTAL_DROPPED=$(tail -1 "$OUTPUT_DIR/metrics.csv" | cut -d',' -f9)

echo "Performance Summary"
echo "==================="
echo "Test duration: $TEST_DURATION_MINUTES minutes"
echo "Samples collected: $TOTAL_SAMPLES"
echo ""
echo "Memory:"
echo "  Initial RSS: $((INITIAL_RSS / 1024)) MB"
echo "  Final RSS: $((FINAL_RSS / 1024)) MB"
echo "  Growth: $RSS_GROWTH_MB MB"
echo ""
echo "Frame Rate:"
echo "  Average: $AVG_FPS fps"
echo "  Min: $MIN_FPS fps"
echo "  Max: $MAX_FPS fps"
echo "  Dropped frames: $TOTAL_DROPPED"
echo ""

# Determine pass/fail
PASS=true

if [ $RSS_GROWTH_MB -gt 50 ]; then
    echo -e "${RED}FAIL: Excessive memory growth (${RSS_GROWTH_MB} MB)${NC}"
    PASS=false
fi

if (( $(echo "$AVG_FPS < 55" | bc -l) )); then
    echo -e "${RED}FAIL: Low average FPS (${AVG_FPS})${NC}"
    PASS=false
fi

if [ $TOTAL_DROPPED -gt 100 ]; then
    echo -e "${YELLOW}WARNING: High dropped frame count (${TOTAL_DROPPED})${NC}"
fi

if [ "$PASS" = true ]; then
    echo -e "${GREEN}PASS: Performance is stable${NC}"
    exit 0
else
    echo -e "${RED}FAIL: Performance issues detected${NC}"
    echo "See detailed metrics in: $OUTPUT_DIR/metrics.csv"
    exit 1
fi
