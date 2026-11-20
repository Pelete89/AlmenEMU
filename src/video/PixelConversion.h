// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "libretro.h"

// Pure presentation logic (pixel formats, texture UVs, framebuffer policy).
//
// It has no OpenGL, SDL or window dependencies on purpose: this is the part of the
// video backend that can be tested headless in CI. The backend is only a thin shell
// around these functions.
namespace video {

struct TextureUV {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

uint32_t Expand5(uint32_t value);
uint32_t Expand6(uint32_t value);

// Converts one software frame into RGBA8 bytes in memory order (R, G, B, A), top-down
// and tightly packed: dst[(y * width + x) * 4] is the red channel of pixel (x, y).
// Rows are read with `pitch` bytes; 0 means the tight width * bytes_per_pixel. Alpha
// is always opaque. The destination is written as explicit bytes (not a packed 32-bit
// value) so the upload does not depend on the host endianness, and the backend uploads
// it with GL_RGBA, a format every core profile accepts.
//
// Returns false for an unknown format or a pitch smaller than one row; dst is not
// touched in that case.
bool ConvertFrameToRGBA8(std::vector<uint8_t>& dst, const void* data, unsigned width,
                         unsigned height, size_t pitch, retro_pixel_format format);

// GL readback is bottom-up: row 0 of the buffer is the bottom of the image, while a
// libretro frame (and a screenshot file) is top-down. dst gets width*height*4 bytes.
// Returns false when `src` is shorter than the frame; dst is left empty so the caller
// cannot report a blank capture as a real frame.
bool FlipRowsToTopDown(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src,
                       unsigned width, unsigned height);

// UV rectangle of the active frame inside the framebuffer texture.
//
// ImGui maps v=0 to the top of the quad, so top-down data (the software path) needs no
// flip. When the core renders with bottom_left_origin, the top of the image is at v=1.
// The texture can be larger than the frame, so the UVs crop the region the core filled.
TextureUV ComputeTextureUVs(unsigned framebuffer_width, unsigned framebuffer_height,
                            unsigned frame_width, unsigned frame_height, bool hardware,
                            bool bottom_left_origin);

// The framebuffer is grow-only: shrinking it would invalidate the texture the core is
// drawing into. Returns true when a new framebuffer must be created.
bool FramebufferNeedsReallocation(unsigned current_width, unsigned current_height,
                                  unsigned wanted_width, unsigned wanted_height);

} // namespace video
