// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "input/note_keys.hpp"

#include "input/note_message.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using irama_studio::NoteKeys;
using irama_studio::NoteMessage;

namespace {

// USB HID usage IDs of the keys the tests press, as SDL's scancodes number them.
constexpr int key_z = 29;
constexpr int key_x = 27;
constexpr int key_q = 20;
constexpr int key_p = 19;
constexpr int key_u = 24;
constexpr int key_comma = 54;
constexpr int key_f = 9;
constexpr int key_keypad_divide = 84;
constexpr int key_keypad_multiply = 85;

constexpr bool pressed = false;
constexpr bool repeated = true;

[[nodiscard]] NoteMessage note_on(int note) {
    return {.kind = NoteMessage::Kind::note_on, .note = note, .velocity = 1.0F};
}

[[nodiscard]] NoteMessage note_off(int note) {
    return {.kind = NoteMessage::Kind::note_off, .note = note};
}

} // namespace

TEST_CASE("a note key sends note-on at velocity 1 when pressed and note-off when released",
          "[studio][keys]") {
    NoteKeys keys;
    CHECK(keys.press(key_z, pressed) == note_on(48));
    CHECK(keys.release(key_z) == note_off(48));
}

TEST_CASE("key repeat sends nothing", "[studio][keys]") {
    NoteKeys keys;
    REQUIRE(keys.press(key_z, pressed) == note_on(48));
    CHECK(keys.press(key_z, repeated) == std::nullopt);
    CHECK(keys.press(key_z, repeated) == std::nullopt);
    CHECK(keys.release(key_z) == note_off(48));
    // A repeat with no press before it, as when a key went down while note keys couldn't play.
    CHECK(keys.press(key_x, repeated) == std::nullopt);
    CHECK(keys.release(key_x) == std::nullopt);
}

TEST_CASE("a second press of a held key sends nothing", "[studio][keys]") {
    NoteKeys keys;
    REQUIRE(keys.press(key_z, pressed) == note_on(48));
    CHECK(keys.press(key_z, pressed) == std::nullopt);
    CHECK(keys.release(key_z) == note_off(48));
    CHECK(keys.release(key_z) == std::nullopt);
}

TEST_CASE("releasing a key that isn't held sends nothing", "[studio][keys]") {
    NoteKeys keys;
    CHECK(keys.release(key_z) == std::nullopt);
    CHECK(keys.release(key_f) == std::nullopt);
}

TEST_CASE("keys off the piano rows send nothing", "[studio][keys]") {
    NoteKeys keys;
    CHECK(keys.press(key_f, pressed) == std::nullopt);
    CHECK(keys.release(key_f) == std::nullopt);
}

TEST_CASE("a key released after an octave change ends the note it started", "[studio][keys]") {
    NoteKeys keys;
    REQUIRE(keys.press(key_z, pressed) == note_on(48));
    keys.set_octave(5);
    CHECK(keys.release(key_z) == note_off(48));
    CHECK(keys.press(key_z, pressed) == note_on(60));
    keys.set_octave(3);
    CHECK(keys.release(key_z) == note_off(60));
}

TEST_CASE("two keys on one note send note-off only with the second release", "[studio][keys]") {
    NoteKeys keys;
    // At octave 4, , is the lower row's C-5, and Q is the upper row's.
    CHECK(keys.press(key_comma, pressed) == note_on(60));
    CHECK(keys.press(key_q, pressed) == note_on(60));
    CHECK(keys.release(key_comma) == std::nullopt);
    CHECK(keys.release(key_q) == note_off(60));

    // Released the other way round, and with the note reached through an octave change.
    CHECK(keys.press(key_q, pressed) == note_on(60));
    keys.set_octave(5);
    CHECK(keys.press(key_z, pressed) == note_on(60));
    CHECK(keys.release(key_q) == std::nullopt);
    CHECK(keys.release(key_z) == note_off(60));
}

TEST_CASE("a key whose note would pass B-9 sends nothing", "[studio][keys]") {
    NoteKeys keys{8};
    CHECK(keys.press(key_u, pressed) == note_on(119));
    CHECK(keys.press(key_p, pressed) == std::nullopt);
    CHECK(keys.release(key_p) == std::nullopt);
    CHECK(keys.release(key_u) == note_off(119));
}

TEST_CASE("release-all releases every note and forgets the held keys", "[studio][keys]") {
    NoteKeys keys;
    REQUIRE(keys.press(key_z, pressed) == note_on(48));
    REQUIRE(keys.press(key_comma, pressed) == note_on(60));
    REQUIRE(keys.press(key_q, pressed) == note_on(60));
    CHECK(keys.release_all() == NoteMessage{.kind = NoteMessage::Kind::release_all});
    CHECK(keys.release(key_z) == std::nullopt);
    CHECK(keys.release(key_comma) == std::nullopt);
    CHECK(keys.release(key_q) == std::nullopt);
    // Held again, the note counts its keys from none.
    CHECK(keys.press(key_q, pressed) == note_on(60));
    CHECK(keys.release(key_q) == note_off(60));
}

TEST_CASE("forgetting the held keys sends nothing for them", "[studio][keys]") {
    NoteKeys keys;
    REQUIRE(keys.press(key_z, pressed) == note_on(48));
    keys.forget();
    CHECK(keys.release(key_z) == std::nullopt);
    CHECK(keys.press(key_z, pressed) == note_on(48));
}

TEST_CASE("numpad / and * play nothing and leave the keyboard octave alone", "[studio][keys]") {
    NoteKeys keys;
    CHECK(keys.press(key_keypad_multiply, pressed) == std::nullopt);
    CHECK(keys.release(key_keypad_multiply) == std::nullopt);
    CHECK(keys.press(key_keypad_divide, pressed) == std::nullopt);
    CHECK(keys.release(key_keypad_divide) == std::nullopt);
    CHECK(keys.octave() == 4);
}

TEST_CASE("the keyboard octave is kept from 0 to 8", "[studio][keys]") {
    CHECK(NoteKeys{-3}.octave() == 0);
    CHECK(NoteKeys{12}.octave() == 8);
    NoteKeys keys;
    keys.set_octave(9);
    CHECK(keys.octave() == 8);
    keys.set_octave(-1);
    CHECK(keys.octave() == 0);
}
