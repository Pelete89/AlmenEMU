// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/Application.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::filesystem::path FromUtf8(const std::string& value) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 7.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 7.0f;
    style.TabRounding = 7.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(9.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.95f, 0.97f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.52f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.063f, 0.082f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.071f, 0.082f, 0.105f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.086f, 0.110f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.25f, 0.85f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.19f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.063f, 0.082f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.075f, 0.086f, 0.110f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.071f, 0.082f, 0.105f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.063f, 0.082f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.80f, 0.72f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.80f, 0.72f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.27f, 0.90f, 0.81f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.13f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.80f, 0.72f, 0.65f);
    colors[ImGuiCol_Header] = ImVec4(0.14f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.80f, 0.72f, 0.55f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.20f, 0.25f, 0.85f);
    colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.14f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.20f, 0.80f, 0.72f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.20f, 0.80f, 0.72f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
}

} // namespace

Application::~Application() {
    Shutdown();
}

int Application::Run(int argc, char** argv) {
    std::filesystem::path cli_core;
    std::filesystem::path cli_game;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            std::printf("AlmenEMU 0.1.0\n"
                        "Uso: AlmenEMU [--core <ruta>] [juego]\n\n"
                        "  --core <ruta>  Core libretro (azahar_libretro.dll/.so/.dylib)\n"
                        "  juego          ROM .3ds, .cci, .cxi, .app, .cia o .3dsx\n");
            return 0;
        }
        if (argument == "--core" && i + 1 < argc) {
            cli_core = FromUtf8(argv[++i]);
            continue;
        }
        if (argument == "--game" && i + 1 < argc) {
            cli_game = FromUtf8(argv[++i]);
            continue;
        }
        if (!argument.empty() && argument[0] != '-') {
            cli_game = FromUtf8(argument);
        }
    }

    if (!Init()) {
        return 1;
    }

    if (!cli_core.empty()) {
        LoadCore(cli_core);
    }
    if (!cli_game.empty()) {
        LoadGame(cli_game);
    }

    MainLoop();
    Shutdown();
    return 0;
}

bool Application::Init() {
    SDL_SetAppMetadata("AlmenEMU", "0.1.0", "com.almenemu.app");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "No se pudo inicializar SDL: %s\n", SDL_GetError());
        return false;
    }

    const char* base_path = SDL_GetBasePath();
    base_dir_ = base_path != nullptr ? FromUtf8(base_path)
                                     : std::filesystem::current_path();
    SDL_free(const_cast<char*>(base_path));

    char* pref_path = SDL_GetPrefPath("AlmenEMU", "AlmenEMU");
    data_dir_ = pref_path != nullptr ? FromUtf8(pref_path) : base_dir_ / "data";
    SDL_free(pref_path);

    system_dir_ = data_dir_ / "system";
    save_dir_ = data_dir_ / "saves";
    states_dir_ = data_dir_ / "states";
    screenshots_dir_ = data_dir_ / "screenshots";
    config_dir_ = data_dir_ / "config";

    std::error_code ec;
    std::filesystem::create_directories(system_dir_, ec);
    std::filesystem::create_directories(save_dir_, ec);
    std::filesystem::create_directories(states_dir_, ec);
    std::filesystem::create_directories(screenshots_dir_, ec);
    std::filesystem::create_directories(config_dir_, ec);

    LoadRecent();

    window_ = SDL_CreateWindow("AlmenEMU", 1280, 800, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::fprintf(stderr, "No se pudo crear la ventana: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, "software");
    }
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "No se pudo crear el renderer: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ApplyStyle();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) {
        std::fprintf(stderr, "No se pudo inicializar ImGui (SDL3).\n");
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer_)) {
        std::fprintf(stderr, "No se pudo inicializar ImGui (renderer).\n");
        return false;
    }
    imgui_ready_ = true;

    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    if (gamepads != nullptr) {
        if (gamepad_count > 0) {
            gamepad_ = SDL_OpenGamepad(gamepads[0]);
        }
        SDL_free(gamepads);
    }

    fps_tick_ = std::chrono::steady_clock::now();
    last_tick_ = fps_tick_;

    const auto default_core = DefaultCorePath();
    if (!default_core.empty()) {
        LoadCore(default_core);
    } else {
        status_ = "No se encontró azahar_libretro. Usa 'Seleccionar core...' o ejecuta "
                  "scripts/fetch_core.py.";
    }

    return true;
}

void Application::Shutdown() {
    if (host_.IsInitialized()) {
        host_.Shutdown();
    }
    if (core_.IsLoaded()) {
        core_.Unload();
    }

    if (game_texture_ != nullptr) {
        SDL_DestroyTexture(game_texture_);
        game_texture_ = nullptr;
    }
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }

    if (imgui_ready_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imgui_ready_ = false;
    }

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void Application::MainLoop() {
    using Clock = std::chrono::steady_clock;

    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            HandleEvent(event);
        }

        if (pending_file_.has_value()) {
            const auto path = *pending_file_;
            const bool is_core = pending_is_core_;
            pending_file_.reset();
            if (is_core) {
                LoadCore(path);
            } else {
                LoadGame(path);
            }
        }

        UpdateInput();

        const auto now = Clock::now();
        double delta = std::chrono::duration<double>(now - last_tick_).count();
        last_tick_ = now;
        if (delta > 0.25) {
            delta = 0.25;
        }

        if (host_.IsGameLoaded() && !host_.IsPaused()) {
            frame_accumulator_ += delta * host_.Fps();
            int frames = 0;
            while (frame_accumulator_ >= 1.0 && frames < 4) {
                host_.RunFrame();
                frame_accumulator_ -= 1.0;
                ++frames;
            }
            if (frame_accumulator_ > 3.0) {
                frame_accumulator_ = 0.0;
            }
        } else {
            frame_accumulator_ = 0.0;
        }

        UpdateTexture();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        DrawUI();
        ImGui::Render();

        SDL_SetRenderDrawColor(renderer_, 14, 16, 21, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);

        ++fps_frames_;
        const auto fps_now = Clock::now();
        const double fps_delta = std::chrono::duration<double>(fps_now - fps_tick_).count();
        if (fps_delta >= 0.5) {
            ui_fps_ = static_cast<double>(fps_frames_) / fps_delta;
            fps_frames_ = 0;
            fps_tick_ = fps_now;
        }

        // Pace the UI at ~60 Hz when vsync is not available.
        const auto frame_end = Clock::now();
        const double frame_seconds =
            std::chrono::duration<double>(frame_end - now).count();
        constexpr double target_seconds = 1.0 / 60.0;
        if (frame_seconds < target_seconds) {
            SDL_Delay(static_cast<Uint32>((target_seconds - frame_seconds) * 1000.0));
        }
    }
}

void Application::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        running_ = false;
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
        if (gamepad_ == nullptr) {
            gamepad_ = SDL_OpenGamepad(event.gdevice.which);
        }
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (gamepad_ != nullptr) {
            SDL_Joystick* joystick = SDL_GetGamepadJoystick(gamepad_);
            if (joystick != nullptr && SDL_GetJoystickID(joystick) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
            }
        }
        break;

    case SDL_EVENT_DROP_FILE: {
        if (event.drop.data == nullptr) {
            break;
        }
        const auto path = FromUtf8(event.drop.data);
        const std::string extension = ToLower(path.extension().string());
        if (extension == ".dll" || extension == ".so" || extension == ".dylib") {
            LoadCore(path);
        } else {
            LoadGame(path);
        }
        break;
    }

    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode key = event.key.key;
        const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;

        if (event.key.repeat) {
            break;
        }

        if (key == SDLK_F1) {
            TogglePause();
        } else if (key == SDLK_F2) {
            ResetGame();
        } else if (key == SDLK_F5) {
            SaveState();
        } else if (key == SDLK_F9) {
            LoadState();
        } else if (key == SDLK_F11) {
            ToggleFullscreen();
        } else if (key == SDLK_F12) {
            TakeScreenshot();
        } else if (key == SDLK_ESCAPE && fullscreen_) {
            ToggleFullscreen();
        } else if (key == SDLK_Q && ctrl) {
            running_ = false;
        } else if (key == SDLK_O && ctrl) {
            OpenFileDialog(false);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            SetKeyState(key, true);
        }
        break;
    }

    case SDL_EVENT_KEY_UP:
        SetKeyState(event.key.key, false);
        break;

    default:
        break;
    }
}

