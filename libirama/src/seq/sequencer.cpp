// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/seq/sequencer.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace {

using irama::NoteCell;

// A line is 256 units long.
constexpr std::uint64_t units_per_line = 256;

// Frames per unit are `rate · 6000 / (bpm100 · lpb · 256)`: 60 seconds a minute, over hundredths of
// a beat.
constexpr std::uint64_t hundredth_minute = 6'000;

constexpr std::size_t last_slot = 999;
constexpr std::size_t last_line = 511;

// The packed play position: the line in the low 16 bits, the slot in the next 16, then the loop
// flag and the playing flag.
constexpr unsigned slot_shift = 16;
constexpr unsigned loop_shift = 32;
constexpr unsigned playing_shift = 33;
constexpr std::uint64_t field_mask = 0xFFFF;

// The volume that plays at a velocity of 1, and the centre of the pan.
constexpr float loudest = 128.0F;
constexpr float centre = 64.0F;

// Whether `left` plays before `right`: by frame, then by note column.
[[nodiscard]] bool plays_before(const irama::SeqNote& left, const irama::SeqNote& right) {
    if (left.frame != right.frame) {
        return left.frame < right.frame;
    }
    if (left.track != right.track) {
        return left.track < right.track;
    }
    return left.column < right.column;
}

// The note a written cell plays at the frame, from the note column.
[[nodiscard]] irama::SeqNote played_note(const NoteCell& written, std::uint64_t frame,
                                         std::uint8_t track, std::uint8_t column) {
    return irama::SeqNote{
        .frame = frame,
        .track = track,
        .column = column,
        .note = written.note,
        .instrument = written.instrument,
        .velocity =
            written.volume == NoteCell::empty ? 1.0F : static_cast<float>(written.volume) / loudest,
        .fixed_pan = written.pan != NoteCell::empty,
        .pan = written.pan == NoteCell::empty ? 0.0F
                                              : (static_cast<float>(written.pan) - centre) / centre,
    };
}

// The pattern that a slot within the sequence names.
[[nodiscard]] const irama::PatternPart* slot_pattern(const irama::EngineSong& song,
                                                     std::span<const std::uint16_t> sequence,
                                                     std::size_t slot) {
    return song.pattern(sequence.subspan(slot).front());
}

} // namespace

namespace irama {

std::uint64_t PlayPosition::pack() const {
    return std::uint64_t{line} | (std::uint64_t{slot} << slot_shift) |
           (std::uint64_t{loop} << loop_shift) | (std::uint64_t{playing} << playing_shift);
}

PlayPosition PlayPosition::unpack(std::uint64_t packed) {
    return PlayPosition{
        .playing = ((packed >> playing_shift) & 1U) != 0,
        .loop = ((packed >> loop_shift) & 1U) != 0,
        .slot = narrow<std::uint16_t>((packed >> slot_shift) & field_mask),
        .line = narrow<std::uint16_t>(packed & field_mask),
    };
}

std::expected<SequencerPtr, AllocationError> Sequencer::create(const Allocator& allocator,
                                                               std::uint32_t sample_rate) {
    IRAMA_ASSERT(sample_rate > 0, "a sample rate is above 0 Hz");
    auto notes = OwnedArray<SeqNote>::create(allocator, max_line_notes);
    if (!notes) {
        return std::unexpected(notes.error());
    }
    return make_owned<Sequencer>(allocator, Key{}, sample_rate, std::move(*notes));
}

Sequencer::Sequencer(Key /*key*/, std::uint32_t sample_rate, OwnedArray<SeqNote> notes)
    : sample_rate_{sample_rate}, notes_{std::move(notes)} {}

void Sequencer::play(std::uint64_t frame, std::size_t slot, std::size_t line, bool loop,
                     const EngineSong& song) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(slot <= last_slot, "a slot is from 0 to 999");
    IRAMA_ASSERT(line <= last_line, "a line is from 0 to 511");
    IRAMA_ASSERT(song.song() != nullptr, "a song played has a song part");
    playing_ = true;
    loop_ = loop;
    slot_ = narrow<std::uint16_t>(slot);
    line_ = narrow<std::uint16_t>(line);
    starting_ = true;
    bpm100_ = song.song()->bpm100();
    lpb_ = song.song()->lpb();
    note_count_ = 0;
    played_ = 0;
    line_frame_ = frame;
    line_remainder_ = 0;
    next_line_frame_ = frame;
}

