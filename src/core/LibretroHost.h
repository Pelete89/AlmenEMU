// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "libretro.h"

class LibretroCore;
struct SDL_AudioStream;

struct CoreOptionValue {
    std::string value;
    std::string label;
};

struct CoreOption {
    std::string key;
    std::string label;
    std::string description;
    std::string category;
    std::string current;
    std::vector<CoreOptionValue> values;
};

// Owns the libretro lifecycle and translates its callbacks into plain C++
// state that the UI can consume. Only one host can be active at a time.
class LibretroHost {
public:
    LibretroHost() = default;
    ~LibretroHost();

    LibretroHost(const LibretroHost&) = delete;
    LibretroHost& operator=(const LibretroHost&) = delete;

    bool Initialize(LibretroCore& core, const std::filesystem::path& system_dir,
                    const std::filesystem::path& save_dir, std::string& error);
    void Shutdown();

    bool LoadGame(const std::filesystem::path& path, std::string& error);
    void UnloadGame();
    void RunFrame();
    void Reset();

    bool SaveState(const std::filesystem::path& path, std::string& error);
    bool LoadState(const std::filesystem::path& path, std::string& error);

    bool IsInitialized() const { return initialized_; }
    bool IsGameLoaded() const { return game_loaded_; }
    bool IsPaused() const { return paused_; }
    void SetPaused(bool paused) { paused_ = paused; }
    bool ShouldQuit() const { return quit_; }

    // Video. The framebuffer is always converted to XRGB8888.
    bool ConsumeVideoFrame();
    const uint32_t* Pixels() const { return video_buffer_.data(); }
    int VideoWidth() const { return video_width_; }
    int VideoHeight() const { return video_height_; }
    double AspectRatio() const;
    double Fps() const { return target_fps_; }

    // Input (RetroPad button ids 0..15).
    void SetButton(unsigned id, bool pressed);
    void SetAxis(unsigned index, int16_t x, int16_t y);

    // Core options.
    const std::map<std::string, CoreOption>& Options() const { return options_; }
    const std::map<std::string, std::string>& Categories() const { return categories_; }
    void SetOption(const std::string& key, const std::string& value);
    bool IsOptionAvailable(const std::string& key) const;

    const std::string& CoreName() const { return core_name_; }
    const std::string& CoreVersion() const { return core_version_; }
    const std::deque<std::string>& Log() const { return log_; }
    const std::string& LastMessage() const { return last_message_; }
    void AddLog(std::string message);

    bool HardwareRenderRequested() const { return hw_render_requested_; }

private:
    static bool EnvironmentCallback(unsigned cmd, void* data);
    static void VideoCallback(const void* data, unsigned width, unsigned height, size_t pitch);
    static void AudioSampleCallback(int16_t left, int16_t right);
    static size_t AudioBatchCallback(const int16_t* data, size_t frames);
    static void InputPollCallback();
    static int16_t InputStateCallback(unsigned port, unsigned device, unsigned index,
                                      unsigned id);
    static void LogCallback(enum retro_log_level level, const char* fmt, ...);

    bool HandleEnvironment(unsigned cmd, void* data);
    void ParseVariables(const retro_variable* variables);
    void ParseCoreOptions(const retro_core_option_definition* definitions);
    void ParseCoreOptionsV2(const retro_core_options_v2* options);
    void AddOption(const std::string& key, const std::string& label,
                   const std::string& description, const std::string& category,
                   const std::string& default_value,
                   std::vector<CoreOptionValue> values);
    void ForceSoftwareRendering();
    void OpenAudio();
    void CloseAudio();
    void QueueAudio(const int16_t* data, size_t frames);

    static std::string PathToUtf8(const std::filesystem::path& path);

    static LibretroHost* s_active;

    LibretroCore* core_ = nullptr;
    bool initialized_ = false;
    bool game_loaded_ = false;
    bool paused_ = false;
    bool quit_ = false;
    bool hw_render_requested_ = false;
    bool variables_dirty_ = false;
    bool audio_error_logged_ = false;

    std::filesystem::path system_dir_;
    std::filesystem::path save_dir_;
    std::string system_dir_utf8_;
    std::string save_dir_utf8_;

    retro_system_info system_info_{};
    retro_system_av_info av_info_{};
    std::string core_name_;
    std::string core_version_;
    double target_fps_ = 60.0;

    retro_pixel_format pixel_format_ = RETRO_PIXEL_FORMAT_XRGB8888;
    std::vector<uint32_t> video_buffer_;
    int video_width_ = 0;
    int video_height_ = 0;
    bool video_dirty_ = false;

    std::array<bool, 16> buttons_{};
    std::array<std::array<int16_t, 2>, 2> analog_{};

    std::map<std::string, CoreOption> options_;
    std::map<std::string, std::string> categories_;

    SDL_AudioStream* audio_stream_ = nullptr;
    int audio_sample_rate_ = 0;

    std::deque<std::string> log_;
    std::string last_message_;
};
