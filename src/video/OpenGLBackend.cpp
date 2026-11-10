// SPDX-License-Identifier: GPL-3.0-or-later
#include "video/OpenGLBackend.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace {

uint32_t Expand5(uint32_t value) {
    return (value << 3) | (value >> 2);
}

uint32_t Expand6(uint32_t value) {
    return (value << 2) | (value >> 4);
}

} // namespace

OpenGLBackend* OpenGLBackend::s_active = nullptr;

OpenGLBackend::~OpenGLBackend() {
    Shutdown();
}

bool OpenGLBackend::Initialize(SDL_Window* window, std::string& error) {
    window_ = window;
    s_active = this;

    if (!LoadFunctions(error)) {
        return false;
    }

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    supports_hardware_ = (major > 4) || (major == 4 && minor >= 3);

    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    driver_name_ = "OpenGL ";
    driver_name_ += version != nullptr ? version : "desconocido";
    if (renderer != nullptr || vendor != nullptr) {
        driver_name_ += " | ";
        driver_name_ += vendor != nullptr ? vendor : "";
        driver_name_ += " ";
        driver_name_ += renderer != nullptr ? renderer : "";
    }

    SDL_Log("Backend: %s", driver_name_.c_str());
    SDL_Log("Render hardware: %s", supports_hardware_ ? "disponible" : "no disponible");

    return true;
}

void OpenGLBackend::Shutdown() {
    DestroyFramebuffer();
    software_pixels_.clear();
    frame_width_ = 0;
    frame_height_ = 0;
    has_frame_ = false;
    hardware_active_ = false;
    hardware_callback_ = nullptr;
    window_ = nullptr;
    if (s_active == this) {
        s_active = nullptr;
    }
}

void OpenGLBackend::Reset() {
    DestroyFramebuffer();
    software_pixels_.clear();
    frame_width_ = 0;
    frame_height_ = 0;
    has_frame_ = false;
    hardware_active_ = false;
    hardware_callback_ = nullptr;
    bottom_left_origin_ = true;
}

bool OpenGLBackend::LoadFunctions(std::string& error) {
    const auto load = [&](auto& function, const char* name) {
        using FunctionType = std::decay_t<decltype(function)>;
        function = reinterpret_cast<FunctionType>(SDL_GL_GetProcAddress(name));
        if (function == nullptr) {
            error = std::string("No se pudo cargar ") + name +
                    ". ¿El contexto OpenGL es 3.0 o superior?";
            return false;
        }
        return true;
    };

    return load(gl_.GenFramebuffers, "glGenFramebuffers") &&
           load(gl_.BindFramebuffer, "glBindFramebuffer") &&
           load(gl_.FramebufferTexture2D, "glFramebufferTexture2D") &&
           load(gl_.CheckFramebufferStatus, "glCheckFramebufferStatus") &&
           load(gl_.DeleteFramebuffers, "glDeleteFramebuffers");
}

bool OpenGLBackend::SetupHardwareRender(retro_hw_render_callback* callback,
                                        std::string& error) {
    if (!supports_hardware_) {
        error = "El contexto OpenGL no es 4.3; se usará el renderer software.";
        return false;
    }
    if (callback == nullptr) {
        error = "El core envió un callback de hardware nulo.";
        return false;
    }

    hardware_callback_ = callback;
    callback->get_current_framebuffer = &OpenGLBackend::GetCurrentFramebuffer;
    callback->get_proc_address = &OpenGLBackend::GetProcAddress;
    bottom_left_origin_ = callback->bottom_left_origin;

    if (framebuffer_ == 0) {
        if (!EnsureFramebuffer(2048, 2048, error)) {
            return false;
        }
    }

    SDL_Log("HW render preparado: FBO %u (%dx%d), bottom-left=%s",
            static_cast<unsigned>(framebuffer_), framebuffer_width_, framebuffer_height_,
            bottom_left_origin_ ? "sí" : "no");
    return true;
}

bool OpenGLBackend::ContextReset(std::string& error) {
    if (hardware_callback_ == nullptr || hardware_callback_->context_reset == nullptr) {
        error = "El core no proporcionó context_reset.";
        return false;
    }

    GLint previous_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    hardware_callback_->context_reset();
    gl_.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
    hardware_active_ = true;
    SDL_Log("Contexto hardware de Azahar inicializado.");
    return true;
}

