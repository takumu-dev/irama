// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "input/key_router.hpp"

#include "input/chord.hpp"
#include "input/note_message.hpp"
#include "input/shortcut_table.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <utility>

using irama_studio::Chord;
using irama_studio::Key;
using irama_studio::KeyEvent;
using irama_studio::KeyFocus;
using irama_studio::KeyRouter;
using irama_studio::Modifiers;
using irama_studio::NoteMessage;
using irama_studio::RoutedKey;
using irama_studio::Shortcut;
using irama_studio::ShortcutContext;
using irama_studio::ShortcutId;

namespace {

// USB HID usage IDs of the keys the tests press, as SDL's scancodes number them.
constexpr int key_z = 29;
constexpr int key_w = 26;
constexpr int key_x = 27;
constexpr int key_left_ctrl = 0xE0;
constexpr int key_right_ctrl = std::to_underlying(Key::right_ctrl);
constexpr int key_right_alt = std::to_underlying(Key::right_alt);
constexpr int key_keypad_divide = std::to_underlying(Key::keypad_divide);
constexpr int key_keypad_multiply = std::to_underlying(Key::keypad_multiply);
constexpr int key_f5 = std::to_underlying(Key::f5);

constexpr Modifiers no_modifiers{};
constexpr Modifiers ctrl{.ctrl = true};
constexpr Modifiers shift{.shift = true};

constexpr KeyFocus no_text_field{};
constexpr KeyFocus text_field{.text_field = true};

// A key of a US layout going down, which types `character`.
[[nodiscard]] KeyEvent press(int key, char32_t character = 0, Modifiers modifiers = no_modifiers) {
    return {.key = key, .character = character, .modifiers = modifiers};
}

[[nodiscard]] KeyEvent repeat(int key, char32_t character = 0) {
    return {.key = key, .character = character, .repeat = true};
}

[[nodiscard]] KeyEvent release(int key, char32_t character = 0,
                               Modifiers modifiers = no_modifiers) {
    return {.key = key, .character = character, .modifiers = modifiers, .down = false};
}

[[nodiscard]] NoteMessage note_on(int note) {
    return {.kind = NoteMessage::Kind::note_on, .note = note, .velocity = 1.0F};
}

[[nodiscard]] NoteMessage note_off(int note) {
    return {.kind = NoteMessage::Kind::note_off, .note = note};
}

[[nodiscard]] RoutedKey runs(ShortcutId id) { return {.shortcut = id}; }

[[nodiscard]] RoutedKey plays(const NoteMessage& message) { return {.note = message}; }

constexpr RoutedKey nothing{};

// Chords for the tests, which the IDs of the real table stand for.
constexpr std::array shift_z{Chord{.character = 'z', .modifiers = shift}};
constexpr std::array ctrl_z{Chord{.character = 'z', .modifiers = ctrl}};
constexpr std::array right_ctrl_or_f5{Chord{.key = Key::right_ctrl}, Chord{.key = Key::f5}};
constexpr std::array right_alt{Chord{.key = Key::right_alt}};

} // namespace

TEST_CASE("the router plays a note key and ends its note on release", "[studio][router]") {
    KeyRouter router;
    CHECK(router.route(press(key_z, U'z'), no_text_field) == plays(note_on(48)));
    CHECK(router.route(release(key_z, U'z'), no_text_field) == plays(note_off(48)));
}

TEST_CASE("a focused text field takes every key first", "[studio][router]") {
    KeyRouter router;
    CHECK(router.route(press(key_z, U'z'), text_field) == nothing);
    CHECK(router.route(release(key_z, U'z'), text_field) == nothing);
    CHECK(router.route(press(key_keypad_multiply), text_field) == nothing);
    CHECK(router.route(release(key_keypad_multiply), text_field) == nothing);
    CHECK(router.note_keys().octave() == 4);
}

TEST_CASE("a note key held before a text field takes focus still ends its note",
          "[studio][router]") {
    KeyRouter router;
    REQUIRE(router.route(press(key_z, U'z'), no_text_field) == plays(note_on(48)));
    CHECK(router.route(release(key_z, U'z'), text_field) == plays(note_off(48)));
}

