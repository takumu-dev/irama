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
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/seq/sequencer.hpp>
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
    /// How many voices can sound at once, from 1 to `Player::max_voices_limit`. Stolen voices
    /// fading out don't count.
    std::size_t max_voices = 32;
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
        /// Starts a note on the note keys.
        note_on,
        /// Releases the note keys' held voices playing a note.
        note_off,
        /// Releases every held voice of the note keys.
        release_all,
        /// Applies a change set to the EngineSong.
        change,
        /// Starts playing the song.
        play,
        /// Switches looping of the playing slot.
        set_loop,
        /// Stops playing, or fades out every voice while stopped.
        stop,
    };

    /// The frame at which the message applies.
    std::uint64_t frame = 0;
    /// What it does.
    Kind kind = Kind::note_on;
    /// The note of a note-on or a note-off.
    std::uint8_t note = 0;
    /// The instrument number of a note-on or a note-off.
    std::uint8_t instrument = 0;
    /// The velocity of a note-on.
    float velocity = 0.0F;
    /// The slot a play starts at.
    std::uint16_t slot = 0;
    /// The line a play starts at.
    std::uint16_t line = 0;
    /// Whether a play or a set-loop loops.
    bool loop = false;
    /// The change set of a change, which the message owns while it travels: the player takes it
    /// from its owning pointer when it queues the message, and gives it one again when it hands it
    /// back.
    ChangeSet* changes = nullptr;
};

} // namespace detail

class Player;

/// The single owner of a player, which frees it through the allocator it was created with.
using PlayerPtr = OwnedPtr<Player>;

