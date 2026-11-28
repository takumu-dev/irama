// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/song_part.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <span>

namespace {

// The new song's tracks and sequence.
constexpr std::array<std::uint8_t, 4> new_song_note_columns{1, 1, 1, 1};
constexpr std::array<std::uint16_t, 1> new_song_sequence{0};

// The last pattern number.
constexpr std::uint16_t last_pattern = 999;

// The description with the new song's tracks and sequence wherever it leaves them empty.
[[nodiscard]] irama::SongDescription completed(irama::SongDescription description) {
    if (description.note_columns.empty()) {
        description.note_columns = new_song_note_columns;
    }
    if (description.sequence.empty()) {
        description.sequence = new_song_sequence;
    }
    return description;
}

} // namespace

namespace irama {

std::expected<SongPartPtr, AllocationError> SongPart::create(const Allocator& allocator,
                                                             const SongDescription& description) {
    const SongDescription song = completed(description);
    IRAMA_ASSERT(song.bpm100 >= 2'000 && song.bpm100 <= 99'900, "a tempo is from 20 to 999 BPM");
    IRAMA_ASSERT(song.lpb >= 1 && song.lpb <= 64, "a beat has from 1 to 64 lines");
    IRAMA_ASSERT(song.beats_per_bar >= 1 && song.beats_per_bar <= 16,
                 "a bar has from 1 to 16 beats");
    IRAMA_ASSERT(song.ticks_per_line >= 1 && song.ticks_per_line <= 32,
                 "a line has from 1 to 32 ticks");
    IRAMA_ASSERT(song.note_columns.size() <= max_tracks, "a song has from 1 to 64 tracks");
    IRAMA_ASSERT(std::ranges::all_of(song.note_columns,
                                     [](std::uint8_t columns) {
                                         return columns >= 1 && columns <= max_note_columns;
                                     }),
                 "a track has from 1 to 12 note columns");
    IRAMA_ASSERT(song.sequence.size() <= max_slots, "a sequence has from 1 to 1,000 slots");
    IRAMA_ASSERT(std::ranges::all_of(song.sequence,
                                     [](std::uint16_t pattern) { return pattern <= last_pattern; }),
                 "a slot holds a pattern number from 0 to 999");
    return make_owned<SongPart>(allocator, Key{}, song);
}

SongPart::SongPart(Key /*key*/, const SongDescription& description)
    : bpm100_{description.bpm100}, lpb_{description.lpb}, beats_per_bar_{description.beats_per_bar},
      ticks_per_line_{description.ticks_per_line},
      tracks_{narrow<std::uint8_t>(description.note_columns.size())},
      slots_{narrow<std::uint16_t>(description.sequence.size())}, seed_{description.seed} {
    std::ranges::copy(description.note_columns, note_columns_.begin());
    std::ranges::copy(description.sequence, sequence_.begin());
}

std::span<const std::uint8_t> SongPart::note_columns() const {
    return std::span{note_columns_}.first(tracks_);
}

std::span<const std::uint16_t> SongPart::sequence() const {
    return std::span{sequence_}.first(slots_);
}

bool operator==(const SongPart& left, const SongPart& right) {
    return left.bpm100_ == right.bpm100_ && left.lpb_ == right.lpb_ &&
           left.beats_per_bar_ == right.beats_per_bar_ &&
           left.ticks_per_line_ == right.ticks_per_line_ && left.seed_ == right.seed_ &&
           std::ranges::equal(left.note_columns(), right.note_columns()) &&
           std::ranges::equal(left.sequence(), right.sequence());
}

} // namespace irama
