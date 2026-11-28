// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace irama_studio {

/// Where each slot or track of the song before an edit went: for each old position, its new one,
/// or `gone`. An empty list means that the edit moved none of that kind.
struct PositionMap {
    /// The entry of a slot or a track the edit removed.
    static constexpr std::uint16_t gone = 0xFFFF;

    /// One entry for each slot before the edit, or none.
    std::vector<std::uint16_t> slots{};
    /// One entry for each track before the edit, or none.
    std::vector<std::uint16_t> tracks{};

    /// The map back: for each position after the edit, the position it had before, or `gone` for
    /// one the edit added.
    ///
    /// \param slots_after How many slots the song has after the edit.
    /// \param tracks_after How many tracks the song has after the edit.
    [[nodiscard]] PositionMap inverse(std::size_t slots_after, std::size_t tracks_after) const;

    friend bool operator==(const PositionMap&, const PositionMap&) = default;
};

/// The parts of the song that applying, undoing or redoing an edit changed, in the EngineSong's
/// terms, each once, in a fixed order: the song part, then patterns by number, then instruments by
/// number, and the position map of an edit that moves slots or tracks.
struct ChangedParts {
    /// Whether anything but patterns and instruments changed: the metadata, timing, seed, tracks,
    /// sequence, sections or song parameters.
    bool song = false;
    /// The pattern numbers whose cells, line count or markers changed, in increasing order.
    std::vector<std::size_t> patterns{};
    /// The instrument numbers that changed, in increasing order.
    std::vector<std::size_t> instruments{};
    std::optional<PositionMap> positions{};

    /// \return Whether nothing changed.
    [[nodiscard]] bool empty() const {
        return !song && patterns.empty() && instruments.empty() && !positions;
    }

    friend bool operator==(const ChangedParts&, const ChangedParts&) = default;
};

} // namespace irama_studio
