# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(source
    SOURCES
        noise_tests.cpp
        oscillator_tests.cpp
        tuning_tests.cpp
)
# The aliasing test measures spectra.
target_link_libraries(irama_source_tests PRIVATE irama_test_support)
