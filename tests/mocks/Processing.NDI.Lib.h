/**
 * Mock NDI SDK Header for Local Testing
 * 
 * This is a simplified mock of the NDI SDK that matches the production API.
 * Use this for local compilation and testing when the full NDI SDK is not available.
 * 
 * IMPORTANT: This must match the actual NDI SDK API signatures to catch type errors.
 */

#ifndef PROCESSING_NDI_LIB_H
#define PROCESSING_NDI_LIB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef void* NDIlib_send_instance_t;
typedef void* NDIlib_recv_instance_t;

// Constants
#define NDIlib_send_timecode_synthesize 0x7FFFFFFFFFFFFFFFLL

// Video formats
typedef enum {
    NDIlib_frame_format_type_progressive = 1,
    NDIlib_frame_format_type_interleaved = 0,
    NDIlib_frame_format_type_field_0 = 2,
    NDIlib_frame_format_type_field_1 = 3
} NDIlib_frame_format_type_e;

// FourCC codes
typedef enum {
    NDIlib_FourCC_video_type_BGRX = 0x58524742,  // 'BGRX'
    NDIlib_FourCC_video_type_BGRA = 0x41524742,  // 'BGRA'
    NDIlib_FourCC_video_type_RGBA = 0x41424752,  // 'RGBA'
    NDIlib_FourCC_video_type_RGBX = 0x58424752   // 'RGBX'
} NDIlib_FourCC_video_type_e;

// Send creation structure
typedef struct {
    const char* p_ndi_name;
    const char* p_groups;
    bool clock_video;
    bool clock_audio;
} NDIlib_send_create_t;

// Video frame structure (v2)
typedef struct {
    int xres;
    int yres;
    NDIlib_FourCC_video_type_e FourCC;
    int frame_rate_N;
    int frame_rate_D;
    float picture_aspect_ratio;
    NDIlib_frame_format_type_e frame_format_type;
    int64_t timecode;
    uint8_t* p_data;
    int line_stride_in_bytes;
    const char* p_metadata;
    int64_t timestamp;
} NDIlib_video_frame_v2_t;

// Audio frame structure (v3)
// CRITICAL: p_data is uint8_t* in the real SDK, not float*
typedef struct {
    int sample_rate;
    int no_channels;
    int no_samples;
    int64_t timecode;
    int channel_stride_in_bytes;
    uint8_t* p_data;  // <- This matches production SDK
    const char* p_metadata;
    int64_t timestamp;
} NDIlib_audio_frame_v3_t;

// Tally structures
typedef struct {
    bool on_program;
    bool on_preview;
} NDIlib_tally_t;

// Source structure
typedef struct {
    const char* p_ndi_name;
    const char* p_url_address;
} NDIlib_source_t;

// Library functions
bool NDIlib_initialize(void);
void NDIlib_destroy(void);
const char* NDIlib_version(void);

// Send functions
NDIlib_send_instance_t NDIlib_send_create(const NDIlib_send_create_t* p_create_settings);
void NDIlib_send_destroy(NDIlib_send_instance_t p_instance);
void NDIlib_send_send_video_v2(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
void NDIlib_send_send_audio_v3(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
int NDIlib_send_get_no_connections(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms);
const NDIlib_source_t* NDIlib_send_get_source_name(NDIlib_send_instance_t p_instance);
bool NDIlib_send_get_tally(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms);

#ifdef __cplusplus
}
#endif

#endif // PROCESSING_NDI_LIB_H
