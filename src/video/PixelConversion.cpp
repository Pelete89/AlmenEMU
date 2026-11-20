// SPDX-License-Identifier: GPL-3.0-or-later
#include "video/PixelConversion.h"

#include <cstring>

namespace video {

uint32_t Expand5(uint32_t value) {
    return (value << 3) | (value >> 2);
}

uint32_t Expand6(uint32_t value) {
    return (value << 2) | (value >> 4);
}

namespace {

void WritePixel(std::vector<uint8_t>& dst, size_t offset, uint32_t r, uint32_t g, uint32_t b) {
    dst[offset + 0] = static_cast<uint8_t>(r);
    dst[offset + 1] = static_cast<uint8_t>(g);
    dst[offset + 2] = static_cast<uint8_t>(b);
    dst[offset + 3] = 0xFF; // opaque
}

} // namespace

bool ConvertFrameToRGBA8(std::vector<uint8_t>& dst, const void* data, unsigned width,
                         unsigned height, size_t pitch, retro_pixel_format format) {
    if (data == nullptr || width == 0 || height == 0) {
        return false;
    }

    const size_t bytes_per_pixel = format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    const size_t row_bytes = static_cast<size_t>(width) * bytes_per_pixel;
    const size_t source_pitch = pitch != 0 ? pitch : row_bytes;

    // A pitch smaller than one row means the frame is malformed: reading it would run
    // past the buffer the core gave us.
    if (source_pitch < row_bytes) {
        return false;
    }

    switch (format) {
    case RETRO_PIXEL_FORMAT_XRGB8888:
    case RETRO_PIXEL_FORMAT_RGB565:
    case RETRO_PIXEL_FORMAT_0RGB1555:
        break;
    default:
        return false;
    }

    dst.assign(static_cast<size_t>(width) * height * 4, 0);
    const auto* source = static_cast<const uint8_t*>(data);

    for (unsigned y = 0; y < height; ++y) {
        const auto* row = source + y * source_pitch;
        for (unsigned x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;

            if (format == RETRO_PIXEL_FORMAT_XRGB8888) {
                uint32_t pixel = 0;
                std::memcpy(&pixel, row + static_cast<size_t>(x) * 4, sizeof(pixel));
                WritePixel(dst, offset, (pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF);
            } else {
                uint16_t pixel = 0;
                std::memcpy(&pixel, row + static_cast<size_t>(x) * 2, sizeof(pixel));

                if (format == RETRO_PIXEL_FORMAT_RGB565) {
                    WritePixel(dst, offset, Expand5((pixel >> 11) & 0x1F),
                               Expand6((pixel >> 5) & 0x3F), Expand5(pixel & 0x1F));
                } else { // RETRO_PIXEL_FORMAT_0RGB1555
                    WritePixel(dst, offset, Expand5((pixel >> 10) & 0x1F),
                               Expand5((pixel >> 5) & 0x1F), Expand5(pixel & 0x1F));
                }
            }
        }
    }

    return true;
}

bool FlipRowsToTopDown(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src,
                       unsigned width, unsigned height) {
    const size_t row_bytes = static_cast<size_t>(width) * 4;
    const size_t needed = row_bytes * height;

    if (src.size() < needed) {
        // Reading src past its end would be out of bounds, and a zeroed buffer is not a
        // frame: leave dst empty so the caller reports the capture as failed.
        dst.clear();
        return false;
    }

    dst.assign(needed, 0);

    for (unsigned y = 0; y < height; ++y) {
        const auto* src_row = src.data() + static_cast<size_t>(height - 1 - y) * row_bytes;
        auto* dst_row = dst.data() + static_cast<size_t>(y) * row_bytes;
        std::memcpy(dst_row, src_row, row_bytes);
    }
    return true;
}

TextureUV ComputeTextureUVs(unsigned framebuffer_width, unsigned framebuffer_height,
                            unsigned frame_width, unsigned frame_height, bool hardware,
                            bool bottom_left_origin) {
    TextureUV uv;

    if (framebuffer_width == 0 || framebuffer_height == 0 || frame_width == 0 ||
        frame_height == 0) {
        return uv;
    }

    const float u_max = static_cast<float>(frame_width) / static_cast<float>(framebuffer_width);
    const float v_max = static_cast<float>(frame_height) / static_cast<float>(framebuffer_height);
    uv.u1 = u_max < 1.0f ? u_max : 1.0f;
    uv.v1 = v_max < 1.0f ? v_max : 1.0f;

    if (hardware && bottom_left_origin) {
        // The core filled the texture bottom-up, so the top of the image is the last
        // texture row: the quad has to be sampled flipped vertically.
        uv.v0 = uv.v1;
        uv.v1 = 0.0f;
    }

    return uv;
}

bool FramebufferNeedsReallocation(unsigned current_width, unsigned current_height,
                                  unsigned wanted_width, unsigned wanted_height) {
    if (current_width == 0 || current_height == 0) {
        return true;
    }
    return wanted_width > current_width || wanted_height > current_height;
}

} // namespace video
