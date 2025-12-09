#!/bin/bash
# Simplified integration tests for CI/CD
# Tests worker startup and HTTP API without requiring NDI network

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORKER_BIN="$PROJECT_ROOT/build/bin/html2ndi.app/Contents/MacOS/html2ndi"

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

echo "========================================="
echo "HTML2NDI CI Integration Tests"
echo "========================================="
echo ""

# Check if worker exists
if [ ! -f "$WORKER_BIN" ]; then
    echo -e "${RED}ERROR: Worker binary not found at $WORKER_BIN${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Worker binary found${NC}"

# Test 1: Worker starts successfully
echo ""
echo "Test 1: Worker startup"
echo "-----------------------"
TESTS_RUN=$((TESTS_RUN + 1))

"$WORKER_BIN" \
    --url "about:blank" \
    --width 1920 \
    --height 1080 \
    --fps 60 \
    --ndi-name "CI-Test" \
    --http-port 8099 \
    --cache-path "/tmp/ci-test-$$" \
    > /tmp/ci-worker-$$.log 2>&1 &

WORKER_PID=$!
echo "Worker PID: $WORKER_PID"

# Wait for startup
sleep 5

if ps -p $WORKER_PID > /dev/null; then
    echo -e "${GREEN}✓ Worker process running${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}✗ Worker process died${NC}"
    cat /tmp/ci-worker-$$.log
    TESTS_FAILED=$((TESTS_FAILED + 1))
    exit 1
fi

# Test 2: HTTP API responds
echo ""
echo "Test 2: HTTP API response"
echo "-------------------------"
TESTS_RUN=$((TESTS_RUN + 1))

if curl -s --max-time 5 "http://localhost:8099/status" > /dev/null; then
    echo -e "${GREEN}✓ HTTP API responding${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}✗ HTTP API not responding${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Test 3: Status endpoint returns correct format
echo ""
echo "Test 3: Status endpoint format"
echo "-------------------------------"
TESTS_RUN=$((TESTS_RUN + 1))

STATUS=$(curl -s "http://localhost:8099/status")
echo "Response: $STATUS"

if echo "$STATUS" | grep -q '"width"' && \
   echo "$STATUS" | grep -q '"height"' && \
   echo "$STATUS" | grep -q '"fps"' && \
   echo "$STATUS" | grep -q '"progressive"'; then
    echo -e "${GREEN}✓ Status format correct${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}✗ Status format incorrect${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Test 4: Progressive field is boolean
echo ""
echo "Test 4: Progressive field validation"
echo "-------------------------------------"
TESTS_RUN=$((TESTS_RUN + 1))

if command -v jq > /dev/null; then
    PROGRESSIVE=$(echo "$STATUS" | jq -r '.progressive')
    if [ "$PROGRESSIVE" = "true" ] || [ "$PROGRESSIVE" = "false" ]; then
        echo -e "${GREEN}✓ Progressive field is boolean: $PROGRESSIVE${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ Progressive field invalid: $PROGRESSIVE${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
else
    echo -e "${YELLOW}⚠ jq not available, skipping validation${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Cleanup
echo ""
echo "Cleaning up..."
kill $WORKER_PID 2>/dev/null || true
wait $WORKER_PID 2>/dev/null || true
rm -rf "/tmp/ci-test-$$"
rm -f "/tmp/ci-worker-$$.log"

# Summary
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "Total tests: $TESTS_RUN"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
else
    echo -e "${GREEN}Failed: $TESTS_FAILED${NC}"
fi
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All CI tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi

