// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/note_keys.hpp"

#include "input/key_map.hpp"
#include "input/note_message.hpp"
#include <irama/engine/base/narrow.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace irama_studio {
namespace {

[[nodiscard]] std::optional<std::size_t> key_index(int key, std::size_t count) {
    if (key < 0 || irama::narrow<std::size_t>(key) >= count) {
        return std::nullopt;
    }
    return irama::narrow<std::size_t>(key);
}

} // namespace

NoteKeys::NoteKeys(int octave) : octave_(clamp_octave(octave)) {}

void NoteKeys::set_octave(int octave) { octave_ = clamp_octave(octave); }

std::optional<NoteMessage> NoteKeys::press(int key, bool repeat) {
    const std::optional index = key_index(key, key_count);
    if (repeat || !index || held_notes_.at(*index)) {
        return std::nullopt;
    }
    const std::optional note = note_for_key(key, octave_);
    if (!note) {
        return std::nullopt;
    }
    held_notes_.at(*index) = irama::narrow<std::uint8_t>(*note);
    ++holders_.at(irama::narrow<std::size_t>(*note));
    return NoteMessage{.kind = NoteMessage::Kind::note_on, .note = *note, .velocity = key_velocity};
}

std::optional<NoteMessage> NoteKeys::release(int key) {
    const std::optional index = key_index(key, key_count);
    if (!index) {
        return std::nullopt;
    }
    std::optional<std::uint8_t>& held_note = held_notes_.at(*index);
    if (!held_note) {
        return std::nullopt;
    }
    const std::uint8_t note = *held_note;
    held_note.reset();
    std::uint8_t& holders = holders_.at(note);
    --holders;
    if (holders > 0) {
        return std::nullopt;
    }
    return NoteMessage{.kind = NoteMessage::Kind::note_off, .note = note};
}

NoteMessage NoteKeys::release_all() {
    forget();
    return NoteMessage{.kind = NoteMessage::Kind::release_all};
}

void NoteKeys::forget() {
    held_notes_.fill(std::nullopt);
    holders_.fill(0);
}

} // namespace irama_studio
