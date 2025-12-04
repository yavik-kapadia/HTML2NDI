/**
 * Image encoding utilities using stb_image_write.
 */

// Suppress sprintf deprecation warning in stb_image_write
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma clang diagnostic pop

#include <vector>
#include <cstdint>

namespace html2ndi {

// Callback to write to a vector
static void write_to_vector(void* context, void* data, int size) {
    auto* vec = static_cast<std::vector<uint8_t>*>(context);
    auto* bytes = static_cast<uint8_t*>(data);
    vec->insert(vec->end(), bytes, bytes + size);
}

bool encode_jpeg(const uint8_t* bgra_data, int width, int height, int quality, 
                 std::vector<uint8_t>& out_jpeg) {
    out_jpeg.clear();
    
    // Convert BGRA to RGB (CEF outputs BGRA on macOS, JPEG needs RGB)
    std::vector<uint8_t> rgb_data(width * height * 3);
    for (int i = 0; i < width * height; i++) {
        rgb_data[i * 3 + 0] = bgra_data[i * 4 + 2]; // R (from B position)
        rgb_data[i * 3 + 1] = bgra_data[i * 4 + 1]; // G
        rgb_data[i * 3 + 2] = bgra_data[i * 4 + 0]; // B (from R position)
    }
    
    int result = stbi_write_jpg_to_func(write_to_vector, &out_jpeg, 
                                        width, height, 3, rgb_data.data(), quality);
    return result != 0;
}

bool encode_jpeg_scaled(const uint8_t* bgra_data, int width, int height, 
                        int target_width, int quality, std::vector<uint8_t>& out_jpeg) {
    // Simple box-filter downscale
    int target_height = (height * target_width) / width;
    if (target_width >= width) {
        // No scaling needed
        return encode_jpeg(bgra_data, width, height, quality, out_jpeg);
    }
    
    float scale_x = static_cast<float>(width) / target_width;
    float scale_y = static_cast<float>(height) / target_height;
    
    std::vector<uint8_t> scaled(target_width * target_height * 3);
    
    for (int y = 0; y < target_height; y++) {
        for (int x = 0; x < target_width; x++) {
            int src_x = static_cast<int>(x * scale_x);
            int src_y = static_cast<int>(y * scale_y);
            int src_idx = (src_y * width + src_x) * 4;
            int dst_idx = (y * target_width + x) * 3;
            
            // BGRA to RGB conversion
            scaled[dst_idx + 0] = bgra_data[src_idx + 2]; // R (from B position)
            scaled[dst_idx + 1] = bgra_data[src_idx + 1]; // G
            scaled[dst_idx + 2] = bgra_data[src_idx + 0]; // B (from R position)
        }
    }
    
    out_jpeg.clear();
    int result = stbi_write_jpg_to_func(write_to_vector, &out_jpeg, 
                                        target_width, target_height, 3, scaled.data(), quality);
    return result != 0;
}

} // namespace html2ndi

