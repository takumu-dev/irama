// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The EngineSong: the song a player renders, as a table of immutable parts.

#pragma once

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <array>
#include <cstddef>

namespace irama {

/// The song a player renders: a fixed table of one song part, 1,000 pattern entries, 000 to 999,
/// and 254 instrument entries, 01 to FE, each owning an immutable part, or empty.
///
/// Parts refer to each other by number, looked up in the table when they are read, so replacing one
/// part never touches another. The table itself allocates nothing: it changes only by `apply()`,
/// which swaps the parts of a change set into it. A new EngineSong is empty, with no song part.
class EngineSong {
  public:
    /// How many pattern entries the table has.
    static constexpr std::size_t pattern_count = 1'000;

    /// The first instrument number.
    static constexpr std::size_t first_instrument = 0x01;

    /// The last instrument number.
    static constexpr std::size_t last_instrument = 0xFE;

    /// How many instrument entries the table has.
    static constexpr std::size_t instrument_count = last_instrument - first_instrument + 1;

    /// Makes an empty song, with no song part and every entry empty.
    EngineSong() = default;

    /// \return The song part, or null for an empty song.
    [[nodiscard]] const SongPart* song() const { return song_.get(); }

    /// \param number The pattern number, from 0 to 999, which is checked.
    /// \return The pattern's part, or null for an empty number.
    [[nodiscard]] const PatternPart* pattern(std::size_t number) const;

    /// \param number The instrument number, from 0x01 to 0xFE, which is checked.
    /// \return The instrument's part, or null for an empty number.
    [[nodiscard]] const InstrumentPart* instrument(std::size_t number) const;

    /// Puts every part of a change set in its entry, together, and the part each entry held in its
    /// place in the change set. A null part empties its entry.
    ///
    /// It allocates and frees nothing, so the audio thread can call it.
    ///
    /// \param changes The parts, which end up as the parts they replaced.
    void apply(ChangeSet& changes) IRAMA_NONBLOCKING;

    /// \return Whether two songs hold equal parts in every entry, and the same entries empty.
    friend bool operator==(const EngineSong& left, const EngineSong& right);

  private:
    SongPartPtr song_;
    std::array<PatternPartPtr, pattern_count> patterns_;
    std::array<InstrumentPartPtr, instrument_count> instruments_;
};

} // namespace irama
