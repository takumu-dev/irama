// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/audio_device.hpp"
#include "audio/instrument_values.hpp"
#include "audio/message_backlog.hpp"
#include "input/note_message.hpp"
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace irama_studio {

/// The backends Studio plays through, in the order it tries them.
inline constexpr std::array studio_backends{AudioBackend::pulseaudio, AudioBackend::alsa};

/// The number of frames in each audio callback.
inline constexpr std::uint32_t default_callback_frames = 256;

/// The number of voices Studio's player has.
inline constexpr std::size_t player_voices = 32;

/// Studio's sound: an audio device, the player it renders while it is open, and Studio's copy of
/// the instrument's parameter values.
///
/// The engine holds a player only while a device is open, at the device's rate. Each new player
/// starts with the whole copy, which outlives every player. The callback
/// renders the player, clips its output to [−1, 1], and reports its CPU load, whether it clipped,
/// and a heartbeat, all through atomics that the UI thread reads. Every function is called on the
/// UI thread.
class AudioEngine {
  public:
    /// Makes an engine with no device open, on the first of `backends` that works.
    explicit AudioEngine(std::span<const AudioBackend> backends);

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    /// Closes the device, if one is open.
    ~AudioEngine();

    /// Opens the system default device at its own rate where the player can run at it, and
    /// otherwise at 48 kHz, which the backend converts. It closes any device open before, and gives
    /// the new one a new player.
    ///
    /// \param callback_frames The number of frames in each callback.
    /// \return Whether the device opened. If it didn't, the engine is silent, and `last_error()`
    ///         says why.
    [[nodiscard]] bool open(std::uint32_t callback_frames = default_callback_frames);

    /// Closes the device and drops its player, with any messages still waiting for it.
    void close();

    /// \return Whether a device is open, and so a player plays.
    [[nodiscard]] bool is_open() const;

    /// Sends a message to the player, after any that its queue has refused before. With no device
    /// open, it does nothing.
    void send(const NoteMessage& message);

    /// Sets one of the instrument's parameters in Studio's copy, and sends it to the player at
    /// once. If the player's queue refuses it, the copy keeps it and `flush()` sends it again. With
    /// no device open, only the copy changes, and the next player starts with it.
    ///
    /// \param id The ID of one of the parameters in `irama::instrument_params`, which is checked.
    /// \param value The new plain value, any number but NaN, constrained to the parameter.
    void set_parameter(irama::ParamId id, float value);

    /// \return Studio's copy of the instrument's parameter values.
    [[nodiscard]] const InstrumentValues& parameters() const { return parameters_; }

    /// Sends the messages and parameter values the player's queue has refused, as far as it now has
    /// room. Call it once a frame.
    void flush();

    /// \return How many voices were sounding at the end of the latest callback.
    [[nodiscard]] std::uint32_t active_voices() const;

    /// \return The callback's longest run since the last call, as a share of its period: the frames
    ///         it rendered over the rate.
    [[nodiscard]] float take_cpu_peak();

    /// \return Whether the callback has clipped a sample since the last call.
    [[nodiscard]] bool take_clipped();

    /// \return The largest magnitude of a sample the callback has played since the last call.
    [[nodiscard]] float take_output_peak();

    /// \return How many times the callback has run. It moves while a device plays.
    [[nodiscard]] std::uint64_t heartbeat() const;

    /// \return The device the engine plays through.
    [[nodiscard]] const AudioDevice& device() const { return device_; }

    /// \return Why the latest open failed, or nothing if it didn't.
    [[nodiscard]] const std::string& last_error() const;

  private:
    // The body of the audio callback.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

    // The rate the open device runs at, or 0 with none open.
    [[nodiscard]] std::uint32_t granted_rate() const;

    // Puts a message in the player's queue, and returns whether the queue took it.
    [[nodiscard]] bool deliver(const NoteMessage& message);

    // Sends the parameter values waiting in the copy, as far as the player's queue has room.
    void send_parameters();

    AudioDevice device_;
    // The callback reads these two only while the device runs, and they change only while it
    // doesn't.
    irama::PlayerPtr player_;
    float sample_rate_ = 0.0F;
    MessageBacklog backlog_;
    InstrumentValues parameters_{irama::instrument_params};
    std::string error_;
    std::atomic<float> cpu_peak_ = 0.0F;
    std::atomic<float> output_peak_ = 0.0F;
    std::atomic<bool> clipped_ = false;
    std::atomic<std::uint64_t> heartbeat_ = 0;
};

} // namespace irama_studio
