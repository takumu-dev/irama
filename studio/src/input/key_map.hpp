// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

namespace irama_studio {

/// The range of the keyboard octave, the octave of the lower piano row's first key.
inline constexpr int min_octave = 0;
inline constexpr int max_octave = 8;
/// The keyboard octave Studio starts with, at which Z plays C-4.
inline constexpr int default_octave = 4;

/// Keeps a keyboard octave within its range.
[[nodiscard]] int clamp_octave(int octave);

/// The note a key plays on the tracker's two piano rows at the keyboard octave `octave`, numbered
/// as the note column numbers notes, from C-0 at 0 to B-9 at 119.
///
/// Keys are named by their USB HID usage ID on the keyboard page, which is what SDL's scancodes
/// are, so a key is its position on the keyboard whatever the layout. On a US keyboard, the lower
/// row `Z S X D C V G B H N J M` plays C to B of the octave, and `, L . ; /` goes on from C to E of
/// the next. The upper row `Q 2 W 3 E R 5 T 6 Y 7 U` plays C to B of the octave above, and
/// `I 9 O 0 P` C to E of the one above that.
///
/// \return The note, or none for a key off the piano rows or one whose note would pass B-9.
[[nodiscard]] std::optional<int> note_for_key(int key, int octave);

/// What a key does to the keyboard octave.
enum class OctaveKey : std::uint8_t {
    /// Nothing.
    none,
    /// Lowers it by one: numpad `/`.
    lower,
    /// Raises it by one: numpad `*`.
    raise,
};

/// What the key with the USB HID usage ID `key` does to the keyboard octave.
[[nodiscard]] OctaveKey octave_key(int key);

} // namespace irama_studio
