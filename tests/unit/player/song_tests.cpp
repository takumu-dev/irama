// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/player/player.hpp>
#include <irama/engine/seq/sequencer.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/test_songs/test_songs.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using irama::InstrumentParam;
using irama::PlayPosition;
using irama_test_songs::SongText;

// The default release at 48 kHz.
constexpr std::size_t release_frames = 9'600;

// A cut voice's fade at 48 kHz.
constexpr std::size_t fade_frames = 240;

// A line at 120 BPM and LPB 4, at 48 kHz.
constexpr std::uint64_t line_frames = 6'000;

// Values that differ from the instrument's defaults, by ID.
using Changed = std::vector<std::pair<irama::ParamId, float>>;

// An instrument's values: every default but the values changed.
[[nodiscard]] std::vector<float> values(const Changed& changed = {}) {
    std::vector<float> result(irama::instrument_params.size());
    std::ranges::transform(irama::instrument_params, result.begin(),
                           &irama::ParamDescriptor::default_value);
    for (const auto& [id, value] : changed) {
        const std::size_t index = irama::find_param(irama::instrument_params, id)
                                      .value_or(irama::instrument_params.size());
        result.at(index) = irama::instrument_params.at(index).constrain(value);
    }
    return result;
}

// Values whose new note action is one of the three.
[[nodiscard]] std::vector<float> with_action(irama::NewNoteAction action) {
    return values({{InstrumentParam::new_note_action, static_cast<float>(action)}});
}

[[nodiscard]] irama::PlayerPtr make_player(std::uint32_t rate = 48'000) {
    auto player = irama::Player::create({.sample_rate = rate});
    REQUIRE(player.has_value());
    return std::move(*player);
}

// Sends a song to the player at frame 0, and drops the change set that comes back.
void load(irama::Player& player, const SongText& song) {
    auto changes = irama_test_songs::song_change(song);
    if (!changes) {
        FAIL(changes.error());
    }
    REQUIRE(player.change(0, *changes));
}

[[nodiscard]] std::vector<float> render(irama::Player& player, std::size_t frames) {
    std::vector<float> out(frames * 2);
    player.render(out);
    while (player.take_returned() != nullptr) {
    }
    return out;
}

// Renders up to, not including, a frame, from the frame rendered next.
void render_until(irama::Player& player, std::uint64_t& rendered, std::uint64_t frame) {
    static_cast<void>(render(player, static_cast<std::size_t>(frame - rendered)));
    rendered = frame;
}

