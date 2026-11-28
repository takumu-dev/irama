# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# The player, the object a host renders, which owns the EngineSong, the instruments' live state, the
# voices and the queues of control messages.
irama_add_component(player
    HEADERS include/irama/engine/player/player.hpp
    SOURCES player.cpp
    DEPENDS seq model voice param base
)
