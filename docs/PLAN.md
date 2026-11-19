# Plan de implementación de AlmenEMU

> **Versión del plan:** 1.3
> **Fecha:** 2026-11-19
> **Estado:** vivo; se actualiza al cerrar cada hito.
> **Revisión 1.3:** cerrado el P0 de robustez: fallback de render hardware, `context_destroy`,
> doble free de SDL3 al cerrar, tests headless con core simulado y CI con `ctest`.
> **Revisión 1.2:** M2 (OpenGL 4.3) implementado y verificado en Windows; Linux pendiente de CI.
> **Licencia del proyecto:** GPL-3.0-or-later.
> **Documento relacionado:** [`docs/ARCHITECTURE.md`](ARCHITECTURE.md).

---

## 1. Resumen ejecutivo

AlmenEMU es un frontend de escritorio para emular **Nintendo 3DS** en Windows, Linux y
macOS. No implementa un emulador desde cero: carga en runtime el **core libretro de
Azahar** y aporta una experiencia propia (interfaz, biblioteca, ajustes, mandos,
savestates, capturas y empaquetado).

La estrategia elegida es la **opción B**:

- Motor de emulación: Azahar, GPL-3.0, mantenido por la comunidad.
- Integración: API libretro estable, core cargado como `.dll`/`.so`/`.dylib`.
- Producto: frontend nativo en C++20 + SDL3 + Dear ImGui.
- Licencia: GPL-3.0-or-later, coherente con Azahar.

El proyecto ya tiene un **MVP funcional** con render software. El siguiente hito crítico
es el **render hardware OpenGL 4.3 en Windows/Linux**, seguido de **Vulkan 1.1 +
MoltenVK en macOS**. A partir de ahí se abordarán biblioteca, input táctil, persistencia,
empaquetado y CI.

**Estimación global:** 12–20 semanas de una persona a tiempo completo para llegar a un
v1.0 estable, dependiendo del alcance de biblioteca/UI y de las pruebas en macOS. Las
estimaciones de este documento son indicativas, no compromisos de fecha.

---

## 2. Objetivos y alcance

### 2.1 Objetivos del producto

1. Ejecutar juegos y homebrew de 3DS con la compatibilidad y el rendimiento de Azahar.
2. Ofrecer una interfaz clara, rápida y multiplataforma que no parezca un wrapper.
3. Gestionar biblioteca, ajustes, mandos, savestates y capturas sin salir de la app.
4. Ser instalable y actualizable en Windows, Linux y macOS.
5. Cumplir estrictamente con GPL-3.0 y con la política de no distribuir contenido de
   Nintendo.

### 2.2 Dentro del alcance (v1.0)

- Windows 10/11 x86_64.
- Linux x86_64 (AppImage y/o Flatpak).
- macOS 13+ Intel y Apple Silicon (app universal; core universal o selección por arquitectura).
- Core Azahar libretro; opcionalmente otros cores libretro compatibles.
- Render software como fallback y render hardware como modo principal.
- UI de biblioteca, ajustes, opciones del core, input, savestates y capturas.
- Empaquetado firmado/notarizado donde aplique.
- CI multiplataforma y documentación de usuario.

### 2.3 Fuera del alcance (v1.0)

- Android/iOS.
- Netplay, cámara, Amiibo o funciones del standalone de Azahar que el core libretro no
  expone. Si se convierten en requisito, habrá que evaluar un fork del standalone.
- Emulador propio o reimplementación del core.
- Distribución de ROMs, BIOS, firmware, `aes_keys`, `seeddb`, NAND o cualquier dato
  copyrighted de Nintendo.
- Licencia propietaria: no hay un core 3DS permisivo maduro; el proyecto asume GPL.
- Auto-actualización del core dentro de la app en v1.0 (puede llegar después).

### 2.4 Matriz de plataformas y cores

| Plataforma | Arquitectura | Core Azahar | Estado |
|---|---|---|---|
| Windows 10/11 | x86_64 | `azahar-libretro-windows-x86_64` | Soportado |
| Windows 10/11 | ARM64 | No hay asset oficial ARM64; usar el core x86_64 bajo emulación x64 o compilar desde fuente | P2 / documentar |
| Linux | x86_64 | `azahar-libretro-linux-x86_64` | Soportado |
| Linux | ARM64 | Compilar Azahar con `-DENABLE_LIBRETRO=ON` | P1 (equipos ARM64; Steam Deck es x86_64) |
| macOS | Apple Silicon | `azahar-libretro-macos-arm64` | Soportado |
| macOS | Intel | `azahar-libretro-macos-x86_64` | Soportado |
| macOS | Universal | Requiere `lipo` de ambos slices o cargar el correcto en runtime | P0 para app universal |

> **Nota macOS:** Azahar publica cores separados para arm64 y x86_64. Una `.app` universal
> no puede incluir un solo `.dylib` y funcionar en ambas arquitecturas. Hay que construir
> un core universal con `lipo`, o incluir ambos y elegir en runtime.

---

## 3. Estado actual: lo que ya está completo

### 3.1 Resumen

| Área | Estado | Evidencia |
|---|---|---|
| Repositorio y build | Completo | Git `main`, commit inicial, CMake + presets |
| Ventana y UI base | Completo | SDL3 + Dear ImGui, tema oscuro, menús, panel lateral |
| Carga de core | Completo | `LibretroCore` con `SDL_LoadObject` y resolución de símbolos |
| Host libretro | Completo | `LibretroHost`: environment, vídeo, audio, input, opciones |
| Vídeo software | Completo | XRGB8888, RGB565 y 0RGB1555 → textura OpenGL |
| Render hardware OpenGL | Completo en Windows | FBO propio, `SET_HW_RENDER`, `context_reset`, 60 fps core/UI |
| Audio | Completo | `SDL_AudioStream`, S16 estéreo, cola limitada a ~500 ms |
| Input | Completo (base) | Teclado + SDL_Gamepad, RetroPad, ejes analógicos |
| Opciones del core | Completo (base) | `SET_VARIABLES`, V1, V2 e INTl; combos en UI |
| Savestates | Completo (base) | `F5`/`F9`, un slot por juego |
| Capturas | Completo | `F12`, BMP en `screenshots/` |
| Recientes | Completo | `config/recent.txt`, máx. 12 |
| Diálogos y drag & drop | Completo | SDL3 file dialog + `SDL_EVENT_DROP_FILE` |
| Documentación | Completo (base) | README, ARCHITECTURE, PLAN, CHANGELOG, THIRD_PARTY_NOTICES |
| Tests del host | Completo | Core simulado + CTest (`host_tests`), 80 comprobaciones sin GPU |
| Verificación funcional | Completo en Windows | Azahar 2126.2 + Universal-Updater 3.4.1 `.3dsx`; OpenGL 4.3 hardware; Linux verificado en build + tests headless; macOS pendiente |

