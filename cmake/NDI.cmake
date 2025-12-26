# NDI.cmake - Configure NDI SDK for macOS
#
# The NDI SDK must be installed separately from:
# https://ndi.video/for-developers/ndi-sdk/
#
# This module expects the NDI SDK to be installed at one of these locations:
# 1. /Library/NDI SDK for Apple/
# 2. ~/NDI SDK for Apple/
# 3. Environment variable NDI_SDK_DIR
# 4. CMake variable NDI_SDK_DIR

set(NDI_VERSION "5" CACHE STRING "NDI SDK version")

# Search for NDI SDK
set(NDI_SEARCH_PATHS
    "$ENV{NDI_SDK_DIR}"
    "${NDI_SDK_DIR}"
    "/Library/NDI SDK for Apple"
    "$ENV{HOME}/NDI SDK for Apple"
    "/usr/local/lib"
    "/opt/ndi"
)

# Find NDI include directory
find_path(NDI_INCLUDE_DIR
    NAMES "Processing.NDI.Lib.h"
    PATHS ${NDI_SEARCH_PATHS}
    PATH_SUFFIXES "include" "lib/macOS"
    DOC "NDI SDK include directory"
)

# Find NDI library
find_library(NDI_LIBRARY
    NAMES ndi libndi.dylib
    PATHS ${NDI_SEARCH_PATHS}
    PATH_SUFFIXES "lib/macOS" "lib/x64" "lib"
    DOC "NDI library"
)

# If not found, provide download instructions
if(NOT NDI_INCLUDE_DIR OR NOT NDI_LIBRARY)
    message(STATUS "")
    message(STATUS "=== NDI SDK Not Found ===")
    message(STATUS "")
    message(STATUS "The NDI SDK is required but was not found.")
    message(STATUS "Please download and install it from:")
    message(STATUS "  https://ndi.video/for-developers/ndi-sdk/")
    message(STATUS "")
    message(STATUS "After installation, either:")
    message(STATUS "  1. Install to /Library/NDI SDK for Apple/")
    message(STATUS "  2. Set NDI_SDK_DIR environment variable")
    message(STATUS "  3. Pass -DNDI_SDK_DIR=/path/to/sdk to cmake")
    message(STATUS "")
    
    # Create placeholder for development
    message(WARNING "Creating NDI stub for development. Real SDK required for runtime.")
    
    set(NDI_STUB_DIR "${CMAKE_BINARY_DIR}/ndi_stub")
    file(MAKE_DIRECTORY "${NDI_STUB_DIR}/include")
    
    # Create minimal NDI header stub
    file(WRITE "${NDI_STUB_DIR}/include/Processing.NDI.Lib.h" "
#pragma once
// NDI SDK Stub - Replace with real SDK for production

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern \"C\" {
#endif

// Version
#define NDI_LIB_VERSION 5

// Initialization
bool NDIlib_initialize(void);
void NDIlib_destroy(void);
const char* NDIlib_version(void);

// Send structures
typedef struct NDIlib_send_instance_type* NDIlib_send_instance_t;

typedef struct {
    const char* p_ndi_name;
    const char* p_groups;
    bool clock_video;
    bool clock_audio;
} NDIlib_send_create_t;

typedef enum {
    NDIlib_FourCC_video_type_RGBA = 0x41424752,
    NDIlib_FourCC_video_type_BGRA = 0x41524742,
    NDIlib_FourCC_video_type_BGRX = 0x58524742,
    NDIlib_FourCC_video_type_UYVY = 0x59565955,
} NDIlib_FourCC_video_type_e;

// Timecode constants
#define NDIlib_send_timecode_synthesize INT64_C(0x7FFFFFFFFFFFFFFF)

typedef enum {
    NDIlib_frame_format_type_progressive = 1,
    NDIlib_frame_format_type_interleaved = 0,
    NDIlib_frame_format_type_field_0 = 2,
    NDIlib_frame_format_type_field_1 = 3,
} NDIlib_frame_format_type_e;

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

typedef struct {
    int sample_rate;
    int no_channels;
    int no_samples;
    int64_t timecode;
    int channel_stride_in_bytes;
    float* p_data;
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
")
    
    set(NDI_INCLUDE_DIR "${NDI_STUB_DIR}/include")
    set(NDI_LIBRARY "")
    set(NDI_LIBRARIES "")
else()
    set(NDI_LIBRARIES "${NDI_LIBRARY}")
    message(STATUS "NDI SDK found: ${NDI_INCLUDE_DIR}")
    message(STATUS "NDI library: ${NDI_LIBRARY}")
endif()

# Mark as advanced
mark_as_advanced(NDI_INCLUDE_DIR NDI_LIBRARY)