void Sequencer::set_loop(bool loop) IRAMA_NONBLOCKING {
    if (playing_) {
        loop_ = loop;
    }
}

void Sequencer::stop() IRAMA_NONBLOCKING {
    playing_ = false;
    note_count_ = 0;
    played_ = 0;
}

PlayPosition Sequencer::position() const {
    return PlayPosition{.playing = playing_, .loop = loop_, .slot = slot_, .line = line_};
}

std::optional<std::uint64_t> Sequencer::next_frame() const {
    if (!playing_) {
        return std::nullopt;
    }
    if (played_ < note_count_) {
        // A line's notes all come before the next line's start, or on it.
        return notes_[played_].frame;
    }
    return next_line_frame_;
}

std::span<const SeqNote> Sequencer::advance(std::uint64_t frame,
                                            const EngineSong& song) IRAMA_NONBLOCKING {
    IRAMA_ASSERT(playing_ && next_frame() == frame, "a sequencer plays at its next frame");
    IRAMA_ASSERT(song.song() != nullptr, "a song played has a song part");
    if (played_ == note_count_) {
        read_line(song);
    }
    const std::size_t first = played_;
    while (played_ < note_count_ && notes_[played_].frame == frame) {
        ++played_;
    }
    return notes_.span().subspan(first, played_ - first);
}

void Sequencer::read_line(const EngineSong& song) IRAMA_NONBLOCKING {
    const SongPart& part = *song.song();
    if (starting_) {
        starting_ = false;
    } else {
        // The line before ends at its own timing's length.
        const std::uint64_t remainder = line_remainder_ + (units_per_line * numerator());
        line_frame_ += remainder / denominator();
        line_remainder_ = remainder % denominator();
        ++line_;
        if (part.bpm100() != bpm100_ || part.lpb() != lpb_) {
            // A new timing starts here, which becomes the anchor, a whole frame.
            bpm100_ = part.bpm100();
            lpb_ = part.lpb();
            line_remainder_ = 0;
        }
    }
    const PatternPart* pattern = settle(song);

    note_count_ = 0;
    played_ = 0;
    if (pattern != nullptr) {
        const std::span<const NoteCell> cells = pattern->line(line_);
        auto written = cells.begin();
        for (const auto [track, columns] : std::views::enumerate(part.note_columns())) {
            for (std::uint8_t column = 0; column < columns && written != cells.end();
                 ++column, ++written) {
                if (written->note == NoteCell::empty) {
                    continue;
                }
                const std::uint64_t delay = written->delay == NoteCell::empty ? 0 : written->delay;
                const std::uint64_t frame =
                    line_frame_ + ((line_remainder_ + (delay * numerator())) / denominator());
                notes_[note_count_++] =
                    played_note(*written, frame, narrow<std::uint8_t>(track), column);
            }
        }
        std::ranges::sort(notes_.span().first(note_count_), plays_before);
    }
    next_line_frame_ =
        line_frame_ + ((line_remainder_ + (units_per_line * numerator())) / denominator());
}

const PatternPart* Sequencer::settle(const EngineSong& song) IRAMA_NONBLOCKING {
    const std::span<const std::uint16_t> sequence = song.song()->sequence();
    if (slot_ >= sequence.size()) {
        slot_ = 0;
        line_ = 0;
    }
    const PatternPart* pattern = slot_pattern(song, sequence, slot_);
    // A slot whose pattern number is empty plays as one empty line.
    const std::size_t lines = pattern == nullptr ? 1 : pattern->lines();
    if (line_ < lines) {
        return pattern;
    }
    line_ = 0;
    if (!loop_) {
        ++slot_;
        if (slot_ >= sequence.size()) {
            slot_ = 0;
        }
    }
    return slot_pattern(song, sequence, slot_);
}

std::uint64_t Sequencer::numerator() const {
    return std::uint64_t{sample_rate_} * hundredth_minute;
}

std::uint64_t Sequencer::denominator() const {
    return std::uint64_t{bpm100_} * lpb_ * units_per_line;
}

} // namespace irama
