// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/seq/sequencer.hpp>
#include <irama/engine/source/tuning.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>
#include <irama/engine/voice/voice_allocation.hpp>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace {

// The player renders in blocks of at most this many frames, counted from its first frame, and
// splits a block further at each message's frame. Nothing it writes depends on the size, so it
// can be tuned for speed alone.
constexpr std::size_t block_frames = 256;
static_assert(block_frames <= irama::Instrument::max_block_frames);

constexpr std::size_t channels = 2;

constexpr int last_slot = 999;
constexpr int last_line = 511;

// A voice's owner: the note keys, nobody, or a note column, as its track in the high byte and its
// column in the low one.
constexpr std::uint16_t note_keys = 0xFFFF;
constexpr std::uint16_t nobody = 0xFFFE;

[[nodiscard]] constexpr std::uint16_t column_owner(std::uint8_t track, std::uint8_t column) {
    return static_cast<std::uint16_t>((unsigned{track} << 8U) | unsigned{column});
}

[[nodiscard]] constexpr bool owned_by_column(std::uint16_t owner) {
    return owner != note_keys && owner != nobody;
}

// Frees a change set that never came back, on the control thread.
void free_unreturned(irama::ChangeSet* changes) {
    irama::AllocatorDelete<irama::ChangeSet>{.allocator = changes->allocator()}(changes);
}

} // namespace

namespace irama {

std::expected<PlayerPtr, CreateError> Player::create(const PlayerConfig& config) {
    IRAMA_ASSERT(config.sample_rate > 0, "a sample rate is above 0 Hz");
    IRAMA_ASSERT(config.max_voices > 0 && config.max_voices <= max_voices_limit,
                 "a player has from 1 to 256 voices");
    auto player = make_owned<Player>(config.allocator, Key{});
    auto song = SongPart::create(config.allocator);
    auto start = ChangeSet::create(config.allocator);
    auto instruments =
        OwnedArray<Instrument>::create(config.allocator, EngineSong::instrument_count,
                                       Instrument{static_cast<float>(config.sample_rate)});
    auto voices = OwnedArray<Voice>::create(config.allocator, config.max_voices);
    auto voice_instruments = OwnedArray<std::uint8_t>::create(config.allocator, config.max_voices);
    auto voice_owners =
        OwnedArray<std::uint16_t>::create(config.allocator, config.max_voices, nobody);
    auto fades = OwnedArray<Voice>::create(config.allocator, fade_slots);
    auto fade_instruments = OwnedArray<std::uint8_t>::create(config.allocator, fade_slots);
    auto scratch =
        OwnedArray<float>::create(config.allocator, block_frames * Voice::scratch_per_frame);
    auto sequencer = Sequencer::create(config.allocator, config.sample_rate);
    if (!player || !song || !start || !instruments || !voices || !voice_instruments ||
        !voice_owners || !fades || !fade_instruments || !scratch || !sequencer) {
        return std::unexpected(CreateError::out_of_memory);
    }
    // The new song's song part goes in as any other does, and the change set that brought it, empty
    // now, is freed here.
    const std::uint64_t seed = (*song)->seed();
    (*start)->replace_song(std::move(*song));
    (*player)->song_.apply(**start);
    (*player)->generator_ = Pcg32{seed, 0};
    (*player)->instruments_ = std::move(*instruments);
    (*player)->voices_ = std::move(*voices);
    (*player)->voice_instruments_ = std::move(*voice_instruments);
    (*player)->voice_owners_ = std::move(*voice_owners);
    (*player)->fades_ = std::move(*fades);
    (*player)->fade_instruments_ = std::move(*fade_instruments);
    (*player)->scratch_ = std::move(*scratch);
    (*player)->sequencer_ = std::move(*sequencer);
    return std::move(*player);
}

Player::Player(Key /*key*/) {}

Player::~Player() {
    // No thread renders now, so the control thread can empty the audio thread's queue too.
    if (waiting_ && waiting_->kind == Message::Kind::change) {
        free_unreturned(waiting_->changes);
    }
    while (const auto message = queue_.try_pop()) {
        if (message->kind == Message::Kind::change) {
            free_unreturned(message->changes);
        }
    }
    while (take_returned() != nullptr) {
    }
}

bool Player::note_on(std::uint64_t frame, int note, float velocity, int instrument) {
    IRAMA_ASSERT(note >= lowest_note && note <= highest_note, "a note is between C-0 and B-9");
    IRAMA_ASSERT(velocity >= 0.0F && velocity <= 1.0F, "a velocity is from 0 to 1");
    IRAMA_ASSERT(instrument >= 0x01 && instrument <= 0xFE, "an instrument number is from 01 to FE");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::note_on,
        .note = narrow<std::uint8_t>(note),
        .instrument = narrow<std::uint8_t>(instrument),
        .velocity = velocity,
    });
}