void Application::SetKeyState(SDL_Keycode key, bool down) {
    switch (key) {
    case SDLK_UP:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_UP] = down;
        break;
    case SDLK_DOWN:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_DOWN] = down;
        break;
    case SDLK_LEFT:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_LEFT] = down;
        break;
    case SDLK_RIGHT:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_RIGHT] = down;
        break;
    case SDLK_W:
        keyboard_axis_[0] = down;
        break;
    case SDLK_S:
        keyboard_axis_[1] = down;
        break;
    case SDLK_A:
        keyboard_axis_[2] = down;
        break;
    case SDLK_D:
        keyboard_axis_[3] = down;
        break;
    case SDLK_K:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_A] = down;
        break;
    case SDLK_J:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_B] = down;
        break;
    case SDLK_L:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_X] = down;
        break;
    case SDLK_I:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_Y] = down;
        break;
    case SDLK_Q:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_L] = down;
        break;
    case SDLK_E:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_R] = down;
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_START] = down;
        break;
    case SDLK_BACKSPACE:
        keyboard_buttons_[RETRO_DEVICE_ID_JOYPAD_SELECT] = down;
        break;
    default:
        break;
    }
}

void Application::UpdateGamepadButtons() {
    gamepad_buttons_.fill(false);
    if (gamepad_ == nullptr) {
        return;
    }

    const auto pressed = [&](SDL_GamepadButton button) {
        return SDL_GetGamepadButton(gamepad_, button);
    };

    // Nintendo layout: South = A, East = B, North = X, West = Y.
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_A] = pressed(SDL_GAMEPAD_BUTTON_SOUTH);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_B] = pressed(SDL_GAMEPAD_BUTTON_EAST);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_X] = pressed(SDL_GAMEPAD_BUTTON_NORTH);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_Y] = pressed(SDL_GAMEPAD_BUTTON_WEST);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_L] = pressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_R] = pressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_START] = pressed(SDL_GAMEPAD_BUTTON_START);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_SELECT] = pressed(SDL_GAMEPAD_BUTTON_BACK);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_UP] = pressed(SDL_GAMEPAD_BUTTON_DPAD_UP);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_DOWN] = pressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_LEFT] = pressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    gamepad_buttons_[RETRO_DEVICE_ID_JOYPAD_RIGHT] = pressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
}

void Application::UpdateInput() {
    UpdateGamepadButtons();

    for (size_t i = 0; i < keyboard_buttons_.size(); ++i) {
        host_.SetButton(static_cast<unsigned>(i), keyboard_buttons_[i] || gamepad_buttons_[i]);
    }

    int16_t x = 0;
    int16_t y = 0;
    if (keyboard_axis_[2]) {
        x = -32767;
    } else if (keyboard_axis_[3]) {
        x = 32767;
    }
    if (keyboard_axis_[0]) {
        y = -32767;
    } else if (keyboard_axis_[1]) {
        y = 32767;
    }

    if (gamepad_ != nullptr) {
        constexpr int16_t deadzone = 8000;
        int16_t pad_x = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX);
        int16_t pad_y = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY);
        if (std::abs(static_cast<int>(pad_x)) < deadzone) {
            pad_x = 0;
        }
        if (std::abs(static_cast<int>(pad_y)) < deadzone) {
            pad_y = 0;
        }
        if (pad_x != 0) {
            x = pad_x;
        }
        if (pad_y != 0) {
            y = pad_y;
        }

        int16_t right_x = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX);
        int16_t right_y = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY);
        if (std::abs(static_cast<int>(right_x)) < deadzone) {
            right_x = 0;
        }
        if (std::abs(static_cast<int>(right_y)) < deadzone) {
            right_y = 0;
        }
        host_.SetAxis(1, right_x, right_y);
    } else {
        host_.SetAxis(1, 0, 0);
    }

    host_.SetAxis(0, x, y);
}

