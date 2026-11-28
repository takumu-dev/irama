// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/audio_device.hpp"
#include "audio/device_choice.hpp"
#include "audio/heartbeat_watch.hpp"
#include "audio/message_backlog.hpp"
#include "audio/sent_song.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "input/note_message.hpp"
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/player/player.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace irama_studio {

/// The backends Studio plays through, in the order it tries them. WASAPI, in shared mode, exists on
/// every Windows that Studio runs on.
#ifdef _WIN32
inline constexpr std::array studio_backends{AudioBackend::wasapi};
#else
inline constexpr std::array studio_backends{AudioBackend::pulseaudio, AudioBackend::alsa};
#endif

/// The number of voices Studio's player has.
inline constexpr std::size_t player_voices = 32;

/// Studio's sound: an audio device, the player it renders while it is open, and the song as last
/// sent, part by part.
///
/// The engine holds a player only while a device is open, at the device's rate. Each new player
/// starts with the whole song, sent as one change set before it renders, and each edit after that
/// sends the parts it changed. The song outlives every player. The callback renders the player,
/// clips its output to [−1, 1], and reports its CPU load, whether it clipped, and a heartbeat, all
/// through atomics that the UI thread reads. Every function is called on the UI thread.
///
/// Every open, whether of another device, at another buffer size, or after a device is lost,
/// closes the device, drops the player, and gives the device it opens a new player. The new player
/// starts silent, so the caller forgets the keys it holds.
class AudioEngine {
  public:
    /// Makes an engine with no device open, on the first of `backends` that works.
    ///
    /// \param backends The backends to try, in order.
    /// \param song The song every player starts with, until edits change it.
    explicit AudioEngine(std::span<const AudioBackend> backends, const Document& song = Document{});

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    /// Closes the device, if one is open.
    ~AudioEngine();

    /// Opens a device at its own rate where the player can run at it, and otherwise at 48 kHz,
    /// which the backend converts. It closes any device open before, and gives the new one a new
    /// player. The device is chosen by `choose_device()`, so a named device that isn't listed opens
    /// the system default in its place, and `note()` says so.
    ///
    /// \param device The `audio_device` setting: a device's name, or empty for the system default.
    ///               It is kept, for `reopen()`, whichever device opens.
    /// \param buffer_size The number of frames in each callback.
    /// \return Whether a device opened. If none did, the engine is silent, and `last_error()` says
    ///         why.
    [[nodiscard]] bool open(std::string_view device = {},
                            std::uint32_t buffer_size = default_buffer_size);

    /// Opens again with the device setting and buffer size of the latest `open()`.
    ///
    /// \return Whether a device opened.
    [[nodiscard]] bool reopen();

    /// Looks at the callback's heartbeat, once a frame. A device whose heartbeat hasn't moved for
    /// a second is lost, and closed. A named device then falls back to the system default, and
    /// `note()` says so. A lost system default, or a fallback that fails, leaves the engine silent,
    /// and `last_error()` says why. Nothing is tried again until the next open.
    ///
    /// \param now The time of the frame.
    /// \return Whether the device was lost, and so the player replaced or dropped.
    [[nodiscard]] bool watch(HeartbeatWatch::Clock::time_point now);

    /// Closes the device and drops its player, with any messages still waiting for it.
    void close();

    /// \return Whether a device is open, and so a player plays.
    [[nodiscard]] bool is_open() const;

    /// Sends a message to the player, after any that its queue has refused before. With no device
    /// open, it does nothing.
    void send(const NoteMessage& message);

    /// Rebuilds the parts an edit, its undo or its redo reported, and sends the player those that
    /// differ from the parts last sent, as one change set, after any message that its queue has
    /// refused before. A change set that would change nothing isn't sent. With no device open,
    /// nothing is sent, and the next player starts with the song as changed.
    ///
    /// \param document The Document after the change.
    /// \param parts What the change reported.
    void send_changes(const Document& document, const ChangedParts& parts);

    /// \return The song as last sent, which every new player starts with.
    [[nodiscard]] const SentSong& sent_song() const { return sent_; }

    /// Sends the messages the player's queue has refused, as far as it now has room, and drops the
    /// change sets the player has handed back, which frees the parts they replaced. Call it once a
    /// frame.
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

    /// \return The names of the devices there are now, as `open()` takes them.
    [[nodiscard]] std::vector<std::string> device_names() const { return device_.device_names(); }

    /// \return The device setting of the latest open: a device's name, or empty for the system
    ///         default.
    [[nodiscard]] const std::string& device_setting() const { return device_setting_; }

    /// \return The buffer size of the latest open, in frames.
    [[nodiscard]] std::uint32_t buffer_size() const { return buffer_size_; }

    /// \return Why the system default opened in place of the device asked for, or nothing.
    [[nodiscard]] const std::string& note() const { return note_; }

    /// \return Why the latest open failed or the device was lost, or nothing if neither.
    [[nodiscard]] const std::string& last_error() const;

  private:
    // The body of the audio callback.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

    // The rate the open device runs at, or 0 with none open.
    [[nodiscard]] std::uint32_t granted_rate() const;

    // Opens the chosen device with a new player.
    [[nodiscard]] bool open_choice(const DeviceChoice& choice);

    // Sends a message after any waiting in the backlog, with a device open.
    void send_message(PlayerMessage message);

    // Puts a message in the player's queue, and returns whether the queue took it.
    [[nodiscard]] bool deliver(PlayerMessage& message);

    AudioDevice device_;
    // The callback reads these two only while the device runs, and they change only while it
    // doesn't.
    irama::PlayerPtr player_;
    float sample_rate_ = 0.0F;
    MessageBacklog backlog_;
    SentSong sent_;
    std::string device_setting_;
    std::uint32_t buffer_size_ = default_buffer_size;
    // The name of the device open, or empty for the system default.
    std::string opened_device_;
    HeartbeatWatch heartbeat_watch_;
    std::string note_;
    std::string error_;
    std::atomic<float> cpu_peak_ = 0.0F;
    std::atomic<float> output_peak_ = 0.0F;
    std::atomic<bool> clipped_ = false;
    std::atomic<std::uint64_t> heartbeat_ = 0;
};

} // namespace irama_studio
