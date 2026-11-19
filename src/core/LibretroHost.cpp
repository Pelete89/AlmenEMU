// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/LibretroHost.h"

#include "core/LibretroCore.h"
#include "video/VideoBackend.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::string Trim(std::string value) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
    return value;
}


} // namespace

LibretroHost* LibretroHost::s_active = nullptr;

LibretroHost::~LibretroHost() {
    Shutdown();
}

bool LibretroHost::Initialize(LibretroCore& core, const std::filesystem::path& system_dir,
                              const std::filesystem::path& save_dir, std::string& error) {
    if (initialized_) {
        Shutdown();
    }

    if (!core.IsLoaded()) {
        error = "El core no está cargado.";
        return false;
    }

    core_ = &core;
    system_dir_ = system_dir;
    save_dir_ = save_dir;

    std::error_code ec;
    std::filesystem::create_directories(system_dir_, ec);
    if (ec) {
        error = "No se pudo crear el directorio de sistema: " + ec.message();
        return false;
    }
    ec.clear();
    std::filesystem::create_directories(save_dir_, ec);
    if (ec) {
        error = "No se pudo crear el directorio de guardado: " + ec.message();
        return false;
    }

    system_dir_utf8_ = PathToUtf8(system_dir_);
    save_dir_utf8_ = PathToUtf8(save_dir_);

    log_.clear();
    options_.clear();
    categories_.clear();
    hw_render_requested_ = false;
    hardware_active_ = false;
    variables_dirty_ = false;
    quit_ = false;

    s_active = this;

    // The environment callback must be installed before retro_init().
    core_->retro_set_environment(&LibretroHost::EnvironmentCallback);
    core_->retro_init();

    initialized_ = true;

    core_->retro_set_video_refresh(&LibretroHost::VideoCallback);
    core_->retro_set_audio_sample(&LibretroHost::AudioSampleCallback);
    core_->retro_set_audio_sample_batch(&LibretroHost::AudioBatchCallback);
    core_->retro_set_input_poll(&LibretroHost::InputPollCallback);
    core_->retro_set_input_state(&LibretroHost::InputStateCallback);

    core_->retro_get_system_info(&system_info_);
    core_name_ = system_info_.library_name ? system_info_.library_name : "Core libretro";
    core_version_ = system_info_.library_version ? system_info_.library_version : "";

    if (core_->retro_set_controller_port_device != nullptr) {
        core_->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    }

    AddLog("Core cargado: " + core_name_ + (core_version_.empty() ? "" : " " + core_version_));
    return true;
}

void LibretroHost::Shutdown() {
    if (game_loaded_) {
        UnloadGame();
    }

    CloseAudio();

    // The core may still own GPU resources created in context_reset(); release
    // them with context_destroy before retro_deinit() and before the core
    // library is unloaded, while the frontend GPU context is still current.
    if (video_ != nullptr) {
        video_->DestroyHardwareContext();
    }

    if (initialized_ && core_ != nullptr) {
        core_->retro_deinit();
    }

    initialized_ = false;
    core_ = nullptr;
    s_active = nullptr;
    game_loaded_ = false;
    paused_ = false;
    hw_render_requested_ = false;
    hardware_active_ = false;
    video_ = nullptr;
    options_.clear();
    categories_.clear();
}

