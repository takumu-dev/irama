// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace {

// The player renders in blocks of at most this many frames, counted from its first frame, and
// splits a block further at each message's frame. Nothing it writes depends on the size, so it
// can be tuned for speed alone.
constexpr std::size_t block_frames = 256;
static_assert(block_frames <= irama::Instrument::max_block_frames);

constexpr std::size_t channels = 2;

} // namespace

namespace irama {

std::expected<PlayerPtr, CreateError> Player::create(const PlayerConfig& config) {
    IRAMA_ASSERT(config.sample_rate > 0, "a sample rate is above 0 Hz");
    IRAMA_ASSERT(config.max_voices > 0 && config.max_voices <= max_voices_limit,
                 "a player has from 1 to 256 voices");
    auto player = make_owned<Player>(config.allocator, Key{}, config.sample_rate);
    if (!player) {
        return std::unexpected(CreateError::out_of_memory);
    }
    auto voices = OwnedArray<Voice>::create(config.allocator, config.max_voices);
    if (!voices) {
        return std::unexpected(CreateError::out_of_memory);
    }
    // Each voice works in one block of the wave and one of its envelope's levels.
    auto scratch = OwnedArray<float>::create(config.allocator, block_frames * channels);
    if (!scratch) {
        return std::unexpected(CreateError::out_of_memory);
    }
    (*player)->voices_ = std::move(*voices);
    (*player)->scratch_ = std::move(*scratch);
    return std::move(*player);
}

Player::Player(Key /*key*/, std::uint32_t sample_rate)
    : instrument_{static_cast<float>(sample_rate)} {}

bool Player::note_on(std::uint64_t frame, int note, float velocity) {
    IRAMA_ASSERT(note >= lowest_note && note <= highest_note, "a note is between C-0 and B-9");
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::note_on,
        .note = narrow<std::uint8_t>(note),
        .velocity = velocity,
    });
}

bool Player::note_off(std::uint64_t frame, int note) {
    IRAMA_ASSERT(note >= lowest_note && note <= highest_note, "a note is between C-0 and B-9");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::note_off,
        .note = narrow<std::uint8_t>(note),
    });
}

bool Player::release_all(std::uint64_t frame) {
    return push(Message{.frame = frame, .kind = Message::Kind::release_all});
}

bool Player::set_parameter(std::uint64_t frame, ParamId id, float value) {
    IRAMA_ASSERT(!std::isnan(value), "a parameter's value is a number");
    const auto index = find_param(instrument_params, id);
    IRAMA_ASSERT(index.has_value(), "a parameter's ID is one of the instrument's");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::set_parameter,
        .param = narrow<std::uint16_t>(*index),
        .value = instrument_params.at(*index).constrain(value),
    });
}

void Player::render(std::span<float> out) IRAMA_NONBLOCKING {
    const RenderScope scope;
    IRAMA_ASSERT(out.size() % channels == 0, "stereo frames have two samples each");
    std::size_t popped = 0;
    while (!out.empty()) {
        apply_due_messages(popped);
        std::uint64_t frames =
            std::min<std::uint64_t>(out.size() / channels, block_frames - (frame_ % block_frames));
        if (waiting_ && waiting_->frame > frame_) {
            frames = std::min(frames, waiting_->frame - frame_);
        }
        const std::size_t length = narrow<std::size_t>(frames) * channels;
        render_block(out.first(length));
        out = out.subspan(length);
        frame_ += frames;
    }
    const auto active = std::ranges::count_if(voices_, &Voice::active);
    active_voices_.store(narrow<std::uint32_t>(active), std::memory_order_relaxed);
}

std::uint32_t Player::active_voices() const {
    return active_voices_.load(std::memory_order_relaxed);
}

bool Player::push(const Message& message) {
    IRAMA_DEBUG_ASSERT(message.frame >= last_queued_frame_,
                       "control messages are sent in the order of their frames");
    if (!queue_.try_push(message)) {
        return false;
    }
    last_queued_frame_ = message.frame;
    return true;
}

void Player::apply_due_messages(std::size_t& popped) IRAMA_NONBLOCKING {
    while (true) {
        if (!waiting_) {
            // A queue refilled as fast as it is emptied mustn't hold up the render call.
            if (popped == queue_capacity) {
                return;
            }
            waiting_ = queue_.try_pop();
            if (!waiting_) {
                return;
            }
            ++popped;
        }
        if (waiting_->frame > frame_) {
            return;
        }
        apply(*waiting_);
        waiting_.reset();
    }
}

void Player::apply(const Message& message) IRAMA_NONBLOCKING {
    switch (message.kind) {
    case Message::Kind::note_on: {
        // With no voice free, the note is ignored.
        auto* const free = std::ranges::find_if_not(voices_, &Voice::active);
        if (free != voices_.end()) {
            free->start(message.note, message.velocity, instrument_);
        }
        break;
    }
    case Message::Kind::note_off:
        for (Voice& voice : voices_) {
            if (voice.held() && voice.note() == message.note) {
                voice.release(instrument_);
            }
        }
        break;
    case Message::Kind::release_all:
        for (Voice& voice : voices_) {
            voice.release(instrument_);
        }
        break;
    case Message::Kind::set_parameter:
        // While no voice sounds, nothing would hear a glide, so the value jumps.
        instrument_.set(message.param, message.value, std::ranges::any_of(voices_, &Voice::active));
        break;
    }
}

void Player::render_block(std::span<float> out) IRAMA_NONBLOCKING {
    std::ranges::fill(out, 0.0F);
    const InstrumentBlock block = instrument_.render(out.size() / channels);
    for (Voice& voice : voices_) {
        voice.render(out, scratch_.span(), block);
    }
}

} // namespace irama
