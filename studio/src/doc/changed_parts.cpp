// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/changed_parts.hpp"

#include <irama/engine/base/narrow.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace irama_studio {
namespace {

// Turns one map around. A kind that moved nothing has no map either way.
[[nodiscard]] std::vector<std::uint16_t> invert(const std::vector<std::uint16_t>& forward,
                                                std::size_t count_after) {
    if (forward.empty()) {
        return {};
    }
    std::vector<std::uint16_t> back(count_after, PositionMap::gone);
    for (std::size_t old_position = 0; old_position < forward.size(); ++old_position) {
        const std::uint16_t new_position = forward.at(old_position);
        if (new_position != PositionMap::gone) {
            back.at(new_position) = irama::narrow<std::uint16_t>(old_position);
        }
    }
    return back;
}

} // namespace

PositionMap PositionMap::inverse(std::size_t slots_after, std::size_t tracks_after) const {
    return {.slots = invert(slots, slots_after), .tracks = invert(tracks, tracks_after)};
}

} // namespace irama_studio
