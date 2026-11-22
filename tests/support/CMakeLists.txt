# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# What the tests measure audio with: a fast Fourier transform, the Blackman-Harris window, and the
# strongest component of a periodic signal that isn't one of its harmonics. It isn't part of
# libirama and links only the standard library, so a component's tests can link it and still link
# no other component.
add_library(irama_test_support STATIC
    include/irama/test_support/spectrum.hpp
    spectrum.cpp
)
target_include_directories(irama_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
irama_target_conventions(irama_test_support)

add_executable(irama_test_support_tests spectrum_tests.cpp)
target_link_libraries(irama_test_support_tests PRIVATE irama_test_support irama_catch_main)
irama_target_conventions(irama_test_support_tests)
catch_discover_tests(irama_test_support_tests
    TEST_PREFIX "test support: "
    PROPERTIES LABELS unit
)
