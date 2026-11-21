# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# A voice, one sounding note, and the envelope that shapes it.
irama_add_component(voice
    HEADERS
        include/irama/engine/voice/adsr.hpp
        include/irama/engine/voice/voice.hpp
    SOURCES
        adsr.cpp
        voice.cpp
    DEPENDS source base
)
