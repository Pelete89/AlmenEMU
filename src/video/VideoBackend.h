// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "libretro.h"

struct SDL_Window;

// Presentation backend for libretro frames.
//
// The backend owns the GPU resources used to display the core output. It can
// operate in two modes:
//   - software: the core delivers XRGB8888/RGB565/0RGB1555 pixels and the
//     backend uploads them to a texture.
//   - hardware: the core renders into an FBO owned by the backend and only
//     signals RETRO_HW_FRAME_BUFFER_VALID.
class VideoBackend {
public:
    virtual ~VideoBackend() = default;

    virtual bool Initialize(SDL_Window* window, std::string& error) = 0;
    virtual void Shutdown() = 0;
    virtual void Reset() = 0;

    virtual bool SupportsHardware() const = 0;
    virtual bool SetupHardwareRender(retro_hw_render_callback* callback,
                                     std::string& error) = 0;
    virtual bool ContextReset(std::string& error) = 0;

    virtual void OnSoftwareFrame(const void* data, unsigned width, unsigned height,
                                 size_t pitch, retro_pixel_format format) = 0;
    virtual void OnHardwareFrame(unsigned width, unsigned height) = 0;
    virtual void ResizeFramebuffer(unsigned width, unsigned height) = 0;

    virtual uintptr_t TextureId() const = 0;
    virtual void GetTextureUVs(float& u0, float& v0, float& u1, float& v1) const = 0;
    virtual void BindDefaultFramebuffer() = 0;

    virtual int FrameWidth() const = 0;
    virtual int FrameHeight() const = 0;
    virtual bool HasFrame() const = 0;
    virtual bool IsHardware() const = 0;
    virtual const char* Name() const = 0;

    virtual bool ReadPixels(std::vector<uint32_t>& pixels, int& width, int& height) = 0;
};
