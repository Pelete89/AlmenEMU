// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless integration tests for LibretroCore + LibretroHost.
//
// These tests run against a fake libretro core (tests/fake_core) and a stub
// video backend, so they need no window, no GPU, no OpenGL context and no ROM.
// They cover:
//   * software lifecycle: options, frames, audio, input, savestates
//   * hardware rejections: unsupported API (Vulkan) and failing backend
//   * hardware lifecycle: context_reset/context_destroy, preferred renderer and
//     the context_destroy -> retro_unload_game ordering when closing a game
//   * the framebuffer policy: a grow-only FBO that has to be recreated must be
//     reported to the core with context_reset on the next frame
//
// Usage: almenemu_tests <path-to-fake-libretro-core>

#include "core/LibretroCore.h"
#include "core/LibretroHost.h"
#include "video/PixelConversion.h"
#include "video/VideoBackend.h"

#include "fake_core_api.h"
#include "test_common.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using tests::Check;
using tests::CheckEqual;
using tests::CheckNear;
using tests::SetEnv;

// VideoBackend stub: records what the host does and simulates hardware support
// on demand. It never touches a GPU.
class StubVideo final : public VideoBackend {
public:
    bool supports_hardware = false;
    bool fail_setup = false;

    bool setup_called = false;
    bool context_ready = false;
    bool hardware = false;

    int software_frames = 0;
    int hardware_frames = 0;
    int resize_calls = 0;
    int context_reset_calls = 0;
    int context_destroy_calls = 0;

    unsigned framebuffer_width = 0;
    unsigned framebuffer_height = 0;
    bool pending_context_reset = false;

    unsigned last_width = 0;
    unsigned last_height = 0;
    retro_pixel_format last_format = RETRO_PIXEL_FORMAT_0RGB1555;
    retro_hw_context_type last_context_type = RETRO_HW_CONTEXT_NONE;
    retro_hw_render_callback* core_callback = nullptr;

    bool Initialize(SDL_Window* window, std::string& error) override {
        (void)window;
        (void)error;
        return true;
    }

    void Shutdown() override { Reset(); }

    void Reset() override {
        core_callback = nullptr;
        context_ready = false;
        hardware = false;
        has_frame = false;
        pending_context_reset = false;
        framebuffer_width = 0;
        framebuffer_height = 0;
    }

    bool SupportsHardware() const override { return supports_hardware; }

    bool SetupHardwareRender(retro_hw_render_callback* callback, std::string& error) override {
        setup_called = true;
        if (!supports_hardware) {
            error = "hardware deshabilitado en el stub";
            return false;
        }
        if (fail_setup) {
            error = "fallo forzado del backend";
            return false;
        }
        if (callback == nullptr) {
            error = "callback nulo";
            return false;
        }
        last_context_type = callback->context_type;
        callback->get_current_framebuffer = &StubVideo::GetCurrentFramebuffer;
        callback->get_proc_address = &StubVideo::GetProcAddress;
        core_callback = callback;
        return true;
    }

    bool ContextReset(std::string& error) override {
        if (core_callback == nullptr) {
            error = "sin callback de hardware";
            return false;
        }
        ++context_reset_calls;
        hardware = true;
        context_ready = true;
        pending_context_reset = false;
        if (core_callback->context_reset != nullptr) {
            core_callback->context_reset();
        }
        return true;
    }

    void DestroyHardwareContext() override {
        if (core_callback != nullptr && context_ready && core_callback->context_destroy != nullptr) {
            ++context_destroy_calls;
            core_callback->context_destroy();
        }
        core_callback = nullptr;
        context_ready = false;
        hardware = false;
    }

