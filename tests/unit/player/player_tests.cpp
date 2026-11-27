// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

constexpr std::uint32_t rate = 48'000;

// The default release at 48 kHz.
constexpr std::size_t release_frames = 9'600;

// A stolen voice's fade at 48 kHz.
constexpr std::size_t fade_frames = 240;

[[nodiscard]] irama::PlayerPtr make_player(std::size_t max_voices = 32) {
    auto player = irama::Player::create({.sample_rate = rate, .max_voices = max_voices});
    REQUIRE(player.has_value());
    return std::move(*player);
}

// Renders the player's next frames, interleaved stereo.
[[nodiscard]] std::vector<float> render(irama::Player& player, std::size_t frames) {
    std::vector<float> out(frames * 2);
    player.render(out);
    return out;
}

// Renders the player's next frames in calls of the lengths given, one after another, repeating
// them until all the frames are rendered.
[[nodiscard]] std::vector<float> render_split(irama::Player& player, std::size_t frames,
                                              const std::vector<std::size_t>& lengths) {
    std::vector<float> out(frames * 2);
    std::span<float> rest{out};
    for (std::size_t call = 0; !rest.empty(); ++call) {
        const std::size_t length = std::min(lengths.at(call % lengths.size()) * 2, rest.size());
        player.render(rest.first(length));
        rest = rest.subspan(length);
    }
    return out;
}

