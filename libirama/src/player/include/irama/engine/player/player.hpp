// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The player, the object that a host creates, sends control messages to and renders.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/spsc_queue.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace irama {

/// How a player is made. Each field is fixed for the player's life.
struct PlayerConfig {
    /// The sample rate in hertz, above 0.
    std::uint32_t sample_rate = 48'000;
    /// How many voices can sound at once, from 1 to `Player::max_voices_limit`.
    std::size_t max_voices = 32;
    /// The seed of the player's random numbers. The player keeps one `Pcg32`, of this seed on
    /// stream 0, and each note-on splits the new voice's noise generator from it, so voices' noise
    /// is uncorrelated, and the output depends only on the seed and the control messages.
    std::uint64_t seed = 0x6972'616d'6131;
    /// The allocator the player draws all its memory from, and frees it to.
    Allocator allocator = default_allocator();
};

/// The error of a player that couldn't be created.
enum class CreateError : std::uint8_t {
    /// The allocator had no memory. Whatever the player had allocated has been freed.
    out_of_memory,
};

namespace detail {

/// A control message on its way from a player's control functions to its render call.
struct ControlMessage {
    /// What a control message does.
    enum class Kind : std::uint8_t {
        /// Starts a note.
        note_on,
        /// Releases the held voices playing a note.
        note_off,
        /// Releases every held voice.
        release_all,
        /// Sets one of the instrument's parameters.
        set_parameter,
    };

    /// The frame at which the message applies.
    std::uint64_t frame = 0;
    /// What it does.
    Kind kind = Kind::note_on;
    /// The note of a note-on or a note-off.
    std::uint8_t note = 0;
    /// The index of a set-parameter's parameter in the instrument's table.
    std::uint16_t param = 0;
    /// The velocity of a note-on.
    float velocity = 0.0F;
    /// The plain value of a set-parameter, already constrained to its parameter.
    float value = 0.0F;
};

} // namespace detail

class Player;

/// The single owner of a player, which frees it through the allocator it was created with.
using PlayerPtr = OwnedPtr<Player>;

/// The object a host renders: it owns the instrument and its voices, and plays the control messages
/// it is sent.
///
/// A player is created and destroyed on a control thread, and rendered on the audio thread, never
/// both at once. Its control functions, `note_on()`, `note_off()`, `release_all()` and
/// `set_parameter()`, are called from one control thread, and queue a message for `render()` to
/// apply on the audio thread, at the frame the message names. Frames are counted from the first
/// frame the player renders, and each message applies exactly at its frame, so the output never
/// depends on how the host splits its render calls. A frame already rendered applies at the first
/// frame of the next render call, so a message meant to apply at once can name frame 0.
///
/// All its memory is allocated when it is created, and it never moves, so the pointer the audio
/// thread holds stays valid.
class Player {
    // Lets create() alone construct a player, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// The most voices a player can have.
    static constexpr std::size_t max_voices_limit = 256;

    /// How many control messages can wait in the queue, and the most that one render call applies.
    static constexpr std::size_t queue_capacity = 1'024;

    /// Creates a player, allocating everything it will need from the config's allocator.
    ///
    /// Call it on a control thread. A sample rate of 0, or a voice count of 0 or above
    /// `max_voices_limit`, breaks a precondition, which is checked.
    ///
    /// \param config The sample rate, the voice count, the seed and the allocator.
    /// \return The player, or `CreateError::out_of_memory` when the allocator has no memory, in
    ///         which case everything allocated has been freed.
    [[nodiscard]] static std::expected<PlayerPtr, CreateError> create(const PlayerConfig& config);

    /// Makes an empty player, with every parameter at its default, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param sample_rate The sample rate in hertz.
    /// \param seed The seed of its random numbers.
    Player(Key key, std::uint32_t sample_rate, std::uint64_t seed);

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;
    ~Player() = default;

    /// Queues a note-on: a free voice starts playing the note at `frame`. With no voice free, the
    /// note is ignored.
    ///
    /// Call it on the control thread, with frames that never go backwards from one control
    /// message to the next, which is checked where `NDEBUG` isn't defined.
    ///
    /// \param frame The frame at which the note starts.
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param velocity How hard the note is played, from 0 to 1, which is checked.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool note_on(std::uint64_t frame, int note, float velocity);

    /// Queues a note-off: every held voice playing the note starts its release at `frame`.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the voices are released.
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool note_off(std::uint64_t frame, int note);

    /// Queues a release-all: every held voice starts its release at `frame`.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the voices are released.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool release_all(std::uint64_t frame);

    /// Queues a set-parameter: one of the instrument's parameters takes a new value at `frame`.
    ///
    /// The value is constrained to the parameter here, on the control thread: clamped to its
    /// range, and rounded to the nearest whole number if it is stepped. At its frame, a parameter
    /// with a smoothing time glides to it over exactly that time if a voice is sounding, and jumps
    /// to it if none is, so values given to a new player at frame 0 apply as if it had been created
    /// with them. A parameter with no smoothing time switches at its frame. Call it on the control
    /// thread, as `note_on()`.
    ///
    /// \param frame The frame at which the parameter takes the value.
    /// \param id The ID of one of the parameters in `instrument_params`, which is checked.
    /// \param value The plain value, any number but NaN, which is checked.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool set_parameter(std::uint64_t frame, ParamId id, float value);

    /// Renders the next frames, applying each queued message at its frame.
    ///
    /// Call it on the audio thread. It keeps the real-time rules, and is the render entry point
    /// that holds a `RenderScope`. It takes any number of frames, none included, and writes what
    /// the voices sum to, unclipped.
    ///
    /// \param out Where to write the frames, interleaved stereo, two samples for each, left then
    ///            right, overwriting what it holds. Its length is even, which is checked.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

    /// \return How many voices were sounding at the end of the latest render call. It may be
    ///         read on any thread.
    [[nodiscard]] std::uint32_t active_voices() const;

  private:
    using Message = detail::ControlMessage;

    // Queues `message` on the control thread, if the queue has room.
    [[nodiscard]] bool push(const Message& message);

    // Applies the messages due at the current frame, popping no more than the render call has left
    // of its limit.
    void apply_due_messages(std::size_t& popped) IRAMA_NONBLOCKING;

    void apply(const Message& message) IRAMA_NONBLOCKING;

    // Renders frames that no message falls inside, at most one block of them.
    void render_block(std::span<float> out) IRAMA_NONBLOCKING;

    // The live value of each of the instrument's parameters, which only the audio thread touches.
    Instrument instrument_;
    OwnedArray<Voice> voices_;
    // The generator each note-on splits its voice's noise generator from.
    Pcg32 generator_;
    // Each voice's working, for one block.
    OwnedArray<float> scratch_;
    SpscQueue<Message, queue_capacity> queue_;
    // The frame of the latest message queued, which only the control thread reads.
    std::uint64_t last_queued_frame_ = 0;
    // The message popped whose frame hasn't come yet, since the queue can't be read without
    // popping.
    std::optional<Message> waiting_;
    // How many frames the player has rendered, which only the audio thread reads.
    std::uint64_t frame_ = 0;
    std::atomic<std::uint32_t> active_voices_ = 0;
};

} // namespace irama