TEST_CASE("a matching chord beats the note keys", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Shift+Z", .chords = shift_z},
    };
    KeyRouter router{table};
    CHECK(router.route(press(key_z, U'z', shift), no_text_field) == runs(ShortcutId::raise_octave));
    CHECK(router.route(release(key_z, U'z', shift), no_text_field) == nothing);
    // With no Shift, Z is a note key again, and with Shift it plays only where no chord matches.
    CHECK(router.route(press(key_z, U'z'), no_text_field) == plays(note_on(48)));
    CHECK(router.route(release(key_z, U'z'), no_text_field) == plays(note_off(48)));
    CHECK(router.route(press(key_x, U'x', shift), no_text_field) == plays(note_on(50)));
}

TEST_CASE("a chord matches the character a key types, and a note key its position",
          "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::lower_octave, .name = "Ctrl+Z", .chords = ctrl_z},
    };
    KeyRouter router{table};
    // On an AZERTY layout, Z is printed where a US keyboard has W, and W where it has Z.
    CHECK(router.route(press(key_w, U'z', ctrl), no_text_field) == runs(ShortcutId::lower_octave));
    CHECK(router.route(press(key_z, U'w', ctrl), no_text_field) == nothing);
    CHECK(router.route(press(key_z, U'w'), no_text_field) == plays(note_on(48)));
    // A layout's character matches in either case.
    CHECK(router.route(press(key_w, U'Z', ctrl), no_text_field) == runs(ShortcutId::lower_octave));
}

TEST_CASE("a chord matches only with exactly its modifiers", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::lower_octave, .name = "Ctrl+Z", .chords = ctrl_z},
    };
    KeyRouter router{table};
    CHECK(router.route(press(key_z, U'z', {.ctrl = true, .shift = true}), no_text_field) ==
          nothing);
    CHECK(router.route(press(key_z, U'z', {.ctrl = true, .alt = true}), no_text_field) == nothing);
    KeyEvent with_super = press(key_z, U'z', ctrl);
    with_super.super = true;
    CHECK(router.route(with_super, no_text_field) == nothing);
}

TEST_CASE("the note keys play nothing while Ctrl, Alt or Super is held", "[studio][router]") {
    KeyRouter router;
    CHECK(router.route(press(key_z, U'z', ctrl), no_text_field) == nothing);
    CHECK(router.route(press(key_z, U'z', {.alt = true}), no_text_field) == nothing);
    KeyEvent with_super = press(key_z, U'z');
    with_super.super = true;
    CHECK(router.route(with_super, no_text_field) == nothing);
    CHECK(router.route(press(key_z, U'z', shift), no_text_field) == plays(note_on(48)));
}

TEST_CASE("a window's shortcut runs only while its window has focus", "[studio][router]") {
    const std::array table{
        Shortcut{
            .id = ShortcutId::lower_octave,
            .name = "Pattern",
            .context = ShortcutContext::pattern,
            .chords = right_ctrl_or_f5,
        },
        Shortcut{
            .id = ShortcutId::raise_octave,
            .name = "Sequence",
            .context = ShortcutContext::sequence,
            .chords = right_ctrl_or_f5,
        },
    };
    KeyRouter router{table};
    CHECK(router.route(press(key_f5), no_text_field) == nothing);
    const KeyFocus pattern{.window = ShortcutContext::pattern};
    CHECK(router.route(press(key_f5), pattern) == runs(ShortcutId::lower_octave));
    const KeyFocus sequence{.window = ShortcutContext::sequence};
    CHECK(router.route(press(key_f5), sequence) == runs(ShortcutId::raise_octave));
}

TEST_CASE("a global shortcut runs whichever window has focus", "[studio][router]") {
    KeyRouter router;
    for (const auto window : {
             std::optional<ShortcutContext>{},
             std::optional{ShortcutContext::pattern},
             std::optional{ShortcutContext::sequence},
         }) {
        CHECK(router.route(press(key_keypad_divide), {.window = window}) ==
              runs(ShortcutId::lower_octave));
    }
}

TEST_CASE("numpad / and * run the octave shortcuts once a press, and play nothing",
          "[studio][router]") {
    KeyRouter router;
    CHECK(router.route(press(key_keypad_divide), no_text_field) == runs(ShortcutId::lower_octave));
    CHECK(router.route(repeat(key_keypad_divide), no_text_field) == nothing);
    CHECK(router.route(release(key_keypad_divide), no_text_field) == nothing);
    CHECK(router.route(press(key_keypad_multiply), no_text_field) ==
          runs(ShortcutId::raise_octave));
    CHECK(router.route(release(key_keypad_multiply), no_text_field) == nothing);
    // The router runs no shortcut itself.
    CHECK(router.note_keys().octave() == 4);
}

