// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace irama_studio {

/// The kinds of part an edit copies the first time it writes one.
enum class PartKind : std::uint8_t {
    /// The metadata, timing and seed.
    info,
    /// The track list: names and column counts.
    tracks,
    /// The sequence and its sections.
    sequence,
    /// The song parameters.
    parameters,
    /// A whole pattern, or its absence: written when a pattern comes or goes, or changes shape.
    pattern,
    /// A pattern's line count and markers.
    pattern_head,
    /// A pattern's cells in one track.
    block,
    /// An instrument, or its absence.
    instrument,
};

/// Which part: its kind, and the pattern or instrument number and the track it belongs to, where
/// its kind has them.
struct PartKey {
    PartKind kind = PartKind::info;
    std::size_t number = 0;
    std::size_t track = 0;

    friend auto operator<=>(const PartKey&, const PartKey&) = default;
};

/// A copy of one part, of the type its kind holds.
using PartValue =
    std::variant<SongInfo, std::vector<Track>, Sequence, std::vector<SongParameter>,
                 std::optional<Pattern>, PatternHead, TrackBlock, std::optional<Instrument>>;

/// The parts one edit has written, each with a copy of its value at another time: before the edit
/// while the edit stands, and after it while it is undone. Swapping the copies with the Document
/// moves it from one to the other.
///
/// A whole pattern covers its head and blocks, so the copies never hold both: they never overlap,
/// and the order of a swap doesn't matter.
class PartCopies {
  public:
    /// Whether a copy of the part is kept, alone or within its whole pattern.
    [[nodiscard]] bool covers(const PartKey& key) const;

    /// Keeps a copy of a part that no copy covers yet. A whole pattern takes in the copies of its
    /// head and blocks, which are older, so it holds the pattern as it was before them.
    ///
    /// \param key The part, which no copy covers, which is checked.
    /// \param value The part's value, of its kind's type.
    void keep(const PartKey& key, PartValue value);

    /// Takes in the copies of a later application of the same edit, as one gesture: each part this
    /// covers keeps its older copy.
    void merge(PartCopies later);

    /// Swaps every copy with the part it was taken from.
    void swap_with(Document& document);

    /// Forgets the copies that are equal to their part in `document`, which the edit didn't change.
    void drop_unchanged(const Document& document);

    /// \return Whether no copy is kept.
    [[nodiscard]] bool empty() const { return copies_.empty(); }

    /// \return The parts the copies belong to, in the EngineSong's terms, with no position map.
    [[nodiscard]] ChangedParts parts() const;

    /// \return Whether a copy of anything but patterns and instruments is kept.
    [[nodiscard]] bool touches_song() const;

    /// \return How many bytes the copies take: their cells, their text, and a fixed amount for
    ///         each.
    [[nodiscard]] std::size_t bytes() const;

  private:
    // Swaps one copy with its part.
    static void swap_part(Document& document, const PartKey& key, PartValue& value);

    std::map<PartKey, PartValue> copies_;
};

} // namespace irama_studio
