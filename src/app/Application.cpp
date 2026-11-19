// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/Application.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "video/OpenGLBackend.h"

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
                        "Uso: AlmenEMU [--core <ruta>] [--software] [juego]\n\n"
                        "  --core <ruta>  Core libretro (azahar_libretro.dll/.so/.dylib)\n"
                        "  --software     Fuerza el renderer software del core\n"
                        "  --game <ruta>  Equivalente a pasar la ROM como argumento\n"
                        "  juego          ROM .3ds, .cci, .cxi, .app, .cia o .3dsx\n");
            return 0;
        }
        if (argument == "--core" && i + 1 < argc) {
            cli_core = FromUtf8(argv[++i]);
            continue;
        }
        if (argument == "--software") {
            force_software_ = true;
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

bool Application::CreateWindowAndContext(std::string& error) {
    struct Attempt {
        int major;
        int minor;
        const char* glsl;
    };

    const Attempt attempts[] = {
        {4, 3, "#version 430 core"},
        {4, 1, "#version 410 core"},
        {3, 3, "#version 330 core"},
    };

    std::string last_error;
    for (const auto& attempt : attempts) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, attempt.major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attempt.minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

        window_ = SDL_CreateWindow("AlmenEMU", 1280, 800,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (window_ == nullptr) {
            last_error = SDL_GetError();
            continue;
        }

        gl_context_ = SDL_GL_CreateContext(window_);
        if (gl_context_ == nullptr) {
            last_error = SDL_GetError();
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            continue;
        }

        if (!SDL_GL_MakeCurrent(window_, gl_context_)) {
            last_error = SDL_GetError();
            SDL_GL_DestroyContext(gl_context_);
            gl_context_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            continue;
        }

        // Vsync is intentionally disabled: on drivers where a core+UI frame
        // takes slightly longer than one refresh interval, blocking vsync
        // halves the UI rate to 30 Hz. The main loop paces at 60 Hz instead.
        SDL_GL_SetSwapInterval(0);

        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

        hardware_supported_ = (major > 4) || (major == 4 && minor >= 3);
        glsl_version_ = attempt.glsl;
        renderer_info_ = version != nullptr ? version : "OpenGL desconocido";
        if (renderer != nullptr) {
            renderer_info_ += " | ";
            renderer_info_ += renderer;
        }
        if (vendor != nullptr) {
            renderer_info_ += " | ";
            renderer_info_ += vendor;
        }
        status_ = std::string("OpenGL ") + std::to_string(major) + "." +
                  std::to_string(minor) +
                  (hardware_supported_ ? " (hardware disponible)" : " (software)");
        return true;
    }

    error = last_error.empty() ? "No se pudo crear un contexto OpenGL 3.3+." : last_error;
    return false;
}

bool Application::Init() {
    SDL_SetAppMetadata("AlmenEMU", "0.1.0", "com.almenemu.app");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "No se pudo inicializar SDL: %s\n", SDL_GetError());
        return false;
    }

    // SDL_GetBasePath() returns a cached buffer owned by SDL (freed by
    // SDL_Quit); it must not be passed to SDL_free(). SDL_GetPrefPath(), on the
    // other hand, returns a freshly allocated string owned by the caller.
    const char* base_path = SDL_GetBasePath();
    base_dir_ = base_path != nullptr ? FromUtf8(base_path)
                                     : std::filesystem::current_path();

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

    std::string gl_error;
    if (!CreateWindowAndContext(gl_error)) {
        std::fprintf(stderr, "No se pudo crear la ventana OpenGL: %s\n", gl_error.c_str());
        return false;
    }

    video_ = std::make_unique<OpenGLBackend>();
    if (!video_->Initialize(window_, gl_error)) {
        std::fprintf(stderr, "No se pudo inicializar el backend OpenGL: %s\n", gl_error.c_str());
        return false;
    }

    hardware_enabled_ = hardware_supported_ && !force_software_;
    host_.SetVideoBackend(video_.get());
    host_.SetHardwareEnabled(hardware_enabled_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ApplyStyle();

    if (!ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_)) {
        std::fprintf(stderr, "No se pudo inicializar ImGui (SDL3 OpenGL).\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version_.c_str())) {
        std::fprintf(stderr, "No se pudo inicializar ImGui (OpenGL3).\n");
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
    // Releasing the core's hardware context requires the GL context to be
    // current, so make it current before tearing the host down.
    if (gl_context_ != nullptr && window_ != nullptr) {
        SDL_GL_MakeCurrent(window_, gl_context_);
    }

    if (host_.IsInitialized()) {
        host_.Shutdown();
    }
    if (core_.IsLoaded()) {
        core_.Unload();
    }

    if (video_ != nullptr) {
        video_->Shutdown();
        video_.reset();
    }
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }

    if (imgui_ready_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imgui_ready_ = false;
    }

    if (gl_context_ != nullptr) {
        SDL_GL_DestroyContext(gl_context_);
        gl_context_ = nullptr;
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

        SDL_GL_MakeCurrent(window_, gl_context_);
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
            core_frames_ += frames;
            if (frame_accumulator_ > 3.0) {
                frame_accumulator_ = 0.0;
            }
        } else {
            frame_accumulator_ = 0.0;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        DrawUI();
        ImGui::Render();

        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GetWindowSizeInPixels(window_, &drawable_width, &drawable_height);
        if (drawable_width <= 0 || drawable_height <= 0) {
            drawable_width = 1;
            drawable_height = 1;
        }

        if (video_ != nullptr) {
            video_->BindDefaultFramebuffer();
        }
        glViewport(0, 0, drawable_width, drawable_height);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.055f, 0.063f, 0.082f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window_);

        ++fps_frames_;
        const auto fps_now = Clock::now();
        const double fps_delta = std::chrono::duration<double>(fps_now - fps_tick_).count();
        if (fps_delta >= 0.5) {
            ui_fps_ = static_cast<double>(fps_frames_) / fps_delta;
            core_fps_ = static_cast<double>(core_frames_) / fps_delta;
            if (SDL_getenv("ALMENEMU_LOG_FPS") != nullptr) {
                SDL_Log("FPS UI: %.2f | core: %.2f", ui_fps_, core_fps_);
            }
            fps_frames_ = 0;
            core_frames_ = 0;
            fps_tick_ = fps_now;
        }

        // Pace the UI/core loop at ~60 Hz. Vsync is disabled in context creation.
        const auto frame_end = Clock::now();
        const double frame_seconds =
            std::chrono::duration<double>(frame_end - now).count();
        constexpr double target_seconds = 1.0 / 60.0;
        if (frame_seconds < target_seconds) {
            SDL_DelayPrecise(
                static_cast<Uint64>((target_seconds - frame_seconds) * 1'000'000'000.0));
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

    if (video_ != nullptr) {
        video_->Reset();
    }

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

    host_.SetVideoBackend(video_.get());
    host_.SetHardwareEnabled(hardware_enabled_);

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

    last_tick_ = std::chrono::steady_clock::now();
    frame_accumulator_ = 0.0;
    core_frames_ = 0;
    core_fps_ = 0.0;
    status_ = "Jugando: " + PathToUtf8(path.filename()) +
              (video_ != nullptr ? " | " + std::string(video_->Name()) : "");
    AddRecent(path);
}

void Application::UnloadGame() {
    if (host_.IsGameLoaded()) {
        host_.UnloadGame();
    }
    game_path_.clear();
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
    if (!host_.IsGameLoaded()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(screenshots_dir_, ec);

    std::vector<uint32_t> pixels;
    int width = 0;
    int height = 0;
    if (video_ == nullptr || !video_->ReadPixels(pixels, width, height)) {
        ShowError("Todavía no hay un frame para capturar.");
        return;
    }

    const auto path = ScreenshotPath();
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        width, height, SDL_PIXELFORMAT_XRGB8888, pixels.data(), width * 4);
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

    const uintptr_t texture_id = video_ != nullptr ? video_->TextureId() : 0;
    if (texture_id != 0) {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        video_->GetTextureUVs(u0, v0, u1, v1);
        ImGui::SetCursorScreenPos(image_pos);
        ImGui::Image(static_cast<ImTextureID>(texture_id), ImVec2(image_width, image_height),
                     ImVec2(u0, v0), ImVec2(u1, v1));
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
    ImGui::TextDisabled("| %s | %s | core %.1f fps | UI %.0f Hz", host_.CoreName().c_str(),
                        video_ != nullptr ? video_->Name() : "sin backend",
                        core_fps_ > 0.0 ? core_fps_ : host_.Fps(), ui_fps_);
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

    if (video_ != nullptr) {
        ImGui::TextDisabled("Render: %s", video_->Name());
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
                ImGui::Text("FPS core: %.1f", core_fps_ > 0.0 ? core_fps_ : host_.Fps());
                ImGui::Text("Resolución: %dx%d",
                            video_ != nullptr ? video_->FrameWidth() : 0,
                            video_ != nullptr ? video_->FrameHeight() : 0);
                ImGui::Text("Aspecto: %.3f", host_.AspectRatio());
                ImGui::Text("Render: %s", video_ != nullptr ? video_->Name() : "sin backend");
                if (!renderer_info_.empty()) {
                    ImGui::TextWrapped("Driver: %s", renderer_info_.c_str());
                }
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

            const bool locked = option->key == "citra_graphics_api" && host_.IsGameLoaded();
            ImGui::BeginDisabled(locked);
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
            ImGui::EndDisabled();

            if (locked) {
                ImGui::TextDisabled("Requiere reiniciar el juego.");
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
    if (video_ != nullptr) {
        ImGui::Text("Render: %s", video_->Name());
    }
    if (!renderer_info_.empty()) {
        ImGui::TextWrapped("Driver: %s", renderer_info_.c_str());
    }
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
