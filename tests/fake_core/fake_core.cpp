// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal libretro core used by the AlmenEMU host tests. It does not emulate
// anything: it only exercises the libretro contract (environment queries,
// core options, hardware render negotiation, video/audio/input callbacks and
// savestates) so the frontend can be tested headless, with no GPU and no ROM.
//
// Behaviour is selected at retro_load_game() time with the environment
// variable ALMENEMU_FAKE_HW:
//   none   (default) render in software
//   opengl request an OpenGL core hardware context
//   vulkan request a Vulkan hardware context (the frontend must reject it)
//   auto   request hardware only if GET_PREFERRED_HW_RENDER is not NONE

#include "libretro.h"

#include "fake_core_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

fake_core::Stats g_stats{};

retro_environment_t g_env = nullptr;
retro_video_refresh_t g_video = nullptr;
retro_audio_sample_t g_audio = nullptr;
retro_audio_sample_batch_t g_audio_batch = nullptr;
retro_input_poll_t g_input_poll = nullptr;
retro_input_state_t g_input_state = nullptr;

retro_hw_render_callback g_hw_callback{};
bool g_hw_accepted = false;
bool g_game_loaded = false;

std::vector<uint32_t> g_pixels(fake_core::kWidth * fake_core::kHeight, 0xFF203040u);
std::vector<int16_t> g_samples(fake_core::kAudioFramesPerRun * 2, 0);

struct SaveState {
    uint32_t magic;
    uint32_t frames;
    uint32_t scale_2x_seen;
    uint32_t padding;
};

const char* HardwareMode() {
    const char* mode = std::getenv("ALMENEMU_FAKE_HW");
    return mode != nullptr ? mode : "none";
}

void ResetStats() {
    g_stats = fake_core::Stats{};
    g_stats.magic = fake_core::kStatsMagic;
    g_stats.version = fake_core::kStatsVersion;
}

void RETRO_CALLCONV HwContextReset() {
    ++g_stats.context_reset_calls;
}

void RETRO_CALLCONV HwContextDestroy() {
    ++g_stats.context_destroy_calls;
}

uintptr_t RETRO_CALLCONV HwGetCurrentFramebuffer() {
    return 1;
}

retro_proc_address_t RETRO_CALLCONV HwGetProcAddress(const char* symbol) {
    (void)symbol;
    return nullptr;
}

bool RequestHardware(retro_hw_context_type type) {
    ++g_stats.hw_requested;
    if (g_env == nullptr) {
        return false;
    }

    std::memset(&g_hw_callback, 0, sizeof(g_hw_callback));
    g_hw_callback.context_type = type;
    g_hw_callback.context_reset = &HwContextReset;
    g_hw_callback.context_destroy = &HwContextDestroy;
    g_hw_callback.get_current_framebuffer = &HwGetCurrentFramebuffer;
    g_hw_callback.get_proc_address = &HwGetProcAddress;
    g_hw_callback.bottom_left_origin = true;
    g_hw_callback.version_major = 4;
    g_hw_callback.version_minor = 3;

    const bool accepted = g_env(RETRO_ENVIRONMENT_SET_HW_RENDER, &g_hw_callback);
    if (accepted) {
        ++g_stats.hw_accepted;
        g_hw_accepted = true;
    }
    return accepted;
}

retro_core_option_v2_category g_categories[] = {
    {"fake", "Fake", "Opciones del core simulado"},
    {nullptr, nullptr, nullptr},
};

retro_core_option_v2_definition g_definitions[] = {
    {"fake_video_scale",
     "Escala de vídeo",
     nullptr,
     "Escala interna usada por las pruebas.",
     nullptr,
     "fake",
     {{"1x", "1x"}, {"2x", "2x"}, {nullptr, nullptr}},
     "1x"},
    {"fake_debug",
     "Mensajes de depuración",
     nullptr,
     "Opciones adicionales de prueba.",
     nullptr,
     nullptr,
     {{"disabled", "Desactivado"}, {"enabled", "Activado"}, {nullptr, nullptr}},
     "disabled"},
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr},
};

retro_core_options_v2 g_options = {g_categories, g_definitions};

} // namespace