bool LibretroHost::LoadGame(const std::filesystem::path& path, std::string& error) {
    if (!initialized_ || core_ == nullptr) {
        error = "Primero hay que cargar un core.";
        return false;
    }
    if (game_loaded_) {
        UnloadGame();
    }

    // A previous failed load may have left a hardware request accepted.
    hw_render_requested_ = false;

    ConfigureGraphicsApi();

    const std::string path_utf8 = PathToUtf8(path);
    retro_game_info info{};
    info.path = path_utf8.c_str();
    info.data = nullptr;
    info.size = 0;
    info.meta = nullptr;

    last_message_.clear();
    AddLog("Cargando juego: " + path_utf8);

    if (!core_->retro_load_game(&info)) {
        error = "El core no pudo cargar el juego.";
        if (!last_message_.empty()) {
            error += "\n" + last_message_;
        }
        if (hw_render_requested_) {
            error += "\nEl core pidió render hardware, pero el backend no pudo inicializarlo. "
                     "Prueba con --software.";
        }
        if (hw_render_requested_ && video_ != nullptr) {
            video_->DestroyHardwareContext();
            hw_render_requested_ = false;
        }
        return false;
    }

    game_loaded_ = true;
    paused_ = false;
    quit_ = false;

    core_->retro_get_system_av_info(&av_info_);
    target_fps_ = av_info_.timing.fps > 1.0 ? av_info_.timing.fps : 60.0;

    const unsigned base_width = av_info_.geometry.base_width > 0
                                    ? av_info_.geometry.base_width : 400;
    const unsigned base_height = av_info_.geometry.base_height > 0
                                     ? av_info_.geometry.base_height : 480;
    if (video_ != nullptr) {
        video_->ResizeFramebuffer(base_width, base_height);
    }

    if (hw_render_requested_) {
        if (video_ == nullptr || !video_->ContextReset(error)) {
            if (error.empty()) {
                error = "No hay backend de vídeo para el render hardware.";
            }
            UnloadGame();
            return false;
        }
        hardware_active_ = video_->IsHardware();
    } else {
        hardware_active_ = false;
    }

    OpenAudio();

    if (core_->retro_set_controller_port_device != nullptr) {
        core_->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    }

    AddLog("Juego cargado. Resolución base: " + std::to_string(base_width) + "x" +
           std::to_string(base_height) + ", " + std::to_string(target_fps_) + " fps, render: " +
           (video_ != nullptr ? video_->Name() : "sin backend"));
    return true;
}

void LibretroHost::UnloadGame() {
    if (core_ != nullptr && game_loaded_) {
        // libretro calls context_destroy before the frontend tears the context
        // down, so it must run while the GPU context is current and the core is
        // still loaded. Azahar frees its renderer here only when the game is
        // still loaded, so it has to happen before retro_unload_game().
        if (video_ != nullptr) {
            video_->DestroyHardwareContext();
        }
        core_->retro_unload_game();
    }
    game_loaded_ = false;
    paused_ = false;
    hw_render_requested_ = false;
    hardware_active_ = false;
    CloseAudio();
}

void LibretroHost::RunFrame() {
    if (initialized_ && game_loaded_ && !paused_ && core_ != nullptr) {
        core_->retro_run();
    }
}

void LibretroHost::Reset() {
    if (initialized_ && game_loaded_ && core_ != nullptr) {
        core_->retro_reset();
        paused_ = false;
    }
}

bool LibretroHost::SaveState(const std::filesystem::path& path, std::string& error) {
    if (core_ == nullptr || !game_loaded_ || core_->retro_serialize_size == nullptr ||
        core_->retro_serialize == nullptr) {
        error = "Este core no soporta savestates.";
        return false;
    }

    const size_t size = core_->retro_serialize_size();
    if (size == 0) {
        error = "El core devolvió un savestate vacío.";
        return false;
    }

    std::vector<uint8_t> data(size);
    if (!core_->retro_serialize(data.data(), data.size())) {
        error = "El core no pudo serializar el estado.";
        return false;
    }

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = "No se pudo escribir el savestate: " + path.string();
        return false;
    }
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    if (!output) {
        error = "Error escribiendo el savestate.";
        return false;
    }

    AddLog("Savestate guardado: " + path.string());
    return true;
}

