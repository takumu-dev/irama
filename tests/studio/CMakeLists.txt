# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Irama Studio's tests: its logic, which needs no window, as one Catch2 program that links
# irama_studio_core, and the smoke test, which starts Studio itself.

add_executable(irama_studio_tests
    audio_device_tests.cpp
    config_files_tests.cpp
    device_choice_tests.cpp
    heartbeat_watch_tests.cpp
    key_map_tests.cpp
    message_backlog_tests.cpp
    meter_display_tests.cpp
    note_keys_tests.cpp
    options_tests.cpp
    play_tests.cpp
    settings_tests.cpp
    video_drivers_tests.cpp
)
target_link_libraries(irama_studio_tests PRIVATE irama_studio_core irama_catch_main)
irama_target_conventions(irama_studio_tests)
catch_discover_tests(irama_studio_tests
    TEST_PREFIX "studio: "
    PROPERTIES LABELS unit
)

set(irama_smoke_test "studio: starts with no display, draws 300 frames and quits cleanly")
add_test(NAME "${irama_smoke_test}"
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:irama-studio>"
        -DFRAMES=300
        "-DCONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/smoke-config"
        -P "${PROJECT_SOURCE_DIR}/cmake/smoke_test.cmake"
)
set_tests_properties("${irama_smoke_test}" PROPERTIES LABELS smoke)
