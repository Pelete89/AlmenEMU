# Arquitectura de AlmenEMU

## Visión general

```text
┌─────────────────────────────────────────────────────────────┐
│ AlmenEMU (C++20 + SDL3 + Dear ImGui)                        │
│                                                             │
│  Application                                                │
│   ├─ ventana / renderer SDL3                                │
│   ├─ UI ImGui (biblioteca, ajustes, opciones, logs)         │
│   ├─ input (teclado + SDL_Gamepad)                          │
│   └─ LibretroHost                                           │
│        ├─ LibretroCore  ── dlopen/LoadLibrary ──┐           │
│        ├─ callbacks environment/video/audio      │           │
│        ├─ opciones del core                      │           │
│        └─ framebuffer XRGB8888                   │           │
└──────────────────────────────────────────────────┼───────────┘
                                                   │
                                    azahar_libretro.dll/.so/.dylib
                                    (Azahar, GPL-3.0)
```

El frontend no enlaza Azahar en tiempo de compilación: lo carga en runtime con
`SDL_LoadObject`/`SDL_LoadFunction`. Eso permite actualizar el core sin recompilar la app
y, más adelante, cargar otros cores libretro (Panda3DS, etc.).

## Ciclo de vida del core

1. `retro_set_environment(callback)` — **antes** de `retro_init()`.
2. `retro_init()`.
3. Se instalan `retro_set_video_refresh`, audio, input poll/state.
4. `retro_get_system_info()`.
5. `retro_load_game()` con `retro_game_info.path` en UTF-8.
6. `retro_get_system_av_info()`.
7. Bucle: `retro_run()` a la frecuencia `timing.fps`.
8. `retro_unload_game()` → `retro_deinit()` → descargar la biblioteca.

El host mantiene un puntero estático `s_active` porque libretro no ofrece `userdata` en sus
callbacks.

## Vídeo

### MVP actual: software

- El core entrega un framebuffer por `retro_video_refresh`.
- Formatos aceptados: `XRGB8888`, `RGB565` y `0RGB1555`; todos se convierten a `uint32_t`
  con layout `0x00RRGGBB`.
- La app crea una textura `SDL_PIXELFORMAT_XRGB8888` y la actualiza con `SDL_UpdateTexture`.
- ImGui dibuja la textura con la relación de aspecto de `retro_game_geometry`.
- Azahar compone ambas pantallas según sus propias opciones (`citra_layout_option`,
  `citra_swap_screen`, `citra_large_screen_proportion`, `citra_render_3d`,
  `citra_factor_3d`).

### Siguiente paso: hardware

Azahar llama a `RETRO_ENVIRONMENT_SET_HW_RENDER` durante `retro_load_game` y espera que el
frontend:

1. Cree un contexto **OpenGL 4.3 core** o **Vulkan 1.1**.
2. Rellene `retro_hw_render_callback`:
   - `context_reset` → el core crea sus recursos.
   - `context_destroy` → el core libera sus recursos.
   - `get_current_framebuffer` → FBO actual (OpenGL).
   - `get_proc_address` → resolver de funciones.
3. Llame a `context_reset()` cuando el contexto esté activo.
4. Trate `RETRO_HW_FRAME_BUFFER_VALID` como “el frame está en el FBO, no en memoria”.
5. Para Vulkan, implemente también
   `RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE` y el
   `retro_hw_render_context_negotiation_interface_vulkan` (instancia, dispositivo,
   colas, extensiones).

**macOS:** OpenGL está limitado a 4.1, así que el camino hardware pasa por Vulkan +
MoltenVK. Además, el JIT de Dynarmic requiere hardened runtime con
`com.apple.security.cs.allow-jit` y notarización para distribuir.

## Audio

- El core entrega `int16_t` estéreo por `retro_set_audio_sample_batch`.
- Se abre un `SDL_AudioStream` con la frecuencia de `retro_system_av_info.timing`.
- Se encola con `SDL_PutAudioStreamData` y se limita a ~500 ms de cola para evitar latency
  drift. Si el dispositivo no acepta la frecuencia nativa, SDL remuestrea.

## Input

- Se mantiene un array de 16 botones RetroPad y dos ejes analógicos.
- Teclado y mando se combinan con OR antes de `retro_run`.
- `RETRO_ENVIRONMENT_GET_INPUT_BITMASKS` está activo; también se soporta
  `RETRO_DEVICE_ID_JOYPAD_MASK`.
- Mapeo Nintendo: Sur=A, Este=B, Norte=X, Oeste=Y.
- Pendiente: touchscreen mediante `RETRO_DEVICE_POINTER` y mapeo del C-Stick.

## Opciones del core

`LibretroHost` implementa:

- `SET_VARIABLES` (formato antiguo).
- `SET_CORE_OPTIONS` / `SET_CORE_OPTIONS_INTL`.
- `SET_CORE_OPTIONS_V2` / `SET_CORE_OPTIONS_V2_INTL`.
- `GET_CORE_OPTIONS_VERSION` → 2.
- `GET_VARIABLE` / `GET_VARIABLE_UPDATE`.

Las opciones se guardan en un `std::map` con valores y etiquetas. La UI las agrupa por
categoría y las muestra como combos. Al cambiar una opción se marca
`variables_dirty_`; el core la relee en el siguiente `retro_run`.

Para el MVP se fuerza `citra_graphics_api=Software` antes de `retro_load_game`, porque el
renderer hardware aún no está implementado.

## Directorios de datos

Se usa `SDL_GetPrefPath("AlmenEMU", "AlmenEMU")`:

- Windows: `%APPDATA%\AlmenEMU\AlmenEMU\`
- macOS: `~/Library/Application Support/AlmenEMU/AlmenEMU/`
- Linux: `~/.local/share/AlmenEMU/AlmenEMU/`

Dentro:

```text
system/       archivos de sistema que pida el core
saves/        partidas del core
states/       savestates de AlmenEMU
screenshots/  capturas BMP
config/       recent.txt
```

## Empaquetado

- **Windows:** MSVC + ZIP/MSI. SDL3 va estático; el core se distribuye aparte o se
  incluye cumpliendo GPL.
- **Linux:** AppImage o Flatpak. Compilar en una glibc antigua para compatibilidad.
- **macOS:** `.app` universal (arm64 + x86_64), `Info.plist`, hardened runtime,
  entitlements JIT y notarización. El core de release es un `.dylib`; hay que firmarlo
  dentro del bundle o cargarlo desde `cores/`.

## Licencia

El frontend es GPL-3.0-or-later. Azahar es GPL-3.0. Cualquier distribución que incluya el
core debe acompañarse del código fuente correspondiente y de los avisos de licencia.