void Application::UpdateTexture() {
    if (!host_.ConsumeVideoFrame()) {
        return;
    }

    const int width = host_.VideoWidth();
    const int height = host_.VideoHeight();
    if (width <= 0 || height <= 0) {
        return;
    }

    if (game_texture_ == nullptr || width != video_width_ || height != video_height_) {
        if (game_texture_ != nullptr) {
            SDL_DestroyTexture(game_texture_);
            game_texture_ = nullptr;
        }
        game_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, width, height);
        if (game_texture_ == nullptr) {
            status_ = std::string("No se pudo crear la textura del juego: ") + SDL_GetError();
            return;
        }
        SDL_SetTextureScaleMode(game_texture_, SDL_SCALEMODE_LINEAR);
        video_width_ = width;
        video_height_ = height;
    }

    SDL_UpdateTexture(game_texture_, nullptr, host_.Pixels(), width * 4);
}

void Application::LoadCore(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        ShowError("El core no existe:\n" + PathToUtf8(path));
        return;
    }

    if (host_.IsInitialized()) {
        host_.Shutdown();
    }
    if (core_.IsLoaded()) {
        core_.Unload();
    }

    core_loaded_ = false;
    game_path_.clear();

    if (game_texture_ != nullptr) {
        SDL_DestroyTexture(game_texture_);
        game_texture_ = nullptr;
    }
    video_width_ = 0;
    video_height_ = 0;

    std::string error;
    if (!core_.Load(path, error)) {
        ShowError("No se pudo cargar el core:\n" + error);
        return;
    }

    if (!host_.Initialize(core_, system_dir_, save_dir_, error)) {
        ShowError("No se pudo inicializar el core:\n" + error);
        host_.Shutdown();
        core_.Unload();
        return;
    }

    core_loaded_ = true;
    core_path_ = path;
    status_ = "Core listo. Abre una ROM para empezar.";
}

void Application::LoadGame(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        ShowError("La ROM no existe:\n" + PathToUtf8(path));
        return;
    }

    if (!core_loaded_) {
        const auto default_core = DefaultCorePath();
        if (default_core.empty()) {
            ShowError("No hay ningún core cargado.\n\nSelecciona azahar_libretro con "
                      "'Archivo > Seleccionar core...' o ejecuta scripts/fetch_core.py.");
            return;
        }
        LoadCore(default_core);
        if (!core_loaded_) {
            return;
        }
    }

    std::string error;
    if (!host_.LoadGame(path, error)) {
        ShowError("No se pudo cargar el juego:\n" + error);
        return;
    }

    game_path_ = path;

    if (game_texture_ != nullptr) {
        SDL_DestroyTexture(game_texture_);
        game_texture_ = nullptr;
    }
    video_width_ = 0;
    video_height_ = 0;

    last_tick_ = std::chrono::steady_clock::now();
    frame_accumulator_ = 0.0;
    status_ = "Jugando: " + PathToUtf8(path.filename());
    AddRecent(path);
}

void Application::UnloadGame() {
    if (host_.IsGameLoaded()) {
        host_.UnloadGame();
    }
    game_path_.clear();
    if (game_texture_ != nullptr) {
        SDL_DestroyTexture(game_texture_);
        game_texture_ = nullptr;
    }
    video_width_ = 0;
    video_height_ = 0;
    status_ = "Juego cerrado.";
}

void Application::TogglePause() {
    if (!host_.IsGameLoaded()) {
        return;
    }
    host_.SetPaused(!host_.IsPaused());
    status_ = host_.IsPaused() ? "Pausado." : "Reanudado.";
}

void Application::ResetGame() {
    if (!host_.IsGameLoaded()) {
        return;
    }
    host_.Reset();
    status_ = "Juego reiniciado.";
}

void Application::SaveState() {
    if (!host_.IsGameLoaded()) {
        return;
    }
    std::string error;
    if (!host_.SaveState(StatePath(), error)) {
        ShowError(error);
        return;
    }
    status_ = "Savestate guardado.";
}

