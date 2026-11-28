# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(seq SOURCES sequencer_tests.cpp)
target_link_libraries(irama_seq_tests PRIVATE irama_test_songs)
