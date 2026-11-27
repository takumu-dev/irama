# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(player SOURCES player_tests.cpp)
# The counting allocator is shared with base's tests.
target_include_directories(irama_player_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../base")
