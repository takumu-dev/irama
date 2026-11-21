# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# A voice, one sounding note, the envelope that shapes it, and the instrument whose parameters every
# voice reads.
irama_add_component(voice
    HEADERS
        include/irama/engine/voice/adsr.hpp
        include/irama/engine/voice/instrument.hpp
        include/irama/engine/voice/voice.hpp
    SOURCES
        adsr.cpp
        instrument.cpp
        voice.cpp
    DEPENDS source param base
)
