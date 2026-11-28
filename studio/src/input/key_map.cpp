// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/key_map.hpp"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>

namespace irama_studio {
namespace {

// The highest note, B-9.
constexpr int highest_note = 119;
constexpr int notes_per_octave = 12;

// USB HID usage IDs on the keyboard page, from the HID Usage Tables.
constexpr int usage_a = 0x04;
constexpr int usage_1 = 0x1E;
constexpr int usage_0 = 0x27;
constexpr int usage_semicolon = 0x33;
constexpr int usage_comma = 0x36;
constexpr int usage_period = 0x37;
constexpr int usage_slash = 0x38;

// Each row of note keys as a US keyboard labels them, from the lowest note up, a semitone apart.
// The lower row starts on the keyboard octave's C, and the upper row on the C an octave above.
constexpr std::string_view lower_row = "ZSXDCVGBHNJM,L.;/";
constexpr std::string_view upper_row = "Q2W3ER5T6Y7UI9O0P";

// The usage ID of the key a US keyboard labels `label`, among the note keys' labels.
[[nodiscard]] constexpr int usage(char label) {
    if (label >= 'A' && label <= 'Z') {
        return usage_a + (label - 'A');
    }
    if (label == '0') {
        return usage_0;
    }
    if (label >= '1' && label <= '9') {
        return usage_1 + (label - '1');
    }
    switch (label) {
    case ';':
        return usage_semicolon;
    case ',':
        return usage_comma;
    case '.':
        return usage_period;
    default:
        return usage_slash;
    }
}

// The key's place in `row`, if it is on it.
[[nodiscard]] std::optional<int> place_in_row(int key, std::string_view row) {
    for (const auto [place, label] : std::views::enumerate(row)) {
        if (usage(label) == key) {
            return static_cast<int>(place);
        }
    }
    return std::nullopt;
}

} // namespace

int clamp_octave(int octave) { return std::clamp(octave, min_octave, max_octave); }

std::optional<int> note_for_key(int key, int octave) {
    std::optional<int> semitones = place_in_row(key, lower_row);
    if (!semitones) {
        semitones = place_in_row(key, upper_row).transform([](int place) {
            return place + notes_per_octave;
        });
    }
    if (!semitones) {
        return std::nullopt;
    }
    const int note = (clamp_octave(octave) * notes_per_octave) + *semitones;
    if (note > highest_note) {
        return std::nullopt;
    }
    return note;
}

bool is_note_key_character(char32_t character) {
    if (character >= U'a' && character <= U'z') {
        character -= U'a' - U'A';
    }
    const auto is_label = [character](char label) {
        return character == static_cast<unsigned char>(label);
    };
    return std::ranges::any_of(lower_row, is_label) || std::ranges::any_of(upper_row, is_label);
}

} // namespace irama_studio
