# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Starts Irama Studio and lets it draw a number of frames and quit. On Linux it runs with no display,
# under SDL's offscreen video driver, and SDL_GPU draws on whatever Vulkan driver the machine has. On
# Windows the offscreen driver has no window that Direct3D 12 can present to, so Studio opens a real
# window under SDL's windows driver, drawn by whatever Direct3D 12 adapter the machine has, WARP on a
# machine with no GPU. The test passes only if Studio exits with status 0 having written nothing to
# stderr, so a warning from Studio, SDL or the driver fails it as surely as a crash, a failed
# assertion or a sanitizer's report. Studio starts from an empty settings directory of its own,
# never the user's, and must leave its settings and its layout there when it quits.
#
#   PROGRAM     irama-studio
#   FRAMES      the frames it draws before it quits
#   CONFIG_DIR  the settings directory to give it, emptied first

file(REMOVE_RECURSE "${CONFIG_DIR}")
file(MAKE_DIRECTORY "${CONFIG_DIR}")
if(CMAKE_HOST_WIN32)
    set(ENV{SDL_VIDEO_DRIVER} windows)
else()
    set(ENV{SDL_VIDEO_DRIVER} offscreen)
endif()
execute_process(
    COMMAND "${PROGRAM}" --exit-after-frames "${FRAMES}" --config-dir "${CONFIG_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result STREQUAL "0" OR NOT error STREQUAL "")
    # Each line indented, so that CMake prints it as it is, rather than wrapping it.
    string(REGEX REPLACE "\n$" "" error "${error}")
    string(REPLACE "\n" "\n  " error "${error}")
    message(FATAL_ERROR "irama-studio exited with ${result}, writing to stderr:\n  ${error}")
endif()
foreach(name IN ITEMS studio.ini imgui.ini)
    if(NOT EXISTS "${CONFIG_DIR}/${name}")
        message(FATAL_ERROR "irama-studio quit without writing ${name} in ${CONFIG_DIR}")
    endif()
endforeach()
