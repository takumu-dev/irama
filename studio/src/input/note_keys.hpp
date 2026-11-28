// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/key_map.hpp"
#include "input/note_message.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace irama_studio {

/// The velocity every note key plays at.
inline constexpr float key_velocity = 1.0F;

/// The computer keyboard as a piano: turns key presses and releases into note messages, and keeps
/// the keyboard octave.
///
/// Keys are named by their USB HID usage ID, as `note_for_key()` reads them. The note a key plays
/// is remembered when it is pressed, so a key released after the octave changes ends the note it
/// started. A note-off releases every voice on its note, so where several held keys play one note,
/// only the release of the last of them sends it.
class NoteKeys {
  public:
    /// Starts with no key held, at the keyboard octave `octave`, kept within its range.
    explicit NoteKeys(int octave = default_octave);

    /// \return The keyboard octave.
    [[nodiscard]] int octave() const { return octave_; }

    /// Sets the keyboard octave, kept within its range. Held keys keep their notes.
    void set_octave(int octave);

    /// Handles a key going down. A piano key that isn't already held sends a note-on. A key repeat
    /// does nothing.
    ///
    /// \param key The key's USB HID usage ID.
    /// \param repeat Whether the press is the key repeating while held.
    /// \return The note-on to send, if any.
    [[nodiscard]] std::optional<NoteMessage> press(int key, bool repeat);

    /// Handles a key going up. A held piano key sends the note-off of the note it started, if it is
    /// the last key holding that note.
    ///
    /// \param key The key's USB HID usage ID.
    /// \return The note-off to send, if any.
    [[nodiscard]] std::optional<NoteMessage> release(int key);

    /// Forgets every held key, as when the window loses the keyboard.
    ///
    /// \return The release-all to send.
    [[nodiscard]] NoteMessage release_all();

    /// Forgets every held key and sends nothing, as when the player is replaced by one that starts
    /// silent.
    void forget();

  private:
    // Usage IDs on the keyboard page fit in a byte.
    static constexpr std::size_t key_count = 256;
    static constexpr std::size_t note_count = 120;

    int octave_;
    // The note each held key started.
    std::array<std::optional<std::uint8_t>, key_count> held_notes_{};
    // How many held keys play each note.
    std::array<std::uint8_t, note_count> holders_{};
};

} // namespace irama_studio
