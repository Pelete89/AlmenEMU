// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/LibretroCore.h"
#include "core/LibretroHost.h"
#include "video/VideoBackend.h"

class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run(int argc, char** argv);

private:
    bool Init();
    bool CreateWindowAndContext(std::string& error);
    void Shutdown();
    void MainLoop();

    void HandleEvent(const SDL_Event& event);
    void UpdateInput();
    void UpdateGamepadButtons();
    void SetKeyState(SDL_Keycode key, bool down);

    void DrawUI();
    void DrawMenuBar();
    void DrawWelcome();
    void DrawGameView();
    void DrawSidePanel();
    void DrawCoreOptions();
    void DrawLog();
    void DrawErrorPopup();
    void DrawAboutPopup();

    void LoadCore(const std::filesystem::path& path);
    void LoadGame(const std::filesystem::path& path);
    void UnloadGame();
    void TogglePause();
    void ResetGame();
    void SaveState();
    void LoadState();
    void TakeScreenshot();
    void ToggleFullscreen();

    void ShowError(std::string message);
    void AddRecent(const std::filesystem::path& path);
    void LoadRecent();
    void SaveRecent();

    std::filesystem::path DefaultCorePath() const;
    std::filesystem::path StatePath() const;
    std::filesystem::path ScreenshotPath() const;

    void OpenFileDialog(bool for_core);
    static void SDLCALL FileDialogCallback(void* userdata, const char* const* filelist,
                                           int filter);

    static std::string PathToUtf8(const std::filesystem::path& path);

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    SDL_Gamepad* gamepad_ = nullptr;
    std::unique_ptr<VideoBackend> video_;

    bool running_ = true;
    bool imgui_ready_ = false;
    bool fullscreen_ = false;
    bool core_loaded_ = false;
    bool hardware_supported_ = false;
    bool hardware_enabled_ = true;
    bool force_software_ = false;
    bool show_side_panel_ = true;
    bool show_log_ = false;
    bool show_about_ = false;

    std::filesystem::path base_dir_;
    std::filesystem::path data_dir_;
    std::filesystem::path system_dir_;
    std::filesystem::path save_dir_;
    std::filesystem::path states_dir_;
    std::filesystem::path screenshots_dir_;
    std::filesystem::path config_dir_;

    std::filesystem::path core_path_;
    std::filesystem::path game_path_;

    LibretroCore core_;
    LibretroHost host_;

    std::string error_message_;
    bool error_pending_ = false;
    std::string status_ = "Selecciona un core y abre un juego.";
    std::string glsl_version_ = "#version 330 core";
    std::string renderer_info_;

    std::vector<std::filesystem::path> recent_;

    std::array<bool, 16> keyboard_buttons_{};
    std::array<bool, 16> gamepad_buttons_{};
    std::array<bool, 4> keyboard_axis_{}; // up, down, left, right

    std::optional<std::filesystem::path> pending_file_;
    bool pending_is_core_ = false;

    std::chrono::steady_clock::time_point last_tick_{};
    double frame_accumulator_ = 0.0;

    double ui_fps_ = 0.0;
    double core_fps_ = 0.0;
    int core_frames_ = 0;
    std::chrono::steady_clock::time_point fps_tick_{};
    int fps_frames_ = 0;
};