void Application::LoadState() {
    if (!host_.IsGameLoaded()) {
        return;
    }
    std::string error;
    if (!host_.LoadState(StatePath(), error)) {
        ShowError(error);
        return;
    }
    status_ = "Savestate cargado.";
}

void Application::TakeScreenshot() {
    if (!host_.IsGameLoaded() || host_.Pixels() == nullptr || host_.VideoWidth() <= 0 ||
        host_.VideoHeight() <= 0) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(screenshots_dir_, ec);

    const auto path = ScreenshotPath();
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        host_.VideoWidth(), host_.VideoHeight(), SDL_PIXELFORMAT_XRGB8888,
        const_cast<uint32_t*>(host_.Pixels()), host_.VideoWidth() * 4);
    if (surface == nullptr) {
        ShowError(std::string("No se pudo crear la captura: ") + SDL_GetError());
        return;
    }

    const std::string utf8_path = PathToUtf8(path);
    const bool saved = SDL_SaveBMP(surface, utf8_path.c_str());
    SDL_DestroySurface(surface);

    if (!saved) {
        ShowError(std::string("No se pudo guardar la captura: ") + SDL_GetError());
        return;
    }
    status_ = "Captura guardada: " + PathToUtf8(path.filename());
}

void Application::ToggleFullscreen() {
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(window_, fullscreen_);
}

void Application::ShowError(std::string message) {
    error_message_ = std::move(message);
    error_pending_ = true;
    status_ = error_message_;
    if (!imgui_ready_) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AlmenEMU", error_message_.c_str(),
                                 window_);
    }
}

void Application::AddRecent(const std::filesystem::path& path) {
    recent_.erase(std::remove(recent_.begin(), recent_.end(), path), recent_.end());
    recent_.insert(recent_.begin(), path);
    if (recent_.size() > 12) {
        recent_.resize(12);
    }
    SaveRecent();
}

void Application::LoadRecent() {
    recent_.clear();
    std::ifstream input(config_dir_ / "recent.txt");
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            recent_.push_back(FromUtf8(line));
        }
        if (recent_.size() >= 12) {
            break;
        }
    }
}

void Application::SaveRecent() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir_, ec);
    std::ofstream output(config_dir_ / "recent.txt", std::ios::trunc);
    if (!output) {
        return;
    }
    for (const auto& path : recent_) {
        output << PathToUtf8(path) << '\n';
    }
}

