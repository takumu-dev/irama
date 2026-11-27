# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

irama_add_tests(base
    SOURCES
        allocator_tests.cpp
        assertion_tests.cpp
        counting_allocator.hpp
        narrow_tests.cpp
        owned_array_tests.cpp
        owned_ptr_tests.cpp
        pcg32_tests.cpp
        render_scope_tests.cpp
        spsc_queue_tests.cpp
)
# The queue's stress test runs a producer and a consumer on threads of their own.
find_package(Threads REQUIRED)
target_link_libraries(irama_base_tests PRIVATE Threads::Threads)