    void OnSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch,
                         retro_pixel_format format) override {
        if (data == nullptr) {
            return;
        }
        (void)pitch;
        ++software_frames;
        SetFrame(width, height);
        last_format = format;
    }

    void OnHardwareFrame(unsigned width, unsigned height) override {
        ++hardware_frames;
        SetFrame(width, height);
    }

    void ResizeFramebuffer(unsigned width, unsigned height) override {
        ++resize_calls;

        // Same policy as the real backend: the framebuffer only grows, and recreating it
        // while the core is drawing invalidates its resources.
        if (video::FramebufferNeedsReallocation(framebuffer_width, framebuffer_height, width,
                                                height)) {
            framebuffer_width = width;
            framebuffer_height = height;
            if (context_ready) {
                pending_context_reset = true;
            }
        }
    }

    bool NeedsContextReset() const override { return pending_context_reset; }

    uintptr_t TextureId() const override { return has_frame ? 1 : 0; }

    void GetTextureUVs(float& u0, float& v0, float& u1, float& v1) const override {
        u0 = 0.0f;
        v0 = 0.0f;
        u1 = 1.0f;
        v1 = 1.0f;
    }

    void BindDefaultFramebuffer() override {}

    int FrameWidth() const override { return static_cast<int>(last_width); }
    int FrameHeight() const override { return static_cast<int>(last_height); }
    bool HasFrame() const override { return has_frame; }
    bool IsHardware() const override { return hardware; }

    const char* Name() const override { return hardware ? "Stub (hardware)" : "Stub (software)"; }

    bool ReadPixels(std::vector<uint8_t>& pixels, int& width, int& height) override {
        if (!has_frame || last_width == 0 || last_height == 0) {
            pixels.clear();
            return false;
        }
        width = static_cast<int>(last_width);
        height = static_cast<int>(last_height);
        // Same contract as the real backend: RGBA8 bytes, top-down, tightly packed.
        pixels.assign(static_cast<size_t>(width) * height * 4, 0);
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = 0x20;
            pixels[i + 1] = 0x20;
            pixels[i + 2] = 0x20;
            pixels[i + 3] = 0xFF;
        }
        return true;
    }

    static uintptr_t GetCurrentFramebuffer() { return 1; }
    static retro_proc_address_t GetProcAddress(const char* symbol) {
        (void)symbol;
        return nullptr;
    }

private:
    void SetFrame(unsigned width, unsigned height) {
        last_width = width;
        last_height = height;
        has_frame = true;
    }

    bool has_frame = false;
};

struct Fixture {
    fs::path root;
    fs::path core_path;
    fs::path rom;
    StubVideo video;
    LibretroCore core;
    LibretroHost host;
    fake_core::Stats* stats = nullptr;

    bool Init(std::string& error) {
        if (!core.Load(core_path, error)) {
            return false;
        }
        host.SetVideoBackend(&video);
        return host.Initialize(core, root / "system", root / "saves", error);
    }

    fake_core::Stats* Stats() {
        if (stats == nullptr && core.retro_get_memory_data != nullptr) {
            stats = static_cast<fake_core::Stats*>(
                core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        }
        return stats;
    }
};

std::unique_ptr<Fixture> MakeFixture(const std::string& name, const fs::path& core_path) {
    auto fixture = std::make_unique<Fixture>();
    fixture->root = fs::temp_directory_path() / "almenemu-tests" / name;
    fixture->core_path = core_path;
    fixture->rom = fixture->root / "game.3dsx";

    std::error_code ec;
    fs::remove_all(fixture->root, ec);
    fs::create_directories(fixture->root / "system", ec);
    fs::create_directories(fixture->root / "saves", ec);
    fs::create_directories(fixture->root / "states", ec);

    std::ofstream rom(fixture->rom, std::ios::binary);
    rom << "fake-rom";
    rom.close();

    return fixture;
}

void TestSoftwareLifecycle(const fs::path& core_path) {
    std::printf("[1] ciclo de vida software\n");
    SetEnv("ALMENEMU_FAKE_HW", "none");

    auto fixture = MakeFixture("software", core_path);
    fixture->video.supports_hardware = false;
    fixture->host.SetHardwareEnabled(false);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }

    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    Check(fixture->core.retro_api_version() == RETRO_API_VERSION,
          "la API libretro del core no es la que espera el frontend");

    // Core options (SET_CORE_OPTIONS_V2).
    const auto& options = fixture->host.Options();
    const auto scale = options.find("fake_video_scale");
    Check(scale != options.end(), "falta la opción fake_video_scale");
    if (scale != options.end()) {
        Check(scale->second.current == "1x", "valor por defecto de la opción: " +
                                                 scale->second.current);
        Check(scale->second.category == "Fake", "categoría de la opción: " +
                                                    scale->second.category);
        CheckEqual(static_cast<long long>(scale->second.values.size()), 2,
                   "número de valores de la opción");
    }
    Check(fixture->host.Options().find("fake_debug") != fixture->host.Options().end(),
          "falta la opción fake_debug");

