// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace irama_studio {

/// What the note keys ask the player to do, to apply as soon as it can.
struct NoteMessage {
    /// Which control message it is.
    enum class Kind : std::uint8_t {
        /// Starts a note.
        note_on,
        /// Releases every held voice playing the note on the instrument.
        note_off,
        /// Releases every held voice.
        release_all,
    };

    Kind kind = Kind::note_on;
    /// The note of a note-on or a note-off, from C-0 at 0 to B-9 at 119.
    int note = 0;
    /// The velocity of a note-on, from 0 to 1.
    float velocity = 0.0F;
    /// The instrument number of a note-on or a note-off, from 0x01 to 0xFE.
    int instrument = 0x01;

    friend bool operator==(const NoteMessage&, const NoteMessage&) = default;
};

} // namespace irama_studio
