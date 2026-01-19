/**
 * Mock NDI SDK Implementation for Testing
 * 
 * Provides stub implementations of NDI functions for unit testing.
 */

#include "Processing.NDI.Lib.h"
#include <cstring>

// Mock state
static bool initialized = false;
static int mock_send_counter = 0;

bool NDIlib_initialize(void) {
    initialized = true;
    return true;
}

void NDIlib_destroy(void) {
    initialized = false;
}

const char* NDIlib_version(void) {
    return "Mock NDI SDK v5.0.0";
}

NDIlib_send_instance_t NDIlib_send_create(const NDIlib_send_create_t* p_create_settings) {
    if (!initialized || !p_create_settings) {
        return nullptr;
    }
    // Return a non-null pointer to indicate success
    return reinterpret_cast<NDIlib_send_instance_t>(0x1);
}

void NDIlib_send_destroy(NDIlib_send_instance_t p_instance) {
    // No-op in mock
}

void NDIlib_send_send_video_v2(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data) {
    if (p_instance && p_video_data) {
        mock_send_counter++;
    }
}

void NDIlib_send_send_audio_v3(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data) {
    if (p_instance && p_audio_data) {
        mock_send_counter++;
    }
}

int NDIlib_send_get_no_connections(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms) {
    return 1;  // Always return 1 connection in mock
}

static NDIlib_source_t mock_source = {
    "Mock NDI Source",
    "127.0.0.1:5960"
};

const NDIlib_source_t* NDIlib_send_get_source_name(NDIlib_send_instance_t p_instance) {
    return &mock_source;
}

bool NDIlib_send_get_tally(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms) {
    if (p_tally) {
        p_tally->on_program = false;
        p_tally->on_preview = false;
        return true;
    }
    return false;
}
