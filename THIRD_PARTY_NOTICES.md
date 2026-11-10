# Third-party notices

AlmenEMU is licensed under GPL-3.0-or-later. It uses the following components:

| Component | License | Use |
|---|---|---|
| **Azahar** (`azahar-emu/azahar`) | GPL-3.0-or-later | Emulation core, loaded at runtime as a libretro shared library. Not bundled in this repository. |
| **SDL3** (`libsdl-org/SDL`) | Zlib | Window, input, audio, renderer and shared-library loading. Fetched by CMake. |
| **Dear ImGui** (`ocornut/imgui`) | MIT | User interface. Fetched by CMake. |
| **libretro.h** (`libretro/libretro-common`) | MIT | libretro API header, vendored under `third_party/libretro/include/`. |

Azahar and its source code are available at <https://github.com/azahar-emu/azahar>.
If you redistribute an Azahar core binary together with AlmenEMU, you must comply
with the GPL: provide the corresponding source code and preserve all notices.

This project does not include Nintendo ROMs, BIOS, firmware, keys, or any other
copyrighted console data.