bool LibretroHost::LoadState(const std::filesystem::path& path, std::string& error) {
    if (core_ == nullptr || !game_loaded_ || core_->retro_unserialize == nullptr) {
        error = "Este core no soporta savestates.";
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "No se encontró el savestate: " + path.string();
        return false;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    if (data.empty()) {
        error = "El savestate está vacío.";
        return false;
    }

    if (!core_->retro_unserialize(data.data(), data.size())) {
        error = "El core no pudo restaurar el estado.";
        return false;
    }

    AddLog("Savestate cargado: " + path.string());
    return true;
}

double LibretroHost::AspectRatio() const {
    if (video_ != nullptr && video_->FrameWidth() > 0 && video_->FrameHeight() > 0) {
        return static_cast<double>(video_->FrameWidth()) /
               static_cast<double>(video_->FrameHeight());
    }
    if (av_info_.geometry.aspect_ratio > 0.01) {
        return av_info_.geometry.aspect_ratio;
    }
    return 4.0 / 3.0;
}

void LibretroHost::SetButton(unsigned id, bool pressed) {
    if (id < buttons_.size()) {
        buttons_[id] = pressed;
    }
}

void LibretroHost::SetAxis(unsigned index, int16_t x, int16_t y) {
    if (index < analog_.size()) {
        analog_[index][0] = x;
        analog_[index][1] = y;
    }
}

void LibretroHost::SetOption(const std::string& key, const std::string& value) {
    if (key == "citra_graphics_api" && game_loaded_) {
        AddLog("Cambiar la API gráfica requiere reiniciar el juego.");
        return;
    }

    auto it = options_.find(key);
    if (it == options_.end()) {
        return;
    }

    if (!it->second.values.empty()) {
        const bool valid = std::any_of(it->second.values.begin(), it->second.values.end(),
                                       [&](const CoreOptionValue& item) {
                                           return item.value == value;
                                       });
        if (!valid) {
            return;
        }
    }

    if (it->second.current != value) {
        it->second.current = value;
        variables_dirty_ = true;
        AddLog("Opción '" + key + "' = " + value);
    }
}

void LibretroHost::AddLog(std::string message) {
    if (message.empty()) {
        return;
    }
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    if (message.empty()) {
        return;
    }

    log_.push_back(message);
    while (log_.size() > 300) {
        log_.pop_front();
    }
    SDL_Log("%s", message.c_str());
}

bool LibretroHost::EnvironmentCallback(unsigned cmd, void* data) {
    if (s_active == nullptr) {
        return false;
    }
    return s_active->HandleEnvironment(cmd, data);
}

bool LibretroHost::HandleEnvironment(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        auto* callback = static_cast<retro_log_callback*>(data);
        callback->log = &LibretroHost::LogCallback;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char**>(data) = system_dir_utf8_.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = save_dir_utf8_.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        *static_cast<const char**>(data) = system_dir_utf8_.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY:
        *static_cast<const char**>(data) = save_dir_utf8_.c_str();
        return true;

    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const auto format = *static_cast<const retro_pixel_format*>(data);
        if (format == RETRO_PIXEL_FORMAT_XRGB8888 || format == RETRO_PIXEL_FORMAT_RGB565 ||
            format == RETRO_PIXEL_FORMAT_0RGB1555) {
            pixel_format_ = format;
            return true;
        }
        return false;
    }

    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        auto* callback = static_cast<retro_hw_render_callback*>(data);
        if (callback == nullptr) {
            AddLog("El core envió SET_HW_RENDER sin callback.");
            return false;
        }
        // The frontend only implements desktop OpenGL. Reject Vulkan, GLES and
        // the D3D backends instead of handing them an OpenGL FBO; the core will
        // fall back to software rendering.
        if (callback->context_type != RETRO_HW_CONTEXT_OPENGL_CORE &&
            callback->context_type != RETRO_HW_CONTEXT_OPENGL) {
            AddLog("Render hardware rechazado: solo se soporta OpenGL de escritorio.");
            return false;
        }
        if (video_ == nullptr) {
            AddLog("El core pidió render hardware, pero no hay backend de vídeo.");
            return false;
        }
        std::string hw_error;
        if (!video_->SetupHardwareRender(callback, hw_error)) {
            AddLog("Render hardware no disponible: " + hw_error);
            return false;
        }
        // Only mark the request as accepted once the backend is ready; LoadGame
        // uses this flag to decide whether it must run context_reset().
        hw_render_requested_ = true;
        AddLog("Core solicitó render hardware OpenGL.");
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
        return false;
    case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
        return false;
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        auto* type = static_cast<retro_hw_context_type*>(data);
        if (hardware_enabled_ && video_ != nullptr && video_->SupportsHardware()) {
            *type = RETRO_HW_CONTEXT_OPENGL_CORE;
            return true;
        }
        *type = RETRO_HW_CONTEXT_NONE;
        return false;
    }

    case RETRO_ENVIRONMENT_SET_VARIABLES:
        ParseVariables(static_cast<const retro_variable*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        ParseCoreOptions(static_cast<const retro_core_option_definition*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
        const auto* intl = static_cast<const retro_core_options_intl*>(data);
        if (intl != nullptr && intl->us != nullptr) {
            ParseCoreOptions(intl->us);
            return true;
        }
        return false;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        ParseCoreOptionsV2(static_cast<const retro_core_options_v2*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
        const auto* intl = static_cast<const retro_core_options_v2_intl*>(data);
        if (intl != nullptr && intl->us != nullptr) {
            ParseCoreOptionsV2(intl->us);
            return true;
        }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* variable = static_cast<retro_variable*>(data);
        const auto it = options_.find(variable->key);
        if (it == options_.end()) {
            variable->value = nullptr;
            return false;
        }
        variable->value = it->second.current.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = variables_dirty_;
        variables_dirty_ = false;
        return true;

    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        return true;

    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
        av_info_.geometry = *static_cast<const retro_game_geometry*>(data);
        if (video_ != nullptr) {
            const unsigned width = av_info_.geometry.base_width > 0
                                       ? av_info_.geometry.base_width
                                       : av_info_.geometry.max_width;
            const unsigned height = av_info_.geometry.base_height > 0
                                        ? av_info_.geometry.base_height
                                        : av_info_.geometry.max_height;
            video_->ResizeFramebuffer(width, height);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
        av_info_ = *static_cast<const retro_system_av_info*>(data);
        if (av_info_.timing.fps > 1.0) {
            target_fps_ = av_info_.timing.fps;
        }
        if (video_ != nullptr) {
            const unsigned width = av_info_.geometry.base_width > 0
                                       ? av_info_.geometry.base_width
                                       : av_info_.geometry.max_width;
            const unsigned height = av_info_.geometry.base_height > 0
                                        ? av_info_.geometry.base_height
                                        : av_info_.geometry.max_height;
            video_->ResizeFramebuffer(width, height);
        }
        return true;
    }

    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *static_cast<int*>(data) = 3; // video + audio
        return true;
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
        *static_cast<float*>(data) = static_cast<float>(target_fps_);
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *static_cast<unsigned*>(data) = RETRO_LANGUAGE_SPANISH;
        return true;

    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        const auto* message = static_cast<const retro_message*>(data);
        if (message != nullptr && message->msg != nullptr) {
            last_message_ = message->msg;
            AddLog(message->msg);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
        const auto* message = static_cast<const retro_message_ext*>(data);
        if (message != nullptr && message->msg != nullptr) {
            last_message_ = message->msg;
            AddLog(message->msg);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SHUTDOWN:
        quit_ = true;
        return true;

    case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
    case RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE:
    case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
    case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
    default:
        return false;
    }
}

void LibretroHost::VideoCallback(const void* data, unsigned width, unsigned height, size_t pitch) {
    if (s_active == nullptr) {
        return;
    }

    LibretroHost& self = *s_active;
    if (self.video_ == nullptr) {
        return;
    }

    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        self.hardware_active_ = true;
        self.video_->OnHardwareFrame(width, height);
        return;
    }

    if (data == nullptr) {
        return;
    }

    self.hardware_active_ = false;
    self.video_->OnSoftwareFrame(data, width, height, pitch, self.pixel_format_);
}

void LibretroHost::AudioSampleCallback(int16_t left, int16_t right) {
    if (s_active == nullptr) {
        return;
    }
    const int16_t frame[2] = {left, right};
    s_active->QueueAudio(frame, 1);
}

size_t LibretroHost::AudioBatchCallback(const int16_t* data, size_t frames) {
    if (s_active != nullptr) {
        s_active->QueueAudio(data, frames);
    }
    return frames;
}

void LibretroHost::InputPollCallback() {
    // Input is pushed by the UI before retro_run().
}

int16_t LibretroHost::InputStateCallback(unsigned port, unsigned device, unsigned index,
                                         unsigned id) {
    if (s_active == nullptr || port != 0) {
        return 0;
    }

    const LibretroHost& self = *s_active;

    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
            uint16_t mask = 0;
            for (unsigned i = 0; i < self.buttons_.size(); ++i) {
                if (self.buttons_[i]) {
                    mask |= static_cast<uint16_t>(1u << i);
                }
            }
            return static_cast<int16_t>(mask);
        }
        if (id < self.buttons_.size()) {
            return self.buttons_[id] ? 1 : 0;
        }
        return 0;
    }

    if (device == RETRO_DEVICE_ANALOG && index < self.analog_.size()) {
        if (id == RETRO_DEVICE_ID_ANALOG_X) {
            return self.analog_[index][0];
        }
        if (id == RETRO_DEVICE_ID_ANALOG_Y) {
            return self.analog_[index][1];
        }
    }

    return 0;
}

