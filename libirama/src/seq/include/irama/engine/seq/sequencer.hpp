// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The sequencer: the play position, and the note-ons and note-offs a song's patterns make, each at
/// its exact frame.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/model/engine_song.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace irama {

/// Where a song is playing: the slot and the line of the latest line started, and whether it plays
/// and loops.
struct PlayPosition {
    /// Whether the song is playing.
    bool playing = false;
    /// Whether the playing slot goes back to its line 0 at its pattern's end.
    bool loop = false;
    /// The slot, from 0 to 999.
    std::uint16_t slot = 0;
    /// The line, from 0 to 511.
    std::uint16_t line = 0;

    /// \return The position packed into one value, which `unpack()` reads back.
    [[nodiscard]] std::uint64_t pack() const;

    /// \param packed A value `pack()` made.
    /// \return The position it holds.
    [[nodiscard]] static PlayPosition unpack(std::uint64_t packed);

    /// \return Whether two positions are the same.
    friend bool operator==(const PlayPosition&, const PlayPosition&) = default;
};

/// One of the sequencer's note-ons or note-offs: a cell of a note column, at the frame it plays.
struct SeqNote {
    /// The frame at which it plays.
    std::uint64_t frame = 0;
    /// The track of its note column, from 0.
    std::uint8_t track = 0;
    /// Its note column within the track, from 0.
    std::uint8_t column = 0;
    /// The note, from 0 for C-0 to 119 for B-9, or `NoteCell::off` for a note-off.
    std::uint8_t note = 0;
    /// The instrument number, from 0x01 to 0xFE, or `NoteCell::empty`, for a note that plays
    /// nothing.
    std::uint8_t instrument = 0;
    /// How hard the note is played, from 0 to 1: the volume over 0x80, or 1 for an empty volume.
    float velocity = 1.0F;
    /// Whether the note sounds at `pan` rather than at its instrument's pan.
    bool fixed_pan = false;
    /// Where the note sounds, from −1, hard left, to +1, hard right, if `fixed_pan`.
    float pan = 0.0F;

    /// \return Whether two notes are the same.
    friend bool operator==(const SeqNote&, const SeqNote&) = default;
};

class Sequencer;

/// The single owner of a sequencer.
using SequencerPtr = OwnedPtr<Sequencer>;

/// Plays an EngineSong's patterns: it keeps the play position, and turns each line's note cells
/// into note-ons and note-offs at exact frames, without touching any voice.
///
/// Time counts in 256ths of a line from an **anchor**, a whole frame at which a line starts. The
/// delay `d` of the line `n` lines after the anchor is unit `u = 256·n + d`, which plays at frame
/// `anchor + floor(u · rate · 6000 / (bpm100 · lpb · 256))`, the frame its exact time falls in. The
/// sequencer carries the remainder from line to line, which gives the same frames and never
/// overflows. `play()` sets the anchor at its frame. A song part of another BPM or LPB lets the
/// line playing finish at its old length, and its timing starts at the next line start, which
/// becomes the anchor. Ticks per line change nothing.
///
/// Each line is read once, at its start, from the pattern its slot names in the song as it is then,
/// and its notes are kept until their frames. A cell with a note plays it, or a note-off for an
/// `OFF`, at its delay, 0 for an empty one, with the volume as its velocity and the pan, if
/// written, fixed. The song's tracks and their note columns name each cell's column. A slot whose
/// pattern number is empty plays as one empty line. After a pattern's last line comes line 0 of the
/// same slot while it loops, and of the next slot otherwise. After the last slot, and at a slot
/// past the sequence's end, comes slot 0, line 0. A line past its pattern's end ends its slot at
/// once.
///
/// It allocates everything when it is created, so the audio thread can play it.
class Sequencer {
    // Lets create() alone construct a sequencer, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// The most notes one line holds: one for each note column.
    static constexpr std::size_t max_line_notes = PatternPart::max_columns;