### 3.2 Hitos cerrados

#### M0 — Fundaciones (completado)

- [x] Repositorio git inicializado en `main`.
- [x] `.gitignore` y `.gitattributes` para build, cores, ROMs de prueba y logs.
- [x] `CHANGELOG.md` y `docs/PLAN.md`.
- [x] CMake 3.24+ con C++20, SDL3 e ImGui vía `FetchContent`.
- [x] Presets para Windows/MSVC, Linux/Ninja y macOS/Ninja.
- [x] `LICENSE` GPL-3.0 y avisos de terceros.
- [x] Script `scripts/fetch_core.py`.

#### M1 — MVP con render software (completado)

- [x] Carga de `azahar_libretro.dll` y arranque del core.
- [x] Carga de `.3ds`, `.cci`, `.cxi`, `.app`, `.cia` y `.3dsx` (según core; solo `.3dsx` verificado).
- [x] Render de las dos pantallas compuestas por Azahar.
- [x] Audio y input básicos.
- [x] Opciones del core visibles y editables.
- [x] Savestate generado (~3,7 MB); restauración en frío pendiente de validar.
- [x] Captura de pantalla.
- [x] UI de bienvenida, recientes e información del core.

#### M2 — Render hardware OpenGL 4.3 (completado en Windows)

- [x] Interfaz `VideoBackend` y `OpenGLBackend`.
- [x] Contexto OpenGL 4.3 core con fallback 4.1/3.3.
- [x] FBO propio y `get_current_framebuffer`.
- [x] `SET_HW_RENDER`, `get_proc_address`, `context_reset` tras `retro_load_game`.
- [x] Presentación del FBO con UVs correctas para `bottom_left_origin`.
- [x] Fallback software y opción `--software`.
- [x] Captura desde FBO en modo hardware.
- [x] Medición de FPS real de core y UI.
- [x] Verificado en Windows: 60 fps de core y 60 Hz de UI con Universal-Updater.
- [ ] Verificación en Linux (pendiente de CI/VM).

### 3.3 Limitaciones conocidas del MVP

- macOS sigue en render software hasta M3 (Vulkan/MoltenVK).
- Linux compila y pasa los tests headless (Mesa/LLVM, core simulado), pero falta validar
  Azahar real y obtener CI en verde.
- El touchscreen y el C-Stick no están mapeados por completo.
- Vsync está desactivado; el frame limiter usa `SDL_DelayPrecise`.
- No hay shaders ni escalado entero configurables.
- No hay persistencia de opciones del core ni configuración por juego.
- No hay biblioteca con carátulas/metadatos.
- La CI compila y ejecuta tests; no hay empaquetado ni releases.
- La app aún no tiene icono, versión visible en binario ni actualizador.
- No se ha probado con Azahar real en Linux ni en macOS desde este entorno.
- El soporte `.cia` depende de las claves del core y no se ha validado con un título propio.

---

## 4. Brechas y prioridades

| Prioridad | Significado | Regla |
|---|---|---|
| **P0** | Bloqueante para el siguiente release | Debe entrar antes de etiquetar |
| **P1** | Necesario para v1.0 | Puede esperar a un hito posterior |
| **P2** | Mejora deseable | No bloquea releases |

### 4.1 Brechas principales

1. **Render hardware**: OpenGL 4.3 ya está implementado en Windows. Falta verificar
   Linux y añadir Vulkan/MoltenVK en macOS.
2. **macOS**: OpenGL 4.3 no existe en macOS; hay que implementar Vulkan/MoltenVK o
   aceptar software. Además, Dynarmic exige entitlement JIT y notarización.
3. **Persistencia**: opciones del core, configuración por juego, perfiles de mando y
   ajustes de vídeo deben sobrevivir al reinicio.
4. **Input táctil**: la pantalla inferior es parte esencial de la 3DS.
5. **Biblioteca**: escaneo de carpetas, búsqueda, favoritos y carátulas.
6. **Distribución**: instaladores, firma, notarización, AppImage/Flatpak. La CI de build y
   tests ya existe; falta empaquetado y releases.
7. **Calidad**: matriz de compatibilidad y ampliar los tests (hoy cubren el host con un
   core simulado; el core simulado ya está implementado).

---

## 5. Arquitectura objetivo

### 5.1 Estado actual

```text
src/
  app/Application.*      ventana, UI, input, hotkeys, biblioteca simple
  core/LibretroCore.*    carga dinámica del core
  core/LibretroHost.*    environment, vídeo, audio, input, opciones
  video/OpenGLBackend.*  FBO y render hardware/software
  main.cpp
third_party/libretro/    cabecera MIT
tests/fake_core/         core libretro simulado
tests/test_host.cpp      tests headless del host (CTest)
```

### 5.2 Estructura propuesta para v1.0

