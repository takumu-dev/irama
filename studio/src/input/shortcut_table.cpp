// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/shortcut_table.hpp"

#include "input/chord.hpp"
#include <irama/engine/base/assertion.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string>
#include <utility>

namespace irama_studio {
namespace {

constexpr std::array lower_octave_chords{Chord{.key = Key::keypad_divide}};
constexpr std::array raise_octave_chords{Chord{.key = Key::keypad_multiply}};
constexpr std::array undo_chords{Chord{.character = 'z', .modifiers = {.ctrl = true}}};
constexpr std::array redo_chords{
    Chord{.character = 'y', .modifiers = {.ctrl = true}},
    Chord{.character = 'z', .modifiers = {.ctrl = true, .shift = true}},
};

constexpr std::array table{
    Shortcut{
        .id = ShortcutId::lower_octave,
        .name = "Lower keyboard octave",
        .context = ShortcutContext::global,
        .chords = lower_octave_chords,
    },
    Shortcut{
        .id = ShortcutId::raise_octave,
        .name = "Raise keyboard octave",
        .context = ShortcutContext::global,
        .chords = raise_octave_chords,
    },
    Shortcut{
        .id = ShortcutId::undo,
        .name = "Undo",
        .context = ShortcutContext::global,
        .chords = undo_chords,
    },
    Shortcut{
        .id = ShortcutId::redo,
        .name = "Redo",
        .context = ShortcutContext::global,
        .chords = redo_chords,
    },
};

// find_shortcut() finds each entry at its ID's place.
static_assert(std::ranges::all_of(std::views::iota(std::size_t{0}, table.size()),
                                  [](std::size_t i) {
                                      return std::cmp_equal(std::to_underlying(table.at(i).id), i);
                                  }));

} // namespace

std::span<const Shortcut> shortcut_table() { return table; }

const Shortcut& find_shortcut(ShortcutId id) {
    const Shortcut& shortcut = table.at(std::to_underlying(id));
    IRAMA_ASSERT(shortcut.id == id, "the table holds each shortcut at its ID's place");
    return shortcut;
}

std::string chords_text(const Shortcut& shortcut) {
    std::string text;
    for (const Chord& chord : shortcut.chords) {
        if (!text.empty()) {
            text += ", ";
        }
        text += chord_text(chord);
    }
    return text;
}

bool contexts_overlap(ShortcutContext first, ShortcutContext second) {
    return first == ShortcutContext::global || second == ShortcutContext::global || first == second;
}

} // namespace irama_studio
