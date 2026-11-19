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

### Changed

- `SET_HW_RENDER` solo acepta contextos OpenGL de escritorio; cualquier otra API
  (Vulkan, GLES, D3D) se rechaza y el core cae a software.
- La petición de hardware solo se marca como aceptada si el backend la admite, así que un
  backend que falle ya no aborta el fallback software.
- `GET_PREFERRED_HW_RENDER` respeta `--software` y el hardware deshabilitado.
- `context_destroy` del core se invoca al cerrar la app, con el contexto GL current, antes
  de `retro_deinit()`.
- README enlaza el plan y el estado del proyecto.
- Normalización de finales de línea con `.gitattributes`.
- La UI deja de usar `SDL_Renderer`; usa un contexto OpenGL 4.3/4.1/3.3.
- Vsync desactivado y frame limiter a ~60 Hz para evitar la caída a 30 Hz.
- El FBO se dimensiona con la resolución base actual, no con el máximo teórico.

### Fixed

- Doble free al cerrar: `SDL_GetBasePath()` devuelve un buffer cacheado por SDL3 (lo libera
  `SDL_Quit()`), así que ya no se pasa a `SDL_free()`.
- El fallback software ya no se cancela cuando el core pide hardware y el backend lo
  rechaza (antes se llamaba a `context_reset` sin callback válido y se descargaba el juego).
- `context_destroy` no se llamaba nunca: los recursos GL del core se liberan al cerrar.

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
