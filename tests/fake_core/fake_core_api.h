// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

// Contract shared between the AlmenEMU host tests and the fake libretro core.
//
// The core publishes its counters through
// retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM), so a test can assert what the
// core actually observed (context_reset, GET_VARIABLE_UPDATE, ...) without any
// extra instrumentation in the host.
namespace fake_core {

constexpr uint32_t kStatsMagic = 0x414C4D53u; // "ALMS"
constexpr uint32_t kStatsVersion = 1u;

constexpr unsigned kWidth = 400;
constexpr unsigned kHeight = 240;
constexpr double kFps = 60.0;
constexpr double kSampleRate = 32768.0;
constexpr unsigned kAudioFramesPerRun = 512;

constexpr size_t kSaveStateSize = 64;
constexpr uint32_t kSaveStateMagic = 0x414C4D45u; // "ALME"

struct Stats {
    uint32_t magic;
    uint32_t version;
    uint32_t frames;
    uint32_t audio_frames;
    uint32_t load_calls;
    uint32_t unload_calls;
    uint32_t deinit_calls;
    uint32_t hw_requested;
    uint32_t hw_accepted;
    uint32_t context_reset_calls;
    uint32_t context_destroy_calls;
    uint32_t preferred_hw;   // retro_hw_context_type seen by the core
    uint32_t option_updates; // GET_VARIABLE_UPDATE returned true
    uint32_t scale_2x_seen;  // GET_VARIABLE("fake_video_scale") == "2x"
    uint32_t save_state_saved;
    uint32_t save_state_loaded;
    uint32_t input_polls;
    uint32_t input_queries;
    uint32_t geometry_calls; // SET_GEOMETRY sent during retro_run
    // Set by retro_unload_game() when context_destroy already happened, i.e. the
    // frontend released the GPU resources before telling the core the game ended.
    uint32_t destroy_before_unload;
};

} // namespace fake_core
