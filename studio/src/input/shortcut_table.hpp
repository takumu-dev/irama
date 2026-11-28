// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/chord.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace irama_studio {

/// Where a shortcut works.
enum class ShortcutContext : std::uint8_t {
    /// Whichever window has focus.
    global,
    /// While the Pattern window has focus.
    pattern,
    /// While the Sequence window has focus.
    sequence,
};

/// The Studio function a shortcut runs.
enum class ShortcutId : std::uint8_t {
    lower_octave,
    raise_octave,
};

/// One entry of the shortcut table: a named Studio function, where it works, and the chords that
/// run it.
struct Shortcut {
    ShortcutId id = ShortcutId::lower_octave;
    /// The name the menus show.
    std::string_view name{};
    ShortcutContext context = ShortcutContext::global;
    /// The default chords, the first of them the main one.
    std::span<const Chord> chords{};
};

/// Every shortcut in Studio, which the menus show and the key router matches keys against.
[[nodiscard]] std::span<const Shortcut> shortcut_table();

/// The shortcut table's entry for `id`.
[[nodiscard]] const Shortcut& find_shortcut(ShortcutId id);

/// How the menus write a shortcut's chords: each chord's text, in order, separated by commas.
[[nodiscard]] std::string chords_text(const Shortcut& shortcut);

/// Whether shortcuts in the two contexts can be in force at once, and so must never share a chord:
/// a global shortcut with any other, or two in the same context.
[[nodiscard]] bool contexts_overlap(ShortcutContext first, ShortcutContext second);

} // namespace irama_studio
