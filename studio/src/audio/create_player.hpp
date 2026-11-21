// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/instrument_values.hpp"
#include <irama/engine/player/player.hpp>

#include <expected>

namespace irama_studio {

/// Creates a player and gives it the whole of Studio's copy of the instrument's values, by
/// set-parameter at frame 0, before anything renders it. So the new player holds every value at
/// its first frame, as if it had been created with them.
///
/// \param config How the player is made.
/// \param values Studio's copy, whose values the player takes, so none is left waiting.
/// \return The player, or why it couldn't be created.
[[nodiscard]] std::expected<irama::PlayerPtr, irama::CreateError>
create_player(const irama::PlayerConfig& config, InstrumentValues& values);

} // namespace irama_studio
