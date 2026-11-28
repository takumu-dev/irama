// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/instrument_values.hpp"
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/player/player.hpp>

#include <expected>

namespace irama_studio {

/// The instrument number that Studio's copy of the instrument plays as.
inline constexpr int studio_instrument = 0x01;

/// Builds a change set of instrument 01 holding Studio's copy of the instrument's values.
///
/// \param values Studio's copy.
/// \return The change set. Running out of memory throws `std::bad_alloc`, as a standard container
///         would.
[[nodiscard]] irama::ChangeSetPtr instrument_change(const InstrumentValues& values);

/// Builds a change set of the whole song: the new song's song part, and instrument 01 holding
/// Studio's copy of the instrument's values.
///
/// \param values Studio's copy.
/// \return The change set. Running out of memory throws `std::bad_alloc`, as a standard container
///         would.
[[nodiscard]] irama::ChangeSetPtr song_change(const InstrumentValues& values);

/// Creates a player and sends it the whole song as one change set at frame 0, before anything
/// renders it. So the new player holds every value at its first frame, as if it had been created
/// with them.
///
/// \param config How the player is made.
/// \param values Studio's copy of the instrument's values.
/// \return The player, or why it couldn't be created.
[[nodiscard]] std::expected<irama::PlayerPtr, irama::CreateError>
create_player(const irama::PlayerConfig& config, const InstrumentValues& values);

} // namespace irama_studio
