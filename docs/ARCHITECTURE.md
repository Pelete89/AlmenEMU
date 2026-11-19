# Arquitectura de AlmenEMU

## Visión general

```text
┌─────────────────────────────────────────────────────────────┐
│ AlmenEMU (C++20 + SDL3 + Dear ImGui)                        │
│                                                             │
│  Application                                                │
│   ├─ ventana + contexto OpenGL 4.3/4.1/3.3                  │
│   ├─ UI ImGui con backend OpenGL3                           │
│   ├─ input (teclado + SDL_Gamepad)                          │
│   ├─ OpenGLBackend (FBO + textura)                          │
│   └─ LibretroHost                                           │
│        ├─ LibretroCore  ── dlopen/LoadLibrary ──┐           │
│        ├─ callbacks environment/video/audio      │           │
│        ├─ opciones del core                      │           │
│        └─ frames hardware o software             │           │
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
6. `retro_get_system_av_info()` y dimensionado del FBO.
7. `context_reset()` si el core pidió render hardware.
8. Bucle: `retro_run()` a la frecuencia `timing.fps`.
9. `retro_unload_game()` → `retro_deinit()` → descargar la biblioteca.

El host mantiene un puntero estático `s_active` porque libretro no ofrece `userdata` en sus
callbacks.

## Vídeo

### Backend actual: OpenGL

La app crea un contexto **OpenGL 4.3 core** (con fallback a 4.1/3.3) y usa
`ImGui_ImplOpenGL3` para toda la interfaz. El backend de vídeo es
`OpenGLBackend` (`src/video/`):

- Crea un **FBO propio** con textura `RGBA8`.
- En `RETRO_ENVIRONMENT_SET_HW_RENDER` rellena `get_current_framebuffer` y
  `get_proc_address`, pero **no** llama todavía a `context_reset`.
- Tras `retro_load_game()` y `retro_get_system_av_info()`, dimensiona el FBO con
  `base_width`/`base_height` y llama a `context_reset()`. Azahar carga el juego dentro de
  ese callback.
- El core renderiza en el FBO; el frontend recibe `RETRO_HW_FRAME_BUFFER_VALID` y muestra
  la textura con ImGui.
- `bottom_left_origin=true` de Azahar se compensa invirtiendo las coordenadas UV.
- Si el FBO es mayor que el frame (por ejemplo, single-screen), las UV recortan la región
  activa.
- `context_destroy` no se invoca al cambiar de juego: el contexto OpenGL es propiedad de
  la app y se destruye al cerrar. El core libera sus recursos en `retro_unload_game()`.
- Al cerrar la app sí se llama `context_destroy` (una sola vez, con el contexto GL current)
  antes de `retro_deinit()`, que es cuando el core debe liberar sus recursos GL.
- `SET_HW_RENDER` solo se acepta para `RETRO_HW_CONTEXT_OPENGL_CORE` u `OPENGL`. Si el core
  pide Vulkan, GLES o D3D se devuelve `false` y el core cae a software; el frontend no le
  entrega callbacks de FBO OpenGL para un contexto que no es OpenGL.
- `hw_render_requested_` solo se marca cuando el backend acepta la petición. Si el backend
  falla (FBO incompleto, sin OpenGL 4.3), el core sigue en software y `LoadGame` no fuerza
  un `context_reset` condenado a fallar.
- `GET_PREFERRED_HW_RENDER` devuelve `OPENGL_CORE` solo si el hardware está habilitado
  (respeta `--software`); si no, devuelve `NONE`.

### Fallback software

- El core entrega `XRGB8888`, `RGB565` o `0RGB1555` por `retro_video_refresh`.
- `OpenGLBackend` los convierte a `0xAARRGGBB` y los sube a la misma textura con
  `glTexSubImage2D`.
- La UI sigue siendo OpenGL; no se usa `SDL_Renderer`.
- `--software` fuerza `citra_graphics_api=Software`. También se fuerza si el contexto no
  llega a OpenGL 4.3 (por ejemplo, macOS 4.1 antes de M3).

### Frame pacing

- Vsync está desactivado (`SDL_GL_SetSwapInterval(0)`): con vsync, un frame de core+UI
  que roza los 16,6 ms cae a 30 Hz.
- El bucle limita a ~60 Hz con `SDL_DelayPrecise` y ejecuta los frames de core que
  correspondan al tiempo transcurrido.
- Medido en Windows/AMD 780M: **60 fps de core y 60 Hz de UI** con Universal-Updater.
- El renderer software sigue siendo lento (aprox. 44 fps de core / 11 Hz de UI en el mismo
  equipo).

### Siguiente paso: macOS (M3)

Azahar llama a `RETRO_ENVIRONMENT_SET_HW_RENDER` durante `retro_load_game` y espera que el
frontend:

1. Cree un contexto **Vulkan 1.1** con MoltenVK.
2. Rellene `retro_hw_render_callback`:
   - `context_reset` → el core crea sus recursos.
   - `context_destroy` → el core libera sus recursos.
   - `get_proc_address` y la interfaz de negociación Vulkan.
3. Llame a `context_reset()` cuando el contexto esté activo.
4. Trate `RETRO_HW_FRAME_BUFFER_VALID` como “el frame está en la imagen Vulkan, no en
   memoria”.
5. Implemente `RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE` y el
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

Antes de `retro_load_game`, `ConfigureGraphicsApi()` fuerza `citra_graphics_api=OpenGL`
cuando hay OpenGL 4.3 y el hardware está habilitado, y `Software` en caso contrario (sin
4.3 o con `--software`). Si el core no expone la clave, se respeta su valor por defecto.

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

## Tests

`tests/fake_core` implementa un core libretro mínimo que no emula nada: expone opciones
(`SET_CORE_OPTIONS_V2`), pide render hardware según `ALMENEMU_FAKE_HW`
(`none`/`opengl`/`vulkan`/`auto`), entrega frames XRGB8888 o `RETRO_HW_FRAME_BUFFER_VALID`,
produce audio/input y soporta savestates. Publica sus contadores vía
`retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)`, así que los tests comprueban lo que el
core observó de verdad (por ejemplo, si `context_reset`/`context_destroy` se llamaron).

`tests/test_host.cpp` inyecta un `VideoBackend` de prueba (sin GPU) en `LibretroHost` y
cubre:

1. Ciclo software: opciones, frames, audio, input, savestate y estado corrupto.
2. Rechazo de `SET_HW_RENDER` con contexto Vulkan.
3. Backend que rechaza el hardware y el juego continúa en software (regresión).
4. Ciclo hardware: `GET_PREFERRED_HW_RENDER`, `context_reset` y `context_destroy`.
5. `GET_PREFERRED_HW_RENDER` con el hardware deshabilitado (`--software`).

Se registran en CTest como `host_tests` y corren en CI en las tres plataformas. No
requieren ventana, OpenGL, ROM ni el core de Azahar.

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