// The frame a unit plays at, by the formula: one 64-bit multiply from the anchor.
[[nodiscard]] std::uint64_t reference_frame(std::uint64_t anchor, std::uint64_t unit,
                                            std::uint64_t rate, std::uint64_t bpm100,
                                            std::uint64_t lpb) {
    return anchor + ((unit * rate * 6'000) / (bpm100 * lpb * 256));
}

constexpr std::array<std::uint8_t, 1> one_column{1};

// A song of one column whose note on line 0 is followed by the cell on line 1, with instruments
// 01 and 02 of their own new note actions.
[[nodiscard]] SongText two_notes(std::string_view second, irama::NewNoteAction first_action,
                                 irama::NewNoteAction second_action = irama::NewNoteAction::cut) {
    return {
        .description = {.note_columns = one_column},
        .patterns = {{.lines = {{"C-4 01"}, {second}}, .length = 64}},
        .instruments =
            {
                {.number = 0x01, .values = with_action(first_action)},
                {.number = 0x02, .values = with_action(second_action)},
            },
    };
}

[[nodiscard]] bool silent(std::span<const float> samples) {
    return std::ranges::all_of(samples, [](float sample) { return sample == 0.0F; });
}

// A note-on, or a note-off, sent as a control message at a delay into a line.
struct Sent {
    std::uint64_t line = 0;
    std::uint64_t delay = 0;
    bool on = false;
    std::uint8_t note = 0;
    float velocity = 0.0F;
    std::uint8_t instrument = 0;
};

[[nodiscard]] constexpr Sent on(std::uint64_t line, std::uint64_t delay, std::uint8_t note,
                                float velocity, std::uint8_t instrument) {
    return {
        .line = line,
        .delay = delay,
        .on = true,
        .note = note,
        .velocity = velocity,
        .instrument = instrument,
    };
}

[[nodiscard]] constexpr Sent off(std::uint64_t line, std::uint64_t delay, std::uint8_t note,
                                 std::uint8_t instrument) {
    return {.line = line, .delay = delay, .note = note, .instrument = instrument};
}

// Sends a note at its frame.
[[nodiscard]] bool send(irama::Player& player, const Sent& sent, std::uint64_t frame) {
    return sent.on ? player.note_on(frame, sent.note, sent.velocity, sent.instrument)
                   : player.note_off(frame, sent.note, sent.instrument);
}

// Plays slot 0 from a frame, with a note jammed over it 1,000 frames in or not, stops it
// 16,384 frames in, and renders long enough a silence after for every tail to end.
[[nodiscard]] std::vector<float> play_from_start(irama::Player& player, std::uint64_t frame,
                                                 bool jam) {
    constexpr std::size_t length = 16'384;
    REQUIRE(player.play(frame, 0, 0, false));
    if (jam) {
        REQUIRE(player.note_on(frame + 1'000, 57, 1.0F, 1));
    }
    REQUIRE(player.stop(frame + length));
    std::vector<float> out = render(player, length);
    static_cast<void>(render(player, 3 * length));
    return out;
}

} // namespace

TEST_CASE("a song renders bit-identically to its notes sent as control messages", "[player]") {
    // Two patterns of two tracks, the first of two note columns, at a BPM whose lines aren't whole
    // frames, with delays, volumes, OFFs, a note that releases the note before it, and the wrap.
    constexpr std::uint32_t rate = 44'100;
    constexpr std::uint32_t bpm100 = 12'345;
    constexpr std::uint8_t lpb = 4;
    constexpr std::array<std::uint8_t, 2> note_columns{2, 1};
    constexpr std::array<std::uint16_t, 2> sequence{3, 7};
    const SongText song{
        .description =
            {
                .bpm100 = bpm100,
                .lpb = lpb,
                .note_columns = note_columns,
                .sequence = sequence,
            },
        .patterns =
            {
                {
                    .number = 3,
                    .lines =
                        {
                            {"C-4 01 .. .. 00", "E-4 02 40", "G-3 01 .. .. 80"},
                            {"", "OFF .. .. .. 20", ""},
                            {"D-4 01 60 .. 10", "", "OFF"},
                            {"OFF", "A-4 02 .. .. FE", "B-3 02 10"},
                        },
                },
                {
                    .number = 7,
                    .lines = {{"F-4 02 .. .. 40", "", ""}, {"OFF", "OFF", "OFF .. .. .. C0"}},
                },
            },
        .instruments =
            {
                {.number = 0x01, .values = values()},
                {
                    .number = 0x02,
                    .values = values({
                        {InstrumentParam::osc1_waveform, 3.0F},
                        {InstrumentParam::release, 0.05F},
                    }),
                },
            },
    };
    const auto frame = [](std::uint64_t line, std::uint64_t delay) {
        return reference_frame(0, (256 * line) + delay, rate, bpm100, lpb);
    };

    const auto played = make_player(rate);
    load(*played, song);
    REQUIRE(played->play(0, 0, 0, false));

    const auto sent = make_player(rate);
    load(*sent, song);
    // Lines 0 to 3 of pattern 3, then lines 4 and 5 of pattern 7. The new note on line 2 releases
    // the column's note before it, as its instrument's note-off says.
    constexpr std::array<Sent, 14> notes{
        on(0, 0x00, 48, 1.0F, 1),  on(0, 0x00, 52, 0.5F, 2), on(0, 0x80, 43, 1.0F, 1),
        off(1, 0x20, 52, 2),       off(2, 0x00, 43, 1),      off(2, 0x10, 48, 1),
        on(2, 0x10, 50, 0.75F, 1), off(3, 0x00, 50, 1),      on(3, 0x00, 47, 0.125F, 2),
        on(3, 0xFE, 57, 1.0F, 2),  on(4, 0x40, 53, 1.0F, 2), off(5, 0x00, 53, 2),
        off(5, 0x00, 57, 2),       off(5, 0xC0, 47, 2),
    };
    for (const Sent& note : notes) {
        REQUIRE(send(*sent, note, frame(note.line, note.delay)));
    }
    // Then pattern 3 again from line 6.
    for (const Sent& note : std::span{notes}.first(10)) {
        REQUIRE(send(*sent, note, frame(6 + note.line, note.delay)));
    }

    const auto frames = static_cast<std::size_t>(frame(9, 0) + 1'000);
    const std::vector<float> expected = render(*sent, frames);
    CHECK_FALSE(silent(expected));
    CHECK(render(*played, frames) == expected);
}

TEST_CASE("a song renders alike however its calls are split", "[player]") {
    const SongText song{
        .patterns =
            {
                {
                    .lines =
                        {
                            {"C-4 01 .. .. 33", "E-4 01 20", "", "G-4 01 .. 10 F0"},
                            {"OFF", "", "B-4 01"},
                            {"D-4 01 .. .. 07"},
                        },
                },
            },
        .instruments = {{.number = 0x01, .values = values()}},
    };
    const auto whole = make_player();
    load(*whole, song);
    REQUIRE(whole->play(100, 0, 0, true));
    const std::vector<float> expected = render(*whole, 13'000);
    for (const std::size_t length : {1U, 7U, 255U, 256U, 4'096U}) {
        const auto split = make_player();
        load(*split, song);
        REQUIRE(split->play(100, 0, 0, true));
        std::vector<float> out(expected.size());
        for (std::span<float> rest{out}; !rest.empty();) {
            const std::size_t samples = std::min(length * 2, rest.size());
            split->render(rest.first(samples));
            rest = rest.subspan(samples);
        }
        CHECK(out == expected);
    }
}

TEST_CASE("a new note gives each held voice its column owns its own instrument's new note action",
          "[player]") {
    std::uint64_t rendered = 0;

    SECTION("note-off releases it") {
        const auto player = make_player();
        load(*player, two_notes("D-4 01", irama::NewNoteAction::note_off));
        REQUIRE(player->play(0, 0, 0, false));
        render_until(*player, rendered, line_frames + fade_frames);
        CHECK(player->active_voices() == 2);
        render_until(*player, rendered, line_frames + release_frames);
        CHECK(player->active_voices() == 1);
    }

    SECTION("cut fades it out over 5 ms") {
        const auto player = make_player();
        load(*player, two_notes("D-4 01", irama::NewNoteAction::cut));
        REQUIRE(player->play(0, 0, 0, false));
        render_until(*player, rendered, line_frames + fade_frames - 1);
        CHECK(player->active_voices() == 2);
        render_until(*player, rendered, line_frames + fade_frames);
        CHECK(player->active_voices() == 1);
    }

    SECTION("continue leaves it sounding, still owned by the column") {
        const auto player = make_player();
        load(*player, {
                          .description = {.note_columns = one_column},
                          .patterns = {{.lines = {{"C-4 01"}, {"D-4 01"}, {"OFF"}}, .length = 64}},
                          .instruments =
                              {
                                  {
                                      .number = 0x01,
                                      .values = with_action(irama::NewNoteAction::carry_on),
                                  },
                              },
                      });
        REQUIRE(player->play(0, 0, 0, false));
        render_until(*player, rendered, (2 * line_frames) - 1);
        CHECK(player->active_voices() == 2);
        // The OFF releases both.
        render_until(*player, rendered, (2 * line_frames) + release_frames);
        CHECK(player->active_voices() == 0);
    }

    SECTION("the action is the held voice's instrument's, not the new note's") {
        const auto player = make_player();
        load(*player,
             two_notes("D-4 02", irama::NewNoteAction::cut, irama::NewNoteAction::carry_on));
        REQUIRE(player->play(0, 0, 0, false));
        render_until(*player, rendered, line_frames + fade_frames);
        CHECK(player->active_voices() == 1);
    }
}

TEST_CASE("a note that plays nothing still applies the new note action", "[player]") {
    // An empty instrument field, and an instrument number with nothing in it.
    const std::string_view second = GENERATE("D-4", "D-4 09");
    const auto player = make_player();
    load(*player, two_notes(second, irama::NewNoteAction::cut));
    REQUIRE(player->play(0, 0, 0, false));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, line_frames - 1);
    CHECK(player->active_voices() == 1);
    render_until(*player, rendered, line_frames + fade_frames);
    CHECK(player->active_voices() == 0);
}

TEST_CASE("the note keys' voices are untouched by the song's new note actions and OFFs",
          "[player]") {
    const auto player = make_player();
    load(*player,
         {
             .description = {.note_columns = one_column},
             .patterns = {{.lines = {{"C-4 01"}, {"C-4 01"}, {"OFF"}}, .length = 64}},
             .instruments = {{.number = 0x01, .values = with_action(irama::NewNoteAction::cut)}},
         });
    REQUIRE(player->note_on(0, 48, 1.0F, 1));
    REQUIRE(player->play(0, 0, 0, false));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, (2 * line_frames) + release_frames);
    CHECK(player->active_voices() == 1);
}

TEST_CASE("note-off and release-all messages reach only the note keys' voices", "[player]") {
    const auto player = make_player();
    load(*player, two_notes("", irama::NewNoteAction::note_off));
    REQUIRE(player->play(0, 0, 0, false));
    REQUIRE(player->note_on(0, 50, 1.0F, 1));
    REQUIRE(player->note_off(100, 48, 1));
    REQUIRE(player->release_all(200));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, 200 + release_frames);
    CHECK(player->active_voices() == 1);
}

TEST_CASE("a stop releases the song's voices, and a second stop fades out every voice over 5 ms",
          "[player]") {
    const auto player = make_player();
    load(*player, two_notes("", irama::NewNoteAction::note_off));
    REQUIRE(player->note_on(0, 50, 1.0F, 1));
    REQUIRE(player->play(0, 0, 0, false));
    REQUIRE(player->stop(1'000));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, 1'000 + release_frames);
    CHECK(player->active_voices() == 1);
    CHECK_FALSE(player->play_position().playing);

    REQUIRE(player->stop(20'000));
    render_until(*player, rendered, 20'000 + fade_frames - 1);
    CHECK(player->active_voices() == 1);
    render_until(*player, rendered, 20'000 + fade_frames);
    CHECK(player->active_voices() == 0);
}

TEST_CASE("a play while playing releases the song's voices and starts afresh", "[player]") {
    const auto player = make_player();
    load(*player, two_notes("", irama::NewNoteAction::carry_on));
    REQUIRE(player->play(0, 0, 0, false));
    REQUIRE(player->play(1'000, 0, 5, false));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, 1'001);
    CHECK(player->play_position() == PlayPosition{.playing = true, .slot = 0, .line = 5});
    render_until(*player, rendered, 1'000 + release_frames);
    CHECK(player->active_voices() == 0);
}

TEST_CASE("the play position is read back after each render call", "[player]") {
    const auto player = make_player();
    load(*player, {
                      .patterns = {{.lines = {{"C-4 01"}}, .length = 4}},
                      .instruments = {{.number = 0x01, .values = values()}},
                  });
    CHECK(player->play_position() == PlayPosition{});
    REQUIRE(player->play(0, 0, 2, false));
    REQUIRE(player->set_loop(line_frames, true));
    std::uint64_t rendered = 0;
    render_until(*player, rendered, 1);
    CHECK(player->play_position() == PlayPosition{.playing = true, .slot = 0, .line = 2});
    render_until(*player, rendered, (2 * line_frames) + 1);
    CHECK(player->play_position() ==
          PlayPosition{.playing = true, .loop = true, .slot = 0, .line = 0});
    REQUIRE(player->stop(rendered));
    render_until(*player, rendered, rendered + 1);
    CHECK(player->play_position() == PlayPosition{.loop = true, .slot = 0, .line = 0});
}

TEST_CASE("a song using noise renders alike at every play from slot 0, and differs with a note "
          "jammed over it",
          "[player]") {
    const auto player = make_player();
    load(*player,
         {
             .patterns =
                 {
                     {
                         .lines = {{"C-4 01", "E-4 01 .. .. 40"}, {"OFF", "OFF"}, {"G-4 01"}},
                         .length = 4,
                     },
                 },
             .instruments =
                 {
                     {
                         .number = 0x01,
                         .values = values({
                             {InstrumentParam::osc1_level, 0.0F},
                             {InstrumentParam::noise_level, 100.0F},
                         }),
                     },
                 },
         });
    const std::vector<float> first = play_from_start(*player, 0, false);
    const std::vector<float> second = play_from_start(*player, 65'536, false);
    const std::vector<float> jammed = play_from_start(*player, 131'072, true);
    CHECK_FALSE(silent(first));
    CHECK(second == first);
    CHECK(jammed != first);
}

TEST_CASE("a written pan holds the voice there, whatever the instrument's pan", "[player]") {
    const auto player = make_player();
    load(*player,
         {
             .description = {.note_columns = one_column},
             .patterns = {{.lines = {{"C-4 01 .. 00"}}, .length = 4}},
             .instruments = {{.number = 0x01, .values = values({{InstrumentParam::pan, 1.0F}})}},
         });
    REQUIRE(player->play(0, 0, 0, false));
    const std::vector<float> out = render(*player, 4'000);
    const auto left = out | std::views::stride(2);
    const auto right = out | std::views::drop(1) | std::views::stride(2);
    CHECK(std::ranges::any_of(left, [](float sample) { return sample != 0.0F; }));
    CHECK(std::ranges::all_of(right, [](float sample) { return sample == 0.0F; }));
}

TEST_CASE("a volume plays as the note's velocity", "[player]") {
    const SongText song{
        .description = {.note_columns = one_column},
        .patterns = {{.lines = {{"C-4 01 40 .. 20"}}, .length = 4}},
        .instruments = {{.number = 0x01, .values = values()}},
    };
    const auto played = make_player();
    load(*played, song);
    REQUIRE(played->play(0, 0, 0, false));
    const auto sent = make_player();
    load(*sent, song);
    REQUIRE(sent->note_on(750, 48, 0.5F, 1));
    CHECK(render(*played, 8'000) == render(*sent, 8'000));
}