```text
src/
  app/
    Application.*        composición y ciclo de vida
    Hotkeys.*            atajos y acciones
  core/
    LibretroCore.*       carga/descarga del core
    LibretroHost.*       callbacks y estado del core
    CoreManager.*        descubrimiento, versión y actualización
    CoreOptions.*        parseo y persistencia de opciones
  video/
    VideoBackend.h       interfaz común
    SoftwareBackend.*    framebuffer actual
    OpenGLBackend.*      OpenGL 4.3 (Windows/Linux)
    VulkanBackend.*      Vulkan 1.1 + MoltenVK (macOS)
    FramePacer.*         vsync, límites y latencia
  audio/
    AudioOutput.*        SDL_AudioStream
  input/
    InputManager.*       teclado, mando, touch, motion
    ControllerMap.*      perfiles y remapeo
  library/
    LibraryScanner.*     escaneo y metadatos
    GameEntry.*          modelo de juego
    CoverCache.*         carátulas locales
  config/
    Config.*             TOML/JSON versionado
    Paths.*              XDG / AppData / Application Support
  ui/
    MainWindow.*         layout principal
    LibraryView.*        grid/list, búsqueda, favoritos
    SettingsView.*       vídeo/audio/input/hotkeys
    CoreOptionsView.*    opciones del core
    OnboardingView.*     primer arranque y legal
    Toasts.*             mensajes no bloqueantes
  diagnostics/
    Log.*                log en memoria + archivo
    CrashHandler.*       minidump / log de arranque
  platform/
    Windows.*            DPI, single-instance, rutas
    macOS.*              menú nativo, JIT entitlements
    Linux.*              XDG, portal, AppImage
```

### 5.3 Flujo de render hardware

```text
Application
  └─ VideoBackend (OpenGL / Vulkan)
       ├─ crea contexto y superficie
       ├─ implementa SET_HW_RENDER
       ├─ expone get_proc_address / get_current_framebuffer
       ├─ llama context_reset cuando el contexto está listo
       └─ presenta el FBO/VkImage que entrega Azahar
```

Puntos críticos:

- El core llama a `RETRO_ENVIRONMENT_SET_HW_RENDER` **durante** `retro_load_game`.
- `context_reset` no puede ejecutarse antes de que el contexto esté activo.
- `RETRO_HW_FRAME_BUFFER_VALID` significa que el frame está en el FBO, no en memoria.
- En Vulkan hay que implementar la interfaz de negociación y crear instancia,
  dispositivo físico/lógico, colas y superficies compatibles con MoltenVK en macOS.
- El backend software debe seguir disponible como fallback y para diagnóstico.

---

## 6. Roadmap por hitos

> Supuesto: una persona, dedicación completa, con acceso a Windows y macOS para pruebas.
> Los rangos incluyen investigación, implementación, pruebas y documentación.

**Dependencias entre hitos:**

- M2 depende de M1.
- M3 depende de la abstracción de vídeo de M2 y de acceso a macOS.
- M4 puede empezar tras M2; no bloquea M5.
- M5 depende de M4 para perfiles de input, pero la biblioteca puede avanzar en paralelo.
- M6 puede empezar en paralelo después de M2; el empaquetado completo espera a M3/M5.
- M7 cierra cuando M2–M6 están estables.

### M2 — Render hardware OpenGL 4.3 (Windows/Linux) · P0 · Completado en Windows

> **Estado:** implementado y verificado en Windows (AMD 780M): 60 fps de core y 60 Hz
> de UI. Linux pendiente de compilación/CI. El detalle técnico está en
> [`docs/ARCHITECTURE.md`](ARCHITECTURE.md).

**Objetivo:** sustituir el render software por OpenGL 4.3 en Windows/Linux.

Entregables:

- Interfaz `VideoBackend` y selección automática de backend.
- Contexto OpenGL 4.3 core con SDL3.
- Implementación de `SET_HW_RENDER`, `context_reset`, `get_current_framebuffer` y
  `get_proc_address`. `context_destroy` no se invoca al cambiar de juego; el contexto
  OpenGL vive mientras la app y se destruye al cerrar.
- Presentación del FBO de Azahar con el layout que elija el core.
- Fallback a software si OpenGL no está disponible.
- Onboarding mínimo: seleccionar core, aviso legal y abrir ROM.
- Prueba con homebrew; falta validar un juego comercial propio.
- Documentación de limitaciones.

**Criterio de aceptación:** un juego 3DS corre a velocidad completa con render hardware
en Windows y Linux; la UI se mantiene a ~60 Hz en hardware objetivo; el fallback software
sigue funcionando.

**Estimación:** 1–2 semanas.

### M3 — macOS: Vulkan 1.1 + MoltenVK · P0

**Objetivo:** render hardware en macOS sin depender de OpenGL 4.3.

Entregables:

- Backend Vulkan 1.1 con MoltenVK.
- Negociación de instancia/dispositivo/colas con el core.
- `context_reset` y presentación del frame.
- App universal Intel/Apple Silicon.
- Core universal con `lipo` o carga del slice correcto (arm64/x86_64) en runtime.
- Firma del `.dylib` dentro del bundle.
- Entitlements de JIT (`com.apple.security.cs.allow-jit`) y notarización.
- Pruebas en macOS 13+ Intel y Apple Silicon.

**Criterio de aceptación:** la app universal arranca, carga el core correcto, renderiza y
mantiene 60 Hz en Apple Silicon; Gatekeeper no bloquea la app notarizada.

**Estimación:** 2–4 semanas (la parte de Vulkan/MoltenVK es la más incierta).

### M4 — Input completo y pulido de UX · P1

**Objetivo:** que la experiencia de juego sea completa sin tocar el teclado.

Entregables:

- Touchscreen mapeado a la pantalla inferior.
- C-Stick y circle pad con deadzones y calibración.
- Remapeo de teclado y mando con perfiles.
- Hotkeys configurables.
- Toasts, diálogos de error y visor de logs.
- Pantalla completa, DPI y escalado entero.
- Onboarding completo de primer arranque (core, archivos de sistema, legal).

**Criterio de aceptación:** se puede jugar, pausar, guardar/cargar y configurar mandos
sin editar archivos; el touch funciona en homebrew y juegos compatibles.

**Estimación:** 1–2 semanas.

### M5 — Biblioteca, configuración y persistencia · P1

**Objetivo:** convertir el MVP en una aplicación de uso diario.

Entregables:

- Escaneo de carpetas y biblioteca con grid/list, búsqueda y favoritos.
- Carátulas locales y metadatos básicos; sin descargar assets copyrighted.
- Configuración por juego (vídeo, audio, input, opciones del core).
- Persistencia global de opciones y perfiles.
- Slots de savestate, autosave opcional y miniaturas.
- Import/export de partidas SRAM.
- Modo portable (`--portable`).

