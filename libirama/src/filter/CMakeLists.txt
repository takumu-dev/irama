# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The state variable filter a voice plays its source through.
irama_add_component(filter
    HEADERS
        include/irama/engine/filter/svf.hpp
    SOURCES
        svf.cpp
    DEPENDS base
)