void OpenGLBackend::OnSoftwareFrame(const void* data, unsigned width, unsigned height,
                                    size_t pitch, retro_pixel_format format) {
    if (data == nullptr || width == 0 || height == 0) {
        return;
    }

    hardware_active_ = false;
    frame_width_ = static_cast<int>(width);
    frame_height_ = static_cast<int>(height);
    has_frame_ = true;

    std::string error;
    if (!EnsureFramebuffer(width, height, error)) {
        SDL_Log("No se pudo preparar la textura software: %s", error.c_str());
        return;
    }

    UploadSoftwarePixels(data, width, height, pitch, format);
}

void OpenGLBackend::OnHardwareFrame(unsigned width, unsigned height) {
    if (width == 0 || height == 0) {
        return;
    }
    hardware_active_ = true;
    frame_width_ = static_cast<int>(width);
    frame_height_ = static_cast<int>(height);
    has_frame_ = true;
}

void OpenGLBackend::ResizeFramebuffer(unsigned width, unsigned height) {
    if (width == 0 || height == 0) {
        return;
    }
    std::string error;
    if (!EnsureFramebuffer(width, height, error)) {
        SDL_Log("No se pudo redimensionar el FBO: %s", error.c_str());
    }
}

void OpenGLBackend::UploadSoftwarePixels(const void* data, unsigned width, unsigned height,
                                         size_t pitch, retro_pixel_format format) {
    software_pixels_.resize(static_cast<size_t>(width) * height);
    const auto* source = static_cast<const uint8_t*>(data);
    const size_t bytes_per_pixel = format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    const size_t source_pitch =
        pitch != 0 ? pitch : static_cast<size_t>(width) * bytes_per_pixel;

    switch (format) {
    case RETRO_PIXEL_FORMAT_XRGB8888: {
        const size_t row_bytes = std::min(source_pitch, static_cast<size_t>(width) * 4);
        for (unsigned y = 0; y < height; ++y) {
            std::memcpy(software_pixels_.data() + static_cast<size_t>(y) * width,
                        source + static_cast<size_t>(y) * source_pitch, row_bytes);
        }
        for (auto& pixel : software_pixels_) {
            pixel |= 0xFF000000u;
        }
        break;
    }
    case RETRO_PIXEL_FORMAT_RGB565: {
        for (unsigned y = 0; y < height; ++y) {
            const auto* row = reinterpret_cast<const uint16_t*>(source + y * source_pitch);
            for (unsigned x = 0; x < width; ++x) {
                const uint16_t pixel = row[x];
                const uint32_t r = Expand5((pixel >> 11) & 0x1F);
                const uint32_t g = Expand6((pixel >> 5) & 0x3F);
                const uint32_t b = Expand5(pixel & 0x1F);
                software_pixels_[static_cast<size_t>(y) * width + x] =
                    0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
        break;
    }
    case RETRO_PIXEL_FORMAT_0RGB1555: {
        for (unsigned y = 0; y < height; ++y) {
            const auto* row = reinterpret_cast<const uint16_t*>(source + y * source_pitch);
            for (unsigned x = 0; x < width; ++x) {
                const uint16_t pixel = row[x];
                const uint32_t r = Expand5((pixel >> 10) & 0x1F);
                const uint32_t g = Expand5((pixel >> 5) & 0x1F);
                const uint32_t b = Expand5(pixel & 0x1F);
                software_pixels_[static_cast<size_t>(y) * width + x] =
                    0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
        break;
    }
    default:
        break;
    }

    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height), GL_BGRA, GL_UNSIGNED_BYTE,
                    software_pixels_.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLBackend::GetTextureUVs(float& u0, float& v0, float& u1, float& v1) const {
    u0 = 0.0f;
    v0 = 0.0f;
    u1 = 1.0f;
    v1 = 1.0f;

    if (framebuffer_width_ <= 0 || framebuffer_height_ <= 0 ||
        frame_width_ <= 0 || frame_height_ <= 0) {
        return;
    }

    const float u_max = std::min(1.0f, static_cast<float>(frame_width_) / framebuffer_width_);
    const float v_max = std::min(1.0f, static_cast<float>(frame_height_) / framebuffer_height_);

    if (!hardware_active_) {
        // Software pixels are uploaded top-down into the lower-left region of
        // the texture. The first row is at v=0, which is the top of the image.
        u1 = u_max;
        v1 = v_max;
        return;
    }

    if (bottom_left_origin_) {
        v0 = v_max;
        v1 = 0.0f;
    } else {
        v0 = 0.0f;
        v1 = v_max;
    }
    u1 = u_max;
}

void OpenGLBackend::BindDefaultFramebuffer() {
    gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool OpenGLBackend::ReadPixels(std::vector<uint32_t>& pixels, int& width, int& height) {
    if (!has_frame_ || frame_width_ <= 0 || frame_height_ <= 0) {
        return false;
    }

    width = frame_width_;
    height = frame_height_;

    if (!hardware_active_) {
        pixels = software_pixels_;
        return !pixels.empty();
    }

    if (framebuffer_ == 0) {
        return false;
    }

    std::vector<uint32_t> raw(static_cast<size_t>(width) * height);
    GLint previous_read_fbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
    gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, raw.data());
    gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read_fbo));

    pixels.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        const auto* src = raw.data() + static_cast<size_t>(height - 1 - y) * width;
        auto* dst = pixels.data() + static_cast<size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            dst[x] = src[x] | 0xFF000000u;
        }
    }
    return true;
}