std::filesystem::path Application::DefaultCorePath() const {
#if defined(_WIN32)
    constexpr const char* filename = "azahar_libretro.dll";
#elif defined(__APPLE__)
    constexpr const char* filename = "azahar_libretro.dylib";
#else
    constexpr const char* filename = "azahar_libretro.so";
#endif

    const std::vector<std::filesystem::path> candidates = {
        base_dir_ / "cores" / filename,
        base_dir_ / filename,
        std::filesystem::current_path() / "cores" / filename,
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::filesystem::path Application::StatePath() const {
    if (game_path_.empty()) {
        return {};
    }
    return states_dir_ / FromUtf8(PathToUtf8(game_path_.stem()) + ".state");
}

std::filesystem::path Application::ScreenshotPath() const {
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const std::string name =
        PathToUtf8(game_path_.stem()) + "-" + std::to_string(stamp) + ".bmp";
    return screenshots_dir_ / FromUtf8(name);
}

void Application::OpenFileDialog(bool for_core) {
    pending_is_core_ = for_core;

    if (for_core) {
        SDL_DialogFileFilter filters[] = {
            {"Core libretro", "dll;so;dylib"},
            {"Todos los archivos", "*"},
        };
        SDL_ShowOpenFileDialog(&Application::FileDialogCallback, this, window_, filters, 2,
                               nullptr, false);
    } else {
        SDL_DialogFileFilter filters[] = {
            {"ROMs de Nintendo 3DS", "3ds;cci;cxi;app;3dsx;cia;elf;axf"},
            {"Todos los archivos", "*"},
        };
        SDL_ShowOpenFileDialog(&Application::FileDialogCallback, this, window_, filters, 2,
                               nullptr, false);
    }
}

void SDLCALL Application::FileDialogCallback(void* userdata, const char* const* filelist,
                                             int filter) {
    (void)filter;
    auto* app = static_cast<Application*>(userdata);
    if (app == nullptr || filelist == nullptr || filelist[0] == nullptr) {
        return;
    }
    app->pending_file_ = FromUtf8(filelist[0]);
}

std::string Application::PathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

void Application::DrawUI() {
    if (error_pending_) {
        ImGui::OpenPopup("Error");
        error_pending_ = false;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##Main", nullptr, flags);
    DrawMenuBar();

    if (show_side_panel_) {
        ImGui::BeginChild("##sidebar", ImVec2(360.0f, 0.0f), ImGuiChildFlags_Borders);
        DrawSidePanel();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (host_.IsGameLoaded()) {
        DrawGameView();
    } else {
        DrawWelcome();
    }
    ImGui::EndChild();

    ImGui::End();

    if (show_log_) {
        ImGui::SetNextWindowSize(ImVec2(760.0f, 280.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Registro", &show_log_);
        DrawLog();
        ImGui::End();
    }

    DrawErrorPopup();
    DrawAboutPopup();
}

void Application::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Archivo")) {
        if (ImGui::MenuItem("Abrir ROM...", "Ctrl+O")) {
            OpenFileDialog(false);
        }
        if (ImGui::MenuItem("Seleccionar core...")) {
            OpenFileDialog(true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Salir", "Ctrl+Q")) {
            running_ = false;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sistema")) {
        const bool has_game = host_.IsGameLoaded();
        if (ImGui::MenuItem(host_.IsPaused() ? "Reanudar" : "Pausar", "F1", false, has_game)) {
            TogglePause();
        }
        if (ImGui::MenuItem("Reiniciar", "F2", false, has_game)) {
            ResetGame();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Guardar estado", "F5", false, has_game)) {
            SaveState();
        }
        if (ImGui::MenuItem("Cargar estado", "F9", false, has_game)) {
            LoadState();
        }
        if (ImGui::MenuItem("Captura de pantalla", "F12", false, has_game)) {
            TakeScreenshot();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Pantalla completa", "F11", fullscreen_)) {
            ToggleFullscreen();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Ver")) {
        ImGui::MenuItem("Panel lateral", nullptr, &show_side_panel_);
        ImGui::MenuItem("Registro", nullptr, &show_log_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Ayuda")) {
        if (ImGui::MenuItem("Acerca de AlmenEMU")) {
            show_about_ = true;
        }
        ImGui::EndMenu();
    }

    if (core_loaded_) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", host_.CoreName().c_str());
    }

    ImGui::EndMenuBar();
}

void Application::DrawWelcome() {
    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImGui::SetWindowFontScale(2.2f);
    ImGui::TextUnformatted("AlmenEMU");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Frontend libretro para Nintendo 3DS");
    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Abrir ROM...", ImVec2(210.0f, 44.0f))) {
        OpenFileDialog(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Seleccionar core...", ImVec2(210.0f, 44.0f))) {
        OpenFileDialog(true);
    }
    ImGui::SameLine();
    if (!recent_.empty() && ImGui::Button("Abrir reciente", ImVec2(180.0f, 44.0f))) {
        LoadGame(recent_.front());
    }

    ImGui::Spacing();
    if (core_loaded_) {
        ImGui::TextColored(ImVec4(0.20f, 0.80f, 0.72f, 1.0f), "Core listo: %s %s",
                           host_.CoreName().c_str(), host_.CoreVersion().c_str());
    } else {
        ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.25f, 1.0f),
                           "Core pendiente: coloca azahar_libretro en cores/ o selecciónalo "
                           "manualmente.");
    }
    ImGui::TextDisabled("%s", status_.c_str());

    ImGui::Spacing();
    ImGui::SeparatorText("Recientes");
    ImGui::BeginChild("##recent", ImVec2(0.0f, 190.0f), ImGuiChildFlags_Borders);
    if (recent_.empty()) {
        ImGui::TextDisabled("Todavía no hay juegos recientes.");
        ImGui::TextDisabled("Arrastra un .3ds o .cia a la ventana para empezar.");
    } else {
        for (size_t i = 0; i < recent_.size(); ++i) {
            const auto& path = recent_[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(PathToUtf8(path.filename()).c_str())) {
                LoadGame(path);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", PathToUtf8(path).c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextDisabled(
        "Usa únicamente ROMs, BIOS y archivos de sistema dumpados de tu propia consola. "
        "AlmenEMU no incluye juegos ni claves.");
}

void Application::DrawGameView() {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    constexpr float footer_height = 132.0f;
    const float view_height = std::max(80.0f, available.y - footer_height);

    float aspect = static_cast<float>(host_.AspectRatio());
    if (aspect <= 0.01f) {
        aspect = 4.0f / 3.0f;
    }

    float image_width = available.x;
    float image_height = image_width / aspect;
    if (image_height > view_height) {
        image_height = view_height;
        image_width = image_height * aspect;
    }

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(cursor,
                             ImVec2(cursor.x + available.x, cursor.y + view_height),
                             IM_COL32(0, 0, 0, 255), 10.0f);

    const ImVec2 image_pos(cursor.x + (available.x - image_width) * 0.5f,
                           cursor.y + (view_height - image_height) * 0.5f);

    if (game_texture_ != nullptr) {
        ImGui::SetCursorScreenPos(image_pos);
        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(game_texture_)),
                     ImVec2(image_width, image_height));
    } else {
        const char* text = "Esperando el primer frame...";
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        draw_list->AddText(ImVec2(cursor.x + (available.x - text_size.x) * 0.5f,
                                  cursor.y + (view_height - text_size.y) * 0.5f),
                           IM_COL32(180, 190, 200, 255), text);
    }

    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + view_height + 10.0f));
    ImGui::Separator();

    ImGui::Text("%s", PathToUtf8(game_path_.filename()).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| %s | %.2f fps | UI %.0f Hz", host_.CoreName().c_str(), host_.Fps(),
                        ui_fps_);
    if (host_.IsPaused()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "PAUSA");
    }

    ImGui::Spacing();
    if (ImGui::Button(host_.IsPaused() ? "Reanudar" : "Pausar", ImVec2(120.0f, 0.0f))) {
        TogglePause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reiniciar", ImVec2(120.0f, 0.0f))) {
        ResetGame();
    }
    ImGui::SameLine();
    if (ImGui::Button("Guardar estado", ImVec2(160.0f, 0.0f))) {
        SaveState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cargar estado", ImVec2(160.0f, 0.0f))) {
        LoadState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Captura", ImVec2(110.0f, 0.0f))) {
        TakeScreenshot();
    }

    if (host_.HardwareRenderRequested()) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f),
                           "El core pidió render hardware; ahora mismo se está usando software.");
    }
}

