# Changelog

Todas las novedades relevantes de AlmenEMU se documentan aquí.
El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/)
y el versionado usa [Semantic Versioning](https://semver.org/lang/es/).

## [Unreleased]

### Added

- Core libretro simulado (`tests/fake_core`) y tests headless del host
  (`tests/test_host.cpp`) registrados en CTest como `host_tests`.
- La CI ejecuta `ctest` en Linux, Windows y macOS además del build y el smoke test.
- La CI cachea SDL3 e ImGui con `actions/cache` (clonados en `deps/`) y cancela runs
  obsoletos mediante `concurrency`.
- Opción CMake `ALMENEMU_BUILD_TESTS` (ON por defecto).
- Plan de implementación completo en `docs/PLAN.md`.
- Backlog priorizado, hitos, criterios de release y checklist GPL.
- Revisión 1.1 del plan: gates alineados, matriz de plataformas/core macOS,
  dependencias entre hitos y decisiones abiertas (ADR).
- Backend de vídeo OpenGL con FBO propio y render hardware de Azahar.
- Fallback software y opción `--software`.
- Presentación de la UI con `ImGui_ImplOpenGL3`.
- Medición de FPS real del core y de la UI.
- Captura desde el FBO en modo hardware.
- Lógica pura de presentación extraída a `src/video/PixelConversion` (formatos de píxel,
  expansión de 5/6 bits, UVs del frame, orden de filas del readback y política del FBO).
  No depende de OpenGL ni SDL y se compila dentro de la app y de los tests.
- Tests unitarios de la lógica de vídeo (`tests/test_video.cpp`, registrados en CTest como
  `video_tests`, 54 comprobaciones) y helpers compartidos en `tests/test_common.h`.
- El readback del backend se verifica en `host_tests` (RGBA8, cuatro bytes por píxel, alfa
  opaco, tamaño `width * height * 4`); `host_tests` llega a 112 comprobaciones.

### Changed

- `SET_HW_RENDER` solo acepta contextos OpenGL de escritorio; cualquier otra API
  (Vulkan, GLES, D3D) se rechaza y el core cae a software.
- La petición de hardware solo se marca como aceptada si el backend la admite, así que un
  backend que falle ya no aborta el fallback software.
- `GET_PREFERRED_HW_RENDER` respeta `--software` y el hardware deshabilitado.
- `context_destroy` del core se invoca al cerrar la app, con el contexto GL current, antes
  de `retro_deinit()`.
- `LibretroCore::Load` rechaza cores cuyo `retro_api_version()` no coincide con
  `RETRO_API_VERSION` (antes se cargaba el símbolo y nunca se comprobaba).
- `Run()` carga el core una sola vez: `Init()` cargaba el core por defecto y luego
  `--core` volvía a cargarlo, ejecutando `retro_init()`/`retro_deinit()` dos veces.
- Los atajos `Ctrl+Q` y `Ctrl+O` no se reenvían como input del juego; `SDLK_Q` está
  mapeado a `RETRO_DEVICE_ID_JOYPAD_L`, así que `Ctrl+Q` pulsaba L.
- `VideoBackend::TextureId()` devuelve 0 hasta que existe un frame, así la UI muestra
  "Esperando el primer frame" en lugar de un rectángulo negro.
- `RETRO_ENVIRONMENT_SHUTDOWN` se consume en el bucle principal (`ShouldQuit()`); antes el
  flag se ponía y nadie lo consultaba.
- Se elimina `IsOptionAvailable()`: código muerto (`options_` ya se expone).
- El FBO se dimensiona con la resolución base cuando aún no existe. Es *grow-only*, así que
  en hardware suele quedarse en el 2048x2048 creado por `SET_HW_RENDER` y la presentación
  recorta la región activa con UVs.
- `.gitignore` ahora cubre `build*/` (antes `build-linux/` no se ignoraba).
- README enlaza el plan y el estado del proyecto.
- Normalización de finales de línea con `.gitattributes`.
- La UI deja de usar `SDL_Renderer`; usa un contexto OpenGL 4.3/4.1/3.3.
- Vsync desactivado y frame limiter a ~60 Hz para evitar la caída a 30 Hz.

### Fixed

- Doble free al cerrar: `SDL_GetBasePath()` devuelve un buffer cacheado por SDL3 (lo libera
  `SDL_Quit()`), así que ya no se pasa a `SDL_free()`.
- El fallback software ya no se cancela cuando el core pide hardware y el backend lo
  rechaza (antes se llamaba a `context_reset` sin callback válido y se descargaba el juego).
- `context_destroy` no se llamaba nunca: los recursos GL del core se liberan al cerrar.
- `context_destroy` se invoca **antes** de `retro_unload_game()` al cerrar un juego. Con
  render hardware activo, cerrar el juego dejaba los recursos GL del core sin liberar;
  Azahar solo suelta el renderer si el juego sigue cargado, por eso el orden importa.
- `LogCallback` ya no es UB cuando un core pasa `fmt == nullptr` a `vsnprintf`.
- `OpenGLBackend::GetTextureUVs()` usaba `TextureUV` sin el namespace `video::` y
  `PixelConversion.cpp` no estaba en `CMakeLists.txt`: ni la app ni los tests compilaban.
  El archivo se añade a los targets `AlmenEMU` y `almenemu_tests`.
- `StubVideo::ReadPixels` no sobrescribía la firma de la interfaz (`std::vector<uint32_t>`
  frente a `std::vector<uint8_t>`), así que el stub quedaba como clase abstracta y el
  fixture no se podía instanciar. Ahora implementa el contrato real: RGBA8 apilado.
- `FlipRowsToTopDown` devuelve `bool` y deja `dst` vacío cuando el readback es más corto que
  el frame; `ReadPixels` ya no informa éxito con un buffer rellenado a ceros, de modo que
  una captura vacía se rechaza en lugar de escribir un BMP negro.
- El test del pitch acolchado crea el buffer con los huecos del padding, así la segunda fila
  empieza en el byte 12; antes se leía fuera del array y la fila salía descolocada.
- El test de crecimiento en software fija `ALMENEMU_FAKE_GEOMETRY=800x480` antes de cargar el
  juego: heredaba `400x240` del paso anterior y el FBO nunca crecía.

## [0.1.0] - 2026-11-10

### Added

- Frontend C++20 + SDL3 + Dear ImGui para cargar el core libretro de Azahar.
- Render software XRGB8888/RGB565/0RGB1555 con pantalla dual.
- Audio S16 estéreo con cola limitada.
- Input de teclado y mandos SDL3 con layout Nintendo.
- Opciones del core (`SET_VARIABLES`, V1, V2, INTl).
- Savestates, capturas, recientes, drag & drop y diálogos nativos.
- Carga dinámica del core y descarga mediante `scripts/fetch_core.py`.
- Documentación inicial, licencia GPL-3.0 y avisos de terceros.

### Known limitations

- Solo render software; el render hardware está pendiente.
- Touchscreen y C-Stick incompletos.
- Sin configuración por juego ni persistencia de opciones.
- Sin CI, empaquetado ni releases.
