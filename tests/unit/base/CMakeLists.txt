# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(base SOURCES assertion_tests.cpp narrow_tests.cpp spsc_queue_tests.cpp)
# The queue's stress test runs a producer and a consumer on threads of their own.
find_package(Threads REQUIRED)
target_link_libraries(irama_base_tests PRIVATE Threads::Threads)
