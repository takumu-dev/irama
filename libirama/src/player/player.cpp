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
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
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
    auto fades = OwnedArray<Voice>::create(config.allocator, fade_slots);
    auto fade_instruments = OwnedArray<std::uint8_t>::create(config.allocator, fade_slots);
    auto scratch =
        OwnedArray<float>::create(config.allocator, block_frames * Voice::scratch_per_frame);
    if (!player || !song || !start || !instruments || !voices || !voice_instruments || !fades ||
        !fade_instruments || !scratch) {
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
    (*player)->fades_ = std::move(*fades);
    (*player)->fade_instruments_ = std::move(*fade_instruments);
    (*player)->scratch_ = std::move(*scratch);
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
        if (waiting_->frame > frame_ || !apply(*waiting_)) {
            return;
        }
        waiting_.reset();
    }
}

bool Player::apply(const Message& message) IRAMA_NONBLOCKING {
    switch (message.kind) {
    case Message::Kind::note_on: {
        if (song_.instrument(message.instrument) == nullptr) {
            break;
        }
        const std::size_t index = choose_voice(voices_.span());
        Voice& voice = voices_[index];
        if (voice.active()) {
            // The stolen voice fades out in a copy, so its own voice starts the note on this frame.
            const std::size_t slot = choose_fade_slot(fades_.span());
            fades_[slot] = voice;
            fades_[slot].fade_out();
            fade_instruments_[slot] = voice_instruments_[index];
        }
        voice.start(message.note, message.velocity, instrument(message.instrument),
                    generator_.split(), notes_started_++);
        voice_instruments_[index] = message.instrument;
        break;
    }
    case Message::Kind::note_off:
        for (const auto [voice, number] : std::views::zip(voices_, voice_instruments_)) {
            if (voice.held() && voice.note() == message.note && number == message.instrument) {
                voice.release(instrument(number));
            }
        }
        break;
    case Message::Kind::release_all:
        for (const auto [voice, number] : std::views::zip(voices_, voice_instruments_)) {
            if (voice.active()) {
                voice.release(instrument(number));
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
