// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/chord.hpp"
#include "input/key_map.hpp"
#include "input/note_keys.hpp"
#include "input/note_message.hpp"
#include "input/shortcut_table.hpp"

#include <bitset>
#include <cstddef>
#include <optional>
#include <span>

namespace irama_studio {

/// A key going down or up.
struct KeyEvent {
    /// The key's USB HID usage ID, which places it on the keyboard whatever the layout.
    int key = 0;
    /// The character the key types in the current layout with no modifier, or 0 for a key that
    /// types none.
    char32_t character = 0;
    /// The modifiers held, either side's.
    Modifiers modifiers{};
    /// Whether Super is held, which no chord uses.
    bool super = false;
    /// Whether the key went down, rather than up.
    bool down = true;
    /// Whether the press is the key repeating while held.
    bool repeat = false;
};

/// What has the keyboard's focus.
struct KeyFocus {
    /// Whether a text field has focus, and so takes every key.
    bool text_field = false;
    /// The context of the window with focus, if it has one.
    std::optional<ShortcutContext> window{};
};

/// What a key does.
struct RoutedKey {
    /// The shortcut to run, if any.
    std::optional<ShortcutId> shortcut{};
    /// The note message to send, if any.
    std::optional<NoteMessage> note{};

    friend bool operator==(const RoutedKey&, const RoutedKey&) = default;
};

/// Sends each key where it belongs. The first rule that applies wins:
///
/// 1. A focused text field takes every key.
/// 2. A key that matches a chord of a shortcut in force runs that shortcut.
/// 3. Otherwise the key goes to the note keys, which play nothing while Ctrl, Alt or Super is held.
///
/// A key going up always reaches the note keys, so a held note key ends its note whatever was
/// pressed meanwhile. A lone modifier's shortcut runs when the key goes up, and only if no other
/// key went down while it was held, so Right Ctrl+Z is still Ctrl+Z.
class KeyRouter {
  public:
    /// Matches keys against `table`, and plays the note keys from the keyboard octave `octave`.
    explicit KeyRouter(std::span<const Shortcut> table = shortcut_table(),
                       int octave = default_octave);

    /// \return The note keys, which keep the keyboard octave.
    [[nodiscard]] NoteKeys& note_keys() { return note_keys_; }

    /// \return The note keys, which keep the keyboard octave.
    [[nodiscard]] const NoteKeys& note_keys() const { return note_keys_; }

    /// Routes a key going down or up.
    ///
    /// \param event The key, and what was held with it.
    /// \param focus What has the keyboard's focus.
    /// \return The shortcut to run and the note message to send, if any.
    [[nodiscard]] RoutedKey route(const KeyEvent& event, const KeyFocus& focus);

    /// Counts a mouse button going down as another key, so that a lone modifier held through a
    /// click runs nothing.
    void pointer_pressed();

    /// Forgets every held key, as when the window loses the keyboard.
    ///
    /// \return The release-all to send.
    [[nodiscard]] NoteMessage release_all();

  private:
    // Usage IDs on the keyboard page fit in a byte.
    static constexpr std::size_t key_count = 256;

    [[nodiscard]] std::optional<ShortcutId> match(const Chord& chord, const KeyFocus& focus) const;

    std::span<const Shortcut> table_;
    NoteKeys note_keys_;
    // The keys held down.
    std::bitset<key_count> held_;
    // The lone modifier held with no other key, whose shortcut runs when it goes up.
    std::optional<Key> lone_modifier_;
};

} // namespace irama_studio
