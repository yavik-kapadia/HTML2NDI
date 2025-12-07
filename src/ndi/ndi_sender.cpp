/**
 * NDI sender implementation.
 */

#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/utils/logger.h"

#include <cstring>
#include <sstream>

namespace html2ndi {

NdiSender::NdiSender(const std::string& name, const std::string& groups)
    : name_(name)
    , groups_(groups) {
    update_metadata();  // Initialize with default Rec.709 settings
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
    video_frame_.FourCC = NDIlib_FourCC_video_type_BGRX;
    video_frame_.frame_rate_N = frame_rate_n;
    video_frame_.frame_rate_D = frame_rate_d;
    video_frame_.picture_aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    video_frame_.frame_format_type = NDIlib_frame_format_type_progressive;



    video_frame_.timecode = next_timecode_.load();  // Use configured timecode
    video_frame_.p_data = const_cast<uint8_t*>(data);
    video_frame_.line_stride_in_bytes = width * 4;
    video_frame_.timestamp = 0;
    
    // Set color space metadata
    video_frame_.p_metadata = color_metadata_.c_str();
    
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

NdiSender::TallyState NdiSender::get_tally(uint32_t timeout_ms) const {
    TallyState state;
    
    if (!initialized_ || !sender_) {
        return state;
    }
    
    NDIlib_tally_t ndi_tally;
    if (NDIlib_send_get_tally(sender_, &ndi_tally, timeout_ms)) {
        state.on_program = ndi_tally.on_program;
        state.on_preview = ndi_tally.on_preview;
    }
    
    return state;
}

std::string NdiSender::get_source_name() const {
    if (!initialized_ || !sender_) {
        return name_;
    }
    
    const NDIlib_source_t* source = NDIlib_send_get_source_name(sender_);
    if (source && source->p_ndi_name) {
        return source->p_ndi_name;
    }
    
    return name_;
}

void NdiSender::update_metadata() {
    std::ostringstream oss;
    oss << "<ndi_color_info>"
        << "<colorimetry>" << color_space_name() << "</colorimetry>"
        << "<gamma>" << gamma_mode_name() << "</gamma>"
        << "<range>" << color_range_name() << "</range>"
        << "</ndi_color_info>";
    color_metadata_ = oss.str();
    LOG_DEBUG("NDI color metadata updated: %s", color_metadata_.c_str());
}

std::string NdiSender::color_space_name() const {
    switch (color_space_) {
        case ColorSpace::Rec709:  return "BT709";
        case ColorSpace::Rec2020: return "BT2020";
        case ColorSpace::sRGB:    return "sRGB";
        case ColorSpace::Rec601:  return "BT601";
    }
    return "BT709";
}

std::string NdiSender::gamma_mode_name() const {
    switch (gamma_mode_) {
        case GammaMode::BT709:   return "BT709";
        case GammaMode::BT2020:  return "BT2020";
        case GammaMode::sRGB:    return "sRGB";
        case GammaMode::Linear:  return "Linear";
    }
    return "BT709";
}

std::string NdiSender::color_range_name() const {
    switch (color_range_) {
        case ColorRange::Full:    return "full";
        case ColorRange::Limited: return "limited";
    }
    return "full";
}

} // namespace html2ndi