    fixture->host.SetOption("fake_video_scale", "2x");
    fixture->host.SetOption("fake_video_scale", "3x"); // invalid: must be ignored
    auto current = fixture->host.Options().find("fake_video_scale");
    if (current != fixture->host.Options().end()) {
        Check(current->second.current == "2x", "SetOption rechazó un valor válido o aceptó uno "
                                               "inválido: " + current->second.current);
    }

    Check(fixture->host.LoadGame(fixture->rom, error), "LoadGame falló: " + error);
    CheckEqual(stats->load_calls, 1, "retro_load_game");
    CheckEqual(stats->hw_requested, 0, "peticiones de hardware");
    Check(!fixture->video.setup_called, "el backend no debía recibir hardware");
    Check(!fixture->host.IsHardwareActive(), "el render hardware no debía estar activo");
    Check(!fixture->host.HardwareRenderRequested(),
          "una petición inexistente no debe marcarse como aceptada");
    CheckNear(fixture->host.Fps(), fake_core::kFps, 0.01, "fps del core");
    CheckNear(fixture->host.AspectRatio(),
              static_cast<double>(fake_core::kWidth) / fake_core::kHeight, 0.001, "aspect ratio");

    for (int i = 0; i < 3; ++i) {
        fixture->host.RunFrame();
    }

    CheckEqual(stats->frames, 3, "frames ejecutados por el core");
    CheckEqual(fixture->video.software_frames, 3, "frames software presentados");
    CheckEqual(fixture->video.hardware_frames, 0, "frames hardware presentados");
    CheckEqual(fixture->video.last_width, fake_core::kWidth, "ancho del frame");
    CheckEqual(fixture->video.last_height, fake_core::kHeight, "alto del frame");
    Check(fixture->video.last_format == RETRO_PIXEL_FORMAT_XRGB8888,
          "formato de píxel entregado por el core");
    CheckEqual(stats->audio_frames, 3, "lotes de audio");
    CheckEqual(stats->input_polls, 3, "polls de input");
    CheckEqual(stats->input_queries, 3, "consultas de input");
    Check(stats->option_updates >= 1, "GET_VARIABLE_UPDATE no propagó el cambio de opción");
    CheckEqual(stats->scale_2x_seen, 3, "lecturas de la opción en 2x");

    // Savestate roundtrip: 3 frames -> save -> 4 frames -> load -> 3 frames.
    const auto state_path = fixture->root / "states" / "game.state";
    Check(fixture->host.SaveState(state_path, error), "SaveState falló: " + error);
    if (fs::exists(state_path)) {
        CheckEqual(static_cast<long long>(fs::file_size(state_path)),
                   static_cast<long long>(fake_core::kSaveStateSize), "tamaño del savestate");
    }
    CheckEqual(stats->save_state_saved, 1, "retro_serialize");

    fixture->host.RunFrame();
    CheckEqual(stats->frames, 4, "frames antes de cargar el estado");

    Check(fixture->host.LoadState(state_path, error), "LoadState falló: " + error);
    CheckEqual(stats->save_state_loaded, 1, "retro_unserialize");
    CheckEqual(stats->frames, 3, "frames restaurados por el savestate");

    // A corrupted state must be rejected, not crash.
    const auto corrupt_path = fixture->root / "states" / "corrupt.state";
    {
        std::ofstream corrupt(corrupt_path, std::ios::binary);
        corrupt << "not-a-state";
    }
    Check(!fixture->host.LoadState(corrupt_path, error),
          "LoadState debía rechazar un savestate corrupto");
    fixture->host.RunFrame();
    CheckEqual(stats->frames, 4, "el juego sigue avanzando tras un estado corrupto");

    fixture->host.UnloadGame();
    CheckEqual(stats->unload_calls, 1, "retro_unload_game");
    CheckEqual(stats->context_destroy_calls, 0,
               "context_destroy no debe invocarse en modo software");
    CheckEqual(stats->destroy_before_unload, 0, "sin contexto hardware no hay destroy");

    fixture->host.Shutdown();
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");
    fixture->core.Unload();
    fixture->stats = nullptr;
}

