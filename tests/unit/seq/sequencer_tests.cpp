// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/seq/sequencer.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/test_songs/test_songs.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using irama::NoteCell;
using irama::PlayPosition;
using irama::SeqNote;
using irama_test_songs::Line;
using irama_test_songs::PatternText;
using irama_test_songs::SongText;

// Loads a song's parts into an EngineSong.
void load(irama::EngineSong& song, const SongText& text) {
    auto changes = irama_test_songs::song_change(text);
    if (!changes) {
        FAIL(changes.error());
    }
    song.apply(**changes);
}

[[nodiscard]] irama::SequencerPtr make_sequencer(std::uint32_t rate) {
    auto sequencer = irama::Sequencer::create(irama::default_allocator(), rate);
    REQUIRE(sequencer.has_value());
    return std::move(*sequencer);
}

// Plays the sequencer up to, not including, a frame, and returns the notes it played.
[[nodiscard]] std::vector<SeqNote> play_until(irama::Sequencer& sequencer,
                                              const irama::EngineSong& song, std::uint64_t end) {
    std::vector<SeqNote> notes;
    while (const std::optional<std::uint64_t> next = sequencer.next_frame()) {
        if (*next >= end) {
            break;
        }
        const std::span<const SeqNote> played = sequencer.advance(*next, song);
        notes.insert(notes.end(), played.begin(), played.end());
    }
    return notes;
}

[[nodiscard]] std::vector<std::uint64_t> frames_of(const std::vector<SeqNote>& notes) {
    std::vector<std::uint64_t> frames;
    frames.reserve(notes.size());
    for (const SeqNote& note : notes) {
        frames.push_back(note.frame);
    }
    return frames;
}

[[nodiscard]] std::vector<std::uint8_t> notes_of(const std::vector<SeqNote>& notes) {
    std::vector<std::uint8_t> values;
    values.reserve(notes.size());
    for (const SeqNote& note : notes) {
        values.push_back(note.note);
    }
    return values;
}

// A pattern of one column with a C-4 on each line, at the delay written for it.
[[nodiscard]] PatternText note_on_each_line(std::size_t lines, std::size_t number = 0,
                                            std::span<const std::string_view> cells = {}) {
    PatternText pattern{.number = number};
    for (const std::string_view cell : cells.first(std::min(lines, cells.size()))) {
        pattern.lines.push_back(Line{cell});
    }
    while (pattern.lines.size() < lines) {
        pattern.lines.push_back(Line{"C-4 01"});
    }
    return pattern;
}

constexpr std::array<std::uint8_t, 1> one_column{1};

// The frames of a song's first lines, written by hand at each rate.
struct RateFrames {
    std::uint32_t rate;
    std::array<std::uint64_t, 4> lines_120;
    std::array<std::uint64_t, 4> lines_123_45;
    // Delays of 80 on line 0, 40 on line 1 and FE on line 2, at 120 BPM.
    std::array<std::uint64_t, 3> delays;
    // 150 BPM from line 2: lines 2 to 4, and a delay of 80 on line 3.
    std::array<std::uint64_t, 3> bpm_150;
    std::uint64_t bpm_150_delay;
    // LPB 8 from line 2: lines 2 to 4.
    std::array<std::uint64_t, 3> lpb_8;
    // Line 2 played from frame 1,000: lines 2 to 4.
    std::array<std::uint64_t, 3> from_line_2;
};

