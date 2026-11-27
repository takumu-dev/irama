# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(filter SOURCES svf_tests.cpp)
# The response test measures spectra.
target_link_libraries(irama_filter_tests PRIVATE irama_test_support)
