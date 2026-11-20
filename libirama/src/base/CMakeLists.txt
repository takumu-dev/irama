# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# What every other component builds on.
irama_add_component(base
    HEADERS
        include/irama/engine/base/assertion.hpp
        include/irama/engine/base/narrow.hpp
        include/irama/engine/base/nonblocking.hpp
    SOURCES assertion.cpp
)