void LibretroHost::LogCallback(enum retro_log_level level, const char* fmt, ...) {
    if (fmt == nullptr) {
        // Some cores call the log callback with a null format string; passing it
        // to vsnprintf would be undefined behaviour.
        return;
    }
    (void)level;

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (s_active != nullptr) {
        s_active->AddLog(buffer);
    } else {
        SDL_Log("%s", buffer);
    }
}

void LibretroHost::ParseVariables(const retro_variable* variables) {
    if (variables == nullptr) {
        return;
    }

    for (const retro_variable* variable = variables; variable->key != nullptr; ++variable) {
        const std::string key = variable->key;
        const std::string raw = variable->value != nullptr ? variable->value : "";
        const auto separator = raw.find(';');

        std::string label = key;
        std::string values_text = raw;
        if (separator != std::string::npos) {
            label = Trim(raw.substr(0, separator));
            values_text = raw.substr(separator + 1);
        }

        std::vector<CoreOptionValue> values;
        std::stringstream stream(values_text);
        std::string item;
        while (std::getline(stream, item, '|')) {
            item = Trim(item);
            if (!item.empty()) {
                values.push_back({item, item});
            }
        }

        const std::string default_value = values.empty() ? "" : values.front().value;
        AddOption(key, label, "", "", default_value, std::move(values));
    }
}

