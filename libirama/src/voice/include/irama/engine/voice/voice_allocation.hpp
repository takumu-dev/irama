// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// How a player chooses the voice a note-on takes, and the fade slot a stolen voice fades out in.

#pragma once

#include <irama/engine/voice/voice.hpp>

#include <cstddef>
#include <span>

namespace irama {

/// Chooses the voice a note-on takes: the first free one, or, with none free, the one it steals.
///
/// With every voice sounding, a note-on steals the oldest voice in its release. With none in its
/// release, it steals the quietest, whose `Voice::level()` is lowest, and of the equally quiet, the
/// oldest. The oldest voice is the one with the smallest `Voice::order()`, whose note started
/// first, however long it has been in its release.
///
/// \param voices The voices, at least one, which is checked.
/// \return The index in `voices` of the voice the note-on takes.
[[nodiscard]] std::size_t choose_voice(std::span<const Voice> voices);

/// Chooses the fade slot a stolen voice fades out in: the first free one, or, with none free, the
/// one whose fade is nearest its end, which is cut. Of fades equally near their end, the first.
///
/// \param slots The fade slots, at least one, which is checked.
/// \return The index in `slots` of the slot the stolen voice fades out in.
[[nodiscard]] std::size_t choose_fade_slot(std::span<const Voice> slots);

} // namespace irama
