# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The basic source a voice plays: the band-limited oscillator and the noise, and the tuning that
# gives each note its frequency.
irama_add_component(source
    HEADERS
        include/irama/engine/source/noise.hpp
        include/irama/engine/source/oscillator.hpp
        include/irama/engine/source/tuning.hpp
    SOURCES
        noise.cpp
        oscillator.cpp
        tuning.cpp
    DEPENDS base
)
