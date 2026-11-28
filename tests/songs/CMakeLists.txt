# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Test songs written as the pattern grid shows them, built into EngineSong parts for the tests of
# seq and player. It isn't part of libirama and links only model, which both of those link, so their
# tests still link nothing above their own component.
add_library(irama_test_songs STATIC
    include/irama/test_songs/test_songs.hpp
    test_songs.cpp
)
target_include_directories(irama_test_songs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(irama_test_songs PUBLIC irama::model)
irama_target_conventions(irama_test_songs)

add_executable(irama_test_songs_tests test_songs_tests.cpp)
target_link_libraries(irama_test_songs_tests PRIVATE irama_test_songs irama_catch_main)
irama_target_conventions(irama_test_songs_tests)
catch_discover_tests(irama_test_songs_tests
    TEST_PREFIX "test songs: "
    PROPERTIES LABELS unit
)