/// The object a host renders: it owns the EngineSong, the sequencer, the live state of each
/// instrument and the voices, and plays the control messages it is sent.
///
/// A player is created and destroyed on a control thread, and rendered on the audio thread, never
/// both at once. Its control functions, `note_on()`, `note_off()`, `release_all()`, `change()`,
/// `play()`, `set_loop()` and `stop()`, are called from one control thread, and queue a message for
/// `render()` to apply on the audio thread, at the frame the message names. Frames are counted from
/// the first frame the player renders, and each message applies exactly at its frame, so the output
/// never depends on how the host splits its render calls. A frame already rendered applies at the
/// first frame of the next render call, so a message meant to apply at once can name frame 0.
///
/// A new player's EngineSong has the new song's song part and every pattern and instrument entry
/// empty. A host loads a song by sending a change set of every part at frame 0, before the first
/// render. A change set applies whole at its frame: its parts replace their entries together, and
/// the player sends it back, holding the parts it replaced, through a return queue that
/// `take_returned()` empties on the control thread, where dropping it frees them. A change set
/// waits, with every message behind it, while the return queue is full, so the audio thread never
/// frees, drops or leaks a part.
///
/// The player keeps one smoother for each parameter of each instrument number, at its default
/// until the number's first instrument part arrives. When a change set replaces an instrument part,
/// each value that differs from the one its smoother was last set to goes to it: a value with a
/// smoothing time glides while a voice of that instrument sounds, and jumps while none does, and a
/// value with none switches at the frame. An instrument emptied while its voices sound fades them
/// out over 5 ms. The player computes each block's values only for the instruments with a voice
/// sounding, into one set of buffers it reuses for each in turn, instrument 01 first.
///
/// The player owns one `Sequencer`, stopped when the player is created. It renders up to the
/// earliest of the sequencer's next frame, the next message's frame and the block's end, and at a
/// frame applies the messages first, then the sequencer's notes. Each voice remembers its owner:
/// the note keys, for a note-on message, or the note column whose note started it, which owns it
/// while it sounds, released or not. A new note in a column gives each held voice the column owns
/// its own instrument's new note action: note-off releases it, cut fades it out over 5 ms and it
/// leaves the column, and continue leaves it be. A note that plays nothing, with an empty
/// instrument field or number, still applies it. A column's `OFF` releases its held voices. A
/// note-off or release-all message reaches only the note keys' voices, so no message cuts the song.
/// A stop releases the held voices the columns own, and a stop while stopped fades out every voice
/// over 5 ms. After each render call the play position is written as one packed value, which
/// `play_position()` reads on any thread.
///
/// The player keeps one `Pcg32`, seeded from the song part's seed on stream 0 when the player is
/// created, whenever a change set brings a song part with a different seed, and at every play.
/// Each note-on splits the new voice's noise generator from it, so voices' noise is uncorrelated,
/// and the output depends only on the song and the control messages. The note keys draw from the
/// same generator, so a song played from its start sounds the same every time, unless notes are
/// played over it.
///
/// A note-on with every voice sounding steals one, as `choose_voice()` chooses: the oldest in its
/// release, or else the quietest, or else the oldest. The stolen voice fades out over 5 ms in one
/// of `fade_slots` fade slots beside the voices, while its own voice starts the new note on the
/// note-on's frame. With every fade slot busy, the fade nearest its end is cut.
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

    /// How many stolen voices can fade out at once.
    static constexpr std::size_t fade_slots = 8;

    /// How many control messages can wait in the queue, and the most that one render call applies.
    static constexpr std::size_t queue_capacity = 1'024;

    /// How many applied change sets can wait in the return queue to be taken back.
    static constexpr std::size_t return_capacity = 1'024;

    /// Creates a player, allocating everything it will need from the config's allocator.
    ///
    /// Call it on a control thread. A sample rate of 0, or a voice count of 0 or above
    /// `max_voices_limit`, breaks a precondition, which is checked.
    ///
    /// \param config The sample rate, the voice count and the allocator.
    /// \return The player, or `CreateError::out_of_memory` when the allocator has no memory, in
    ///         which case everything allocated has been freed.
    [[nodiscard]] static std::expected<PlayerPtr, CreateError> create(const PlayerConfig& config);

    /// Makes an empty player, with no song part, instruments or voices, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    explicit Player(Key key);

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    /// Frees the EngineSong's parts, and every change set still waiting in either queue.
    ~Player();

    /// Queues a note-on: a free voice starts playing the note on an instrument at `frame`, owned by
    /// the note keys. With no voice free, the note steals one, which fades out. An instrument
    /// number that is empty at the frame plays nothing.
    ///
    /// Call it on the control thread, with frames that never go backwards from one control
    /// message to the next, which is checked where `NDEBUG` isn't defined.
    ///
    /// \param frame The frame at which the note starts.
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param velocity How hard the note is played, from 0 to 1, which is checked.
    /// \param instrument The instrument number, from 0x01 to 0xFE, which is checked.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool note_on(std::uint64_t frame, int note, float velocity, int instrument);

    /// Queues a note-off: every held voice of the note keys playing the note on the instrument
    /// starts its release at `frame`.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the voices are released.
    /// \param note The note, from `lowest_note` to `highest_note`, which is checked.
    /// \param instrument The instrument number, from 0x01 to 0xFE, which is checked.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool note_off(std::uint64_t frame, int note, int instrument);

    /// Queues a release-all: every held voice of the note keys starts its release at `frame`.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the voices are released.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool release_all(std::uint64_t frame);

    /// Queues a change: the change set's parts replace their entries in the EngineSong at `frame`,
    /// together, and the change set comes back, holding the parts it replaced, through
    /// `take_returned()`.
    ///
    /// Each instrument part holds one value for each parameter in `instrument_params`, each
    /// already constrained to its parameter, which is checked. Call it on the control thread, as
    /// `note_on()`.
    ///
    /// \param frame The frame at which the parts are swapped.
    /// \param changes The change set, never null, which is checked. The player takes it if the
    ///                message is queued, and leaves it as it was if not.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool change(std::uint64_t frame, ChangeSetPtr& changes);

    /// Queues a play: the line of the slot starts exactly at `frame`, which becomes the
    /// sequencer's anchor, and the generator is reseeded from the song part's seed. Notes started
    /// before the line don't sound. A play while playing first does what a stop does, at the same
    /// frame. A slot past the sequence's end plays slot 0, and a line past its pattern's end ends
    /// its slot at once.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the line starts.
    /// \param slot The slot, from 0 to 999, which is checked.
    /// \param line The line, from 0 to 511, which is checked.
    /// \param loop Whether the slot loops, going back to its line 0 at its pattern's end.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool play(std::uint64_t frame, int slot, int line, bool loop);

    /// Queues a set-loop: switches looping of the playing slot at `frame`. While stopped it changes
    /// nothing.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which looping switches.
    /// \param loop Whether the slot loops.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool set_loop(std::uint64_t frame, bool loop);

    /// Queues a stop: at `frame` the song stops, and every held voice the note columns own starts
    /// its release, so the tails ring out. A stop while stopped fades out every voice, the note
    /// keys' included, over 5 ms.
    ///
    /// Call it on the control thread, as `note_on()`.
    ///
    /// \param frame The frame at which the song stops.
    /// \return Whether the message was queued. A full queue refuses it and changes nothing.
    [[nodiscard]] bool stop(std::uint64_t frame);

    /// Takes back the oldest change set the player has applied and not yet handed back. It holds
    /// the parts it replaced, which dropping it frees. Call it on the control thread, until it
    /// returns null, often enough that the return queue doesn't fill while sound plays.
    ///
    /// \return The change set, or null if none is waiting.
    [[nodiscard]] ChangeSetPtr take_returned();

    /// Renders the next frames, applying each queued message at its frame.
    ///
    /// Call it on the audio thread. It keeps the real-time rules, and is the render entry point
    /// that holds a `RenderScope`. It takes any number of frames, none included, and writes what
    /// the voices sum to, unclipped.
    ///
    /// \param out Where to write the frames, interleaved stereo, two samples for each, left then
    ///            right, overwriting what it holds. Its length is even, which is checked.
    void render(std::span<float> out) IRAMA_NONBLOCKING;

    /// \return How many voices were sounding at the end of the latest render call, not counting
    ///         stolen voices fading out, so never more than the config's `max_voices`. It may be
    ///         read on any thread.
    [[nodiscard]] std::uint32_t active_voices() const;

    /// \return The play position at the end of the latest render call. It may be read on any
    ///         thread.
    [[nodiscard]] PlayPosition play_position() const;

  private:
    using Message = detail::ControlMessage;

    // Queues `message` on the control thread, if the queue has room.
    [[nodiscard]] bool push(const Message& message);

    // Applies the messages due at the current frame, popping no more than the render call has left
    // of its limit.
    void apply_due_messages(std::size_t& popped) IRAMA_NONBLOCKING;

    // Applies a message, and returns whether it could: a change waits while the return queue is
    // full.
    [[nodiscard]] bool apply(const Message& message) IRAMA_NONBLOCKING;

    // Plays the sequencer's notes due at the current frame.
    void play_due_notes() IRAMA_NONBLOCKING;

    // Plays one of the sequencer's notes on the voices its column owns.
    void play_note(const SeqNote& note) IRAMA_NONBLOCKING;

    // Starts a voice playing a note, owned by `owner`, at the instrument's pan or a fixed one,
    // unless the instrument number is empty.
    void start_voice(int note, float velocity, std::size_t number, std::uint16_t owner,
                     std::optional<float> pan) IRAMA_NONBLOCKING;

    // Releases every held voice the note columns own.
    void release_columns() IRAMA_NONBLOCKING;

    // Swaps a change set's parts into the EngineSong, and hands their effects to the voices.
    void apply_changes(ChangeSet& changes) IRAMA_NONBLOCKING;

    // Gives the instrument's smoothers the values of its new part that differ from theirs, or fades
    // out its voices if it was emptied.
    void change_instrument(std::size_t number) IRAMA_NONBLOCKING;

    // Whether any voice of the instrument is sounding, a fading one included.
    [[nodiscard]] bool sounding(std::size_t number) const;

    // The live state of an instrument number.
    [[nodiscard]] Instrument& instrument(std::size_t number);

    // Renders frames that no message falls inside, at most one block of them.
    void render_block(std::span<float> out) IRAMA_NONBLOCKING;

    EngineSong song_;
    // The live value of each instrument number's parameters, instrument 01 first, which only the
    // audio thread touches.
    OwnedArray<Instrument> instruments_;
    // Where each instrument computes its values for a block, one after another.
    InstrumentBuffers buffers_;
    OwnedArray<Voice> voices_;
    // The instrument number of each voice's latest note.
    OwnedArray<std::uint8_t> voice_instruments_;
    // Who owns each voice: the note keys, a note column, or nobody.
    OwnedArray<std::uint16_t> voice_owners_;
    // The stolen voices fading out, and the instrument number of each.
    OwnedArray<Voice> fades_;
    OwnedArray<std::uint8_t> fade_instruments_;
    // How many notes the player has started, which orders its voices from oldest to newest.
    std::uint64_t notes_started_ = 0;
    // The generator each note-on splits its voice's noise generator from.
    Pcg32 generator_;
    SequencerPtr sequencer_;
    // Each voice's working, for one block.
    OwnedArray<float> scratch_;
    SpscQueue<Message, queue_capacity> queue_;
    // The change sets applied, on their way back to the control thread.
    SpscQueue<ChangeSet*, return_capacity> returned_;
    // The frame of the latest message queued, which only the control thread reads.
    std::uint64_t last_queued_frame_ = 0;
    // The message popped whose frame hasn't come yet, or a change waiting for room in the return
    // queue, since the queue can't be read without popping.
    std::optional<Message> waiting_;
    // How many frames the player has rendered, which only the audio thread reads.
    std::uint64_t frame_ = 0;
    std::atomic<std::uint32_t> active_voices_ = 0;
    std::atomic<std::uint64_t> play_position_ = PlayPosition{}.pack();
};

} // namespace irama