    /// Makes a stopped sequencer, on a control thread.
    ///
    /// \param allocator The allocator to draw on, which the sequencer is freed to.
    /// \param sample_rate The sample rate in hertz, above 0, which is checked.
    /// \return The sequencer, or `AllocationError::out_of_memory` when the allocator has no
    ///         memory.
    [[nodiscard]] static std::expected<SequencerPtr, AllocationError>
    create(const Allocator& allocator, std::uint32_t sample_rate);

    /// Makes the sequencer, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param sample_rate The sample rate in hertz.
    /// \param notes Room for the notes of one line.
    Sequencer(Key key, std::uint32_t sample_rate, OwnedArray<SeqNote> notes);

    /// Starts playing: the line of the slot starts exactly at the frame, which becomes the anchor,
    /// with the song's timing. Notes waiting from a line already playing are dropped.
    ///
    /// \param frame The frame at which the line starts.
    /// \param slot The slot, from 0 to 999, which is checked.
    /// \param line The line, from 0 to 511, which is checked.
    /// \param loop Whether the slot loops.
    /// \param song The song, which has a song part, which is checked.
    void play(std::uint64_t frame, std::size_t slot, std::size_t line, bool loop,
              const EngineSong& song) IRAMA_NONBLOCKING;

    /// Switches looping of the playing slot. While stopped it changes nothing.
    ///
    /// \param loop Whether the slot loops.
    void set_loop(bool loop) IRAMA_NONBLOCKING;

    /// Stops playing, dropping the notes still waiting.
    void stop() IRAMA_NONBLOCKING;

    /// \return Whether it is playing.
    [[nodiscard]] bool playing() const { return playing_; }

    /// \return The play position.
    [[nodiscard]] PlayPosition position() const;

    /// \return The frame of its next note or line start, or nothing while stopped.
    [[nodiscard]] std::optional<std::uint64_t> next_frame() const;

    /// Plays what comes at the next frame: the notes waiting for it, or else the line starting at
    /// it, which it reads.
    ///
    /// Call it while playing, at `next_frame()`, which is checked, until `next_frame()` is later.
    ///
    /// \param frame The frame, `next_frame()`.
    /// \param song The song as it is at the frame, which has a song part, which is checked.
    /// \return The notes that play at the frame, by note column, the first track's first. They stay
    ///         valid until the next call.
    [[nodiscard]] std::span<const SeqNote> advance(std::uint64_t frame,
                                                   const EngineSong& song) IRAMA_NONBLOCKING;

  private:
    // Reads the line starting at `line_frame_`, keeping its notes in time order.
    void read_line(const EngineSong& song) IRAMA_NONBLOCKING;

    // Moves the play position onto a line that exists, by the end-of-pattern and end-of-sequence
    // rules, and returns its pattern, or null for a slot whose pattern number is empty.
    const PatternPart* settle(const EngineSong& song) IRAMA_NONBLOCKING;

    // A unit lasts `rate · 6000 / (bpm100 · lpb · 256)` frames: the numerator, and the denominator
    // of the timing in effect.
    [[nodiscard]] std::uint64_t numerator() const;
    [[nodiscard]] std::uint64_t denominator() const;

    std::uint32_t sample_rate_;
    OwnedArray<SeqNote> notes_;
    // How many notes the line holds, and how many of them have played.
    std::size_t note_count_ = 0;
    std::size_t played_ = 0;
    bool playing_ = false;
    bool loop_ = false;
    std::uint16_t slot_ = 0;
    std::uint16_t line_ = 0;
    // Whether the next line read is the one `play()` named, rather than the one after the latest.
    bool starting_ = false;
    // The timing in effect, which a song part of another changes at the next line start.
    std::uint32_t bpm100_ = 0;
    std::uint8_t lpb_ = 0;
    // The latest line's start: the frame its exact time falls in, and how far past that frame it
    // is, in units of one denominator-th of a frame.
    std::uint64_t line_frame_ = 0;
    std::uint64_t line_remainder_ = 0;
    // The next line's start, as a whole frame.
    std::uint64_t next_line_frame_ = 0;
};

} // namespace irama