void LibretroHost::ParseCoreOptions(const retro_core_option_definition* definitions) {
    if (definitions == nullptr) {
        return;
    }

    for (const retro_core_option_definition* definition = definitions; definition->key != nullptr;
         ++definition) {
        std::vector<CoreOptionValue> values;
        for (size_t i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX; ++i) {
            const retro_core_option_value& value = definition->values[i];
            if (value.value == nullptr) {
                break;
            }
            values.push_back({value.value, value.label != nullptr ? value.label : value.value});
        }

        const std::string default_value =
            definition->default_value != nullptr ? definition->default_value : "";
        AddOption(definition->key, definition->desc != nullptr ? definition->desc : definition->key,
                  definition->info != nullptr ? definition->info : "", "", default_value,
                  std::move(values));
    }
}

void LibretroHost::ParseCoreOptionsV2(const retro_core_options_v2* options) {
    if (options == nullptr) {
        return;
    }

    if (options->categories != nullptr) {
        for (const retro_core_option_v2_category* category = options->categories;
             category->key != nullptr; ++category) {
            categories_[category->key] =
                category->desc != nullptr ? category->desc : category->key;
        }
    }

    if (options->definitions == nullptr) {
        return;
    }

    for (const retro_core_option_v2_definition* definition = options->definitions;
         definition->key != nullptr; ++definition) {
        std::vector<CoreOptionValue> values;
        for (size_t i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX; ++i) {
            const retro_core_option_value& value = definition->values[i];
            if (value.value == nullptr) {
                break;
            }
            values.push_back({value.value, value.label != nullptr ? value.label : value.value});
        }

        std::string category;
        if (definition->category_key != nullptr) {
            const auto it = categories_.find(definition->category_key);
            category = it != categories_.end() ? it->second : definition->category_key;
        }

        std::string label = definition->desc != nullptr ? definition->desc : definition->key;
        if (label.empty() && definition->desc_categorized != nullptr) {
            label = definition->desc_categorized;
        }

        const std::string default_value =
            definition->default_value != nullptr ? definition->default_value : "";
        AddOption(definition->key, label,
                  definition->info != nullptr ? definition->info : "", category, default_value,
                  std::move(values));
    }
}

