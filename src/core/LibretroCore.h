// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL_loadso.h>

#include <filesystem>
#include <string>

#include "libretro.h"

// Thin wrapper around a libretro core shared library.
// It only resolves symbols; lifecycle/callbacks live in LibretroHost.
class LibretroCore {
public:
    LibretroCore() = default;
    ~LibretroCore();

    LibretroCore(const LibretroCore&) = delete;
    LibretroCore& operator=(const LibretroCore&) = delete;

    bool Load(const std::filesystem::path& path, std::string& error);
    void Unload();

    bool IsLoaded() const { return handle_ != nullptr; }
    const std::filesystem::path& Path() const { return path_; }

    void (*retro_init)() = nullptr;
    void (*retro_deinit)() = nullptr;
    unsigned (*retro_api_version)() = nullptr;

    void (*retro_get_system_info)(retro_system_info*) = nullptr;
    void (*retro_get_system_av_info)(retro_system_av_info*) = nullptr;

    void (*retro_set_environment)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state)(retro_input_state_t) = nullptr;

    void (*retro_set_controller_port_device)(unsigned port, unsigned device) = nullptr;

    void (*retro_reset)() = nullptr;
    void (*retro_run)() = nullptr;

    size_t (*retro_serialize_size)() = nullptr;
    bool (*retro_serialize)(void* data, size_t size) = nullptr;
    bool (*retro_unserialize)(const void* data, size_t size) = nullptr;

    void (*retro_cheat_reset)() = nullptr;
    void (*retro_cheat_set)(unsigned index, bool enabled, const char* code) = nullptr;

    bool (*retro_load_game)(const retro_game_info*) = nullptr;
    bool (*retro_load_game_special)(unsigned game_type, const retro_game_info* info,
                                    size_t num_info) = nullptr;
    void (*retro_unload_game)() = nullptr;

    unsigned (*retro_get_region)() = nullptr;
    void* (*retro_get_memory_data)(unsigned id) = nullptr;
    size_t (*retro_get_memory_size)(unsigned id) = nullptr;

private:
    SDL_SharedObject* handle_ = nullptr;
    std::filesystem::path path_;
};
