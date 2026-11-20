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

    // Releases the resources the core created in context_reset(). Must be
    // called while the GPU context is still current and the core library is
    // still loaded (libretro's context_destroy contract). Idempotent.
    virtual void DestroyHardwareContext() = 0;

    virtual void OnSoftwareFrame(const void* data, unsigned width, unsigned height,
                                 size_t pitch, retro_pixel_format format) = 0;
    virtual void OnHardwareFrame(unsigned width, unsigned height) = 0;
    virtual void ResizeFramebuffer(unsigned width, unsigned height) = 0;

    // A framebuffer is grow-only, so when it has to be recreated the texture the core
    // draws into changes: its resources are invalidated. libretro allows the frontend
    // to call context_reset() again without a prior context_destroy exactly for that,
    // so the core can rebuild them. The host does it between frames, never from inside
    // an environment callback.
    virtual bool NeedsContextReset() const { return false; }

    virtual uintptr_t TextureId() const = 0;
    virtual void GetTextureUVs(float& u0, float& v0, float& u1, float& v1) const = 0;
    virtual void BindDefaultFramebuffer() = 0;

    virtual int FrameWidth() const = 0;
    virtual int FrameHeight() const = 0;
    virtual bool HasFrame() const = 0;
    virtual bool IsHardware() const = 0;
    virtual const char* Name() const = 0;

    // RGBA8 bytes in memory order (R, G, B, A), top-down, tightly packed (width * 4).
    virtual bool ReadPixels(std::vector<uint8_t>& pixels, int& width, int& height) = 0;
};
