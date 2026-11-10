// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL_opengl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "video/VideoBackend.h"

class OpenGLBackend final : public VideoBackend {
public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    OpenGLBackend(const OpenGLBackend&) = delete;
    OpenGLBackend& operator=(const OpenGLBackend&) = delete;

    bool Initialize(SDL_Window* window, std::string& error) override;
    void Shutdown() override;
    void Reset() override;

    bool SupportsHardware() const override { return supports_hardware_; }
    bool SetupHardwareRender(retro_hw_render_callback* callback,
                             std::string& error) override;
    bool ContextReset(std::string& error) override;

    void OnSoftwareFrame(const void* data, unsigned width, unsigned height,
                         size_t pitch, retro_pixel_format format) override;
    void OnHardwareFrame(unsigned width, unsigned height) override;
    void ResizeFramebuffer(unsigned width, unsigned height) override;

    uintptr_t TextureId() const override { return texture_; }
    void GetTextureUVs(float& u0, float& v0, float& u1, float& v1) const override;
    void BindDefaultFramebuffer() override;

    int FrameWidth() const override { return frame_width_; }
    int FrameHeight() const override { return frame_height_; }
    bool HasFrame() const override { return has_frame_; }
    bool IsHardware() const override { return hardware_active_; }
    const char* Name() const override;

    bool ReadPixels(std::vector<uint32_t>& pixels, int& width, int& height) override;

private:
    struct Functions {
        PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
        PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
        PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
        PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
        PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;
    };

    static uintptr_t GetCurrentFramebuffer();
    static retro_proc_address_t GetProcAddress(const char* name);

    bool LoadFunctions(std::string& error);
    bool EnsureFramebuffer(unsigned width, unsigned height, std::string& error);
    void DestroyFramebuffer();
    void UploadSoftwarePixels(const void* data, unsigned width, unsigned height,
                              size_t pitch, retro_pixel_format format);

    static OpenGLBackend* s_active;

    SDL_Window* window_ = nullptr;
    Functions gl_{};
    bool supports_hardware_ = false;
    bool hardware_active_ = false;
    bool bottom_left_origin_ = true;

    GLuint framebuffer_ = 0;
    GLuint texture_ = 0;
    int framebuffer_width_ = 0;
    int framebuffer_height_ = 0;

    int frame_width_ = 0;
    int frame_height_ = 0;
    bool has_frame_ = false;

    retro_hw_render_callback* hardware_callback_ = nullptr;
    std::vector<uint32_t> software_pixels_;
    std::string driver_name_;
};