RETRO_API void retro_set_environment(retro_environment_t cb) {
    g_env = cb;
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    g_video = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {
    g_audio = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    g_audio_batch = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    g_input_poll = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    g_input_state = cb;
}

RETRO_API void retro_init(void) {
    ResetStats();
    g_game_loaded = false;
    g_hw_accepted = false;

    if (g_env == nullptr) {
        return;
    }

    unsigned options_version = 0;
    if (g_env(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &options_version) &&
        options_version >= 2) {
        g_env(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &g_options);
    }

    retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    g_env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}

RETRO_API void retro_deinit(void) {
    ++g_stats.deinit_calls;
    g_game_loaded = false;
    g_hw_accepted = false;
}

RETRO_API unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "Fake libretro core";
    info->library_version = "1.0";
    info->valid_extensions = "fake|3dsx";
    info->need_fullpath = true;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width = fake_core::kWidth;
    info->geometry.base_height = fake_core::kHeight;
    info->geometry.max_width = fake_core::kWidth;
    info->geometry.max_height = fake_core::kHeight;
    info->geometry.aspect_ratio =
        static_cast<float>(fake_core::kWidth) / static_cast<float>(fake_core::kHeight);
    info->timing.fps = fake_core::kFps;
    info->timing.sample_rate = fake_core::kSampleRate;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

RETRO_API void retro_reset(void) {
    // Nothing to reset in the fake core.
}

RETRO_API void retro_run(void) {
    if (!g_game_loaded) {
        return;
    }

    if (g_env != nullptr) {
        bool updated = false;
        if (g_env(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
            ++g_stats.option_updates;
        }

        retro_variable variable{};
        variable.key = "fake_video_scale";
        if (g_env(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value != nullptr &&
            std::strcmp(variable.value, "2x") == 0) {
            ++g_stats.scale_2x_seen;
        }
    }

    if (g_input_poll != nullptr) {
        ++g_stats.input_polls;
        g_input_poll();
    }
    if (g_input_state != nullptr) {
        ++g_stats.input_queries;
        g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    }

    if (g_audio_batch != nullptr) {
        ++g_stats.audio_frames;
        g_audio_batch(g_samples.data(), fake_core::kAudioFramesPerRun);
    } else if (g_audio != nullptr) {
        g_audio(0, 0);
    }

    ++g_stats.frames;

    if (g_video == nullptr) {
        return;
    }
    if (g_hw_accepted) {
        g_video(RETRO_HW_FRAME_BUFFER_VALID, fake_core::kWidth, fake_core::kHeight, 0);
    } else {
        g_video(g_pixels.data(), fake_core::kWidth, fake_core::kHeight,
                fake_core::kWidth * sizeof(uint32_t));
    }
}

RETRO_API bool retro_load_game(const struct retro_game_info* game) {
    if (game == nullptr || game->path == nullptr || g_game_loaded) {
        return false;
    }

    FILE* file = std::fopen(game->path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);

    ++g_stats.load_calls;
    g_game_loaded = true;

    retro_hw_context_type preferred = RETRO_HW_CONTEXT_NONE;
    if (g_env != nullptr) {
        g_env(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred);
    }
    g_stats.preferred_hw = static_cast<uint32_t>(preferred);

    const std::string mode = HardwareMode();
    if (mode == "opengl") {
        RequestHardware(RETRO_HW_CONTEXT_OPENGL_CORE);
    } else if (mode == "vulkan") {
        RequestHardware(RETRO_HW_CONTEXT_VULKAN);
    } else if (mode == "auto" && preferred != RETRO_HW_CONTEXT_NONE) {
        RequestHardware(RETRO_HW_CONTEXT_OPENGL_CORE);
    }

    return true;
}

RETRO_API void retro_unload_game(void) {
    ++g_stats.unload_calls;
    g_game_loaded = false;
    g_hw_accepted = false;
}

RETRO_API size_t retro_serialize_size(void) {
    return fake_core::kSaveStateSize;
}

RETRO_API bool retro_serialize(void* data, size_t size) {
    if (data == nullptr || size < fake_core::kSaveStateSize) {
        return false;
    }

    std::memset(data, 0, fake_core::kSaveStateSize);
    SaveState state{};
    state.magic = fake_core::kSaveStateMagic;
    state.frames = g_stats.frames;
    state.scale_2x_seen = g_stats.scale_2x_seen;
    std::memcpy(data, &state, sizeof(state));

    ++g_stats.save_state_saved;
    return true;
}

RETRO_API bool retro_unserialize(const void* data, size_t size) {
    if (data == nullptr || size < fake_core::kSaveStateSize) {
        return false;
    }

    SaveState state{};
    std::memcpy(&state, data, sizeof(state));
    if (state.magic != fake_core::kSaveStateMagic) {
        return false;
    }

    g_stats.frames = state.frames;
    g_stats.scale_2x_seen = state.scale_2x_seen;
    ++g_stats.save_state_loaded;
    return true;
}

RETRO_API void* retro_get_memory_data(unsigned id) {
    return id == RETRO_MEMORY_SYSTEM_RAM ? &g_stats : nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned id) {
    return id == RETRO_MEMORY_SYSTEM_RAM ? sizeof(fake_core::Stats) : 0;
}