TEST_CASE("a lone Right Ctrl runs its shortcut on release", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
    };
    KeyRouter router{table};
    CHECK(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    // A held modifier repeats, as any other key does.
    CHECK(router.route(repeat(key_right_ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == runs(ShortcutId::raise_octave));
    // Its second chord runs on the press, as any other key's does.
    CHECK(router.route(press(key_f5), no_text_field) == runs(ShortcutId::raise_octave));
}

TEST_CASE("a lone Right Ctrl runs nothing when another key went down while it was held",
          "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
        Shortcut{.id = ShortcutId::lower_octave, .name = "Ctrl+Z", .chords = ctrl_z},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(press(key_z, U'z', ctrl), no_text_field) == runs(ShortcutId::lower_octave));
    CHECK(router.route(release(key_z, U'z', ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);

    // Another key down and up again before it goes up counts all the same.
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    REQUIRE(router.route(press(key_x, U'x', ctrl), no_text_field) == nothing);
    REQUIRE(router.route(release(key_x, U'x', ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);

    // And so does another key still held when it goes up.
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    REQUIRE(router.route(press(key_x, U'x', ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);
    REQUIRE(router.route(release(key_x, U'x'), no_text_field) == nothing);

    // Alone again, it runs.
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == runs(ShortcutId::raise_octave));
}

TEST_CASE("a lone Right Ctrl runs nothing when another key was held before it",
          "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_left_ctrl, 0, ctrl), no_text_field) == nothing);
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_left_ctrl), no_text_field) == nothing);
}

TEST_CASE("a lone Right Ctrl runs nothing when a mouse button went down while it was held",
          "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    router.pointer_pressed();
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);
}

TEST_CASE("a lone Right Ctrl runs nothing while a text field has focus", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), text_field) == nothing);
}

TEST_CASE("a lone Right Alt runs its own shortcut, and Right Ctrl's doesn't", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::lower_octave, .name = "Right Alt", .chords = right_alt},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);
    // As AltGr, Right Alt may come with Ctrl and Alt both held.
    REQUIRE(router.route(press(key_right_alt, 0, {.ctrl = true, .alt = true}), no_text_field) ==
            nothing);
    CHECK(router.route(release(key_right_alt), no_text_field) == runs(ShortcutId::lower_octave));
}

TEST_CASE("a held note key ends its note on release whatever is pressed meanwhile",
          "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
        Shortcut{.id = ShortcutId::lower_octave, .name = "Ctrl+Z", .chords = ctrl_z},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_x, U'x'), no_text_field) == plays(note_on(50)));
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    REQUIRE(router.route(press(key_z, U'z', ctrl), no_text_field) ==
            runs(ShortcutId::lower_octave));
    REQUIRE(router.route(press(key_f5, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_x, U'x', ctrl), no_text_field) == plays(note_off(50)));
}

TEST_CASE("a held note key ends the note it started after an octave shortcut", "[studio][router]") {
    KeyRouter router;
    REQUIRE(router.route(press(key_z, U'z'), no_text_field) == plays(note_on(48)));
    REQUIRE(router.route(press(key_keypad_multiply), no_text_field) ==
            runs(ShortcutId::raise_octave));
    router.note_keys().set_octave(5);
    CHECK(router.route(release(key_z, U'z'), no_text_field) == plays(note_off(48)));
}

TEST_CASE("release-all forgets the held keys and a lone modifier", "[studio][router]") {
    const std::array table{
        Shortcut{.id = ShortcutId::raise_octave, .name = "Right Ctrl", .chords = right_ctrl_or_f5},
    };
    KeyRouter router{table};
    REQUIRE(router.route(press(key_z, U'z'), no_text_field) == plays(note_on(48)));
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.release_all() == NoteMessage{.kind = NoteMessage::Kind::release_all});
    CHECK(router.route(release(key_right_ctrl), no_text_field) == nothing);
    // Z went up where Studio couldn't see it, and counts as up.
    REQUIRE(router.route(press(key_right_ctrl, 0, ctrl), no_text_field) == nothing);
    CHECK(router.route(release(key_right_ctrl), no_text_field) == runs(ShortcutId::raise_octave));
}

TEST_CASE("keys outside the keyboard page are ignored", "[studio][router]") {
    KeyRouter router;
    CHECK(router.route(press(-1), no_text_field) == nothing);
    CHECK(router.route(press(1'000), no_text_field) == nothing);
    CHECK(router.route(release(1'000), no_text_field) == nothing);
}
