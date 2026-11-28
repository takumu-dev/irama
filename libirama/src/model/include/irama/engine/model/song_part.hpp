// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The song part: the timing, the seed, the tracks' note columns and the sequence.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace irama {

/// What a song part is built from. Every field has the new song's value until it is set.
struct SongDescription {
    /// The tempo in hundredths of a beat per minute, from 2,000 to 99,900.
    std::uint32_t bpm100 = 12'000;
    /// Lines per beat, from 1 to 64.
    std::uint8_t lpb = 4;
    /// Beats per bar, from 1 to 16.
    std::uint8_t beats_per_bar = 4;
    /// Ticks per line, from 1 to 32.
    std::uint8_t ticks_per_line = 12;
    /// The seed of the player's random numbers.
    std::uint64_t seed = 0x6972'616d'6131;
    /// Each track's count of note columns, from 1 to 12, for from 1 to 64 tracks. It is read only
    /// while the part is built.
    std::span<const std::uint8_t> note_columns = {};
    /// The pattern number of each slot, from 0 to 999, for from 1 to 1,000 slots. It is read only
    /// while the part is built.
    std::span<const std::uint16_t> sequence = {};
};

class SongPart;

/// The single owner of a song part.
using SongPartPtr = OwnedPtr<SongPart>;

/// The part of an EngineSong that holds what belongs to the whole song: the timing, the seed, each
/// track's count of note columns, and the sequence, as pattern numbers.
///
/// A song part never changes once it is built. A song part built from a description that leaves
/// the tracks or the sequence empty has the new song's: 4 tracks of one note column, and one slot
/// holding pattern 0.
class SongPart {
    // Lets create() alone construct a part, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// The most tracks a song has.
    static constexpr std::size_t max_tracks = 64;

    /// The most note columns a track has.
    static constexpr std::size_t max_note_columns = 12;

    /// The most slots a sequence has.
    static constexpr std::size_t max_slots = 1'000;

    /// Builds a song part, on a control thread.
    ///
    /// Each field of the description is in its range, which is checked.
    ///
    /// \param allocator The allocator to draw on, which the part is freed to.
    /// \param description What the part holds.
    /// \return The part, or `AllocationError::out_of_memory` when the allocator has no memory.
    [[nodiscard]] static std::expected<SongPartPtr, AllocationError>
    create(const Allocator& allocator, const SongDescription& description = {});

    /// Makes the part, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param description What the part holds, already checked.
    SongPart(Key key, const SongDescription& description);

    /// \return The tempo in hundredths of a beat per minute.
    [[nodiscard]] std::uint32_t bpm100() const { return bpm100_; }

    /// \return Lines per beat.
    [[nodiscard]] std::uint8_t lpb() const { return lpb_; }

    /// \return Beats per bar.
    [[nodiscard]] std::uint8_t beats_per_bar() const { return beats_per_bar_; }

    /// \return Ticks per line.
    [[nodiscard]] std::uint8_t ticks_per_line() const { return ticks_per_line_; }

    /// \return The seed of the player's random numbers.
    [[nodiscard]] std::uint64_t seed() const { return seed_; }

    /// \return Each track's count of note columns, one for each track.
    [[nodiscard]] std::span<const std::uint8_t> note_columns() const;

    /// \return The pattern number of each slot, one for each slot.
    [[nodiscard]] std::span<const std::uint16_t> sequence() const;

    /// \return Whether two parts hold the same song.
    friend bool operator==(const SongPart& left, const SongPart& right);

  private:
    std::uint32_t bpm100_;
    std::uint8_t lpb_;
    std::uint8_t beats_per_bar_;
    std::uint8_t ticks_per_line_;
    std::uint8_t tracks_;
    std::uint16_t slots_;
    std::uint64_t seed_;
    std::array<std::uint8_t, max_tracks> note_columns_{};
    std::array<std::uint16_t, max_slots> sequence_{};
};

} // namespace irama
