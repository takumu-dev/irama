// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <irama/engine/model/change_set.hpp>
#include <irama/engine/player/player.hpp>

#include <expected>

namespace irama_studio {

/// Creates a player and sends it the whole song as one change set at frame 0, before anything
/// renders it. So the new player holds the song at its first frame, as if it had been created
/// with it.
///
/// \param config How the player is made.
/// \param song A change set of every part of the song.
/// \return The player, or why it couldn't be created.
[[nodiscard]] std::expected<irama::PlayerPtr, irama::CreateError>
create_player(const irama::PlayerConfig& config, irama::ChangeSetPtr song);

} // namespace irama_studio
