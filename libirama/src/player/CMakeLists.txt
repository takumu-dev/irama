# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The player, the object a host renders, which owns the instrument, the voices and the queue of
# control messages.
irama_add_component(player
    HEADERS include/irama/engine/player/player.hpp
    SOURCES player.cpp
    DEPENDS voice param base
)
