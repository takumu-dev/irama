// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/create_player.hpp"

#include "audio/instrument_values.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/player/player.hpp>

#include <expected>
#include <new>
#include <utility>

namespace irama_studio {
namespace {

// A change set of instrument 01 holding the copy, and of the new song's song part if `with_song`.
[[nodiscard]] irama::ChangeSetPtr change(const InstrumentValues& values, bool with_song) {
    const irama::Allocator allocator = irama::default_allocator();
    auto changes = irama::ChangeSet::create(allocator, {.instruments = 1});
    auto part = irama::InstrumentPart::create(allocator, values.values());
    if (!changes || !part) {
        throw std::bad_alloc{};
    }
    (*changes)->replace_instrument(studio_instrument, std::move(*part));
    if (with_song) {
        auto song = irama::SongPart::create(allocator);
        if (!song) {
            throw std::bad_alloc{};
        }
        (*changes)->replace_song(std::move(*song));
    }
    return std::move(*changes);
}

} // namespace

irama::ChangeSetPtr instrument_change(const InstrumentValues& values) {
    return change(values, false);
}

irama::ChangeSetPtr song_change(const InstrumentValues& values) { return change(values, true); }

std::expected<irama::PlayerPtr, irama::CreateError> create_player(const irama::PlayerConfig& config,
                                                                  const InstrumentValues& values) {
    auto player = irama::Player::create(config);
    if (player) {
        irama::ChangeSetPtr song = song_change(values);
        const bool queued = (*player)->change(0, song);
        IRAMA_ASSERT(queued, "a new player's queue takes the whole song");
    }
    return player;
}

} // namespace irama_studio