void Application::DrawSidePanel() {
    if (ImGui::BeginTabBar("##side_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Juego")) {
            if (host_.IsGameLoaded()) {
                ImGui::TextWrapped("%s", PathToUtf8(game_path_.filename()).c_str());
                ImGui::TextWrapped("%s", PathToUtf8(game_path_).c_str());
                ImGui::Separator();

                if (ImGui::Button(host_.IsPaused() ? "Reanudar" : "Pausar", ImVec2(-1.0f, 0.0f))) {
                    TogglePause();
                }
                if (ImGui::Button("Reiniciar", ImVec2(-1.0f, 0.0f))) {
                    ResetGame();
                }
                if (ImGui::Button("Guardar estado (F5)", ImVec2(-1.0f, 0.0f))) {
                    SaveState();
                }
                if (ImGui::Button("Cargar estado (F9)", ImVec2(-1.0f, 0.0f))) {
                    LoadState();
                }
                if (ImGui::Button("Captura (F12)", ImVec2(-1.0f, 0.0f))) {
                    TakeScreenshot();
                }
                ImGui::Spacing();
                if (ImGui::Button("Cerrar juego", ImVec2(-1.0f, 0.0f))) {
                    UnloadGame();
                }

                ImGui::SeparatorText("Estado");
                ImGui::Text("FPS core: %.2f", host_.Fps());
                ImGui::Text("Resolución: %dx%d", host_.VideoWidth(), host_.VideoHeight());
                ImGui::Text("Aspecto: %.3f", host_.AspectRatio());
            } else {
                ImGui::TextWrapped("No hay ningún juego cargado.");
                ImGui::Spacing();
                if (ImGui::Button("Abrir ROM...", ImVec2(-1.0f, 0.0f))) {
                    OpenFileDialog(false);
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Core")) {
            if (core_loaded_) {
                ImGui::Text("Nombre: %s", host_.CoreName().c_str());
                ImGui::Text("Versión: %s", host_.CoreVersion().c_str());
                ImGui::TextDisabled("%s", PathToUtf8(core_path_).c_str());
                ImGui::Spacing();
                if (ImGui::Button("Cambiar core...", ImVec2(-1.0f, 0.0f))) {
                    OpenFileDialog(true);
                }
                if (ImGui::Button("Recargar core", ImVec2(-1.0f, 0.0f))) {
                    LoadCore(core_path_);
                }
                ImGui::SeparatorText("Opciones del core");
                DrawCoreOptions();
            } else {
                ImGui::TextWrapped("No hay ningún core cargado.");
                ImGui::Spacing();
                if (ImGui::Button("Seleccionar core...", ImVec2(-1.0f, 0.0f))) {
                    OpenFileDialog(true);
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void Application::DrawCoreOptions() {
    if (host_.Options().empty()) {
        ImGui::TextDisabled("Este core no expone opciones.");
        return;
    }

    std::map<std::string, std::vector<const CoreOption*>> groups;
    for (const auto& [key, option] : host_.Options()) {
        (void)key;
        groups[option.category.empty() ? "General" : option.category].push_back(&option);
    }

    for (const auto& [category, options] : groups) {
        ImGui::SeparatorText(category.c_str());
        for (const CoreOption* option : options) {
            ImGui::PushID(option->key.c_str());
            ImGui::TextUnformatted(option->label.c_str());
            if (!option->description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", option->description.c_str());
            }

            std::string current_label = option->current;
            for (const auto& value : option->values) {
                if (value.value == option->current) {
                    current_label = value.label;
                    break;
                }
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##value", current_label.c_str())) {
                for (const auto& value : option->values) {
                    const bool selected = value.value == option->current;
                    if (ImGui::Selectable(value.label.c_str(), selected)) {
                        host_.SetOption(option->key, value.value);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (option->key == "citra_graphics_api" && option->current != "Software") {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                                   "El render hardware aún no está implementado.");
            }
            ImGui::PopID();
        }
    }
}

void Application::DrawLog() {
    ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (host_.Log().empty()) {
        ImGui::TextDisabled("Sin mensajes.");
    } else {
        for (const auto& line : host_.Log()) {
            ImGui::TextUnformatted(line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void Application::DrawErrorPopup() {
    if (!ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextWrapped("%s", error_message_.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Cerrar", ImVec2(140.0f, 0.0f))) {
        error_message_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void Application::DrawAboutPopup() {
    if (show_about_) {
        ImGui::OpenPopup("Acerca de AlmenEMU");
        show_about_ = false;
    }

    if (!ImGui::BeginPopupModal("Acerca de AlmenEMU", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("AlmenEMU 0.1.0");
    ImGui::TextDisabled("Frontend libretro para emulación de Nintendo 3DS");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Motor de emulación: Azahar (GPL-3.0), cargado como core libretro. "
        "Este frontend es software libre bajo GPL-3.0-or-later.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "No incluye ROMs, BIOS, firmware ni claves. Usa únicamente volcados de tu propia consola. "
        "No está afiliado a Nintendo ni al proyecto Azahar.");
    ImGui::Spacing();
    if (ImGui::Button("Cerrar", ImVec2(140.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
