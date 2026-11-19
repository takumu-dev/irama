# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(capi SOURCES version_tests.cpp)

# The version as CMake read it from irama.h, so the tests catch the two drifting apart.
target_compile_definitions(irama_capi_tests PRIVATE
    IRAMA_TEST_PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    IRAMA_TEST_PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    IRAMA_TEST_PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# irama.h included from C99, strictly, as a game written in C would.
add_executable(irama_capi_c99_tests irama_h_c99_tests.c)
target_link_libraries(irama_capi_c99_tests PRIVATE irama::capi)
set_target_properties(irama_capi_c99_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
irama_target_warnings(irama_capi_c99_tests)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(irama_capi_c99_tests PRIVATE -pedantic-errors)
endif()
add_test(NAME "capi: irama.h compiles as C99 and reports its version" COMMAND irama_capi_c99_tests)
set_tests_properties("capi: irama.h compiles as C99 and reports its version" PROPERTIES
    LABELS unit
)
