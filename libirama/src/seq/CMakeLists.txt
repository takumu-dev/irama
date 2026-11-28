# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The sequencer, which keeps the play position and turns an EngineSong's patterns into note-ons and
# note-offs at exact frames, without touching any voice.
irama_add_component(seq
    HEADERS include/irama/engine/seq/sequencer.hpp
    SOURCES sequencer.cpp
    DEPENDS model base
)
