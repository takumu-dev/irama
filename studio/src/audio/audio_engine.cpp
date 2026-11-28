// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/audio_engine.hpp"

#include "audio/audio_device.hpp"
#include "audio/create_player.hpp"
#include "audio/device_choice.hpp"
#include "audio/heartbeat_watch.hpp"
#include "audio/message_backlog.hpp"
#include "input/note_message.hpp"
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace irama_studio {
namespace {

// The rates the player runs at. A device at any other rate is opened again at the fallback.
constexpr std::array<std::uint32_t, 4> player_rates{44'100, 48'000, 88'200, 96'000};
constexpr std::uint32_t fallback_rate = 48'000;

// Messages from the note keys and the Instrument window apply as soon as the player can: frame 0
// has always passed.
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

bool AudioEngine::open(std::string_view device, std::uint32_t buffer_size) {
    close();
    device_setting_ = device;
    buffer_size_ = buffer_size;
    // The system default needs no list.
    const std::vector<std::string> listed =
        device.empty() ? std::vector<std::string>{} : device_.device_names();
    return open_choice(choose_device(device, listed));
}

bool AudioEngine::reopen() { return open(device_setting_, buffer_size_); }

bool AudioEngine::watch(HeartbeatWatch::Clock::time_point now) {
    if (!is_open() || !heartbeat_watch_.lost(now, heartbeat())) {
        return false;
    }
    const std::string lost = opened_device_;
    close();
    const std::string_view lost_name = lost.empty() ? system_default_name : std::string_view{lost};
    const std::optional<DeviceChoice> fallback = fall_back_after_loss(lost);
    if (!fallback) {
        note_.clear();
        error_ = std::format("{} lost", lost_name);
    } else if (!open_choice(*fallback)) {
        note_.clear();
        error_ = std::format("{} lost. {}", lost_name, error_);
    }
    return true;
}

bool AudioEngine::open_choice(const DeviceChoice& choice) {
    close();
    error_.clear();
    note_ = choice.note;
    opened_device_ = choice.name;
    heartbeat_watch_.reset();
    const auto callback = [this](std::span<float> out) { render(out); };
    AudioRequest request{
        .device_name = choice.name,
        .sample_rate = 0,
        .callback_frames = buffer_size_,
    };
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
    auto player =
        create_player({.sample_rate = sample_rate, .max_voices = player_voices}, parameters_);
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

void AudioEngine::send(const NoteMessage& message) { send_message(message); }

void AudioEngine::set_parameter(irama::ParamId id, float value) {
    parameters_.set(id, value);
    if (player_) {
        send_message(instrument_change(parameters_));
    }
}

void AudioEngine::flush() {
    if (player_) {
        backlog_.flush([this](PlayerMessage& waiting) { return deliver(waiting); });
        while (player_->take_returned() != nullptr) {
        }
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

void AudioEngine::send_message(PlayerMessage message) {
    if (player_) {
        backlog_.send(std::move(message),
                      [this](PlayerMessage& waiting) { return deliver(waiting); });
    }
}

bool AudioEngine::deliver(PlayerMessage& message) {
    if (auto* changes = std::get_if<irama::ChangeSetPtr>(&message)) {
        return player_->change(now_frame, *changes);
    }
    const auto& note = std::get<NoteMessage>(message);
    switch (note.kind) {
    case NoteMessage::Kind::note_on:
        return player_->note_on(now_frame, note.note, note.velocity, note.instrument);
    case NoteMessage::Kind::note_off:
        return player_->note_off(now_frame, note.note, note.instrument);
    case NoteMessage::Kind::release_all:
        return player_->release_all(now_frame);
    }
    return true;
}

} // namespace irama_studio
