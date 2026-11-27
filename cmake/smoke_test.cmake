# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Starts Irama Studio with no display, under SDL's offscreen video driver, and lets it draw a number
# of frames and quit. SDL_GPU draws them on whatever Vulkan driver the machine has. The test passes
# only if Studio exits with status 0 having written nothing to stderr, so a warning from Studio, SDL
# or the driver fails it as surely as a crash, a failed assertion or a sanitizer's report. Studio
# starts from an empty settings directory of its own, never the user's, and must leave its settings
# and its layout there when it quits.
#
#   PROGRAM     irama-studio
#   FRAMES      the frames it draws before it quits
#   CONFIG_DIR  the settings directory to give it, emptied first

file(REMOVE_RECURSE "${CONFIG_DIR}")
file(MAKE_DIRECTORY "${CONFIG_DIR}")
set(ENV{SDL_VIDEO_DRIVER} offscreen)
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