void TestVulkanRejected(const fs::path& core_path) {
    std::printf("[2] petición Vulkan rechazada\n");
    SetEnv("ALMENEMU_FAKE_HW", "vulkan");

    auto fixture = MakeFixture("vulkan", core_path);
    fixture->video.supports_hardware = true;
    fixture->host.SetHardwareEnabled(true);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }
    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    Check(fixture->host.LoadGame(fixture->rom, error),
          "el core debe poder seguir en software tras rechazar Vulkan: " + error);
    CheckEqual(stats->hw_requested, 1, "peticiones de hardware");
    CheckEqual(stats->hw_accepted, 0, "peticiones de hardware aceptadas");
    Check(!fixture->video.setup_called,
          "el backend OpenGL no debe configurarse para un contexto Vulkan");
    Check(!fixture->host.IsHardwareActive(), "el render hardware no debía activarse");
    Check(!fixture->host.HardwareRenderRequested(),
          "una petición rechazada no debe marcarse como aceptada");

    fixture->host.RunFrame();
    CheckEqual(fixture->video.software_frames, 1, "frames software presentados");
    CheckEqual(fixture->video.hardware_frames, 0, "frames hardware presentados");

    fixture->host.Shutdown();
    CheckEqual(stats->context_destroy_calls, 0,
               "context_destroy no debe invocarse si nunca hubo context_reset");
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");
    fixture->core.Unload();
    fixture->stats = nullptr;
}

void TestBackendRejectsHardware(const fs::path& core_path) {
    std::printf("[3] backend rechaza hardware y el juego sigue en software\n");
    SetEnv("ALMENEMU_FAKE_HW", "opengl");

    auto fixture = MakeFixture("backend-reject", core_path);
    fixture->video.supports_hardware = true;
    fixture->video.fail_setup = true;
    fixture->host.SetHardwareEnabled(true);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }
    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    // Regression: a rejected hardware request must not abort the software
    // fallback by forcing a context_reset() that cannot succeed.
    Check(fixture->host.LoadGame(fixture->rom, error),
          "LoadGame debe continuar en software si el backend rechaza el hardware: " + error);
    CheckEqual(stats->hw_requested, 1, "peticiones de hardware");
    CheckEqual(stats->hw_accepted, 0, "peticiones de hardware aceptadas");
    Check(fixture->video.setup_called, "el backend debía recibir la petición");
    CheckEqual(fixture->video.context_reset_calls, 0, "context_reset en el backend");
    Check(!fixture->host.IsHardwareActive(), "el render hardware no debía activarse");
    Check(!fixture->host.HardwareRenderRequested(),
          "una petición rechazada no debe marcarse como aceptada");

    fixture->host.RunFrame();
    CheckEqual(fixture->video.software_frames, 1, "frames software presentados");
    CheckEqual(stats->frames, 1, "frames ejecutados por el core");

    fixture->host.Shutdown();
    CheckEqual(stats->context_destroy_calls, 0,
               "context_destroy no debe invocarse sin context_reset");
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");
    fixture->core.Unload();
    fixture->stats = nullptr;
}

