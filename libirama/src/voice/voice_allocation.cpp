// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/voice_allocation.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/voice/voice.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

namespace {

// Whether a note-on would rather take `left` than `right`, the stealing order: a free voice, then
// the oldest in its release, then the quietest, then the oldest.
[[nodiscard]] bool takes_before(const irama::Voice& left, const irama::Voice& right) {
    if (left.active() != right.active()) {
        return !left.active();
    }
    if (!left.active()) {
        return false;
    }
    if (left.releasing() != right.releasing()) {
        return left.releasing();
    }
    if (!left.releasing() && left.level() != right.level()) {
        return left.level() < right.level();
    }
    return left.order() < right.order();
}

// How near a fade slot is to free: 0 when it is free, or the frames left of its fade.
[[nodiscard]] std::uint32_t frames_until_free(const irama::Voice& slot) {
    return slot.active() ? slot.fade_frames_left() : 0;
}

} // namespace

namespace irama {

std::size_t choose_voice(std::span<const Voice> voices) {
    IRAMA_ASSERT(!voices.empty(), "a note-on chooses from at least one voice");
    // min_element keeps the first of equals, so of free voices the first.
    return static_cast<std::size_t>(
        std::distance(voices.begin(), std::ranges::min_element(voices, takes_before)));
}

std::size_t choose_fade_slot(std::span<const Voice> slots) {
    IRAMA_ASSERT(!slots.empty(), "a steal chooses from at least one fade slot");
    return static_cast<std::size_t>(
        std::distance(slots.begin(), std::ranges::min_element(slots, {}, frames_until_free)));
}

} // namespace irama