bool Player::note_off(std::uint64_t frame, int note, int instrument) {
    IRAMA_ASSERT(note >= lowest_note && note <= highest_note, "a note is between C-0 and B-9");
    IRAMA_ASSERT(instrument >= 0x01 && instrument <= 0xFE, "an instrument number is from 01 to FE");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::note_off,
        .note = narrow<std::uint8_t>(note),
        .instrument = narrow<std::uint8_t>(instrument),
    });
}

bool Player::release_all(std::uint64_t frame) {
    return push(Message{.frame = frame, .kind = Message::Kind::release_all});
}

bool Player::change(std::uint64_t frame, ChangeSetPtr& changes) {
    IRAMA_ASSERT(changes != nullptr, "a change has a change set");
    for (const ChangeSet::InstrumentEntry& entry : changes->instruments()) {
        if (entry.part == nullptr) {
            continue;
        }
        const std::span<const float> values = entry.part->values();
        IRAMA_ASSERT(values.size() == instrument_params.size(),
                     "an instrument part has a value for each of the instrument's parameters");
        for (const auto [param, value] : std::views::zip(instrument_params, values)) {
            IRAMA_ASSERT(value == param.constrain(value),
                         "an instrument's value is constrained to its parameter");
        }
    }
    if (!push(Message{.frame = frame, .kind = Message::Kind::change, .changes = changes.get()})) {
        return false;
    }
    // The message owns the change set now.
    [[maybe_unused]] const ChangeSet* const queued = changes.release();
    return true;
}

bool Player::play(std::uint64_t frame, int slot, int line, bool loop) {
    IRAMA_ASSERT(slot >= 0 && slot <= last_slot, "a slot is from 0 to 999");
    IRAMA_ASSERT(line >= 0 && line <= last_line, "a line is from 0 to 511");
    return push(Message{
        .frame = frame,
        .kind = Message::Kind::play,
        .slot = narrow<std::uint16_t>(slot),
        .line = narrow<std::uint16_t>(line),
        .loop = loop,
    });
}

bool Player::set_loop(std::uint64_t frame, bool loop) {
    return push(Message{.frame = frame, .kind = Message::Kind::set_loop, .loop = loop});
}

bool Player::stop(std::uint64_t frame) {
    return push(Message{.frame = frame, .kind = Message::Kind::stop});
}

ChangeSetPtr Player::take_returned() {
    const std::optional<ChangeSet*> changes = returned_.try_pop();
    if (!changes) {
        return nullptr;
    }
    return ChangeSetPtr{*changes, AllocatorDelete<ChangeSet>{.allocator = (*changes)->allocator()}};
}

void Player::render(std::span<float> out) IRAMA_NONBLOCKING {
    const RenderScope scope;
    IRAMA_ASSERT(out.size() % channels == 0, "stereo frames have two samples each");
    std::size_t popped = 0;
    while (!out.empty()) {
        apply_due_messages(popped);
        play_due_notes();
        std::uint64_t frames =
            std::min<std::uint64_t>(out.size() / channels, block_frames - (frame_ % block_frames));
        if (waiting_ && waiting_->frame > frame_) {
            frames = std::min(frames, waiting_->frame - frame_);
        }
        if (const std::optional<std::uint64_t> next = sequencer_->next_frame()) {
            frames = std::min(frames, *next - frame_);
        }
        const std::size_t length = narrow<std::size_t>(frames) * channels;
        render_block(out.first(length));
        out = out.subspan(length);
        frame_ += frames;
    }
    const auto active = std::ranges::count_if(voices_, &Voice::active);
    active_voices_.store(narrow<std::uint32_t>(active), std::memory_order_relaxed);
    play_position_.store(sequencer_->position().pack(), std::memory_order_relaxed);
}

