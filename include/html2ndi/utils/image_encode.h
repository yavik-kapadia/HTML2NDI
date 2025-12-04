#pragma once

#include <cstdint>
#include <vector>

namespace html2ndi {

/**
 * Encode RGBA data to JPEG.
 * @param rgba_data Input RGBA pixel data
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality (1-100)
 * @param out_jpeg Output JPEG data
 * @return true if encoding succeeded
 */
bool encode_jpeg(const uint8_t* rgba_data, int width, int height, int quality, 
                 std::vector<uint8_t>& out_jpeg);

/**
 * Encode RGBA data to JPEG with scaling.
 * @param rgba_data Input RGBA pixel data
 * @param width Image width
 * @param height Image height
 * @param target_width Target width (height is calculated to maintain aspect ratio)
 * @param quality JPEG quality (1-100)
 * @param out_jpeg Output JPEG data
 * @return true if encoding succeeded
 */
bool encode_jpeg_scaled(const uint8_t* rgba_data, int width, int height, 
                        int target_width, int quality, std::vector<uint8_t>& out_jpeg);

} // namespace html2ndi