**Criterio de aceptación:** cerrar y abrir la app conserva biblioteca, ajustes, perfiles y
partidas; cada juego puede tener su propia configuración.

**Estimación:** 2–3 semanas.

### M6 — Empaquetado, CI y primer release público · P0

**Objetivo:** distribuir AlmenEMU de forma reproducible.

Entregables:

- Windows: ZIP y MSI; icono y metadatos de versión.
- Linux: AppImage y Flatpak.
- macOS: `.app` universal, core universal, `Info.plist`, firma, notarización y DMG.
- CI matrix Windows/Linux/macOS con build y tests.
- Checksums, SBOM y tarball de fuente en cada release.
- Release automatizado desde tags.
- Comprobaciones GPL y avisos de terceros en el artefacto.
- Página de descarga y notas de release.

**Criterio de aceptación:** un usuario puede descargar, instalar y ejecutar en las tres
plataformas; los artefactos se generan desde un tag sin pasos manuales.

**Estimación:** 2–3 semanas.

### M7 — Calidad y v1.0 · P1

**Objetivo:** estabilizar compatibilidad, rendimiento y documentación.

Entregables:

- Matriz de compatibilidad con homebrew y juegos propios.
- Tests unitarios e integración con un core simulado.
- Benchmarks de rendimiento y latencia de audio.
- Corrección de bugs de la matriz.
- Documentación de usuario, troubleshooting y contribución.
- Revisión legal/GPL y de marca.

**Criterio de aceptación:** no hay bugs P0/P1 abiertos; las tres plataformas pasan CI;
la documentación permite compilar y usar la app sin ayuda externa.

**Estimación:** 4–6 semanas.

---

## 7. Backlog detallado

> Estado: **Pendiente** salvo que se indique lo contrario. Los identificadores son
> estables para referenciarlos en issues y commits.
>
> **Completado en M2:** V-01, V-02, C-01, V-04 (fallback), V-07 (base) y V-08 (base).
> Linux y macOS siguen pendientes de verificación.

### 7.1 Vídeo

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| V-01 | P0 | Abstracción `VideoBackend` | Software y hardware implementan la misma interfaz; selección en runtime | 2 d |
| V-02 | P0 | Backend OpenGL 4.3 | FBO de Azahar presentado; 60 Hz; funciona en Win/Linux | 5–8 d |
| V-03 | P0 | Backend Vulkan 1.1 + MoltenVK | Instancia/dispositivo/colas negociados; 60 Hz en macOS | 8–12 d |
| V-04 | P1 | Fallback y diagnóstico | Si falla HW, se usa software con aviso claro | 2 d |
| V-05 | P1 | Escalado entero y filtros | Presets nearest/linear/integer; sin artefactos | 2 d |
| V-06 | P2 | Shaders post-proceso | Pipeline de shaders seleccionable; sin afectar al core | 4 d |
| V-07 | P1 | Readback HW | Leer el FBO a memoria sin bloquear; base para capturas | 1 d |
| V-08 | P1 | Frame pacing | Sin stutter con 59,826 Hz; vsync configurable | 2 d |
| V-09 | P2 | Capturas PNG | Convertir el framebuffer a PNG sin bloquear | 1 d |
| V-10 | P2 | Diagnóstico GPU | Vendor/renderer/versión visibles en logs y UI | 1 d |
| V-11 | P1 | Sincronización A/V | Audio y vídeo sin desfase; dynamic rate control | 2 d |

### 7.2 Core y ciclo de vida

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| C-01 | P0 | Handshake de render HW | El core recibe contexto válido y `context_reset` | 1 d |
| C-02 | P0 | Robustez de errores | Init/load/unload fallidos no dejan la app bloqueada | 2 d |
| C-03 | P1 | Persistencia de opciones | Opciones globales sobreviven al reinicio | 2 d |
| C-04 | P1 | Configuración por juego | Overrides por ROM y perfil de vídeo/audio | 2 d |
| C-05 | P1 | Slots de savestate | 5–10 slots, miniatura y autosave opcional | 2 d |
| C-06 | P2 | Import/export SRAM | Exportar/importar partida del core | 1 d |
| C-07 | P2 | Core manager | Detectar versión, ruta y actualización manual | 2 d |
| C-08 | P2 | Segundo core | Cargar Panda3DS sin cambios de UI | 2 d |
| C-09 | P1 | Recarga de opciones | Cambios en caliente sin reiniciar cuando sea posible | 1 d |
| C-10 | P1 | Pin y hash del core | Versión mínima/máxima y verificación de integridad | 1 d |
| C-11 | P1 | Formato de savestate | Versión, escritura atómica y aviso de incompatibilidad | 1 d |
| C-12 | P2 | Descubrimiento de cores | Rutas estándar, portable y selección manual | 1 d |

### 7.3 Input

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| I-01 | P1 | Touchscreen | `RETRO_DEVICE_POINTER` mapeado a pantalla inferior | 2 d |
| I-02 | P1 | C-Stick y calibración | Eje derecho, deadzone e inversión configurables | 1 d |
| I-03 | P1 | Remapeo y perfiles | UI de mapeo, guardar/cargar perfiles | 3 d |
| I-04 | P2 | Remapeo de teclado | Cualquier tecla a cualquier botón | 1 d |
| I-05 | P2 | Motion/giro | Sensores SDL si el core los expone | 2 d |
| I-06 | P2 | Micrófono | Interfaz libretro si el core la soporta | 2 d |
| I-07 | P1 | Hotplug de mandos | Conectar/desconectar sin reiniciar | 1 d |
| I-08 | P1 | Mapeo táctil por layout | Coordenadas correctas en side-by-side, híbrido y fullscreen | 2 d |

