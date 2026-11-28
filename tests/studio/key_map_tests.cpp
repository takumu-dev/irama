// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "input/key_map.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace {

// USB HID usage IDs of the keyboard page, by name, as SDL's scancodes number them.
constexpr int key_a = 4;
constexpr int key_1 = 30;
constexpr int key_0 = 39;
constexpr int key_semicolon = 51;
constexpr int key_comma = 54;
constexpr int key_period = 55;
constexpr int key_slash = 56;
constexpr int key_space = 44;
constexpr int key_left_bracket = 47;
constexpr int key_keypad_divide = 84;

[[nodiscard]] constexpr int letter(char name) { return key_a + (name - 'A'); }

[[nodiscard]] constexpr int digit(char name) { return name == '0' ? key_0 : key_1 + (name - '1'); }

// The usage ID of a key on the piano rows, by the character it shows on a US keyboard.
[[nodiscard]] constexpr int key(char name) {
    if (name >= 'A' && name <= 'Z') {
        return letter(name);
    }
    if (name >= '0' && name <= '9') {
        return digit(name);
    }
    switch (name) {
    case ',':
        return key_comma;
    case '.':
        return key_period;
    case ';':
        return key_semicolon;
    case '/':
        return key_slash;
    default:
        return 0;
    }
}

// C-4, the note Z plays at octave 4, numbered as the note column numbers notes.
constexpr int c4 = 48;

// Each row's keys, from the lowest note up, a semitone apart.
constexpr std::string_view lower_row = "ZSXDCVGBHNJM,L.;/";
constexpr std::string_view upper_row = "Q2W3ER5T6Y7UI9O0P";

} // namespace

TEST_CASE("the lower row plays C-4 to E-5 at octave 4", "[studio][keys]") {
    for (std::size_t index = 0; index < lower_row.size(); ++index) {
        INFO("key " << lower_row.at(index));
        CHECK(irama_studio::note_for_key(key(lower_row.at(index)), 4) ==
              c4 + static_cast<int>(index));
    }
}

TEST_CASE("the upper row plays C-5 to E-6 at octave 4", "[studio][keys]") {
    for (std::size_t index = 0; index < upper_row.size(); ++index) {
        INFO("key " << upper_row.at(index));
        CHECK(irama_studio::note_for_key(key(upper_row.at(index)), 4) ==
              c4 + 12 + static_cast<int>(index));
    }
}

TEST_CASE("Z plays C-4, note 48, at the default octave", "[studio][keys]") {
    CHECK(irama_studio::default_octave == 4);
    CHECK(irama_studio::note_for_key(key('Z'), irama_studio::default_octave) == 48);
}

TEST_CASE("the keyboard octave moves both rows by twelve notes", "[studio][keys]") {
    CHECK(irama_studio::note_for_key(key('Z'), 0) == 0);
    CHECK(irama_studio::note_for_key(key('Q'), 0) == 12);
    CHECK(irama_studio::note_for_key(key('Z'), 3) == 36);
    CHECK(irama_studio::note_for_key(key('M'), 8) == 107);
    CHECK(irama_studio::note_for_key(key('Q'), 8) == 108);
    CHECK(irama_studio::note_for_key(key('U'), 8) == 119);
}

TEST_CASE("the keyboard octave runs from 0 to 8", "[studio][keys]") {
    CHECK(irama_studio::min_octave == 0);
    CHECK(irama_studio::max_octave == 8);
    CHECK(irama_studio::clamp_octave(-1) == 0);
    CHECK(irama_studio::clamp_octave(0) == 0);
    CHECK(irama_studio::clamp_octave(8) == 8);
    CHECK(irama_studio::clamp_octave(9) == 8);
}

TEST_CASE("a key whose note would pass B-9 plays nothing", "[studio][keys]") {
    // At octave 8 the upper row reaches B-9 at U, and the keys after it would pass it.
    CHECK(irama_studio::note_for_key(key('U'), 8) == 119);
    for (const char name : std::string_view{"I9O0P"}) {
        INFO("key " << name);
        CHECK(irama_studio::note_for_key(key(name), 8) == std::nullopt);
    }
    // The lower row's last keys still fit.
    CHECK(irama_studio::note_for_key(key('/'), 8) == 112);
}

TEST_CASE("keys off the piano rows play nothing", "[studio][keys]") {
    const std::array others{
        letter('A'), letter('F'),      letter('K'),       digit('1'), digit('4'), digit('8'),
        key_space,   key_left_bracket, key_keypad_divide, 0,          -1,         1'000,
    };
    for (const int other : others) {
        INFO("usage ID " << other);
        CHECK(irama_studio::note_for_key(other, 4) == std::nullopt);
    }
}

TEST_CASE("the note keys type the characters a US keyboard prints on them", "[studio][keys]") {
    constexpr std::u32string_view labels =
        U"ZSXDCVGBHNJM,L.;/Q2W3ER5T6Y7UI9O0Pzsxdcvgbhnjmlqwertyuiop";
    CHECK(std::ranges::all_of(labels, irama_studio::is_note_key_character));
    constexpr std::u32string_view others = U"aF14[-= \u00E9";
    CHECK(std::ranges::none_of(others, irama_studio::is_note_key_character));
}