void TestHardwareLifecycle(const fs::path& core_path) {
    std::printf("[4] ciclo de vida del render hardware\n");
    SetEnv("ALMENEMU_FAKE_HW", "auto");

    auto fixture = MakeFixture("hardware", core_path);
    fixture->video.supports_hardware = true;
    fixture->host.SetHardwareEnabled(true);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }
    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    Check(fixture->host.LoadGame(fixture->rom, error), "LoadGame falló: " + error);
    CheckEqual(stats->preferred_hw, RETRO_HW_CONTEXT_OPENGL_CORE,
               "GET_PREFERRED_HW_RENDER con hardware habilitado");
    CheckEqual(stats->hw_requested, 1, "peticiones de hardware");
    CheckEqual(stats->hw_accepted, 1, "peticiones de hardware aceptadas");
    Check(fixture->video.setup_called, "el backend debía configurarse");
    Check(fixture->video.last_context_type == RETRO_HW_CONTEXT_OPENGL_CORE,
          "tipo de contexto entregado al backend");
    CheckEqual(fixture->video.context_reset_calls, 1, "context_reset en el backend");
    CheckEqual(stats->context_reset_calls, 1, "context_reset en el core");
    Check(fixture->host.IsHardwareActive(), "el render hardware debía estar activo");
    Check(fixture->host.HardwareRenderRequested(),
          "una petición aceptada debe marcarse como tal");

    fixture->host.RunFrame();
    fixture->host.RunFrame();
    CheckEqual(fixture->video.hardware_frames, 2, "frames hardware presentados");
    CheckEqual(fixture->video.software_frames, 0, "frames software presentados");
    CheckEqual(stats->frames, 2, "frames ejecutados por el core");

    // Readback is the base of captures (F12) and of the hardware path: the contract is
    // RGBA8 bytes, top-down, tightly packed (width * 4).
    std::vector<uint8_t> pixels;
    int read_width = 0;
    int read_height = 0;
    Check(fixture->video.ReadPixels(pixels, read_width, read_height),
          "readback del backend con un frame hardware activo");
    CheckEqual(read_width, fake_core::kWidth, "ancho informado por el readback");
    CheckEqual(read_height, fake_core::kHeight, "alto informado por el readback");
    CheckEqual(static_cast<long long>(pixels.size()),
               static_cast<long long>(read_width) * read_height * 4,
               "readback RGBA8: cuatro bytes por píxel");
    CheckEqual(pixels[0], 0x20, "readback: primer píxel tiene canal R");
    CheckEqual(pixels[3], 0xFF, "readback: alfa opaco");

    // Regression: closing a game must release the GPU resources the core created
    // in context_reset, through context_destroy, while the core is still loaded
    // and before retro_unload_game().
    fixture->host.UnloadGame();
    CheckEqual(fixture->video.context_destroy_calls, 1,
               "context_destroy en el backend al cerrar el juego");
    CheckEqual(stats->context_destroy_calls, 1,
               "context_destroy en el core al cerrar el juego");
    CheckEqual(stats->destroy_before_unload, 1,
               "context_destroy debe ocurrir antes de retro_unload_game");

    fixture->host.Shutdown();
    CheckEqual(fixture->video.context_destroy_calls, 1,
               "context_destroy no se invoca dos veces");
    CheckEqual(stats->context_destroy_calls, 1,
               "context_destroy en el core (una sola vez)");
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");

    fixture->core.Unload();
    fixture->stats = nullptr;
}

void TestPreferredHardwareDisabled(const fs::path& core_path) {
    std::printf("[5] GET_PREFERRED_HW_RENDER con hardware deshabilitado\n");
    SetEnv("ALMENEMU_FAKE_HW", "auto");

    auto fixture = MakeFixture("preferred-disabled", core_path);
    fixture->video.supports_hardware = true;
    fixture->host.SetHardwareEnabled(false);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }
    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    Check(fixture->host.LoadGame(fixture->rom, error), "LoadGame falló: " + error);
    CheckEqual(stats->preferred_hw, RETRO_HW_CONTEXT_NONE,
               "GET_PREFERRED_HW_RENDER con hardware deshabilitado");
    CheckEqual(stats->hw_requested, 0, "peticiones de hardware");
    Check(!fixture->video.setup_called, "el backend no debía configurarse");
    Check(!fixture->host.IsHardwareActive(), "el render hardware no debía activarse");

    fixture->host.Shutdown();
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");
    fixture->core.Unload();
    fixture->stats = nullptr;
}

