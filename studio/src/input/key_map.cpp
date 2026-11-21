// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/key_map.hpp"

#include <algorithm>
#include <array>
#include <optional>

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
constexpr int usage_keypad_divide = 0x54;
constexpr int usage_keypad_multiply = 0x55;

[[nodiscard]] constexpr int letter(char name) { return usage_a + (name - 'A'); }

[[nodiscard]] constexpr int digit(char name) {
    return name == '0' ? usage_0 : usage_1 + (name - '1');
}

// A piano key: the key's usage ID, and its note above the keyboard octave's C.
struct PianoKey {
    int key;
    int semitones;
};

constexpr auto piano_keys = std::to_array<PianoKey>({
    // The lower row, from the keyboard octave's C.
    {.key = letter('Z'), .semitones = 0},
    {.key = letter('S'), .semitones = 1},
    {.key = letter('X'), .semitones = 2},
    {.key = letter('D'), .semitones = 3},
    {.key = letter('C'), .semitones = 4},
    {.key = letter('V'), .semitones = 5},
    {.key = letter('G'), .semitones = 6},
    {.key = letter('B'), .semitones = 7},
    {.key = letter('H'), .semitones = 8},
    {.key = letter('N'), .semitones = 9},
    {.key = letter('J'), .semitones = 10},
    {.key = letter('M'), .semitones = 11},
    {.key = usage_comma, .semitones = 12},
    {.key = letter('L'), .semitones = 13},
    {.key = usage_period, .semitones = 14},
    {.key = usage_semicolon, .semitones = 15},
    {.key = usage_slash, .semitones = 16},
    // The upper row, from the C an octave above.
    {.key = letter('Q'), .semitones = 12},
    {.key = digit('2'), .semitones = 13},
    {.key = letter('W'), .semitones = 14},
    {.key = digit('3'), .semitones = 15},
    {.key = letter('E'), .semitones = 16},
    {.key = letter('R'), .semitones = 17},
    {.key = digit('5'), .semitones = 18},
    {.key = letter('T'), .semitones = 19},
    {.key = digit('6'), .semitones = 20},
    {.key = letter('Y'), .semitones = 21},
    {.key = digit('7'), .semitones = 22},
    {.key = letter('U'), .semitones = 23},
    {.key = letter('I'), .semitones = 24},
    {.key = digit('9'), .semitones = 25},
    {.key = letter('O'), .semitones = 26},
    {.key = digit('0'), .semitones = 27},
    {.key = letter('P'), .semitones = 28},
});

} // namespace

int clamp_octave(int octave) { return std::clamp(octave, min_octave, max_octave); }

std::optional<int> note_for_key(int key, int octave) {
    for (const PianoKey& piano_key : piano_keys) {
        if (piano_key.key == key) {
            const int note = (clamp_octave(octave) * notes_per_octave) + piano_key.semitones;
            if (note > highest_note) {
                return std::nullopt;
            }
            return note;
        }
    }
    return std::nullopt;
}

OctaveKey octave_key(int key) {
    switch (key) {
    case usage_keypad_divide:
        return OctaveKey::lower;
    case usage_keypad_multiply:
        return OctaveKey::raise;
    default:
        return OctaveKey::none;
    }
}

} // namespace irama_studio
