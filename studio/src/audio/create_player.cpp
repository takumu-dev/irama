// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/create_player.hpp"

#include "audio/instrument_values.hpp"
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>

#include <expected>

namespace irama_studio {

std::expected<irama::PlayerPtr, irama::CreateError> create_player(const irama::PlayerConfig& config,
                                                                  InstrumentValues& values) {
    auto player = irama::Player::create(config);
    if (player) {
        values.send_all();
        values.flush([&player](irama::ParamId id, float value) {
            return (*player)->set_parameter(0, id, value);
        });
        IRAMA_ASSERT(!values.waiting(), "a new player's queue takes every value of the instrument");
    }
    return player;
}

} // namespace irama_studio
