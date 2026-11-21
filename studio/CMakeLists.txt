# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: GPL-3.0-or-later

# Irama Studio: the static library irama_studio_core, which holds all of Studio but main() and which
# Studio's tests link too, and the executable irama-studio. It draws Dear ImGui through SDL_GPU, so
# on Linux it needs a Vulkan driver.

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(inter CONFIG REQUIRED)
find_package(Threads REQUIRED)
# miniaudio is a single header, with no CMake package of its own.
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)

# The UI font is compiled into Studio, so there is no file to find beside the executable.
set(font_source "${CMAKE_CURRENT_BINARY_DIR}/inter_regular_ttf.cpp")
set(embed_file "${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake")
add_custom_command(
    OUTPUT "${font_source}"
    COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=${INTER_REGULAR_TTF}"
        "-DOUTPUT=${font_source}"
        -DHEADER=ui/fonts.hpp
        -DFUNCTION=irama_studio::inter_regular_ttf
        -P "${embed_file}"
    DEPENDS "${INTER_REGULAR_TTF}" "${embed_file}"
    COMMENT "Embedding Inter Regular"
    VERBATIM
)
# A list of bytes gives clang-tidy nothing to check, and takes it long to read.
set_source_files_properties("${font_source}" PROPERTIES SKIP_LINTING ON)

# miniaudio's implementation is a header too: it gives clang-tidy nothing of Irama's to check.
set_source_files_properties(src/audio/miniaudio.cpp PROPERTIES SKIP_LINTING ON)

add_library(irama_studio_core STATIC
    src/app/application.cpp
    src/app/config_files.cpp
    src/app/options.cpp
    src/app/settings.cpp
    src/audio/audio_device.cpp
    src/audio/audio_engine.cpp
    src/audio/create_player.cpp
    src/audio/instrument_values.cpp
    src/audio/miniaudio.cpp
    src/input/key_map.cpp
    src/input/note_keys.cpp
    src/ui/fonts.cpp
    src/ui/param_text.cpp
    src/ui/theme.cpp
    src/views/about.cpp
    src/views/audio_window.cpp
    src/views/instrument_layout.cpp
    src/views/instrument_window.cpp
    src/views/status_bar.cpp
    src/views/workspace.cpp
    "${font_source}"
)
target_include_directories(irama_studio_core PUBLIC src)
target_include_directories(irama_studio_core SYSTEM PRIVATE "${MINIAUDIO_INCLUDE_DIR}")
# miniaudio loads each backend's library at run time, and runs its devices on threads of its own.
target_link_libraries(irama_studio_core
    PUBLIC irama::engine SDL3::SDL3 imgui::imgui
    PRIVATE Threads::Threads ${CMAKE_DL_LIBS}
)
target_compile_definitions(irama_studio_core PRIVATE IRAMA_VERSION_STRING="${PROJECT_VERSION}")
irama_target_conventions(irama_studio_core)

add_executable(irama-studio src/main.cpp)
target_link_libraries(irama-studio PRIVATE irama_studio_core)
irama_target_conventions(irama-studio)
