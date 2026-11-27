// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace irama_studio {

/// The audio backends a device may open through.
enum class AudioBackend : std::uint8_t {
    pulseaudio,
    alsa,
    /// A device with no hardware behind it, driven by a timer, for tests.
    null,
};

/// The backend's name, as a user would know it.
[[nodiscard]] std::string_view backend_name(AudioBackend backend);

/// What a device is opened with.
struct AudioRequest {
    /// The device's name, as `device_names()` lists it, or empty for the system default.
    std::string device_name{};
    /// The rate to run at, in frames per second, or 0 for the device's own.
    std::uint32_t sample_rate = 0;
    /// The number of frames in each callback.
    std::uint32_t callback_frames = 256;
};

/// What an open device runs with.
struct AudioGranted {
    AudioBackend backend = AudioBackend::null;
    /// In frames per second.
    std::uint32_t sample_rate = 0;
    /// The number of frames in every callback.
    std::uint32_t callback_frames = 0;
    /// The device's name, as the backend gives it.
    std::string device_name{};
};

/// Audio output through miniaudio: one context, opened on the first backend that works, and at
/// most one device on it, which plays interleaved stereo float frames from a callback.
///
/// Every function is called from one thread, the UI thread, while the callback runs on the
/// backend's own thread. It knows nothing of what the callback plays. miniaudio's log never reaches
/// stderr: the device keeps its latest error and drops the rest.
class AudioDevice {
  public:
    /// Fills the frames of one callback, interleaved stereo, two samples for each, left then right.
    /// It runs on the backend's thread, and must keep the real-time rules. miniaudio leaves the
    /// samples it writes unclipped.
    using Callback = std::function<void(std::span<float>)>;

    /// Starts a context on the first of `backends` that works, trying them in order. With none,
    /// no device can open, and `last_error()` says why.
    explicit AudioDevice(std::span<const AudioBackend> backends);

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
    AudioDevice(AudioDevice&&) = delete;
    AudioDevice& operator=(AudioDevice&&) = delete;

    /// Closes the device, if one is open, and ends the context.
    ~AudioDevice();

    /// \return The backend the context runs on, or none if no backend worked.
    [[nodiscard]] std::optional<AudioBackend> backend() const;

    /// \return The names of the playback devices there are now, in the backend's order, as `open()`
    ///         takes them. Each call asks the backend again.
    [[nodiscard]] std::vector<std::string> device_names() const;

    /// Opens the device the request names, or the system default, closing any device open before.
    /// The callback doesn't run until `start()`, so the caller can first prepare for the rate it
    /// was granted.
    ///
    /// \return Whether the device opened. If it didn't, `last_error()` says why.
    [[nodiscard]] bool open(const AudioRequest& request, Callback callback);

    /// Starts the callback of the open device.
    ///
    /// \return Whether it started. If it didn't, the device is closed and `last_error()` says why.
    [[nodiscard]] bool start();

    /// Stops the callback and closes the device, if one is open. Once it returns, the callback
    /// doesn't run again.
    void close();

    /// \return Whether a device is open.
    [[nodiscard]] bool is_open() const;

    /// \return What the open device runs with, or none if no device is open.
    [[nodiscard]] const std::optional<AudioGranted>& granted() const;

    /// \return Why the context, or the latest device to open or start, failed, or nothing if it
    ///         didn't.
    [[nodiscard]] const std::string& last_error() const;

    /// \return The latest error miniaudio has logged, from any thread, or nothing if it hasn't.
    [[nodiscard]] std::string last_log_error() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace irama_studio