constexpr std::array<RateFrames, 3> rate_frames{
    RateFrames{
        .rate = 44'100,
        .lines_120 = {0, 5'512, 11'025, 16'537},
        .lines_123_45 = {0, 5'358, 10'716, 16'075},
        .delays = {2'756, 6'890, 16'494},
        .bpm_150 = {11'025, 15'435, 19'845},
        .bpm_150_delay = 17'640,
        .lpb_8 = {11'025, 13'781, 16'537},
        .from_line_2 = {1'000, 6'512, 12'025},
    },
    RateFrames{
        .rate = 48'000,
        .lines_120 = {0, 6'000, 12'000, 18'000},
        .lines_123_45 = {0, 5'832, 11'664, 17'496},
        .delays = {3'000, 7'500, 17'953},
        .bpm_150 = {12'000, 16'800, 21'600},
        .bpm_150_delay = 19'200,
        .lpb_8 = {12'000, 15'000, 18'000},
        .from_line_2 = {1'000, 7'000, 13'000},
    },
    RateFrames{
        .rate = 96'000,
        .lines_120 = {0, 12'000, 24'000, 36'000},
        .lines_123_45 = {0, 11'664, 23'329, 34'993},
        .delays = {6'000, 15'000, 35'906},
        .bpm_150 = {24'000, 33'600, 43'200},
        .bpm_150_delay = 38'400,
        .lpb_8 = {24'000, 30'000, 36'000},
        .from_line_2 = {1'000, 13'000, 25'000},
    },
};

// The frame a unit plays at, by the formula: one 64-bit multiply from the anchor.
[[nodiscard]] std::uint64_t reference_frame(std::uint64_t anchor, std::uint64_t unit,
                                            std::uint64_t rate, std::uint64_t bpm100,
                                            std::uint64_t lpb) {
    return anchor + ((unit * rate * 6'000) / (bpm100 * lpb * 256));
}

// One run of the seeded sweep: a pattern of one column with a C-4 on each line, at its delay,
// played from a line at a frame.
struct SweepRun {
    std::uint32_t rate = 0;
    std::uint32_t bpm100 = 0;
    std::uint8_t lpb = 0;
    std::vector<std::uint8_t> delays;
    std::size_t start_line = 0;
    std::uint64_t anchor = 0;
};

// Draws a run, at a common rate four runs in five and at any rate the fifth.
[[nodiscard]] SweepRun draw_run(std::mt19937_64& random, int run) {
    constexpr std::array<std::uint32_t, 4> rates{44'100, 48'000, 88'200, 96'000};
    SweepRun drawn;
    drawn.rate = run % 5 == 4 ? std::uniform_int_distribution<std::uint32_t>{8'000, 192'000}(random)
                              : rates.at(static_cast<std::size_t>(run % 4));
    drawn.bpm100 = std::uniform_int_distribution<std::uint32_t>{2'000, 99'900}(random);
    drawn.lpb = static_cast<std::uint8_t>(std::uniform_int_distribution<int>{1, 64}(random));
    const auto lines = std::uniform_int_distribution<std::size_t>{1, 64}(random);
    drawn.start_line = std::uniform_int_distribution<std::size_t>{0, lines - 1}(random);
    drawn.anchor = std::uniform_int_distribution<std::uint64_t>{0, 1'000'000}(random);
    for (std::size_t line = 0; line < lines; ++line) {
        drawn.delays.push_back(
            static_cast<std::uint8_t>(std::uniform_int_distribution<int>{0, 254}(random)));
    }
    return drawn;
}

// Loads a run's song.
void load_run(irama::EngineSong& song, const SweepRun& drawn) {
    std::vector<std::string> cells;
    cells.reserve(drawn.delays.size());
    for (const std::uint8_t delay : drawn.delays) {
        cells.push_back(std::format("C-4 01 .. .. {:02X}", delay));
    }
    PatternText pattern;
    pattern.lines.reserve(cells.size());
    for (const std::string& cell : cells) {
        pattern.lines.push_back(Line{cell});
    }
    load(song, {
                   .description =
                       {
                           .bpm100 = drawn.bpm100,
                           .lpb = drawn.lpb,
                           .note_columns = one_column,
                       },
                   .patterns = {pattern},
               });
}

// Plays a run for long enough to carry the remainder over many lines, checking each line start
// and each note against the formula.
void check_run(const SweepRun& drawn) {
    irama::EngineSong song;
    load_run(song, drawn);
    const auto sequencer = make_sequencer(drawn.rate);
    sequencer->play(drawn.anchor, 0, drawn.start_line, true, song);
    constexpr std::uint64_t played_lines = 2'000;
    for (std::uint64_t n = 0; n < played_lines; ++n) {
        const std::uint8_t delay = drawn.delays.at((drawn.start_line + n) % drawn.delays.size());
        const std::uint64_t expected =
            reference_frame(drawn.anchor, (256 * n) + delay, drawn.rate, drawn.bpm100, drawn.lpb);
        const std::uint64_t line_start =
            reference_frame(drawn.anchor, 256 * n, drawn.rate, drawn.bpm100, drawn.lpb);
        REQUIRE(sequencer->next_frame() == line_start);
        std::span<const SeqNote> notes = sequencer->advance(line_start, song);
        if (notes.empty()) {
            REQUIRE(sequencer->next_frame() == expected);
            notes = sequencer->advance(expected, song);
        }
        REQUIRE(notes.size() == 1);
        REQUIRE(notes.front().frame == expected);
    }
}

} // namespace

TEST_CASE("notes start at the frames of the timing table", "[seq]") {
    const RateFrames& expected = GENERATE(from_range(rate_frames));
    CAPTURE(expected.rate);

    SECTION("at 120 BPM") {
        irama::EngineSong song;
        load(song,
             {.description = {.note_columns = one_column}, .patterns = {note_on_each_line(4)}});
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        CHECK(frames_of(play_until(*sequencer, song, expected.lines_120.back() + 1)) ==
              std::vector<std::uint64_t>(expected.lines_120.begin(), expected.lines_120.end()));
    }

    SECTION("at a fractional BPM") {
        irama::EngineSong song;
        load(song, {
                       .description = {.bpm100 = 12'345, .note_columns = one_column},
                       .patterns = {note_on_each_line(4)},
                   });
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        CHECK(
            frames_of(play_until(*sequencer, song, expected.lines_123_45.back() + 1)) ==
            std::vector<std::uint64_t>(expected.lines_123_45.begin(), expected.lines_123_45.end()));
    }

    SECTION("with delays") {
        constexpr std::array<std::string_view, 3> cells{
            "C-4 01 .. .. 80",
            "C-4 01 .. .. 40",
            "C-4 01 .. .. FE",
        };
        irama::EngineSong song;
        load(song, {
                       .description = {.note_columns = one_column},
                       .patterns = {note_on_each_line(3, 0, cells)},
                   });
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        CHECK(frames_of(play_until(*sequencer, song, expected.delays.back() + 1)) ==
              std::vector<std::uint64_t>(expected.delays.begin(), expected.delays.end()));
    }

    SECTION("with the BPM changed during a line, from the next line's start") {
        constexpr std::array<std::uint8_t, 1> two_columns{2};
        const PatternText pattern{
            .lines =
                {
                    {"C-4 01"},
                    {"C-4 01"},
                    {"C-4 01"},
                    {"C-4 01", "D-4 01 .. .. 80"},
                    {"C-4 01"},
                },
        };
        irama::EngineSong song;
        load(song, {.description = {.note_columns = two_columns}, .patterns = {pattern}});
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        static_cast<void>(play_until(*sequencer, song, expected.lines_120.at(1) + 1));
        load(song, {
                       .description = {.bpm100 = 15'000, .note_columns = two_columns},
                       .patterns = {pattern},
                   });
        const std::vector<SeqNote> notes =
            play_until(*sequencer, song, expected.bpm_150.back() + 1);
        CHECK(frames_of(notes) ==
              std::vector<std::uint64_t>{expected.bpm_150.at(0), expected.bpm_150.at(1),
                                         expected.bpm_150_delay, expected.bpm_150.at(2)});
    }

    SECTION("with the LPB changed during a line, from the next line's start") {
        const PatternText pattern = note_on_each_line(5);
        irama::EngineSong song;
        load(song, {.description = {.note_columns = one_column}, .patterns = {pattern}});
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        static_cast<void>(play_until(*sequencer, song, expected.lines_120.at(1) + 1));
        load(song, {.description = {.lpb = 8, .note_columns = one_column}, .patterns = {pattern}});
        CHECK(frames_of(play_until(*sequencer, song, expected.lpb_8.back() + 1)) ==
              std::vector<std::uint64_t>(expected.lpb_8.begin(), expected.lpb_8.end()));
    }

    SECTION("played from a line in the middle of the pattern") {
        irama::EngineSong song;
        load(song,
             {.description = {.note_columns = one_column}, .patterns = {note_on_each_line(8)}});
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(1'000, 0, 2, false, song);
        CHECK(sequencer->next_frame() == 1'000);
        CHECK(frames_of(play_until(*sequencer, song, expected.from_line_2.back() + 1)) ==
              std::vector<std::uint64_t>(expected.from_line_2.begin(), expected.from_line_2.end()));
    }

    SECTION("looping a pattern") {
        constexpr std::array<std::string_view, 2> cells{"C-4 01", "D-4 01"};
        irama::EngineSong song;
        load(song, {
                       .description = {.note_columns = one_column},
                       .patterns = {note_on_each_line(2, 0, cells)},
                   });
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, true, song);
        const std::vector<SeqNote> notes =
            play_until(*sequencer, song, expected.lines_120.back() + 1);
        CHECK(frames_of(notes) ==
              std::vector<std::uint64_t>(expected.lines_120.begin(), expected.lines_120.end()));
        CHECK(notes_of(notes) == std::vector<std::uint8_t>{48, 50, 48, 50});
    }

    SECTION("wrapping at the end of the sequence") {
        constexpr std::array<std::uint16_t, 2> sequence{0, 1};
        constexpr std::array<std::string_view, 2> cells{"C-4 01", "D-4 01"};
        constexpr std::array<std::string_view, 1> last{"E-4 01"};
        irama::EngineSong song;
        load(song, {
                       .description = {.note_columns = one_column, .sequence = sequence},
                       .patterns = {note_on_each_line(2, 0, cells), note_on_each_line(1, 1, last)},
                   });
        const auto sequencer = make_sequencer(expected.rate);
        sequencer->play(0, 0, 0, false, song);
        const std::vector<SeqNote> notes =
            play_until(*sequencer, song, expected.lines_120.back() + 1);
        CHECK(frames_of(notes) ==
              std::vector<std::uint64_t>(expected.lines_120.begin(), expected.lines_120.end()));
        CHECK(notes_of(notes) == std::vector<std::uint8_t>{48, 50, 52, 48});
    }
}

TEST_CASE("a seeded sweep of BPM, LPB, delay and rate plays at the formula's frames", "[seq]") {
    // NOLINTNEXTLINE(bugprone-random-generator-seed): every run of the sweep is the same.
    std::mt19937_64 random{20'261'128};
    for (int run = 0; run < 200; ++run) {
        const SweepRun drawn = draw_run(random, run);
        CAPTURE(run, drawn.rate, drawn.bpm100, drawn.lpb, drawn.delays.size(), drawn.start_line,
                drawn.anchor);
        check_run(drawn);
    }
}

TEST_CASE("a line's cells become notes by column, with their velocity and pan", "[seq]") {
    constexpr std::array<std::uint8_t, 2> note_columns{2, 1};
    irama::EngineSong song;
    load(song, {
                   .description = {.note_columns = note_columns},
                   .patterns =
                       {
                           {
                               .lines =
                                   {
                                       {"C-4 01 40 00", "OFF", "D#5 FE .. 80 00"},
                                       {"... 02 40 40", "E-4 .. 80 40", ""},
                                   },
                           },
                       },
               });
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    const std::vector<SeqNote> notes = play_until(*sequencer, song, 6'001);
    const std::vector<SeqNote> expected{
        {
            .frame = 0,
            .track = 0,
            .column = 0,
            .note = 48,
            .instrument = 0x01,
            .velocity = 0.5F,
            .fixed_pan = true,
            .pan = -1.0F,
        },
        {.frame = 0, .track = 0, .column = 1, .note = NoteCell::off, .instrument = NoteCell::empty},
        {
            .frame = 0,
            .track = 1,
            .column = 0,
            .note = 63,
            .instrument = 0xFE,
            .velocity = 1.0F,
            .fixed_pan = true,
            .pan = 1.0F,
        },
        {
            .frame = 6'000,
            .track = 0,
            .column = 1,
            .note = 52,
            .instrument = NoteCell::empty,
            .velocity = 1.0F,
            .fixed_pan = true,
            .pan = 0.0F,
        },
    };
    CHECK(notes == expected);
}

TEST_CASE("notes of a line play in the order of their frames, then of their columns", "[seq]") {
    irama::EngineSong song;
    load(song, {
                   .patterns =
                       {
                           {
                               .lines =
                                   {
                                       {
                                           "C-4 01 .. .. 80",
                                           "D-4 01 .. .. 10",
                                           "E-4 01",
                                           "F-4 01 .. .. 10",
                                       },
                                   },
                           },
                       },
               });
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    const std::vector<SeqNote> notes = play_until(*sequencer, song, 6'000);
    CHECK(notes_of(notes) == std::vector<std::uint8_t>{52, 50, 53, 48});
    CHECK(frames_of(notes) == std::vector<std::uint64_t>{0, 375, 375, 3'000});
}

TEST_CASE("each line is read once, at its start", "[seq]") {
    const SongText before{
        .description = {.note_columns = one_column},
        .patterns = {{.lines = {{"C-4 01 .. .. 80"}, {"D-4 01"}}}},
    };
    const SongText after{
        .description = {.note_columns = one_column},
        .patterns = {{.lines = {{"G-4 01 .. .. 80"}, {"A-4 01"}}}},
    };
    irama::EngineSong song;
    load(song, before);
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    std::vector<SeqNote> notes = play_until(*sequencer, song, 1);
    CHECK(notes.empty());
    // The edit lands after line 0 started, before line 1 does.
    load(song, after);
    notes = play_until(*sequencer, song, 6'001);
    CHECK(notes_of(notes) == std::vector<std::uint8_t>{48, 57});
}

TEST_CASE("a slot whose pattern number is empty plays as one empty line", "[seq]") {
    constexpr std::array<std::uint16_t, 3> sequence{0, 5, 0};
    irama::EngineSong song;
    load(song, {
                   .description = {.note_columns = one_column, .sequence = sequence},
                   .patterns = {note_on_each_line(1)},
               });
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    CHECK(frames_of(play_until(*sequencer, song, 30'001)) ==
          std::vector<std::uint64_t>{0, 12'000, 18'000, 30'000});
}

TEST_CASE("a sequence of empty slots wraps a line at a time", "[seq]") {
    constexpr std::array<std::uint16_t, 2> sequence{7, 8};
    irama::EngineSong song;
    load(song, {.description = {.note_columns = one_column, .sequence = sequence}});
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    CHECK(play_until(*sequencer, song, 30'000).empty());
    CHECK(sequencer->next_frame() == 30'000);
    CHECK(sequencer->position() == PlayPosition{.playing = true, .slot = 0, .line = 0});
}

TEST_CASE("a play past its pattern's end or its sequence's end starts the next slot", "[seq]") {
    constexpr std::array<std::uint16_t, 2> sequence{0, 1};
    constexpr std::array<std::string_view, 1> second{"E-4 01"};
    irama::EngineSong song;
    load(song, {
                   .description = {.note_columns = one_column, .sequence = sequence},
                   .patterns = {note_on_each_line(4), note_on_each_line(1, 1, second)},
               });
    const auto sequencer = make_sequencer(48'000);

    SECTION("a line past the pattern's end ends its slot at once") {
        sequencer->play(500, 0, 10, false, song);
        const std::vector<SeqNote> notes = play_until(*sequencer, song, 501);
        CHECK(notes_of(notes) == std::vector<std::uint8_t>{52});
        CHECK(sequencer->position() == PlayPosition{.playing = true, .slot = 1, .line = 0});
    }

    SECTION("a line past the pattern's end of a looping slot goes back to its line 0") {
        sequencer->play(500, 1, 10, true, song);
        static_cast<void>(play_until(*sequencer, song, 501));
        CHECK(sequencer->position() ==
              PlayPosition{.playing = true, .loop = true, .slot = 1, .line = 0});
    }

    SECTION("a slot past the sequence's end goes to slot 0") {
        sequencer->play(500, 7, 3, false, song);
        static_cast<void>(play_until(*sequencer, song, 501));
        CHECK(sequencer->position() == PlayPosition{.playing = true, .slot = 0, .line = 0});
    }
}

TEST_CASE("the play position follows each line started", "[seq]") {
    constexpr std::array<std::uint16_t, 2> sequence{0, 0};
    irama::EngineSong song;
    load(song, {
                   .description = {.note_columns = one_column, .sequence = sequence},
                   .patterns = {note_on_each_line(2)},
               });
    const auto sequencer = make_sequencer(48'000);
    CHECK(sequencer->position() == PlayPosition{});
    CHECK_FALSE(sequencer->next_frame().has_value());

    sequencer->play(0, 0, 1, false, song);
    static_cast<void>(play_until(*sequencer, song, 1));
    CHECK(sequencer->position() == PlayPosition{.playing = true, .slot = 0, .line = 1});
    static_cast<void>(play_until(*sequencer, song, 6'001));
    CHECK(sequencer->position() == PlayPosition{.playing = true, .slot = 1, .line = 0});

    sequencer->set_loop(true);
    static_cast<void>(play_until(*sequencer, song, 18'001));
    CHECK(sequencer->position() ==
          PlayPosition{.playing = true, .loop = true, .slot = 1, .line = 0});

    sequencer->stop();
    CHECK_FALSE(sequencer->next_frame().has_value());
    CHECK(sequencer->position() == PlayPosition{.loop = true, .slot = 1, .line = 0});
    sequencer->set_loop(false);
    CHECK(sequencer->position().loop);
}

TEST_CASE("a stop drops the notes still waiting, and a play starts afresh", "[seq]") {
    irama::EngineSong song;
    load(song, {
                   .description = {.note_columns = one_column},
                   .patterns = {{.lines = {{"C-4 01 .. .. 80"}, {"D-4 01"}}}},
               });
    const auto sequencer = make_sequencer(48'000);
    sequencer->play(0, 0, 0, false, song);
    static_cast<void>(play_until(*sequencer, song, 1));
    sequencer->stop();
    CHECK(play_until(*sequencer, song, 100'000).empty());

    sequencer->play(2'000, 0, 1, false, song);
    sequencer->play(4'000, 0, 0, false, song);
    CHECK(frames_of(play_until(*sequencer, song, 10'001)) ==
          std::vector<std::uint64_t>{7'000, 10'000});
}

TEST_CASE("a play position packs into one value and back", "[seq]") {
    const PlayPosition position =
        GENERATE(PlayPosition{}, PlayPosition{.playing = true, .slot = 999, .line = 511},
                 PlayPosition{.loop = true, .slot = 1, .line = 0},
                 PlayPosition{.playing = true, .loop = true, .slot = 12, .line = 34});
    CHECK(PlayPosition::unpack(position.pack()) == position);
}
