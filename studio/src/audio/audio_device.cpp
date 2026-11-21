// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/audio_device.hpp"

#include <irama/engine/base/narrow.hpp>

#include <miniaudio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace irama_studio {
namespace {

constexpr std::uint32_t channels = 2;

[[nodiscard]] ma_backend to_miniaudio(AudioBackend backend) {
    switch (backend) {
    case AudioBackend::pulseaudio:
        return ma_backend_pulseaudio;
    case AudioBackend::alsa:
        return ma_backend_alsa;
    case AudioBackend::null:
        return ma_backend_null;
    }
    return ma_backend_null;
}

[[nodiscard]] std::optional<AudioBackend> from_miniaudio(ma_backend backend) {
    switch (backend) {
    case ma_backend_pulseaudio:
        return AudioBackend::pulseaudio;
    case ma_backend_alsa:
        return AudioBackend::alsa;
    case ma_backend_null:
        return AudioBackend::null;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::string describe(std::string_view what, ma_result result) {
    return std::format("{}: {}", what, ma_result_description(result));
}

} // namespace

std::string_view backend_name(AudioBackend backend) {
    switch (backend) {
    case AudioBackend::pulseaudio:
        return "PulseAudio";
    case AudioBackend::alsa:
        return "ALSA";
    case AudioBackend::null:
        return "Null";
    }
    return "";
}

// miniaudio's context and device hold pointers into themselves, so they live here, where they
// never move.
struct AudioDevice::State {
    ma_context context{};
    std::optional<AudioBackend> backend;
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization): ma_device_init() fills it in.
    ma_device device{};
    bool device_open = false;
    Callback callback;
    std::optional<AudioGranted> granted;
    std::string last_error;
};

AudioDevice::AudioDevice(std::span<const AudioBackend> backends)
    : state_(std::make_unique<State>()) {
    // miniaudio reads an empty list as its own list of every backend, which would end with Null.
    if (backends.empty()) {
        state_->last_error = "No audio backend to try";
        return;
    }
    std::vector<ma_backend> list;
    list.reserve(backends.size());
    for (const AudioBackend backend : backends) {
        list.push_back(to_miniaudio(backend));
    }
    ma_context_config config = ma_context_config_init();
    // miniaudio lowers this to normal priority itself where the system refuses it.
    config.threadPriority = ma_thread_priority_realtime;
    const ma_result result = ma_context_init(list.data(), irama::narrow<ma_uint32>(list.size()),
                                             &config, &state_->context);
    if (result != MA_SUCCESS) {
        state_->last_error = describe("No audio backend works", result);
        return;
    }
    state_->backend = from_miniaudio(state_->context.backend);
}

AudioDevice::~AudioDevice() {
    close();
    if (state_->backend) {
        ma_context_uninit(&state_->context);
    }
}

std::optional<AudioBackend> AudioDevice::backend() const { return state_->backend; }

bool AudioDevice::open(const AudioRequest& request, Callback callback) {
    close();
    if (!state_->backend) {
        return false;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    // A null device ID is the system default, which follows the user's choice of default.
    config.playback.pDeviceID = nullptr;
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = request.sample_rate;
    // Each callback is given exactly this many frames.
    config.periodSizeInFrames = request.callback_frames;
    config.noFixedSizedCallback = MA_FALSE;
    // The callback clips, and must see each clipped sample.
    config.noClip = MA_TRUE;
    config.pUserData = state_.get();
    config.dataCallback = [](ma_device* device, void* output, const void* /*input*/,
                             ma_uint32 frames) {
        const auto* const state = static_cast<const State*>(device->pUserData);
        state->callback(std::span{static_cast<float*>(output), std::size_t{frames} * channels});
    };
    // The backend's thread reads the callback only once the device has started.
    state_->callback = std::move(callback);
    const ma_result result = ma_device_init(&state_->context, &config, &state_->device);
    if (result != MA_SUCCESS) {
        state_->callback = nullptr;
        state_->last_error = describe("Couldn't open the audio device", result);
        return false;
    }
    state_->device_open = true;
    std::array<char, MA_MAX_DEVICE_NAME_LENGTH + 1> name{};
    if (ma_device_get_name(&state_->device, ma_device_type_playback, name.data(), name.size(),
                           nullptr) != MA_SUCCESS) {
        name.front() = '\0';
    }
    state_->granted = AudioGranted{
        .backend = *state_->backend,
        .sample_rate = state_->device.sampleRate,
        .callback_frames = state_->device.playback.intermediaryBufferCap,
        .device_name = name.data(),
    };
    state_->last_error.clear();
    return true;
}

bool AudioDevice::start() {
    if (!state_->device_open) {
        state_->last_error = "No audio device is open";
        return false;
    }
    const ma_result result = ma_device_start(&state_->device);
    if (result != MA_SUCCESS) {
        close();
        state_->last_error = describe("Couldn't start the audio device", result);
        return false;
    }
    return true;
}

void AudioDevice::close() {
    if (!state_->device_open) {
        return;
    }
    // Uninitialising a device that has lost its server waits forever unless it is stopped first.
    ma_device_stop(&state_->device);
    ma_device_uninit(&state_->device);
    state_->device_open = false;
    state_->granted.reset();
    state_->callback = nullptr;
}

bool AudioDevice::is_open() const { return state_->device_open; }

const std::optional<AudioGranted>& AudioDevice::granted() const { return state_->granted; }

const std::string& AudioDevice::last_error() const { return state_->last_error; }

} // namespace irama_studio
