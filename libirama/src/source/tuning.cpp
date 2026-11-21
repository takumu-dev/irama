// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/tuning.hpp>

#include <irama/engine/base/assertion.hpp>

#include <cmath>

namespace {

// A-4, the note the tuning is pitched from, and its frequency in hertz.
constexpr int reference_note = 57;
constexpr double reference_frequency = 440.0;

constexpr double notes_per_octave = 12.0;
constexpr float semitones_per_octave = 12.0F;

} // namespace

namespace irama {

float note_frequency(int note) {
    IRAMA_ASSERT(note >= lowest_note && note <= highest_note, "a note is between C-0 and B-9");
    return static_cast<float>(reference_frequency *
                              std::exp2((note - reference_note) / notes_per_octave));
}

float tuning_ratio(float semitones) { return std::exp2(semitones / semitones_per_octave); }

} // namespace irama
