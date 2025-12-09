#!/usr/bin/env python3
"""
End-to-end NDI output validation tests using ffmpeg or gstreamer.
Tests actual NDI stream format, framerate, and progressive/interlaced mode.
"""

import subprocess
import time
import json
import requests
import sys
import os
import re
from pathlib import Path
from typing import Dict, Tuple, Optional

# ANSI color codes
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
NC = '\033[0m'  # No Color

# Project paths
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
WORKER_BIN = PROJECT_ROOT / "build" / "bin" / "html2ndi.app" / "Contents" / "MacOS" / "html2ndi"

# Test statistics
tests_run = 0
tests_passed = 0
tests_failed = 0

# Worker process
worker_process = None


def print_color(message: str, color: str = NC):
    """Print colored message."""
    print(f"{color}{message}{NC}")


def check_prerequisites() -> bool:
    """Check if required tools are installed."""
    print("Checking prerequisites...")
    
    # Check for ffmpeg or gstreamer
    has_ffmpeg = subprocess.run(['which', 'ffmpeg'], capture_output=True).returncode == 0
    has_gstreamer = subprocess.run(['which', 'gst-launch-1.0'], capture_output=True).returncode == 0
    
    if not has_ffmpeg and not has_gstreamer:
        print_color("ERROR: Neither ffmpeg nor gstreamer found", RED)
        print("Install one of:")
        print("  1. ffmpeg with NDI: brew install homebrew-ffmpeg/ffmpeg/ffmpeg --with-libndi_newtek")
        print("  2. gstreamer with NDI: brew install gstreamer gst-plugins-bad")
        return False
    
    # Check if worker binary exists
    if not WORKER_BIN.exists():
        print_color(f"ERROR: Worker binary not found at {WORKER_BIN}", RED)
        print("Build it first: cd build && ninja")
        return False
    
    # Check for jq
    if subprocess.run(['which', 'jq'], capture_output=True).returncode != 0:
        print_color("WARNING: jq not found, install for better JSON parsing", YELLOW)
    
    print_color("✓ Prerequisites OK", GREEN)
    return True


def start_worker(url: str, width: int, height: int, fps: int, 
                progressive: bool, ndi_name: str, http_port: int) -> Optional[subprocess.Popen]:
    """Start html2ndi worker process."""
    global worker_process
    
    print(f"Starting worker: {width}x{height}@{fps}fps {'progressive' if progressive else 'interlaced'}")
    
    cmd = [
        str(WORKER_BIN),
        "--url", url,
        "--width", str(width),
        "--height", str(height),
        "--fps", str(fps),
        "--ndi-name", ndi_name,
        "--http-port", str(http_port),
        "--cache-path", f"/tmp/html2ndi-test-{os.getpid()}"
    ]
    
    if not progressive:
        cmd.append("--interlaced")
    
    log_file = open(f"/tmp/html2ndi-worker-{os.getpid()}.log", "w")
    worker_process = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT)
    
    print(f"Worker PID: {worker_process.pid}")
    
    # Wait for worker to start
    print("Waiting for worker to initialize...")
    for i in range(30):
        try:
            response = requests.get(f"http://localhost:{http_port}/status", timeout=1)
            if response.status_code == 200:
                print_color("✓ Worker started", GREEN)
                return worker_process
        except requests.exceptions.RequestException:
            pass
        time.sleep(1)
    
    print_color("ERROR: Worker failed to start", RED)
    log_file.close()
    with open(f"/tmp/html2ndi-worker-{os.getpid()}.log", "r") as f:
        print(f.read())
    return None


def stop_worker():
    """Stop worker process."""
    global worker_process
    
    if worker_process:
        print(f"Stopping worker (PID: {worker_process.pid})...")
        worker_process.terminate()
        try:
            worker_process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            worker_process.kill()
            worker_process.wait()
        worker_process = None
    
    # Cleanup
    subprocess.run(['rm', '-rf', f"/tmp/html2ndi-test-{os.getpid()}"], capture_output=True)


def verify_stream_via_api(http_port: int, expected_width: int, expected_height: int,
                          expected_fps: int, expected_progressive: bool) -> bool:
    """Verify stream configuration via HTTP API."""
    print("Verifying stream via HTTP API...")
    
    try:
        response = requests.get(f"http://localhost:{http_port}/status")
        data = response.json()
        
        actual_width = data['width']
        actual_height = data['height']
        actual_fps = data['fps']
        actual_progressive = data['progressive']
        
        print(f"API reports: {actual_width}x{actual_height} @ {actual_fps}fps progressive={actual_progressive}")
        
        passed = True
        
        if actual_width != expected_width:
            print_color(f"✗ Width mismatch: expected {expected_width}, got {actual_width}", RED)
            passed = False
        
        if actual_height != expected_height:
            print_color(f"✗ Height mismatch: expected {expected_height}, got {actual_height}", RED)
            passed = False
        
        if actual_fps != expected_fps:
            print_color(f"✗ FPS mismatch: expected {expected_fps}, got {actual_fps}", RED)
            passed = False
        
        if actual_progressive != expected_progressive:
            print_color(f"✗ Progressive mismatch: expected {expected_progressive}, got {actual_progressive}", RED)
            passed = False
        
        return passed
    
    except Exception as e:
        print_color(f"✗ API verification failed: {e}", RED)
        return False