void TestFramebufferGrowth(const fs::path& core_path) {
    std::printf("[6] FBO grow-only y aviso al core tras SET_GEOMETRY\n");
    SetEnv("ALMENEMU_FAKE_HW", "auto");
    SetEnv("ALMENEMU_FAKE_GEOMETRY", "800x480");

    auto fixture = MakeFixture("framebuffer-growth", core_path);
    fixture->video.supports_hardware = true;
    fixture->host.SetHardwareEnabled(true);

    std::string error;
    if (!fixture->Init(error)) {
        Check(false, "no se pudo inicializar el host: " + error);
        return;
    }
    fake_core::Stats* stats = fixture->Stats();
    if (stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas");
        return;
    }

    Check(fixture->host.LoadGame(fixture->rom, error), "LoadGame falló: " + error);

    // LoadGame sizes the framebuffer with the base resolution before context_reset, so
    // there is nothing to notify.
    CheckEqual(fixture->video.framebuffer_width, fake_core::kWidth, "FBO con la resolución base");
    CheckEqual(fixture->video.framebuffer_height, fake_core::kHeight,
               "FBO con la resolución base (alto)");
    Check(!fixture->video.NeedsContextReset(), "la primera asignación no requiere aviso");

    // The core grows its internal resolution during retro_run(). The host cannot call
    // context_reset() from inside the environment callback, so it defers it.
    fixture->host.RunFrame();
    CheckEqual(stats->geometry_calls, 1, "SET_GEOMETRY durante retro_run");
    CheckEqual(fixture->video.context_reset_calls, 1,
               "context_reset no se llama dentro del callback de entorno");
    Check(fixture->video.NeedsContextReset(), "un FBO recrecido debe avisar al core");
    CheckEqual(fixture->video.framebuffer_width, 800, "el FBO crece, no se queda recortado");
    CheckEqual(fixture->video.framebuffer_height, 480, "el FBO crece (alto)");

    // On the next frame the core is told to rebuild its resources.
    fixture->host.RunFrame();
    CheckEqual(fixture->video.context_reset_calls, 2, "context_reset en el frame siguiente");
    CheckEqual(stats->context_reset_calls, 2, "el core recibió el aviso");
    Check(!fixture->video.NeedsContextReset(), "el aviso se consume al atenderse");

    // A frame smaller than the current framebuffer must not reallocate it.
    SetEnv("ALMENEMU_FAKE_GEOMETRY", "400x240");
    fixture->host.RunFrame();
    CheckEqual(fixture->video.context_reset_calls, 2, "un frame menor no recrea el FBO");
    CheckEqual(fixture->video.framebuffer_width, 800, "el FBO conserva el tamaño alcanzado");

    fixture->host.UnloadGame();
    CheckEqual(stats->context_destroy_calls, 1, "context_destroy al cerrar el juego");
    fixture->host.Shutdown();
    CheckEqual(stats->deinit_calls, 1, "retro_deinit");

    // In software mode there is no core GPU context, so a growth is not a reset.
    auto software = MakeFixture("framebuffer-growth-software", core_path);
    software->video.supports_hardware = false;
    software->host.SetHardwareEnabled(false);
    SetEnv("ALMENEMU_FAKE_HW", "none");
    SetEnv("ALMENEMU_FAKE_GEOMETRY", "800x480");

    if (!software->Init(error)) {
        Check(false, "no se pudo inicializar el host (software): " + error);
        return;
    }
    fake_core::Stats* software_stats = software->Stats();
    if (software_stats == nullptr) {
        Check(false, "el core simulado no expone estadísticas (software)");
        return;
    }

    Check(software->host.LoadGame(software->rom, error), "LoadGame (software) falló: " + error);
    software->host.RunFrame();
    CheckEqual(software_stats->geometry_calls, 1, "SET_GEOMETRY en software");
    CheckEqual(software->video.framebuffer_width, 800, "el FBO software también crece");
    Check(!software->video.NeedsContextReset(), "en software no hay contexto que resetear");
    CheckEqual(software->video.context_reset_calls, 0, "context_reset no se llama en software");

    software->host.Shutdown();
    SetEnv("ALMENEMU_FAKE_GEOMETRY", nullptr);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Uso: %s <ruta-al-core-falso>\n", argv[0]);
        return 2;
    }

    const fs::path core_path = fs::absolute(fs::path(argv[1]));
    std::error_code ec;
    if (!fs::exists(core_path, ec)) {
        std::fprintf(stderr, "No existe el core simulado: %s\n", core_path.string().c_str());
        return 2;
    }

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::printf("Aviso: SDL no pudo inicializar audio (%s); se continúa.\n", SDL_GetError());
    }

    std::printf("AlmenEMU host tests\n");
    TestSoftwareLifecycle(core_path);
    TestVulkanRejected(core_path);
    TestBackendRejectsHardware(core_path);
    TestHardwareLifecycle(core_path);
    TestPreferredHardwareDisabled(core_path);
    TestFramebufferGrowth(core_path);

    SDL_Quit();

    std::printf("\n%d comprobaciones, %d fallos\n", tests::checks, tests::failures);
    return tests::failures == 0 ? 0 : 1;
}
