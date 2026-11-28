// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace irama_studio {

/// A key that a chord names by what it is rather than by the character it types: one whose place
/// and meaning are the same on every keyboard layout. Each is the key's USB HID usage ID on the
/// keyboard page, as the note keys read keys.
enum class Key : std::uint8_t {
    /// No named key: the chord names a character instead.
    none = 0x00,
    enter = 0x28,
    escape = 0x29,
    backspace = 0x2A,
    tab = 0x2B,
    space = 0x2C,
    f1 = 0x3A,
    f2 = 0x3B,
    f3 = 0x3C,
    f4 = 0x3D,
    f5 = 0x3E,
    f6 = 0x3F,
    f7 = 0x40,
    f8 = 0x41,
    f9 = 0x42,
    f10 = 0x43,
    f11 = 0x44,
    f12 = 0x45,
    scroll_lock = 0x47,
    insert = 0x49,
    home = 0x4A,
    page_up = 0x4B,
    delete_key = 0x4C,
    end = 0x4D,
    page_down = 0x4E,
    right = 0x4F,
    left = 0x50,
    down = 0x51,
    up = 0x52,
    keypad_divide = 0x54,
    keypad_multiply = 0x55,
    keypad_minus = 0x56,
    keypad_plus = 0x57,
    keypad_enter = 0x58,
    keypad_1 = 0x59,
    keypad_2 = 0x5A,
    keypad_3 = 0x5B,
    keypad_4 = 0x5C,
    keypad_5 = 0x5D,
    keypad_6 = 0x5E,
    keypad_7 = 0x5F,
    keypad_8 = 0x60,
    keypad_9 = 0x61,
    keypad_0 = 0x62,
    keypad_period = 0x63,
    right_ctrl = 0xE4,
    right_alt = 0xE6,
};

/// The modifiers a chord holds. Each means either side's key.
struct Modifiers {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    /// \return Whether no modifier is held.
    [[nodiscard]] constexpr bool none() const { return !ctrl && !alt && !shift; }

    friend bool operator==(const Modifiers&, const Modifiers&) = default;
};

/// The keys that run a shortcut: a named key, or the character a key types in the current layout,
/// with modifiers. A chord by character is on whichever key types it, so Ctrl+Z is where Z is
/// printed, while the note keys stay where they are on every layout.
struct Chord {
    /// The named key, or none for a chord by character.
    Key key = Key::none;
    /// The character, in lower case, of a chord with no named key.
    char character = '\0';
    Modifiers modifiers{};

    friend bool operator==(const Chord&, const Chord&) = default;
};

/// Whether `key` is a lone modifier: Right Ctrl or Right Alt, which as a chord of its own is
/// pressed and released with no other key.
[[nodiscard]] bool is_lone_modifier(Key key);

/// Whether a chord is one Studio allows. It names a key or a character, never both or neither. A
/// lone modifier holds no other modifier. A chord with no modifier may use only Space, Enter, Esc,
/// Tab, the arrows, Page Up, Page Down, Home, End, Insert, Delete, Backspace, the F-keys, the
/// numpad, Scroll Lock and the lone modifiers, since plain letters, digits and punctuation belong
/// to the note keys and to entry. So it never sits on a note key or a character the note keys type.
[[nodiscard]] bool is_allowed(const Chord& chord);

/// How the menus write a chord, as `Ctrl+Shift+Z`, `Numpad /` or `Right Ctrl`.
[[nodiscard]] std::string chord_text(const Chord& chord);

} // namespace irama_studio
