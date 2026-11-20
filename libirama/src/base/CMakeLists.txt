# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# What every other component builds on.
irama_add_component(base
    HEADERS
        include/irama/engine/base/assertion.hpp
        include/irama/engine/base/narrow.hpp
        include/irama/engine/base/nonblocking.hpp
        include/irama/engine/base/render_scope.hpp
        include/irama/engine/base/spsc_queue.hpp
    SOURCES
        assertion.cpp
        realtime_sanitizer.hpp
        render_scope.cpp
)
if(IRAMA_REALTIME_GUARD)
    target_compile_definitions(irama_base PRIVATE IRAMA_REALTIME_GUARD)
endif()