const char* OpenGLBackend::Name() const {
    return hardware_active_ ? "OpenGL (hardware)" : "OpenGL (software)";
}

bool OpenGLBackend::EnsureFramebuffer(unsigned width, unsigned height, std::string& error) {
    width = std::max(width, 64u);
    height = std::max(height, 64u);

    if (framebuffer_ != 0 && width <= static_cast<unsigned>(framebuffer_width_) &&
        height <= static_cast<unsigned>(framebuffer_height_)) {
        return true;
    }

    GLint max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (max_texture_size > 0 &&
        (width > static_cast<unsigned>(max_texture_size) ||
         height > static_cast<unsigned>(max_texture_size))) {
        error = "El tamaño del FBO supera GL_MAX_TEXTURE_SIZE.";
        return false;
    }

    GLint previous_draw_fbo = 0;
    GLint previous_read_fbo = 0;
    GLint previous_texture = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

    const GLuint old_framebuffer = framebuffer_;
    const GLuint old_texture = texture_;
    DestroyFramebuffer();

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    gl_.GenFramebuffers(1, &framebuffer_);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    gl_.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

    const GLenum status = gl_.CheckFramebufferStatus(GL_FRAMEBUFFER);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        error = "El FBO de OpenGL está incompleto (0x" + std::to_string(status) + ").";
        DestroyFramebuffer();
        glBindTexture(GL_TEXTURE_2D,
                      previous_texture == static_cast<GLint>(old_texture)
                          ? 0
                          : static_cast<GLuint>(previous_texture));
        gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER,
                            previous_draw_fbo == static_cast<GLint>(old_framebuffer)
                                ? 0
                                : static_cast<GLuint>(previous_draw_fbo));
        gl_.BindFramebuffer(GL_READ_FRAMEBUFFER,
                            previous_read_fbo == static_cast<GLint>(old_framebuffer)
                                ? 0
                                : static_cast<GLuint>(previous_read_fbo));
        return false;
    }

    framebuffer_width_ = static_cast<int>(width);
    framebuffer_height_ = static_cast<int>(height);

    const GLuint restore_texture =
        previous_texture == static_cast<GLint>(old_texture) ? texture_
                                                           : static_cast<GLuint>(previous_texture);
    const GLuint restore_draw =
        previous_draw_fbo == static_cast<GLint>(old_framebuffer) ? framebuffer_
                                                                 : static_cast<GLuint>(previous_draw_fbo);
    const GLuint restore_read =
        previous_read_fbo == static_cast<GLint>(old_framebuffer) ? framebuffer_
                                                                 : static_cast<GLuint>(previous_read_fbo);
    glBindTexture(GL_TEXTURE_2D, restore_texture);
    gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER, restore_draw);
    gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, restore_read);

    SDL_Log("FBO de vídeo: %ux%u", width, height);
    return true;
}

void OpenGLBackend::DestroyFramebuffer() {
    if (framebuffer_ != 0) {
        gl_.DeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    framebuffer_width_ = 0;
    framebuffer_height_ = 0;
}

uintptr_t OpenGLBackend::GetCurrentFramebuffer() {
    return s_active != nullptr ? static_cast<uintptr_t>(s_active->framebuffer_) : 0;
}

retro_proc_address_t OpenGLBackend::GetProcAddress(const char* name) {
    return reinterpret_cast<retro_proc_address_t>(SDL_GL_GetProcAddress(name));
}
