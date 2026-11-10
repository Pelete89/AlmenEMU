cmake_minimum_required(VERSION 3.24)
project(AlmenEMU VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(OpenGL REQUIRED)

# --- SDL3 (static) -----------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.20
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# --- Dear ImGui --------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.93.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(imgui PUBLIC cxx_std_17)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static OpenGL::GL)

# --- Application -------------------------------------------------------------
add_executable(AlmenEMU
    src/main.cpp
    src/app/Application.cpp
    src/core/LibretroCore.cpp
    src/core/LibretroHost.cpp
    src/video/OpenGLBackend.cpp
)

target_include_directories(AlmenEMU PRIVATE
    src
    third_party/libretro/include
)
target_link_libraries(AlmenEMU PRIVATE imgui SDL3::SDL3-static OpenGL::GL ${CMAKE_DL_LIBS})
target_compile_definitions(AlmenEMU PRIVATE SDL_MAIN_HANDLED)

if(MSVC)
    target_compile_options(AlmenEMU PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(AlmenEMU PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(APPLE)
    target_link_libraries(AlmenEMU PRIVATE
        "-framework CoreFoundation"
        "-framework Foundation"
    )
    target_compile_definitions(AlmenEMU PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Copy cores/ next to the executable at build time.
add_custom_target(copy_cores ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:AlmenEMU>/cores"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/cores"
            "$<TARGET_FILE_DIR:AlmenEMU>/cores"
    DEPENDS AlmenEMU
    COMMENT "Copying libretro cores next to AlmenEMU"
)
