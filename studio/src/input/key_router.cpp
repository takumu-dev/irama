// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/key_router.hpp"

#include "input/chord.hpp"
#include "input/note_message.hpp"
#include "input/shortcut_table.hpp"
#include <irama/engine/base/narrow.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace irama_studio {
namespace {

// Whether a shortcut in `context` is in force with the keyboard's focus `focus`.
[[nodiscard]] bool in_force(ShortcutContext context, const KeyFocus& focus) {
    return context == ShortcutContext::global || focus.window == context;
}

// Whether a key going down, with what was held with it, plays `chord`.
[[nodiscard]] bool plays_chord(const Chord& chord, const KeyEvent& event) {
    if (event.super || chord.modifiers != event.modifiers) {
        return false;
    }
    if (chord.key != Key::none) {
        return std::cmp_equal(std::to_underlying(chord.key), event.key);
    }
    char32_t character = event.character;
    if (character >= U'A' && character <= U'Z') {
        character += U'a' - U'A';
    }
    return character == static_cast<unsigned char>(chord.character);
}

} // namespace

KeyRouter::KeyRouter(std::span<const Shortcut> table, int octave)
    : table_(table), note_keys_(octave) {}

RoutedKey KeyRouter::route(const KeyEvent& event, const KeyFocus& focus) {
    if (event.key < 0 || irama::narrow<std::size_t>(event.key) >= key_count) {
        return {};
    }
    const auto index = irama::narrow<std::size_t>(event.key);
    if (!event.down) {
        held_.reset(index);
        // A key going up always reaches the note keys, so that a held note key ends its note.
        RoutedKey routed{.note = note_keys_.release(event.key)};
        const std::optional lone = std::exchange(lone_modifier_, std::nullopt);
        if (lone && std::cmp_equal(std::to_underlying(*lone), event.key) && !focus.text_field) {
            routed.shortcut = match(Chord{.key = *lone}, focus);
        }
        return routed;
    }
    if (!event.repeat) {
        // Any key going down ends a lone modifier, and a lone modifier starts one only with no
        // other key held.
        lone_modifier_.reset();
        const auto key = static_cast<Key>(event.key);
        if (is_lone_modifier(key) && held_.none() && !focus.text_field) {
            lone_modifier_ = key;
        }
        held_.set(index);
    }
    if (focus.text_field || is_lone_modifier(static_cast<Key>(event.key))) {
        return {};
    }
    for (const Shortcut& shortcut : table_) {
        if (!in_force(shortcut.context, focus)) {
            continue;
        }
        const auto played = [&event](const Chord& chord) { return plays_chord(chord, event); };
        if (std::ranges::any_of(shortcut.chords, played)) {
            // A held key's repeats run nothing, and play nothing either.
            return event.repeat ? RoutedKey{} : RoutedKey{.shortcut = shortcut.id};
        }
    }
    if (event.modifiers.ctrl || event.modifiers.alt || event.super) {
        return {};
    }
    return {.note = note_keys_.press(event.key, event.repeat)};
}

void KeyRouter::pointer_pressed() { lone_modifier_.reset(); }

NoteMessage KeyRouter::release_all() {
    held_.reset();
    lone_modifier_.reset();
    return note_keys_.release_all();
}

std::optional<ShortcutId> KeyRouter::match(const Chord& chord, const KeyFocus& focus) const {
    for (const Shortcut& shortcut : table_) {
        if (in_force(shortcut.context, focus) && std::ranges::contains(shortcut.chords, chord)) {
            return shortcut.id;
        }
    }
    return std::nullopt;
}

} // namespace irama_studio
