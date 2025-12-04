#!/bin/bash
#
# Test Script: CEF Initialization
# Validates that the application can initialize CEF properly
#

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
APP_BUNDLE="$BUILD_DIR/bin/html2ndi.app"

echo "=== HTML2NDI CEF Initialization Test ==="
echo ""

# Test 1: Check bundle structure exists
test_bundle_structure() {
    echo "Test 1: Bundle Structure"
    
    local errors=0
    
    # Check main bundle
    if [[ ! -d "$APP_BUNDLE" ]]; then
        echo "  ❌ FAIL: Bundle not found at $APP_BUNDLE"
        return 1
    fi
    echo "  ✅ Bundle exists"
    
    # Check Contents directories
    for dir in MacOS Frameworks Resources; do
        if [[ ! -d "$APP_BUNDLE/Contents/$dir" ]]; then
            echo "  ❌ FAIL: Missing Contents/$dir"
            ((errors++))
        else
            echo "  ✅ Contents/$dir exists"
        fi
    done
    
    # Check main executable
    if [[ ! -x "$APP_BUNDLE/Contents/MacOS/html2ndi" ]]; then
        echo "  ❌ FAIL: Main executable not found or not executable"
        ((errors++))
    else
        echo "  ✅ Main executable exists"
    fi
    
    # Check CEF framework
    if [[ ! -d "$APP_BUNDLE/Contents/Frameworks/Chromium Embedded Framework.framework" ]]; then
        echo "  ❌ FAIL: CEF framework not found"
        ((errors++))
    else
        echo "  ✅ CEF framework exists"
    fi
    
    # Check helper apps
    local helper_suffixes=("" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)")
    for suffix in "${helper_suffixes[@]}"; do
        local helper_name="html2ndi Helper${suffix}.app"
        if [[ ! -d "$APP_BUNDLE/Contents/Frameworks/$helper_name" ]]; then
            echo "  ❌ FAIL: Helper app not found: $helper_name"
            ((errors++))
        else
            echo "  ✅ Helper app exists: $helper_name"
        fi
    done
    
    # Check CEF resources
    local resources=("icudtl.dat" "resources.pak" "chrome_100_percent.pak")
    for res in "${resources[@]}"; do
        if [[ ! -f "$APP_BUNDLE/Contents/Resources/$res" ]]; then
            echo "  ❌ FAIL: Resource not found: $res"
            ((errors++))
        else
            echo "  ✅ Resource exists: $res"
        fi
    done
    
    if [[ $errors -gt 0 ]]; then
        return 1
    fi
    return 0
}

# Test 2: Check CLI works
test_cli() {
    echo ""
    echo "Test 2: CLI Parsing"
    
    local output
    output=$("$APP_BUNDLE/Contents/MacOS/html2ndi" --help 2>&1 || true)
    
    if echo "$output" | grep -q "HTML2NDI - Render HTML"; then
        echo "  ✅ Help message displays correctly"
    else
        echo "  ❌ FAIL: Help message not found"
        return 1
    fi
    
    if echo "$output" | grep -q "\-\-url"; then
        echo "  ✅ URL option present"
    else
        echo "  ❌ FAIL: URL option not found"
        return 1
    fi
    
    return 0
}

# Test 3: NDI initialization
test_ndi_init() {
    echo ""
    echo "Test 3: NDI Initialization"
    
    local output
    # Run with timeout and capture output (will crash at CEF, but NDI should init first)
    # Use perl for timeout since macOS doesn't have timeout command
    output=$(perl -e 'alarm 3; exec @ARGV' "$APP_BUNDLE/Contents/MacOS/html2ndi" \
        --url "about:blank" \
        --no-http \
        --ndi-name "TestSource" 2>&1 || true)
    
    if echo "$output" | grep -q "NDI sender created"; then
        echo "  ✅ NDI initializes successfully"
        return 0
    else
        echo "  ❌ FAIL: NDI initialization failed"
        echo "  Output: $output"
        return 1
    fi
}

# Test 4: CEF initialization (the critical test)
test_cef_init() {
    echo ""
    echo "Test 4: CEF Initialization"
    
    local output
    # Run with timeout and capture output
    # Use perl for timeout since macOS doesn't have timeout command
    output=$(perl -e 'alarm 5; exec @ARGV' "$APP_BUNDLE/Contents/MacOS/html2ndi" \
        --url "about:blank" \
        --no-http \
        --verbose 2>&1 || true)
    
    # Check for successful CEF initialization
    if echo "$output" | grep -q "CEF renderer initialized"; then
        echo "  ✅ CEF initializes successfully"
    else
        echo "  ❌ FAIL: CEF initialization failed"
        
        # Check for specific error patterns
        if echo "$output" | grep -q "icudtl.dat not found"; then
            echo "  → Error: ICU data file not found in expected location"
        fi
        if echo "$output" | grep -q "Failed to load CEF"; then
            echo "  → Error: CEF framework loading failed"
        fi
        if echo "$output" | grep -q "segmentation fault\|trace trap"; then
            echo "  → Error: Crash during initialization"
        fi
        
        echo ""
        echo "  Last 20 lines of output:"
        echo "$output" | tail -20 | sed 's/^/    /'
        return 1
    fi
    
    # Check if browser was created
    if echo "$output" | grep -q "Browser created"; then
        echo "  ✅ Browser created successfully"
    else
        echo "  ⚠️ Warning: Browser creation message not found"
    fi
    
    return 0
}

# Run all tests
run_tests() {
    local failed=0
    
    test_bundle_structure || ((failed++))
    test_cli || ((failed++))
    test_ndi_init || ((failed++))
    test_cef_init || ((failed++))
    
    echo ""
    echo "=== Test Summary ==="
    if [[ $failed -eq 0 ]]; then
        echo "✅ All tests passed!"
        return 0
    else
        echo "❌ $failed test(s) failed"
        return 1
    fi
}

# Main
run_tests