### 7.4 UI/UX

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| U-01 | P1 | Escáner de biblioteca | Carpetas, extensiones, dedupe y refresco | 3 d |
| U-02 | P1 | Grid/list, búsqueda y favoritos | Navegación fluida con teclado y mando | 3 d |
| U-03 | P1 | Carátulas y metadatos | Carátulas locales; placeholders si faltan | 4 d |
| U-04 | P0 | Onboarding | Primer arranque guía core, sistema y legal; amplía el mínimo de M2 | 2 d |
| U-05 | P1 | Ajustes completos | Vídeo, audio, input, hotkeys y rutas | 3 d |
| U-06 | P1 | DPI, temas e idioma | Escalado correcto; ES/EN; tema claro/oscuro | 2 d |
| U-07 | P1 | Toasts y logs | Errores no bloqueantes + visor de logs | 1 d |
| U-08 | P2 | Accesibilidad | Navegación por teclado; contraste; escala | 2 d |
| U-09 | P1 | Recuperación de errores | Modo seguro si falla config/core; opción de reset | 1 d |
| U-10 | P1 | Hotkeys configurables | Remapeo de pausa, reset, estados y captura | 1 d |

### 7.5 Datos y configuración

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| D-01 | P1 | Esquema de configuración | TOML/JSON versionado con migraciones | 2 d |
| D-02 | P1 | Modo portable | `--portable` usa carpetas junto al ejecutable | 0,5 d |
| D-03 | P1 | Rutas por plataforma | XDG, AppData y Application Support correctos | 1 d |
| D-04 | P1 | Recientes/favoritos | Persistencia robusta y sin duplicados | 0,5 d |
| D-05 | P2 | Backup/restore | Exportar/importar configuración y partidas | 1 d |
| D-06 | P1 | Log a archivo | Rotación, nivel configurable y ruta por plataforma | 1 d |

### 7.6 Plataforma y empaquetado

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| P-01 | P0 | Windows ZIP/MSI | Instalación limpia; desinstalación deja datos de usuario | 2 d |
| P-02 | P0 | Linux AppImage/Flatpak | Ejecuta en Ubuntu 22.04+ y Fedora reciente | 2 d |
| P-03 | P0 | macOS `.app` universal | arm64 + x86_64; Info.plist correcto | 3 d |
| P-04 | P0 | Firma/notarización macOS | Gatekeeper abre la app; JIT funciona | 2 d |
| P-05 | P1 | Iconos y metadatos | Icono, versión y copyright visibles | 1 d |
| P-06 | P2 | Auto-update | Comprobación opt-in; sin telemetría | 2 d |
| P-07 | P1 | CI matrix | Build Win/Linux/macOS en cada PR | 2 d |
| P-08 | P1 | Release pipeline | Tag → artefactos + notas | 1 d |
| P-09 | P0 | Core universal macOS | `lipo` o selección arm64/x86_64; firma incluida | 2 d |
| P-10 | P1 | Integración de escritorio | Asociación de archivos y single-instance | 1 d |
| P-11 | P0 | Fuente, checksums y SBOM | Tarball de fuente y hashes en cada release | 1 d |
| P-12 | P1 | Política Windows ARM64 | Documentar soporte x64 emulado o compilar core | 0,5 d |
| P-13 | P2 | Firma Windows | Certificado de código; reducir avisos SmartScreen | 1 d |

### 7.7 Calidad

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| Q-01 | P0 | CI de build y smoke test | Tres plataformas verdes en `main` | 3 d |
| Q-02 | P1 | Tests unitarios | Config, rutas, opciones y mapeo con cobertura | 3 d |
| Q-03 | P1 | Core simulado | Core de prueba con vídeo/audio determinista para CI | 3 d |
| Q-04 | P1 | Matriz de compatibilidad | Plantilla y lista de homebrew/juegos propios | continuo |
| Q-05 | P1 | Benchmarks | Tiempo de frame, UI Hz, audio underruns, RAM | 2 d |
| Q-06 | P2 | ASan/UBSan | Pipeline Linux sin errores | 1 d |
| Q-07 | P2 | Crash handler | Log y minidump local, sin subida automática | 1 d |
| Q-08 | P2 | Fuzz de configuración | Parser robusto ante archivos corruptos | 1 d |
| Q-09 | P1 | Auditoría de licencias | Headers SPDX y SBOM en CI | 1 d |
| Q-10 | P2 | Actualización de dependencias | Dependabot/Renovate con pin y revisión | 0,5 d |
| Q-11 | P1 | Límites de CI gráfica | CI usa software/mock; matriz HW es manual | 0,5 d |
| Q-12 | P1 | Tests de migración | Config y savestates antiguos migran sin pérdida | 1 d |

### 7.8 Legal y documentación

| ID | Prio | Tarea | Criterio de aceptación | Est. |
|---|---|---|---|---|
| L-01 | P0 | Checklist GPL | Fuente, avisos y oferta de código en cada release | 1 d |
| L-02 | P0 | Política de assets | Onboarding claro: sin ROMs/BIOS/claves | 1 d |
| L-03 | P1 | Revisión de marca | Nombre/logo no infringen; disclaimer visible | 0,5 d |
| L-04 | P1 | Guía de usuario | Instalación, controles, troubleshooting | 2 d |
| L-05 | P1 | Avisos de terceros | Actualizados con cada dependencia | 0,5 d |
| L-06 | P1 | SECURITY/CONTRIBUTING | Guías de reporte y contribución | 1 d |
| L-07 | P1 | Lista de compatibilidad | Plantilla y disclaimer de resultados | 2 d |

---

## 8. Criterios de release

### 8.1 Gates por versión

| Versión | Contenido mínimo | Gate |
|---|---|---|
| **v0.1.0** | MVP + OpenGL 4.3 Win/Linux + onboarding mínimo + GPL | Build reproducible, sin crashes P0, README actualizado |
| **v0.2.0** | macOS Vulkan/MoltenVK + touch + onboarding completo | App universal notarizada, touch validado |
| **v0.3.0** | Biblioteca, configuración por juego, carátulas, perfiles, slots de savestate y empaquetado completo | Persistencia validada; CI en tres OS |
| **v1.0.0** | Empaquetado completo, CI, tests, docs, matriz de compatibilidad | Sin bugs P0/P1; release notes; checklist GPL |

### 8.2 Definition of Done (por tarea)

Una tarea se considera terminada cuando:

- [ ] El código compila en Windows, Linux y macOS (o se documenta una excepción).
- [ ] No añade warnings nuevos con `/W4` o `-Wall -Wextra`.
- [ ] Tiene pruebas manuales documentadas; tests automáticos si es lógica pura.
- [ ] La UI muestra errores de forma comprensible.
- [ ] La documentación relevante está actualizada.
- [ ] Respeta la licencia GPL y no incluye assets de Nintendo.
- [ ] El cambio está en un commit atómico con mensaje Conventional Commits.

