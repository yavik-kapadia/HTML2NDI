/**
 * NDI sender implementation.
 */

#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/utils/logger.h"

#include <cstring>

namespace html2ndi {

NdiSender::NdiSender(const std::string& name, const std::string& groups)
    : name_(name)
    , groups_(groups) {
}

NdiSender::~NdiSender() {
    shutdown();
}

bool NdiSender::initialize() {
    if (initialized_) {
        return true;
    }
    
    LOG_DEBUG("Initializing NDI...");
    
    // Initialize NDI library
    if (!NDIlib_initialize()) {
        LOG_ERROR("Failed to initialize NDI library");
        LOG_ERROR("Make sure NDI runtime is installed");
        return false;
    }
    
    LOG_DEBUG("NDI library version: %s", NDIlib_version());
    
    // Create sender
    NDIlib_send_create_t create_desc;
    create_desc.p_ndi_name = name_.c_str();
    create_desc.p_groups = groups_.empty() ? nullptr : groups_.c_str();
    create_desc.clock_video = true;
    create_desc.clock_audio = true;
    
    sender_ = NDIlib_send_create(&create_desc);
    if (!sender_) {
        LOG_ERROR("Failed to create NDI sender");
        NDIlib_destroy();
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("NDI sender created: %s", name_.c_str());
    
    return true;
}

void NdiSender::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_DEBUG("Shutting down NDI sender...");
    
    if (sender_) {
        NDIlib_send_destroy(sender_);
        sender_ = nullptr;
    }
    
    NDIlib_destroy();
    
    initialized_ = false;
    LOG_DEBUG("NDI sender shutdown complete");
}

void NdiSender::send_video_frame(
    const uint8_t* data,
    int width,
    int height,
    int frame_rate_n,
    int frame_rate_d) {
    
    if (!initialized_ || !sender_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Setup video frame
    video_frame_.xres = width;
    video_frame_.yres = height;
    video_frame_.FourCC = NDIlib_FourCC_video_type_RGBA;
    video_frame_.frame_rate_N = frame_rate_n;
    video_frame_.frame_rate_D = frame_rate_d;
    video_frame_.picture_aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    video_frame_.frame_format_type = NDIlib_frame_format_type_progressive;
    video_frame_.timecode = NDIlib_send_timecode_synthesize;
    video_frame_.p_data = const_cast<uint8_t*>(data);
    video_frame_.line_stride_in_bytes = width * 4;
    video_frame_.p_metadata = nullptr;
    video_frame_.timestamp = 0;
    
    // Send frame
    NDIlib_send_send_video_v2(sender_, &video_frame_);
}

void NdiSender::send_audio_frame(
    const float* data,
    int sample_rate,
    int channels,
    int samples) {
    
    if (!initialized_ || !sender_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Setup audio frame
    audio_frame_.sample_rate = sample_rate;
    audio_frame_.no_channels = channels;
    audio_frame_.no_samples = samples;
    audio_frame_.timecode = NDIlib_send_timecode_synthesize;
    audio_frame_.p_data = reinterpret_cast<uint8_t*>(const_cast<float*>(data));
    audio_frame_.channel_stride_in_bytes = samples * sizeof(float);
    audio_frame_.p_metadata = nullptr;
    audio_frame_.timestamp = 0;
    
    // Send frame
    NDIlib_send_send_audio_v3(sender_, &audio_frame_);
}

int NdiSender::get_connection_count(uint32_t timeout_ms) const {
    if (!initialized_ || !sender_) {
        return 0;
    }
    
    return NDIlib_send_get_no_connections(sender_, timeout_ms);
}

} // namespace html2ndi

