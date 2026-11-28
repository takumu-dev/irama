// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/chord.hpp"

#include "input/key_map.hpp"

#include <format>
#include <string>
#include <utility>

namespace irama_studio {
namespace {

// Whether `key` is one of the named keys. A chord with no modifier may use any of them, since the
// note keys and entry use none.
[[nodiscard]] bool is_named(Key key) {
    switch (key) {
    case Key::none:
        return false;
    case Key::enter:
    case Key::escape:
    case Key::backspace:
    case Key::tab:
    case Key::space:
    case Key::f1:
    case Key::f2:
    case Key::f3:
    case Key::f4:
    case Key::f5:
    case Key::f6:
    case Key::f7:
    case Key::f8:
    case Key::f9:
    case Key::f10:
    case Key::f11:
    case Key::f12:
    case Key::scroll_lock:
    case Key::insert:
    case Key::home:
    case Key::page_up:
    case Key::delete_key:
    case Key::end:
    case Key::page_down:
    case Key::right:
    case Key::left:
    case Key::down:
    case Key::up:
    case Key::keypad_divide:
    case Key::keypad_multiply:
    case Key::keypad_minus:
    case Key::keypad_plus:
    case Key::keypad_enter:
    case Key::keypad_1:
    case Key::keypad_2:
    case Key::keypad_3:
    case Key::keypad_4:
    case Key::keypad_5:
    case Key::keypad_6:
    case Key::keypad_7:
    case Key::keypad_8:
    case Key::keypad_9:
    case Key::keypad_0:
    case Key::keypad_period:
    case Key::right_ctrl:
    case Key::right_alt:
        return true;
    }
    return false;
}

// The name the menus give a named key.
[[nodiscard]] std::string key_name(Key key) {
    const int usage = std::to_underlying(key);
    if (key >= Key::f1 && key <= Key::f12) {
        return std::format("F{}", usage - std::to_underlying(Key::f1) + 1);
    }
    if (key >= Key::keypad_1 && key <= Key::keypad_9) {
        return std::format("Numpad {}", usage - std::to_underlying(Key::keypad_1) + 1);
    }
    switch (key) {
    case Key::enter:
        return "Enter";
    case Key::escape:
        return "Esc";
    case Key::backspace:
        return "Backspace";
    case Key::tab:
        return "Tab";
    case Key::space:
        return "Space";
    case Key::scroll_lock:
        return "Scroll Lock";
    case Key::insert:
        return "Insert";
    case Key::home:
        return "Home";
    case Key::page_up:
        return "Page Up";
    case Key::delete_key:
        return "Delete";
    case Key::end:
        return "End";
    case Key::page_down:
        return "Page Down";
    case Key::right:
        return "Right";
    case Key::left:
        return "Left";
    case Key::down:
        return "Down";
    case Key::up:
        return "Up";
    case Key::keypad_divide:
        return "Numpad /";
    case Key::keypad_multiply:
        return "Numpad *";
    case Key::keypad_minus:
        return "Numpad -";
    case Key::keypad_plus:
        return "Numpad +";
    case Key::keypad_enter:
        return "Numpad Enter";
    case Key::keypad_0:
        return "Numpad 0";
    case Key::keypad_period:
        return "Numpad .";
    case Key::right_ctrl:
        return "Right Ctrl";
    case Key::right_alt:
        return "Right Alt";
    default:
        return {};
    }
}

} // namespace

bool is_lone_modifier(Key key) { return key == Key::right_ctrl || key == Key::right_alt; }

bool is_allowed(const Chord& chord) {
    const bool has_character = chord.character != '\0';
    if ((chord.key == Key::none) == !has_character) {
        return false;
    }
    if (is_lone_modifier(chord.key)) {
        return chord.modifiers.none();
    }
    if (!chord.modifiers.none()) {
        return chord.key == Key::none || is_named(chord.key);
    }
    return is_named(chord.key) &&
           !note_for_key(std::to_underlying(chord.key), min_octave).has_value() &&
           !is_note_key_character(static_cast<unsigned char>(chord.character));
}

std::string chord_text(const Chord& chord) {
    std::string text;
    if (chord.modifiers.ctrl) {
        text += "Ctrl+";
    }
    if (chord.modifiers.alt) {
        text += "Alt+";
    }
    if (chord.modifiers.shift) {
        text += "Shift+";
    }
    if (chord.key != Key::none) {
        text += key_name(chord.key);
    } else if (chord.character >= 'a' && chord.character <= 'z') {
        text += static_cast<char>(chord.character - 'a' + 'A');
    } else {
        text += chord.character;
    }
    return text;
}

} // namespace irama_studio