std::uint32_t Player::active_voices() const {
    return active_voices_.load(std::memory_order_relaxed);
}

PlayPosition Player::play_position() const {
    return PlayPosition::unpack(play_position_.load(std::memory_order_relaxed));
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
        if (waiting_->frame > frame_ || !apply(*waiting_)) {
            return;
        }
        waiting_.reset();
    }
}

bool Player::apply(const Message& message) IRAMA_NONBLOCKING {
    switch (message.kind) {
    case Message::Kind::note_on:
        start_voice(message.note, message.velocity, message.instrument, note_keys, std::nullopt);
        break;
    case Message::Kind::note_off:
        for (const auto [voice, number, owner] :
             std::views::zip(voices_, voice_instruments_, voice_owners_)) {
            if (owner == note_keys && voice.held() && voice.note() == message.note &&
                number == message.instrument) {
                voice.release(instrument(number));
            }
        }
        break;
    case Message::Kind::release_all:
        for (const auto [voice, number, owner] :
             std::views::zip(voices_, voice_instruments_, voice_owners_)) {
            if (owner == note_keys && voice.held()) {
                voice.release(instrument(number));
            }
        }
        break;
    case Message::Kind::play:
        if (sequencer_->playing()) {
            release_columns();
        }
        generator_ = Pcg32{song_.song()->seed(), 0};
        sequencer_->play(frame_, message.slot, message.line, message.loop, song_);
        break;
    case Message::Kind::set_loop:
        sequencer_->set_loop(message.loop);
        break;
    case Message::Kind::stop:
        if (sequencer_->playing()) {
            release_columns();
            sequencer_->stop();
            break;
        }
        // A stop while stopped is a panic.
        for (Voice& voice : voices_) {
            if (voice.active() && !voice.fading()) {
                voice.fade_out();
            }
        }
        break;
    case Message::Kind::change:
        // The change set goes back holding the parts it replaced, so it waits until it can.
        if (!returned_.has_room()) {
            return false;
        }
        apply_changes(*message.changes);
        static_cast<void>(returned_.try_push(message.changes));
        break;
    }
    return true;
}

void Player::play_due_notes() IRAMA_NONBLOCKING {
    while (sequencer_->next_frame() == frame_) {
        for (const SeqNote& note : sequencer_->advance(frame_, song_)) {
            play_note(note);
        }
    }
}

void Player::play_note(const SeqNote& note) IRAMA_NONBLOCKING {
    const std::uint16_t owner = column_owner(note.track, note.column);
    for (const auto [voice, number, voice_owner] :
         std::views::zip(voices_, voice_instruments_, voice_owners_)) {
        if (voice_owner != owner || !voice.held()) {
            continue;
        }
        if (note.note == NoteCell::off) {
            voice.release(instrument(number));
            continue;
        }
        switch (instrument(number).new_note_action()) {
        case NewNoteAction::note_off:
            voice.release(instrument(number));
            break;
        case NewNoteAction::cut:
            voice.fade_out();
            voice_owner = nobody;
            break;
        case NewNoteAction::carry_on:
            break;
        }
    }
    if (note.note == NoteCell::off || note.instrument == NoteCell::empty) {
        return;
    }
    start_voice(note.note, note.velocity, note.instrument, owner,
                note.fixed_pan ? std::optional<float>{note.pan} : std::nullopt);
}

