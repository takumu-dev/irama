# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(model
    SOURCES
        change_set_tests.cpp
        engine_song_tests.cpp
        instrument_part_tests.cpp
        pattern_part_tests.cpp
        song_part_tests.cpp
)
# The counting allocator is shared with base's tests.
target_include_directories(irama_model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../base")
