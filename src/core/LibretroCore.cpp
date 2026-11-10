// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/LibretroCore.h"

#include <SDL3/SDL_loadso.h>

namespace {

std::string ToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

template <typename T>
bool LoadRequired(SDL_SharedObject* handle, const char* name, T& target, std::string& error) {
    SDL_FunctionPointer symbol = SDL_LoadFunction(handle, name);
    if (symbol == nullptr) {
        error = std::string("Falta el símbolo obligatorio '") + name + "': " + SDL_GetError();
        return false;
    }
    target = reinterpret_cast<T>(symbol);
    return true;
}

template <typename T>
void LoadOptional(SDL_SharedObject* handle, const char* name, T& target) {
    SDL_FunctionPointer symbol = SDL_LoadFunction(handle, name);
    if (symbol != nullptr) {
        target = reinterpret_cast<T>(symbol);
    }
}

} // namespace

LibretroCore::~LibretroCore() {
    Unload();
}

bool LibretroCore::Load(const std::filesystem::path& path, std::string& error) {
    Unload();

    path_ = path;
    const std::string path_utf8 = ToUtf8(path);
    handle_ = SDL_LoadObject(path_utf8.c_str());
    if (handle_ == nullptr) {
        error = std::string("No se pudo abrir el core '") + ToUtf8(path) + "': " + SDL_GetError();
        return false;
    }

    bool ok = true;
    ok &= LoadRequired(handle_, "retro_init", retro_init, error);
    ok &= LoadRequired(handle_, "retro_deinit", retro_deinit, error);
    ok &= LoadRequired(handle_, "retro_api_version", retro_api_version, error);
    ok &= LoadRequired(handle_, "retro_get_system_info", retro_get_system_info, error);
    ok &= LoadRequired(handle_, "retro_get_system_av_info", retro_get_system_av_info, error);
    ok &= LoadRequired(handle_, "retro_set_environment", retro_set_environment, error);
    ok &= LoadRequired(handle_, "retro_set_video_refresh", retro_set_video_refresh, error);
    ok &= LoadRequired(handle_, "retro_set_audio_sample", retro_set_audio_sample, error);
    ok &= LoadRequired(handle_, "retro_set_audio_sample_batch", retro_set_audio_sample_batch, error);
    ok &= LoadRequired(handle_, "retro_set_input_poll", retro_set_input_poll, error);
    ok &= LoadRequired(handle_, "retro_set_input_state", retro_set_input_state, error);
    ok &= LoadRequired(handle_, "retro_reset", retro_reset, error);
    ok &= LoadRequired(handle_, "retro_run", retro_run, error);
    ok &= LoadRequired(handle_, "retro_load_game", retro_load_game, error);
    ok &= LoadRequired(handle_, "retro_unload_game", retro_unload_game, error);

    if (!ok) {
        Unload();
        error += "\n¿Es realmente un core libretro compatible?";
        return false;
    }

    LoadOptional(handle_, "retro_set_controller_port_device", retro_set_controller_port_device);
    LoadOptional(handle_, "retro_serialize_size", retro_serialize_size);
    LoadOptional(handle_, "retro_serialize", retro_serialize);
    LoadOptional(handle_, "retro_unserialize", retro_unserialize);
    LoadOptional(handle_, "retro_cheat_reset", retro_cheat_reset);
    LoadOptional(handle_, "retro_cheat_set", retro_cheat_set);
    LoadOptional(handle_, "retro_load_game_special", retro_load_game_special);
    LoadOptional(handle_, "retro_get_region", retro_get_region);
    LoadOptional(handle_, "retro_get_memory_data", retro_get_memory_data);
    LoadOptional(handle_, "retro_get_memory_size", retro_get_memory_size);

    return true;
}

void LibretroCore::Unload() {
    if (handle_ != nullptr) {
        SDL_UnloadObject(handle_);
        handle_ = nullptr;
    }

    retro_init = nullptr;
    retro_deinit = nullptr;
    retro_api_version = nullptr;
    retro_get_system_info = nullptr;
    retro_get_system_av_info = nullptr;
    retro_set_environment = nullptr;
    retro_set_video_refresh = nullptr;
    retro_set_audio_sample = nullptr;
    retro_set_audio_sample_batch = nullptr;
    retro_set_input_poll = nullptr;
    retro_set_input_state = nullptr;
    retro_set_controller_port_device = nullptr;
    retro_reset = nullptr;
    retro_run = nullptr;
    retro_serialize_size = nullptr;
    retro_serialize = nullptr;
    retro_unserialize = nullptr;
    retro_cheat_reset = nullptr;
    retro_cheat_set = nullptr;
    retro_load_game = nullptr;
    retro_load_game_special = nullptr;
    retro_unload_game = nullptr;
    retro_get_region = nullptr;
    retro_get_memory_data = nullptr;
    retro_get_memory_size = nullptr;
    path_.clear();
}