void Player::start_voice(int note, float velocity, std::size_t number, std::uint16_t owner,
                         std::optional<float> pan) IRAMA_NONBLOCKING {
    if (song_.instrument(number) == nullptr) {
        return;
    }
    const std::size_t index = choose_voice(voices_.span());
    Voice& voice = voices_[index];
    if (voice.active()) {
        // The stolen voice fades out in a copy, so its own voice starts the note on this frame. It
        // leaves its owner, which the new note's owner replaces.
        const std::size_t slot = choose_fade_slot(fades_.span());
        fades_[slot] = voice;
        if (!fades_[slot].fading()) {
            fades_[slot].fade_out();
        }
        fade_instruments_[slot] = voice_instruments_[index];
    }
    voice.start(note, velocity, instrument(number), generator_.split(), notes_started_++);
    if (pan) {
        voice.fix_pan(*pan);
    }
    voice_instruments_[index] = narrow<std::uint8_t>(number);
    voice_owners_[index] = owner;
}

void Player::release_columns() IRAMA_NONBLOCKING {
    for (const auto [voice, number, owner] :
         std::views::zip(voices_, voice_instruments_, voice_owners_)) {
        if (owned_by_column(owner) && voice.held()) {
            voice.release(instrument(number));
        }
    }
}

void Player::apply_changes(ChangeSet& changes) IRAMA_NONBLOCKING {
    song_.apply(changes);
    if (changes.replaces_song()) {
        const std::uint64_t seed = song_.song()->seed();
        if (changes.song() == nullptr || changes.song()->seed() != seed) {
            generator_ = Pcg32{seed, 0};
        }
    }
    for (const ChangeSet::InstrumentEntry& entry : changes.instruments()) {
        change_instrument(entry.number);
    }
}

void Player::change_instrument(std::size_t number) IRAMA_NONBLOCKING {
    const InstrumentPart* part = song_.instrument(number);
    if (part == nullptr) {
        // With no values left to read, the instrument's voices fade out as stolen ones do.
        for (const auto [voice, voice_number] : std::views::zip(voices_, voice_instruments_)) {
            if (voice_number == number && voice.active() && !voice.fading()) {
                voice.fade_out();
            }
        }
        return;
    }
    // While no voice of the instrument sounds, nothing would hear a glide, so its values jump.
    const bool smooth = sounding(number);
    Instrument& live = instrument(number);
    for (const auto [index, value] : std::views::enumerate(part->values())) {
        const auto param = narrow<std::size_t>(index);
        if (value != live.target(param)) {
            live.set(param, value, smooth);
        }
    }
}

bool Player::sounding(std::size_t number) const {
    const auto of_instrument = [number](const auto& slot) {
        const auto [voice, voice_number] = slot;
        return voice_number == number && voice.active();
    };
    return std::ranges::any_of(std::views::zip(voices_, voice_instruments_), of_instrument) ||
           std::ranges::any_of(std::views::zip(fades_, fade_instruments_), of_instrument);
}

Instrument& Player::instrument(std::size_t number) {
    return instruments_[number - EngineSong::first_instrument];
}

void Player::render_block(std::span<float> out) IRAMA_NONBLOCKING {
    std::ranges::fill(out, 0.0F);
    const std::size_t frames = out.size() / channels;
    std::bitset<EngineSong::instrument_count + EngineSong::first_instrument> heard;
    for (const auto [voice, number] : std::views::zip(voices_, voice_instruments_)) {
        if (voice.active()) {
            heard.set(number);
        }
    }
    for (const auto [fade, number] : std::views::zip(fades_, fade_instruments_)) {
        if (fade.active()) {
            heard.set(number);
        }
    }
    // Each instrument heard computes its values into the one set of buffers, and its voices read
    // them, voices before fades, before the next instrument overwrites them.
    for (std::size_t number = EngineSong::first_instrument; number <= EngineSong::last_instrument;
         ++number) {
        Instrument& live = instrument(number);
        if (!heard.test(number)) {
            live.advance(frames);
            continue;
        }
        const InstrumentBlock block = live.render(buffers_, frames);
        for (const auto [voice, voice_number] : std::views::zip(voices_, voice_instruments_)) {
            if (voice_number == number) {
                voice.render(out, scratch_.span(), block);
            }
        }
        for (const auto [fade, fade_number] : std::views::zip(fades_, fade_instruments_)) {
            if (fade_number == number) {
                fade.render(out, scratch_.span(), block);
            }
        }
    }
}

} // namespace irama
