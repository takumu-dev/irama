# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The oscillators that a voice plays, and the tuning that gives each note its frequency.
irama_add_component(source
    HEADERS
        include/irama/engine/source/sine_oscillator.hpp
        include/irama/engine/source/tuning.hpp
    SOURCES
        sine_oscillator.cpp
        tuning.cpp
    DEPENDS base
)