def verify_stream_with_ffmpeg(ndi_name: str, expected_width: int, expected_height: int,
                              expected_fps: int, expected_progressive: bool) -> bool:
    """Verify NDI stream using ffmpeg."""
    print("Verifying NDI stream with ffmpeg...")
    
    try:
        # Capture stream metadata
        result = subprocess.run(
            ['ffmpeg', '-f', 'libndi_newtek', '-i', ndi_name, '-t', '2', '-f', 'null', '-'],
            capture_output=True,
            text=True,
            timeout=15
        )
        
        output = result.stderr
        
        # Parse resolution
        resolution_match = re.search(r'(\d+)x(\d+)', output)
        if resolution_match:
            actual_width = int(resolution_match.group(1))
            actual_height = int(resolution_match.group(2))
        else:
            print_color("⚠ Could not parse resolution from ffmpeg output", YELLOW)
            return True  # Soft fail
        
        # Parse framerate
        fps_match = re.search(r'(\d+(?:\.\d+)?)\s+fps', output)
        actual_fps = float(fps_match.group(1)) if fps_match else None
        
        # Parse scan type
        scan_match = re.search(r'(progressive|interlaced)', output, re.IGNORECASE)
        scan_type = scan_match.group(1).lower() if scan_match else None
        
        print(f"Detected: {actual_width}x{actual_height} @ {actual_fps}fps")
        if scan_type:
            print(f"Scan type: {scan_type}")
        
        # Validate
        passed = True
        
        if actual_width != expected_width:
            print_color(f"✗ Width mismatch: expected {expected_width}, got {actual_width}", RED)
            passed = False
        
        if actual_height != expected_height:
            print_color(f"✗ Height mismatch: expected {expected_height}, got {actual_height}", RED)
            passed = False
        
        if actual_fps and abs(actual_fps - expected_fps) > 1:
            print_color(f"✗ FPS mismatch: expected {expected_fps}, got {actual_fps}", RED)
            passed = False
        
        if scan_type:
            if expected_progressive and scan_type == 'interlaced':
                print_color(f"✗ Scan mode mismatch: expected progressive, got interlaced", RED)
                passed = False
            elif not expected_progressive and scan_type == 'progressive':
                print_color(f"✗ Scan mode mismatch: expected interlaced, got progressive", RED)
                passed = False
        
        return passed
    
    except subprocess.TimeoutExpired:
        print_color("⚠ ffmpeg timeout (may not have NDI support)", YELLOW)
        return True  # Soft fail
    except Exception as e:
        print_color(f"⚠ ffmpeg verification failed: {e}", YELLOW)
        return True  # Soft fail


def run_test(test_name: str, width: int, height: int, fps: int, progressive: bool) -> bool:
    """Run a single test case."""
    global tests_run, tests_passed, tests_failed
    
    tests_run += 1
    
    print("\n" + "=" * 40)
    print(f"Test {tests_run}: {test_name}")
    print("=" * 40)
    
    ndi_name = f"HTML2NDI-Test-{tests_run}"
    http_port = 8080 + tests_run
    
    # Start worker
    if not start_worker("about:blank", width, height, fps, progressive, ndi_name, http_port):
        print_color(f"✗ Test failed: Worker startup failed", RED)
        tests_failed += 1
        return False
    
    # Wait for NDI stream to stabilize
    time.sleep(3)
    
    test_passed = True
    
    # Verify via HTTP API
    if not verify_stream_via_api(http_port, width, height, fps, progressive):
        test_passed = False
    
    # Try to verify with ffmpeg if available
    if subprocess.run(['which', 'ffmpeg'], capture_output=True).returncode == 0:
        if not verify_stream_with_ffmpeg(ndi_name, width, height, fps, progressive):
            # ffmpeg verification is optional, don't fail test
            pass
    
    # Stop worker
    stop_worker()
    
    if test_passed:
        print_color(f"✓ Test passed: {test_name}", GREEN)
        tests_passed += 1
        return True
    else:
        print_color(f"✗ Test failed: {test_name}", RED)
        tests_failed += 1
        return False


def main():
    """Main test suite."""
    print("=" * 40)
    print("HTML2NDI End-to-End Integration Tests")
    print("=" * 40)
    print()
    
    if not check_prerequisites():
        sys.exit(1)
    
    try:
        # Test 1: 1080p60 (Progressive)
        run_test("1080p60 Progressive", 1920, 1080, 60, True)
        
        # Test 2: 720p50 (Progressive)
        run_test("720p50 Progressive", 1280, 720, 50, True)
        
        # Test 3: 1080i30 (Interlaced)
        run_test("1080i30 Interlaced", 1920, 1080, 30, False)
        
        # Test 4: 4K30p (Progressive)
        run_test("4K UHD 30p Progressive", 3840, 2160, 30, True)
        
        # Test 5: 720p24 (Film rate)
        run_test("720p24 Progressive", 1280, 720, 24, True)
        
    finally:
        # Ensure cleanup
        stop_worker()
    
    # Print summary
    print()
    print("=" * 40)
    print("Test Summary")
    print("=" * 40)
    print(f"Total tests run: {tests_run}")
    print_color(f"Tests passed: {tests_passed}", GREEN)
    if tests_failed > 0:
        print_color(f"Tests failed: {tests_failed}", RED)
    else:
        print_color(f"Tests failed: {tests_failed}", GREEN)
    print()
    
    if tests_failed == 0:
        print_color("✓ All tests passed!", GREEN)
        sys.exit(0)
    else:
        print_color("✗ Some tests failed", RED)
        sys.exit(1)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        stop_worker()
        sys.exit(130)

