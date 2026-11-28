// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/create_player.hpp"

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/player/player.hpp>

#include <expected>

namespace irama_studio {

std::expected<irama::PlayerPtr, irama::CreateError> create_player(const irama::PlayerConfig& config,
                                                                  irama::ChangeSetPtr song) {
    auto player = irama::Player::create(config);
    if (player) {
        const bool queued = (*player)->change(0, song);
        IRAMA_ASSERT(queued, "a new player's queue takes the whole song");
    }
    return player;
}

} // namespace irama_studio
