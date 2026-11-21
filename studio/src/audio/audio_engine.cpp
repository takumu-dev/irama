// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/audio_engine.hpp"

#include "audio/audio_device.hpp"
#include "input/note_message.hpp"
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/player/player.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace irama_studio {
namespace {

// The rates the player runs at. A device at any other rate is opened again at the fallback.
constexpr std::array<std::uint32_t, 4> player_rates{44'100, 48'000, 88'200, 96'000};
constexpr std::uint32_t fallback_rate = 48'000;

// Messages from the note keys apply as soon as the player can: frame 0 has always passed.
constexpr std::uint64_t now_frame = 0;

// Raises a peak shared with another thread to `value`, if `value` is the higher.
void raise(std::atomic<float>& peak, float value) IRAMA_NONBLOCKING {
    float current = peak.load(std::memory_order_relaxed);
    while (value > current &&
           !peak.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

} // namespace

AudioEngine::AudioEngine(std::span<const AudioBackend> backends) : device_(backends) {}

AudioEngine::~AudioEngine() { close(); }

bool AudioEngine::open(std::uint32_t callback_frames) {
    close();
    error_.clear();
    const auto callback = [this](std::span<float> out) { render(out); };
    AudioRequest request{.sample_rate = 0, .callback_frames = callback_frames};
    if (!device_.open(request, callback)) {
        error_ = device_.last_error();
        return false;
    }
    if (!std::ranges::contains(player_rates, granted_rate())) {
        request.sample_rate = fallback_rate;
        if (!device_.open(request, callback)) {
            error_ = device_.last_error();
            return false;
        }
    }
    const std::uint32_t sample_rate = granted_rate();
    auto player = irama::Player::create({.sample_rate = sample_rate, .max_voices = player_voices});
    if (!player) {
        device_.close();
        error_ = "Not enough memory for the player";
        return false;
    }
    player_ = std::move(*player);
    sample_rate_ = static_cast<float>(sample_rate);
    if (!device_.start()) {
        player_.reset();
        error_ = device_.last_error();
        return false;
    }
    return true;
}

void AudioEngine::close() {
    device_.close();
    player_.reset();
    backlog_.clear();
}

bool AudioEngine::is_open() const { return device_.is_open(); }

void AudioEngine::send(const NoteMessage& message) {
    if (player_) {
        backlog_.send(message, [this](const NoteMessage& waiting) { return deliver(waiting); });
    }
}

void AudioEngine::flush() {
    if (player_) {
        backlog_.flush([this](const NoteMessage& waiting) { return deliver(waiting); });
    }
}

std::uint32_t AudioEngine::active_voices() const { return player_ ? player_->active_voices() : 0; }

float AudioEngine::take_cpu_peak() { return cpu_peak_.exchange(0.0F, std::memory_order_relaxed); }

bool AudioEngine::take_clipped() { return clipped_.exchange(false, std::memory_order_relaxed); }

float AudioEngine::take_output_peak() {
    return output_peak_.exchange(0.0F, std::memory_order_relaxed);
}

std::uint64_t AudioEngine::heartbeat() const { return heartbeat_.load(std::memory_order_relaxed); }

const std::string& AudioEngine::last_error() const { return error_; }

std::uint32_t AudioEngine::granted_rate() const {
    const std::optional<AudioGranted>& granted = device_.granted();
    return granted ? granted->sample_rate : 0;
}

void AudioEngine::render(std::span<float> out) IRAMA_NONBLOCKING {
    // Studio's own work in the callback is inside the scope too, so the real-time checks watch it.
    const irama::RenderScope scope;
    const auto start = std::chrono::steady_clock::now();

    player_->render(out);
    float peak = 0.0F;
    bool clipped = false;
    for (float& sample : out) {
        if (std::abs(sample) > 1.0F) {
            sample = std::copysign(1.0F, sample);
            clipped = true;
        }
        peak = std::max(peak, std::abs(sample));
    }
    if (clipped) {
        clipped_.store(true, std::memory_order_relaxed);
    }
    raise(output_peak_, peak);

    if (!out.empty()) {
        const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - start;
        const float frames = static_cast<float>(out.size()) / 2.0F;
        raise(cpu_peak_, elapsed.count() * sample_rate_ / frames);
    }
    heartbeat_.fetch_add(1, std::memory_order_relaxed);
}

bool AudioEngine::deliver(const NoteMessage& message) {
    switch (message.kind) {
    case NoteMessage::Kind::note_on:
        return player_->note_on(now_frame, message.note, message.velocity);
    case NoteMessage::Kind::note_off:
        return player_->note_off(now_frame, message.note);
    case NoteMessage::Kind::release_all:
        return player_->release_all(now_frame);
    }
    return true;
}

} // namespace irama_studio