void LibretroHost::AddOption(const std::string& key, const std::string& label,
                             const std::string& description, const std::string& category,
                             const std::string& default_value,
                             std::vector<CoreOptionValue> values) {
    if (key.empty()) {
        return;
    }
    if (values.empty() && !default_value.empty()) {
        values.push_back({default_value, default_value});
    }

    auto& option = options_[key];
    option.key = key;
    option.label = label.empty() ? key : label;
    option.description = description;
    option.category = category;
    option.values = std::move(values);

    const bool current_is_valid =
        std::any_of(option.values.begin(), option.values.end(),
                    [&](const CoreOptionValue& value) { return value.value == option.current; });

    if (!current_is_valid) {
        option.current = default_value;
    }
    if (option.current.empty() && !option.values.empty()) {
        option.current = option.values.front().value;
    }
}

void LibretroHost::ConfigureGraphicsApi() {
    const auto it = options_.find("citra_graphics_api");
    if (it == options_.end()) {
        AddLog("El core no expone citra_graphics_api; se usará su valor por defecto.");
        return;
    }

    const bool wants_hardware = hardware_enabled_ && video_ != nullptr &&
                                video_->SupportsHardware();
    std::string desired = wants_hardware ? "OpenGL" : "Software";
    const auto has_value = [&](const std::string& value) {
        return std::any_of(it->second.values.begin(), it->second.values.end(),
                           [&](const CoreOptionValue& option) { return option.value == value; });
    };

    if (!has_value(desired)) {
        AddLog("El core no ofrece '" + desired + "'; se usará Software.");
        desired = "Software";
    }
    if (it->second.current != desired && has_value(desired)) {
        it->second.current = desired;
        variables_dirty_ = true;
        AddLog("Graphics API: " + desired +
               (desired == "OpenGL" ? " (hardware)" : " (software)"));
    }

    const auto touch = options_.find("citra_enable_mouse_touchscreen");
    if (touch != options_.end() && touch->second.current != "enabled") {
        touch->second.current = "enabled";
        variables_dirty_ = true;
    }
}

void LibretroHost::OpenAudio() {
    CloseAudio();

    int frequency = static_cast<int>(std::lround(av_info_.timing.sample_rate));
    if (frequency <= 0) {
        frequency = 32768;
    }

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = frequency;

    audio_stream_ =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (audio_stream_ == nullptr) {
        AddLog(std::string("No se pudo abrir el dispositivo de audio: ") + SDL_GetError());
        audio_sample_rate_ = 0;
        return;
    }

    audio_sample_rate_ = frequency;
    SDL_ResumeAudioStreamDevice(audio_stream_);
    AddLog("Audio: " + std::to_string(frequency) + " Hz, estéreo S16");
}

void LibretroHost::CloseAudio() {
    if (audio_stream_ != nullptr) {
        SDL_DestroyAudioStream(audio_stream_);
        audio_stream_ = nullptr;
    }
    audio_sample_rate_ = 0;
    audio_error_logged_ = false;
}

void LibretroHost::QueueAudio(const int16_t* data, size_t frames) {
    if (audio_stream_ == nullptr || data == nullptr || frames == 0 || audio_sample_rate_ <= 0) {
        return;
    }

    constexpr int bytes_per_frame = 2 * static_cast<int>(sizeof(int16_t));
    const int max_queued = audio_sample_rate_ * bytes_per_frame / 2; // ~500 ms
    const int queued = SDL_GetAudioStreamQueued(audio_stream_);
    if (queued > max_queued) {
        return;
    }

    const int bytes = static_cast<int>(frames * bytes_per_frame);
    if (!SDL_PutAudioStreamData(audio_stream_, data, bytes) && !audio_error_logged_) {
        audio_error_logged_ = true;
        AddLog(std::string("Error encolando audio: ") + SDL_GetError());
    }
}

std::string LibretroHost::PathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}