---

## 9. Estrategia de pruebas y calidad

### 9.1 Pirámide de pruebas

| Nivel | Qué cubre | Herramienta |
|---|---|---|
| Unitario | Config, rutas, parser de opciones, mapeo de input | Catch2/GoogleTest o framework propio |
| Integración | Host libretro con un **core simulado** | C++ + CMake/CTest |
| Smoke | Cargar Azahar + homebrew, primer frame, audio, savestate | Script multiplataforma |
| Manual | Juegos propios y homebrew en las tres plataformas | Checklist + capturas |
| Rendimiento | Frame time, UI Hz, audio underruns, RAM | Benchmarks + logs |

### 9.2 Core simulado para CI

**Estado: implementado** en `tests/fake_core` (ver [`ARCHITECTURE.md`](ARCHITECTURE.md#tests)).

Para no depender de ROMs ni de Azahar en CI:

- [x] Implementar un core libretro mínimo (`tests/fake_core`).
- [x] Expone `retro_init`, `retro_load_game`, `retro_run`, vídeo y audio.
- [x] Genera un patrón de color y un tono determinista.
- [x] Permite probar: carga, callbacks, formatos de píxel, audio, savestates y errores de
      render hardware (Vulkan rechazado, backend que rechaza OpenGL).
- [x] Se compila como `.dll`/`.so`/`.dylib` y se carga con el mismo `LibretroCore`.
- [x] Se ejecuta con CTest (`host_tests`) en la CI de las tres plataformas.

### 9.3 Matriz manual mínima

| Plataforma | Versión | Prueba |
|---|---|---|
| Windows 10/11 x64 | MSVC 2022 | Arranque, core, homebrew, mando, savestate |
| Ubuntu 22.04/24.04 | GCC/Clang | Arranque, GL, audio, AppImage |
| Fedora reciente | GCC | Flatpak, Wayland/X11 |
| macOS 13+ Intel | Clang | Vulkan/MoltenVK, notarización |
| macOS 14+ Apple Silicon | Clang | Rendimiento, JIT entitlement |

### 9.4 Presupuestos de rendimiento

- Core a velocidad completa (100 %) con render hardware.
- UI ≥ 60 Hz con vsync; sin caídas sostenidas por debajo de 30 Hz.
- Audio: < 1 underrun por minuto en condiciones normales.
- Arranque en frío: < 3 s hasta ventana usable.
- RAM: < 1 GB con un juego típico (sin contar shaders/caché del core).
- Cierre: < 2 s, sin bloquear por guardado.

### 9.5 Reglas de CI

- No descargar ROMs comerciales ni BIOS.
- El smoke test usa un homebrew de código abierto o el core simulado.
- Las dependencias de FetchContent (SDL3, ImGui) se cachean en `deps/` con
  `actions/cache` y se pasan a CMake con `FETCHCONTENT_SOURCE_DIR_*`.
- Ejecutar en PR y en `main`; releases solo desde tags.
- Guardar logs y artefactos de test fallidos.

### 9.6 Límites de CI gráfica

- Los runners de macOS no garantizan GPU/Metal ni sesión gráfica; el render hardware se
  valida en una matriz manual.
- El CI cubre compilación, tests unitarios, core simulado y render software.
- Los tests de OpenGL/Vulkan deben poder saltarse si no hay contexto disponible.
- No usar capturas de juegos comerciales como fixtures.

### 9.7 Datos de prueba

- Homebrew de código abierto con licencia compatible (por ejemplo, Universal-Updater).
- Core simulado propio para tests deterministas.
- ROMs/BIOS/claves del usuario solo en pruebas manuales locales, nunca en el repo.
- Cualquier fixture binario debe tener licencia y tamaño justificados.

---

## 10. Riesgos y mitigaciones

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| Complejidad del render hardware | Alta | Alto | Empezar por OpenGL 4.3; usar Azahar como referencia; fallback software |
| macOS OpenGL 4.1 | Alta | Alto | Vulkan + MoltenVK; probar pronto en Apple Silicon |
| Firma/notarización y JIT en macOS | Media | Alto | Entitlements, firma del `.dylib`, pruebas en Mac limpio |
| Cambios en la API/opciones de Azahar | Media | Medio | Pin de versión del core; CI de compatibilidad; capa de opciones |
| Falta de netplay/cámara/Amiibo en el core | Alta | Medio | Comunicarlo; evaluar fork del standalone si es requisito |
| Rendimiento software insuficiente | Alta | Medio | Priorizar M2; medir con benchmarks; no prometer 60 Hz en software |
| Latencia de audio | Media | Medio | Cola ajustable, dynamic rate control, pruebas |
| Scope creep de biblioteca/UI | Alta | Medio | Gates por hito; P0 antes que P1/P2 |
| Incumplimiento GPL | Baja | Muy alto | Checklist, fuente publicada, avisos, revisión legal |
| Marca Nintendo/Azahar | Baja | Alto | Rebrand, disclaimer, revisión de nombre/logo |
| Build no reproducible | Media | Medio | Tags fijos en FetchContent, lockfile, CI desde cero |
| Windows ARM64 sin core oficial | Media | Bajo | Documentar; compilar core desde fuente o no soportar |
| Core macOS no universal | Alta | Medio | `lipo` de ambos slices o carga por arquitectura; probar en Intel y ARM |
| CI sin GPU/Metal | Alta | Medio | CI de software/mock + matriz manual documentada |
| Caída o retirada del core (DMCA/legal) | Media | Alto | Pin de fuente, build reproducible, espejo local y revisión legal |
| Corrupción de savestate/config | Media | Medio | Escritura atómica, backups y validación de esquema |
| Migración de configuración | Media | Bajo | Esquema versionado y tests de migración |

---

## 11. Git, ramas y releases

### 11.1 Estado del repositorio

- Repositorio local inicializado en `main`.
- Remoto `origin`: <https://github.com/Pelete89/AlmenEMU> (público).
- Identidad configurada: `Pelete89 <25388925+Pelete89@users.noreply.github.com>`.
- Autenticación: GitHub CLI (`gh`) con la cuenta `Pelete89`.
- El historial inicial contiene la scaffold, el plan y su revisión 1.1.
- Estado esperado: árbol limpio y `main` sincronizado con `origin/main`.

Comandos de publicación:

```bash
git push -u origin main
```

Para un espejo en GitLab:

```bash
git remote add gitlab git@gitlab.com:<usuario>/AlmenEMU.git
git push -u gitlab main
```

### 11.2 Estrategia de ramas

| Rama | Uso |
|---|---|
| `main` | Estable; siempre compila y pasa CI |
| `feat/<tema>` | Nueva funcionalidad |
| `fix/<tema>` | Corrección |
| `chore/<tema>` | Build, docs, dependencias |
| `release/vX.Y.Z` | Preparación de release (opcional) |

- Squash merge o merge commit, pero mantener historia legible.
- PRs pequeños; un PR = una intención.
- No commitear binarios, ROMs, BIOS, claves ni `build/`.

### 11.3 Convención de commits

```text
feat(video): add OpenGL 4.3 hardware renderer
fix(core): handle failed retro_load_game cleanly
docs(plan): update M2 status
build(cmake): pin SDL3 3.4.20
test(host): add mock libretro core
```

Tipos: `feat`, `fix`, `docs`, `build`, `test`, `refactor`, `perf`, `chore`.

### 11.4 Tags y releases

- Versionado semántico: `v0.1.0`, `v0.2.0`, `v1.0.0`.
- Tag anotado: `git tag -a v0.1.0 -m "AlmenEMU 0.1.0"`.
- Release desde tag; artefactos generados por CI.
- Notas de release con: novedades, breaking changes, requisitos, checksums y aviso GPL.
- Mantener `CHANGELOG.md` actualizado.

### 11.5 Checklist antes de cada release

- [ ] `main` verde en CI (Windows/Linux/macOS).
- [ ] Tests unitarios e integración pasan.
- [ ] Smoke test con Azahar + homebrew.
- [ ] `LICENSE` y `THIRD_PARTY_NOTICES.md` actualizados.
- [ ] Fuente del core correspondiente disponible si se redistribuye.
- [ ] Sin ROMs/BIOS/claves en el artefacto.
- [ ] Instaladores probados en máquina limpia.
- [ ] Notas de release y checksums publicados.

---

## 12. Próximos 7 días (sprint de arranque de M2)

### Día 1 — Diseño

- [ ] Crear `src/video/VideoBackend.h` con interfaz:
  `Initialize`, `Shutdown`, `CanUse`, `Present`, `OnVideoRefresh`.
- [ ] Mover la lógica actual de textura SDL3 a `SoftwareBackend`.
- [ ] Definir cómo se selecciona backend: preferencia de usuario → HW disponible →
  software.
- [ ] Documentar el flujo de `SET_HW_RENDER` en `docs/ARCHITECTURE.md`.

### Día 2–3 — Contexto OpenGL

- [ ] Crear ventana SDL3 con `SDL_WINDOW_OPENGL` cuando el backend sea GL.
- [ ] Configurar `SDL_GL_CONTEXT_MAJOR_VERSION=4`, `MINOR=3`,
  `PROFILE_CORE`, doble buffer.
- [ ] Implementar `get_proc_address` con `SDL_GL_GetProcAddress`.
- [ ] Implementar `get_current_framebuffer` (FBO 0 o FBO propio).
- [ ] Gestionar `context_reset` / `context_destroy` con el contexto current.

### Día 4–5 — Integración con Azahar

- [ ] Aceptar `SET_HW_RENDER` y devolver `true`.
- [ ] No forzar `citra_graphics_api=Software` cuando el backend GL esté listo.
- [ ] Manejar `RETRO_HW_FRAME_BUFFER_VALID`.
- [ ] Presentar el FBO respetando aspect ratio y layout del core.
- [ ] Probar con Universal-Updater y un juego propio.

### Día 6 — Robustez y fallback

- [ ] Si OpenGL 4.3 no está disponible, avisar y usar software.
- [ ] Si `retro_load_game` falla tras `SET_HW_RENDER`, limpiar contexto y permitir reintento.
- [ ] Añadir logs de backend, versión GL y errores.
- [ ] Añadir hotkey para cambiar backend (requiere reinicio).

### Día 7 — Calidad y cierre

- [ ] Medir UI Hz y frame time antes/después.
- [ ] Actualizar README, PLAN y CHANGELOG.
- [ ] Commit `feat(video): add OpenGL 4.3 hardware backend`.
- [ ] Abrir issues para lo pendiente de M2.

**Definición de éxito del sprint:** Azahar renderiza por OpenGL 4.3 en Windows, el juego
corre a velocidad completa y la UI no se degrada; el fallback software sigue operativo.

---

## 13. Cumplimiento GPL y política de assets

### 13.1 Obligaciones GPL-3.0

- Publicar el código fuente completo de AlmenEMU bajo GPL-3.0-or-later.
- Conservar avisos de copyright y licencia.
- No imponer restricciones adicionales (DRM, anti-tamper, cláusulas de no redistribución).
- Si se distribuye un binario del core Azahar, ofrecer el código correspondiente:
  - lo más simple es compilar el core desde una versión publicada y enlazar la fuente,
  - o incluir una oferta escrita válida,
  - o usar la oferta del upstream solo si se cumplen las condiciones de GPLv3 §6.
- Incluir scripts de build y dependencias necesarias para reproducir el binario.
- Permitir la redistribución comercial y no comercial.

### 13.2 Assets y contenido

- No incluir ROMs, BIOS, firmware, `aes_keys`, `seeddb`, NAND ni archivos de sistema.
- No descargar automáticamente contenido copyrighted.
- Las carátulas deben ser locales o de dominio público; no empaquetar arte de juegos.
- El onboarding debe explicar cómo hacer dumps legales de la propia consola.

### 13.3 Marca

- No usar “Nintendo”, “3DS” o logotipos como nombre/marca del producto.
- Usar “compatible con Nintendo 3DS” de forma nominativa y con disclaimer.
- No usar el nombre/logo de Azahar como marca propia; acreditar el motor.
- Revisar nombre y logo antes de publicar.

### 13.4 Privacidad

- Sin telemetría por defecto.
- Crash logs locales; cualquier envío debe ser opt-in y explicado.
- No incluir identificadores de consola ni rutas personales en logs compartidos.

---

## 14. Decisiones abiertas (ADR)

| ID | Decisión | Opciones | Recomendación | Cuándo |
|---|---|---|---|---|
| ADR-01 | Formato de configuración | TOML / JSON | TOML con `toml++`, esquema versionado | Antes de M5 |
| ADR-02 | Política de versión del core | Última / pin | Pin de versión conocida + actualización manual | M2 |
| ADR-03 | Core macOS universal | `lipo` / dos slices | Universal con `lipo`; fallback a selección runtime | M3 |
| ADR-04 | Empaquetado | CPack+NSIS/WiX, AppImage, Flatpak, dmgbuild | CPack + NSIS, AppImage, dmgbuild; Flatpak después | M6 |
| ADR-05 | Auto-update | Ninguno / GitHub Releases / framework | Comprobación manual en v1.0; auto-update después | M6 |
| ADR-06 | Carátulas/metadatos | Locales / scraping | Solo locales o proveedor elegido por el usuario | M5 |
| ADR-07 | Telemetría | Ninguna / opt-in | Ninguna por defecto; opt-in explícito | v1.0 |
| ADR-08 | Segundo core | Solo Azahar / multi-core | Arquitectura multi-core, solo Azahar validado | M5 |
| ADR-09 | Netplay/cámara/Amiibo | Core / fork standalone | Fuera de alcance; evaluar fork si es requisito | v1.0 |
| ADR-10 | Modo portable | No / `--portable` | Implementar `--portable` | M5 |

---

## 15. Referencias

- Azahar: <https://github.com/azahar-emu/azahar>
- Azahar libretro: <https://github.com/azahar-emu/azahar/tree/master/src/citra_libretro>
- Releases de Azahar: <https://github.com/azahar-emu/azahar/releases>
- Especificación libretro: <https://docs.libretro.com/development/cores/developing-cores/>
- `libretro.h`: <https://github.com/libretro/libretro-common/blob/master/include/libretro.h>
- SDL3: <https://wiki.libsdl.org/SDL3/FrontPage>
- Dear ImGui: <https://github.com/ocornut/imgui>
- MoltenVK: <https://github.com/KhronosGroup/MoltenVK>
- Vulkan: <https://docs.vulkan.org/spec/latest/>
- GPL-3.0: <https://www.gnu.org/licenses/gpl-3.0.html>

---

## Anexo A — Mapa de archivos actual

| Archivo | Responsabilidad |
|---|---|
| `src/main.cpp` | Entrada de la aplicación |
| `src/app/Application.h/.cpp` | Ventana, UI, input, biblioteca, hotkeys |
| `src/core/LibretroCore.h/.cpp` | Carga dinámica y símbolos del core |
| `src/core/LibretroHost.h/.cpp` | Callbacks libretro, vídeo, audio, opciones |
| `third_party/libretro/include/libretro.h` | API libretro (MIT) |
| `scripts/fetch_core.py` | Descarga del core Azahar |
| `CMakeLists.txt` | Build y FetchContent (SDL3/ImGui) |
| `CMakePresets.json` | Presets Windows/Linux/macOS |
| `docs/ARCHITECTURE.md` | Diseño técnico detallado |
| `docs/PLAN.md` | Este plan |

## Anexo B — Estado de verificación

| Prueba | Resultado | Entorno |
|---|---|---|
| Build CMake/MSVC | ✅ | Windows 11, MSVC 2022, CMake 3.31 |
| Carga de Azahar 2126.2 | ✅ | Windows x64 |
| Boot de Universal-Updater 3.4.1 `.3dsx` | ✅ | Render hardware OpenGL 4.3 |
| Audio init | ✅ | SDL3, 32.768 Hz |
| Savestate `F5`/`F9` | ✅ | Guardado y restaurado en hardware |
| Captura `F12` | ✅ | BMP desde FBO (400x480) |
| Linux/macOS | ⏳ | CI pendiente de ejecutarse; macOS sigue en render software (M3) |
| Render hardware OpenGL 4.3 | ✅ | Windows, AMD Radeon 780M; core 60 fps / UI 60 Hz |
| Fallback `--software` | ✅ | Windows; core ~44 fps / UI ~11 Hz |
| Savestate carga | ✅ | Windows, hardware; estado restaurado |
| Captura FBO hardware | ✅ | Windows; BMP 400x480 correcto |
| Linux | ✅ | GCC 15.3 + SDL3 3.4.14 del sistema + Mesa: build completo, tests headless
  80/0 y `--help`; Azahar real pendiente |
| Vulkan/macOS | ⏳ | M3 |

---

*Este plan es un documento vivo. Al cerrar cada hito se actualizan las tablas de estado,
los gates de release y el backlog; los cambios se registran en `CHANGELOG.md`.*

---

## Anexo C — Registro de revisiones

| Versión | Fecha | Cambios |
|---|---|---|
| 1.0 | 2026-11-10 | Plan inicial: alcance, estado, hitos, backlog, riesgos y Git. |
| 1.1 | 2026-11-10 | Revisión completa: gates de release alineados con hitos; matriz de plataformas/core; core universal macOS; dependencias entre hitos; onboarding mínimo; tareas de pin/hash, savestate atómico, SBOM, accesibilidad de CI y documentación; decisiones abiertas (ADR). |
| 1.2 | 2026-11-10 | M2 implementado: backend OpenGL 4.3, FBO, fallback software, `--software`, captura FBO, frame pacing y verificación en Windows. Linux/macOS pendientes. |
| 1.3 | 2026-11-19 | P0 cerrado: `SET_HW_RENDER` validado (solo OpenGL de escritorio), fallback software sin `context_reset` forzado, `context_destroy` al cerrar, doble free de SDL3 corregido, core simulado + `host_tests` en CTest y CI con `ctest` y caché de dependencias. Linux verificado en build + tests headless. |