// A change of the gain while notes sound, and of the release before they are released.
void send_changes(irama::Player& player) {
    REQUIRE(player.set_parameter(9'000, irama::InstrumentParam::gain, -6.0F));
    REQUIRE(player.set_parameter(9'500, irama::InstrumentParam::release, 0.5F));
}

// Chords that overlap, with a note-off, notes sharing a frame, parameter changes and a release-all,
// to be sent to players that render them in different ways.
void send_chords(irama::Player& player) {
    REQUIRE(player.note_on(0, 48, 0.8F));
    REQUIRE(player.note_on(1'000, 52, 0.7F));
    REQUIRE(player.note_on(1'000, 55, 0.6F));
    REQUIRE(player.note_off(5'000, 52));
    REQUIRE(player.note_on(7'777, 60, 1.0F));
    REQUIRE(player.note_on(7'777, 64, 0.9F));
    send_changes(player);
    REQUIRE(player.release_all(12'000));
}

// Values other than the defaults for the gain, the pan and the sustain level.
void send_values(irama::Player& player, std::uint64_t frame) {
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::gain, 0.0F));
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::pan, 0.75F));
    REQUIRE(player.set_parameter(frame, irama::InstrumentParam::sustain, 40.0F));
}

// A player of a seed playing two notes of noise alone, overlapping.
[[nodiscard]] irama::PlayerPtr make_noise_player(std::uint64_t seed) {
    auto player = irama::Player::create({.sample_rate = rate, .seed = seed});
    REQUIRE(player.has_value());
    REQUIRE((*player)->set_parameter(0, irama::InstrumentParam::osc1_level, 0.0F));
    REQUIRE((*player)->set_parameter(0, irama::InstrumentParam::noise_level, 100.0F));
    REQUIRE((*player)->note_on(0, 57, 1.0F));
    REQUIRE((*player)->note_on(500, 60, 1.0F));
    return std::move(*player);
}

// A player of noise alone, panned hard left, starting `notes` notes of A-4 at frame 0.
[[nodiscard]] irama::PlayerPtr make_noise_chord(int notes) {
    auto player = make_player();
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::osc1_level, 0.0F));
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::noise_level, 100.0F));
    REQUIRE(player->set_parameter(0, irama::InstrumentParam::pan, -1.0F));
    for (int note = 0; note < notes; ++note) {
        REQUIRE(player->note_on(0, 57, 1.0F));
    }
    return player;
}

// The frames of one note played alone at a frame, by a player of one voice at the instrument's
// defaults, whose noise is silent.
[[nodiscard]] std::vector<float> note_alone(std::uint64_t frame, int note, float velocity,
                                            std::size_t frames) {
    const auto player = make_player(1);
    REQUIRE(player->note_on(frame, note, velocity));
    return render(*player, frames);
}

// More notes than a player of 32 voices has voices and fade slots, so notes steal and fades are
// cut.
void send_crowd(irama::Player& player) {
    for (std::uint64_t note = 0; note < 64; ++note) {
        REQUIRE(player.note_on(note * 10, 57, 1.0F));
    }
}

// The gain of a stolen note's fade at a frame: 1 until the frame it is stolen, then
// `1 − (i + 1) / n` on frame `i` of the fade, and 0 from `end`, where the fade ends or is cut.
[[nodiscard]] double fade_gain(std::uint64_t frame, std::uint64_t stolen, std::uint64_t end) {
    if (frame < stolen) {
        return 1.0;
    }
    if (frame >= end) {
        return 0.0;
    }
    return 1.0 - (static_cast<double>(frame - stolen + 1) / static_cast<double>(fade_frames));
}

// Adds a note's frames into `sum`, faded as `fade_gain()` says.
void add_faded(std::vector<double>& sum, std::span<const float> note, std::uint64_t stolen,
               std::uint64_t end) {
    for (const auto [index, total, sample] :
         std::views::zip(std::views::iota(std::size_t{0}), sum, note)) {
        const std::uint64_t frame = index / 2;
        total += fade_gain(frame, stolen, end) * static_cast<double>(sample);
    }
}

// Requires each sample to be within float rounding of the one expected.
void require_near(std::span<const float> out, std::span<const double> expected) {
    REQUIRE(out.size() == expected.size());
    for (const auto [sample, wanted] : std::views::zip(out, expected)) {
        REQUIRE_THAT(sample, WithinAbs(wanted, 1e-6));
    }
}

[[nodiscard]] bool silent(std::span<const float> samples) {
    return std::ranges::all_of(samples, [](float sample) { return sample == 0.0F; });
}

} // namespace

TEST_CASE("a player renders the same messages alike however its calls are split", "[player]") {
    constexpr std::size_t frames = 24'000;
    const auto whole = make_player();
    send_chords(*whole);
    const auto once = render(*whole, frames);
    REQUIRE_FALSE(silent(once));

    const auto blocks = make_player();
    send_chords(*blocks);
    REQUIRE(render_split(*blocks, frames, {256}) == once);

    const auto ragged = make_player();
    send_chords(*ragged);
    REQUIRE(render_split(*ragged, frames, {1, 7, 0, 255, 257, 1'000, 4'096}) == once);
}

TEST_CASE("a player that steals renders alike however its calls are split", "[player]") {
    // Two voices for five notes, so notes steal and their fades cross the calls.
    constexpr std::size_t frames = 24'000;
    const auto whole = make_player(2);
    send_chords(*whole);
    const auto once = render(*whole, frames);
    const auto ragged = make_player(2);
    send_chords(*ragged);
    REQUIRE(render_split(*ragged, frames, {1, 7, 0, 255, 257, 1'000, 4'096}) == once);
}

TEST_CASE("a player frees every block it allocated, with its size and alignment", "[player]") {
    irama::test::CountingAllocator counting;
    {
        auto player = irama::Player::create(
            {.sample_rate = rate, .max_voices = 32, .allocator = counting.allocator()});
        REQUIRE(player.has_value());
        REQUIRE(counting.allocations() > 0);
        const std::size_t allocated = counting.allocations();
        send_crowd(**player);
        static_cast<void>(render(**player, 1'000));
        REQUIRE((*player)->active_voices() == 32);
        REQUIRE(counting.allocations() == allocated);
    }
    REQUIRE(counting.frees() == counting.allocations());
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a player whose allocator runs out of memory isn't created, and leaks nothing",
          "[player]") {
    std::size_t failures = 0;
    for (std::size_t failing = 1;; ++failing) {
        irama::test::CountingAllocator counting;
        counting.fail_at(failing);
        const auto player = irama::Player::create(
            {.sample_rate = rate, .max_voices = 32, .allocator = counting.allocator()});
        if (player.has_value()) {
            break;
        }
        REQUIRE(player.error() == irama::CreateError::out_of_memory);
        REQUIRE(counting.mismatched_frees() == 0);
        REQUIRE(counting.live().empty());
        ++failures;
    }
    REQUIRE(failures > 0);
}

TEST_CASE("a player applies a message exactly at its frame", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(100, 57, 1.0F));
    const auto out = render(*player, 300);
    // The note's sine starts at phase 0, so its first frame is silent too.
    constexpr std::size_t first_sound = std::size_t{101} * 2;
    REQUIRE(silent(std::span{out}.first(first_sound)));
    REQUIRE(out.at(first_sound) != 0.0F);
}

TEST_CASE("a message at a frame already past applies at the first frame of the next render",
          "[player]") {
    const auto late = make_player();
    static_cast<void>(render(*late, 1'000));
    REQUIRE(late->note_on(500, 57, 1.0F));
    const auto late_out = render(*late, 2'000);

    const auto immediate = make_player();
    static_cast<void>(render(*immediate, 1'000));
    REQUIRE(immediate->note_on(0, 57, 1.0F));

    const auto on_time = make_player();
    REQUIRE(on_time->note_on(1'000, 57, 1.0F));
    static_cast<void>(render(*on_time, 1'000));
    const auto on_time_out = render(*on_time, 2'000);

    REQUIRE_FALSE(silent(on_time_out));
    REQUIRE(late_out == on_time_out);
    REQUIRE(render(*immediate, 2'000) == on_time_out);
}

TEST_CASE("a player writes what its voices sum to", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    REQUIRE(player->note_on(0, 64, 0.5F));
    REQUIRE(player->set_parameter(1'000, irama::InstrumentParam::pan, -0.5F));
    const auto out = render(*player, 2'000);

    // The same voices, reading the same instrument, which glides to its new pan at frame 1000,
    // each with the noise generator the player's default seed gives it.
    irama::Instrument instrument{static_cast<float>(rate)};
    irama::Pcg32 generator{irama::PlayerConfig{}.seed, 0};
    irama::Voice first;
    irama::Voice second;
    first.start(57, 1.0F, instrument, generator.split());
    second.start(64, 0.5F, instrument, generator.split());
    std::vector<float> expected(out.size());
    std::vector<float> scratch(irama::Instrument::max_block_frames *
                               irama::Voice::scratch_per_frame);
    std::span<float> rest{expected};
    for (std::size_t frame = 0; frame < 2'000;) {
        if (frame == 1'000) {
            instrument.set(irama::find_param(irama::instrument_params, irama::InstrumentParam::pan)
                               .value_or(irama::instrument_params.size()),
                           -0.5F, true);
        }
        const auto frames = std::min<std::size_t>(
            {irama::Instrument::max_block_frames, (frame < 1'000 ? 1'000 : 2'000) - frame});
        const irama::RenderScope scope;
        const auto block = instrument.render(frames);
        first.render(rest.first(frames * 2), scratch, block);
        second.render(rest.first(frames * 2), scratch, block);
        rest = rest.subspan(frames * 2);
        frame += frames;
    }
    REQUIRE(out == expected);
}

TEST_CASE("a player given every value by set-parameter at frame 0 renders the same as one created "
          "with them",
          "[player]") {
    constexpr std::size_t frames = 24'000;
    const auto created = make_player();
    send_chords(*created);
    const auto expected = render(*created, frames);

    const auto replayed = make_player();
    for (const irama::ParamDescriptor& param : irama::instrument_params) {
        REQUIRE(replayed->set_parameter(0, param.id, param.default_value));
    }
    send_chords(*replayed);
    REQUIRE(render(*replayed, frames) == expected);
}

TEST_CASE("a value set while no voice sounds jumps to it as if set at frame 0", "[player]") {
    constexpr std::size_t frames = 12'000;
    const auto at_start = make_player();
    send_values(*at_start, 0);
    REQUIRE(at_start->note_on(12'000, 57, 0.9F));
    static_cast<void>(render(*at_start, 12'000));
    const auto expected = render(*at_start, frames);

    // A note sounds and ends before the values are set, at the frame of the next note.
    const auto later = make_player();
    REQUIRE(later->note_on(0, 60, 1.0F));
    REQUIRE(later->note_off(100, 60));
    send_values(*later, 12'000);
    REQUIRE(later->note_on(12'000, 57, 0.9F));
    static_cast<void>(render(*later, 12'000));
    // The first note's release has ended.
    REQUIRE(later->active_voices() == 0);
    REQUIRE(render(*later, frames) == expected);
}

TEST_CASE("a value set while a voice sounds glides there", "[player]") {
    const auto jumped = make_player();
    REQUIRE(jumped->set_parameter(0, irama::InstrumentParam::pan, -1.0F));
    REQUIRE(jumped->note_on(0, 57, 1.0F));

    const auto glided = make_player();
    REQUIRE(glided->note_on(0, 57, 1.0F));
    REQUIRE(glided->set_parameter(0, irama::InstrumentParam::pan, -1.0F));

    // The glide takes 20 ms, 960 frames, to reach the pan the other player jumped to.
    const auto jumped_out = render(*jumped, 2'000);
    const auto glided_out = render(*glided, 2'000);
    constexpr std::size_t arrived = std::size_t{959} * 2;
    REQUIRE(glided_out.at(arrived - 1) != 0.0F);
    REQUIRE(std::ranges::equal(std::span{glided_out}.subspan(arrived),
                               std::span{jumped_out}.subspan(arrived)));
    REQUIRE(jumped_out.at(arrived + 1) == 0.0F);
}

TEST_CASE("a set-parameter clamps its value to the parameter's range", "[player]") {
    const auto clamped = make_player();
    REQUIRE(clamped->set_parameter(0, irama::InstrumentParam::gain, 100.0F));
    REQUIRE(clamped->set_parameter(0, irama::InstrumentParam::sustain,
                                   -std::numeric_limits<float>::infinity()));
    send_chords(*clamped);

    const auto bounds = make_player();
    REQUIRE(bounds->set_parameter(0, irama::InstrumentParam::gain, 12.0F));
    REQUIRE(bounds->set_parameter(0, irama::InstrumentParam::sustain, 0.0F));
    send_chords(*bounds);
    REQUIRE(render(*clamped, 20'000) == render(*bounds, 20'000));
}

TEST_CASE("a player counts the voices sounding at the end of each render call", "[player]") {
    const auto player = make_player();
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 48, 1.0F));
    REQUIRE(player->note_on(0, 52, 1.0F));
    REQUIRE(player->note_on(0, 55, 1.0F));
    REQUIRE(player->active_voices() == 0);
    static_cast<void>(render(*player, 10));
    REQUIRE(player->active_voices() == 3);

    REQUIRE(player->note_off(100, 52));
    static_cast<void>(render(*player, 90 + release_frames - 1));
    REQUIRE(player->active_voices() == 3);
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 2);

    // Frames never go backwards, so this one is already past, and applies at once.
    REQUIRE(player->release_all(100));
    static_cast<void>(render(*player, release_frames));
    REQUIRE(player->active_voices() == 0);
}

TEST_CASE("a note-off releases every held voice playing its note", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    REQUIRE(player->note_on(100, 57, 0.5F));
    REQUIRE(player->note_on(100, 60, 0.5F));
    REQUIRE(player->note_off(1'000, 57));
    static_cast<void>(render(*player, 1'000 + release_frames));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("with no voice free, a note-on steals a voice, which fades out over exactly 5 ms",
          "[player]") {
    constexpr std::uint64_t steal = 1'000;
    constexpr std::size_t frames = 2'000;
    const auto player = make_player(1);
    REQUIRE(player->note_on(0, 48, 1.0F));
    REQUIRE(player->note_on(steal, 55, 0.8F));
    const auto out = render(*player, frames);
    REQUIRE(player->active_voices() == 1);

    // The new note starts on its frame, while the stolen one fades. The new note's saw starts at
    // phase 0, so its first frame is silent too.
    const auto stolen = note_alone(0, 48, 1.0F, frames);
    const auto stealing = note_alone(steal, 55, 0.8F, frames);
    REQUIRE(silent(std::span{stealing}.first((steal + 1) * 2)));
    REQUIRE(stealing.at((steal + 1) * 2) != 0.0F);
    std::vector<double> expected(stealing.begin(), stealing.end());
    add_faded(expected, stolen, steal, steal + fade_frames);
    require_near(out, expected);

    // Before the steal, the stolen note is alone, and from the fade's last frame on, the new one.
    REQUIRE(
        std::ranges::equal(std::span{out}.first(steal * 2), std::span{stolen}.first(steal * 2)));
    constexpr std::size_t last_faded = (steal + fade_frames - 2) * 2;
    REQUIRE(out.at(last_faded) != stealing.at(last_faded));
    REQUIRE(std::ranges::equal(std::span{out}.subspan(last_faded + 2),
                               std::span{stealing}.subspan(last_faded + 2)));
}

TEST_CASE("with all 8 fade slots busy, a steal cuts the fade nearest its end", "[player]") {
    // One voice, and a note every 10 frames, so each note steals the one before. The ninth steal
    // finds all 8 fade slots busy, and cuts the first stolen note's fade.
    constexpr std::size_t notes = 10;
    constexpr std::uint64_t spacing = 10;
    constexpr std::size_t frames = 600;
    const auto player = make_player(1);
    for (std::size_t note = 0; note < notes; ++note) {
        REQUIRE(player->note_on(note * spacing, 40 + static_cast<int>(note), 1.0F));
    }
    const auto out = render(*player, frames);

    std::vector<double> expected(out.size());
    for (std::size_t note = 0; note < notes; ++note) {
        const std::uint64_t start = note * spacing;
        // The last note is never stolen.
        const std::uint64_t stolen = note + 1 < notes ? start + spacing : frames;
        const std::uint64_t end = note == 0 ? (notes - 1) * spacing : stolen + fade_frames;
        add_faded(expected, note_alone(start, 40 + static_cast<int>(note), 1.0F, frames), stolen,
                  end);
    }
    require_near(out, expected);
}

TEST_CASE("a player never counts more voices than it has, however many notes steal", "[player]") {
    constexpr std::size_t max_voices = 4;
    const auto player = make_player(max_voices);
    for (std::uint64_t note = 0; note < 40; ++note) {
        REQUIRE(player->note_on(note * 37, 40 + static_cast<int>(note % 30), 1.0F));
        if (note % 3 == 0) {
            REQUIRE(player->note_off((note * 37) + 20, 40 + static_cast<int>(note % 30)));
        }
    }
    std::uint32_t most = 0;
    for (std::size_t call = 0; call < 40; ++call) {
        static_cast<void>(render(*player, 50));
        REQUIRE(player->active_voices() <= max_voices);
        most = std::max(most, player->active_voices());
    }
    REQUIRE(most == max_voices);
}

TEST_CASE("a released voice is free again for the next note-on", "[player]") {
    const auto player = make_player(1);
    REQUIRE(player->note_on(0, 48, 1.0F));
    REQUIRE(player->note_off(10, 48));
    REQUIRE(player->note_on(10 + release_frames, 52, 1.0F));
    static_cast<void>(render(*player, 20 + release_frames));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a full queue refuses a message, and takes messages again once rendered", "[player]") {
    const auto player = make_player();
    for (std::size_t index = 0; index < irama::Player::queue_capacity; ++index) {
        REQUIRE(player->release_all(0));
    }
    REQUIRE_FALSE(player->note_on(0, 57, 1.0F));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 57, 1.0F));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a player renders no frames when asked for none", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F));
    player->render({});
    REQUIRE(player->active_voices() == 0);
    const auto out = render(*player, 300);
    REQUIRE_FALSE(silent(out));
}

TEST_CASE("two players with the same seed render the same noise", "[player]") {
    const auto first = make_noise_player(1'234);
    const auto second = make_noise_player(1'234);
    const auto out = render(*first, 2'000);
    REQUIRE_FALSE(silent(out));
    REQUIRE(render(*second, 2'000) == out);
}

TEST_CASE("players with different seeds render different noise", "[player]") {
    const auto first = make_noise_player(1);
    const auto second = make_noise_player(2);
    REQUIRE(render(*first, 1'000) != render(*second, 1'000));
}

TEST_CASE("each note-on gives its voice a noise stream of its own", "[player]") {
    // Two notes at once on the same pitch, with nothing but noise, would sound as one note twice
    // as loud if their noise were the same.
    const auto one_note = make_noise_chord(1);
    const auto two_notes = make_noise_chord(2);
    const auto one = render(*one_note, 1'000);
    const auto two = render(*two_notes, 1'000);
    std::vector<float> doubled(one.size());
    std::ranges::transform(one, doubled.begin(), [](float sample) { return 2.0F * sample; });
    REQUIRE(two != doubled);
}
