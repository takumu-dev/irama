// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The tuning that gives each note its frequency.

#pragma once

namespace irama {

/// The lowest note, C-0, as the note column numbers notes.
inline constexpr int lowest_note = 0;

/// The highest note, B-9, as the note column numbers notes.
inline constexpr int highest_note = 119;

/// Returns the frequency of a note in twelve-tone equal temperament, with A-4 at 440 Hz.
///
/// Notes are numbered as the note column numbers them, from C-0 at 0 to B-9 at 119, so C-4 is 48
/// and A-4 is 57. The frequency is `440 · 2^((note − 57) / 12)`.
///
/// \param note The note, from `lowest_note` to `highest_note`, which is checked.
/// \return The note's frequency in hertz.
[[nodiscard]] float note_frequency(int note);

/// Returns the ratio by which a number of semitones raises a frequency, `2^(semitones / 12)`. A
/// note tuned by a coarse offset in semitones and a fine one in cents plays at its frequency times
/// the ratio of `coarse + fine / 100`.
///
/// \param semitones How far to tune, in semitones, which may be fractional or below 0.
/// \return The ratio, above 0.
[[nodiscard]] float tuning_ratio(float semitones);

} // namespace irama
