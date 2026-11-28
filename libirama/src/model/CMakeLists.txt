# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The song a player renders: the EngineSong, a table of immutable parts, and the change sets that
# replace them.
irama_add_component(model
    HEADERS
        include/irama/engine/model/change_set.hpp
        include/irama/engine/model/engine_song.hpp
        include/irama/engine/model/instrument_part.hpp
        include/irama/engine/model/pattern_part.hpp
        include/irama/engine/model/song_part.hpp
    SOURCES
        change_set.cpp
        engine_song.cpp
        instrument_part.cpp
        pattern_part.cpp
        song_part.cpp
    DEPENDS base
)
