# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Parameters: the descriptors that say what each one is, and the smoother that glides it to each
# new value.
irama_add_component(param
    HEADERS
        include/irama/engine/param/param_descriptor.hpp
        include/irama/engine/param/param_id.hpp
        include/irama/engine/param/param_smoother.hpp
    SOURCES
        param_descriptor.cpp
        param_smoother.cpp
    DEPENDS base
)
