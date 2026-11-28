// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/player/player.hpp>

#include "counting_allocator.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/song_part.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>
#include <irama/engine/voice/voice.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
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

using irama::InstrumentParam;

// Values that differ from the instrument's defaults, by ID.
using Changed = std::vector<std::pair<irama::ParamId, float>>;

// An instrument part of every default but the values changed, as a host builds one from its
// edited instrument.
[[nodiscard]] irama::InstrumentPartPtr
instrument_part(const Changed& changed,
                const irama::Allocator& allocator = irama::default_allocator()) {
    std::array<float, irama::instrument_params.size()> values{};
    std::ranges::transform(irama::instrument_params, values.begin(),
                           &irama::ParamDescriptor::default_value);
    for (const auto& [id, value] : changed) {
        const std::size_t index = irama::find_param(irama::instrument_params, id)
                                      .value_or(irama::instrument_params.size());
        values.at(index) = irama::instrument_params.at(index).constrain(value);
    }
    auto part = irama::InstrumentPart::create(allocator, values);
    REQUIRE(part.has_value());
    return std::move(*part);
}

// A change set of one instrument number's part, or of none, to empty it.
[[nodiscard]] irama::ChangeSetPtr
instrument_change(int number, irama::InstrumentPartPtr part,
                  const irama::Allocator& allocator = irama::default_allocator()) {
    auto changes = irama::ChangeSet::create(allocator, {.instruments = 1});
    REQUIRE(changes.has_value());
    (*changes)->replace_instrument(static_cast<std::size_t>(number), std::move(part));
    return std::move(*changes);
}

// Sends a change set at a frame, which the player's queue must take.
void send(irama::Player& player, std::uint64_t frame, irama::ChangeSetPtr changes) {
    REQUIRE(player.change(frame, changes));
    REQUIRE(changes == nullptr);
}

// Sends an instrument of every default but the values changed, at a frame.
void send_instrument(irama::Player& player, std::uint64_t frame, const Changed& changed = {},
                     int number = 1) {
    send(player, frame, instrument_change(number, instrument_part(changed)));
}

// Drops every change set the player has handed back, and returns how many there were.
std::size_t drain(irama::Player& player) {
    std::size_t count = 0;
    while (player.take_returned() != nullptr) {
        ++count;
    }
    return count;
}

// Each change set of the return queue's test gives instrument 01 a gain of its own, so the parts
// it takes back say which change set put them in.
[[nodiscard]] float queue_gain(std::size_t index) {
    return -60.0F + (static_cast<float>(index) / 32.0F);
}

[[nodiscard]] irama::InstrumentPartPtr part_of_gain(float decibels,
                                                    const irama::Allocator& allocator) {
    return instrument_part({{InstrumentParam::gain, decibels}}, allocator);
}

// Takes back the change set `index` of the return queue's test, which holds the part the one before
// it put in, or none for the first.
void require_returned(irama::Player& player, std::size_t index) {
    const irama::ChangeSetPtr returned = player.take_returned();
    REQUIRE(returned != nullptr);
    const auto instruments = returned->instruments();
    REQUIRE(instruments.size() == 1);
    const irama::InstrumentPart* old = instruments.front().part.get();
    if (index == 0) {
        REQUIRE(old == nullptr);
        return;
    }
    REQUIRE(old != nullptr);
    const std::size_t gain = irama::find_param(irama::instrument_params, InstrumentParam::gain)
                                 .value_or(irama::instrument_params.size());
    REQUIRE(old->values().subspan(gain, 1).front() == queue_gain(index - 1));
}

// A player with every instrument number empty.
[[nodiscard]] irama::PlayerPtr make_empty_player(std::size_t max_voices = 32) {
    auto player = irama::Player::create({.sample_rate = rate, .max_voices = max_voices});
    REQUIRE(player.has_value());
    return std::move(*player);
}

// A player with instrument 01 at its defaults from frame 0.
[[nodiscard]] irama::PlayerPtr make_player(std::size_t max_voices = 32) {
    auto player = make_empty_player(max_voices);
    send_instrument(*player, 0);
    return player;
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

// Applies as many change sets as the return queue holds, each of its own gain, and takes none back.
void fill_return_queue(irama::Player& player, const irama::Allocator& allocator) {
    for (std::size_t index = 0; index < irama::Player::return_capacity; ++index) {
        send(player, 0,
             instrument_change(1, part_of_gain(queue_gain(index), allocator), allocator));
    }
    static_cast<void>(render(player, 1));
}

// A change of the gain while notes sound, and of the release before they are released.
void send_changes(irama::Player& player) {
    send_instrument(player, 9'000, {{InstrumentParam::gain, -6.0F}});
    send_instrument(player, 9'500,
                    {{InstrumentParam::gain, -6.0F}, {InstrumentParam::release, 0.5F}});
}

// Chords that overlap, with a note-off, notes sharing a frame, instrument changes and a
// release-all, to be sent to players that render them in different ways.
void send_chords(irama::Player& player) {
    REQUIRE(player.note_on(0, 48, 0.8F, 1));
    REQUIRE(player.note_on(1'000, 52, 0.7F, 1));
    REQUIRE(player.note_on(1'000, 55, 0.6F, 1));
    REQUIRE(player.note_off(5'000, 52, 1));
    REQUIRE(player.note_on(7'777, 60, 1.0F, 1));
    REQUIRE(player.note_on(7'777, 64, 0.9F, 1));
    send_changes(player);
    REQUIRE(player.release_all(12'000));
}

// Values other than the defaults for the gain, the pan and the sustain level.
[[nodiscard]] Changed other_values() {
    return {
        {InstrumentParam::gain, 0.0F},
        {InstrumentParam::pan, 0.75F},
        {InstrumentParam::sustain, 40.0F},
    };
}

// Noise alone, with no oscillator.
[[nodiscard]] Changed noise_alone() {
    return {
        {InstrumentParam::osc1_level, 0.0F},
        {InstrumentParam::noise_level, 100.0F},
    };
}

// A change set of a song part of a seed.
[[nodiscard]] irama::ChangeSetPtr seed_change(std::uint64_t seed) {
    auto changes = irama::ChangeSet::create(irama::default_allocator());
    auto song = irama::SongPart::create(irama::default_allocator(), {.seed = seed});
    REQUIRE(changes.has_value());
    REQUIRE(song.has_value());
    (*changes)->replace_song(std::move(*song));
    return std::move(*changes);
}

// A player of a seed playing two notes of noise alone, overlapping.
[[nodiscard]] irama::PlayerPtr make_noise_player(std::uint64_t seed) {
    auto player = make_empty_player();
    send(*player, 0, seed_change(seed));
    send_instrument(*player, 0, noise_alone());
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    REQUIRE(player->note_on(500, 60, 1.0F, 1));
    return player;
}

// A player of noise alone, panned hard left, starting `notes` notes of A-4 at frame 0.
[[nodiscard]] irama::PlayerPtr make_noise_chord(int notes) {
    auto player = make_empty_player();
    send_instrument(*player, 0,
                    {
                        {InstrumentParam::osc1_level, 0.0F},
                        {InstrumentParam::noise_level, 100.0F},
                        {InstrumentParam::pan, -1.0F},
                    });
    for (int note = 0; note < notes; ++note) {
        REQUIRE(player->note_on(0, 57, 1.0F, 1));
    }
    return player;
}

// The frames of one note played alone at a frame, by a player of one voice at the instrument's
// defaults, whose noise is silent.
[[nodiscard]] std::vector<float> note_alone(std::uint64_t frame, int note, float velocity,
                                            std::size_t frames) {
    const auto player = make_player(1);
    REQUIRE(player->note_on(frame, note, velocity, 1));
    return render(*player, frames);
}

// More notes than a player of 32 voices has voices and fade slots, so notes steal and fades are
// cut.
void send_crowd(irama::Player& player) {
    for (std::uint64_t note = 0; note < 64; ++note) {
        REQUIRE(player.note_on(note * 10, 57, 1.0F, 1));
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
        send(**player, 0,
             instrument_change(1, instrument_part({}, counting.allocator()), counting.allocator()));
        const std::size_t allocated = counting.allocations();
        send_crowd(**player);
        static_cast<void>(render(**player, 1'000));
        REQUIRE((*player)->active_voices() == 32);
        REQUIRE(counting.allocations() == allocated);
        REQUIRE(drain(**player) == 1);
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
    REQUIRE(player->note_on(100, 57, 1.0F, 1));
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
    REQUIRE(late->note_on(500, 57, 1.0F, 1));
    const auto late_out = render(*late, 2'000);

    const auto immediate = make_player();
    static_cast<void>(render(*immediate, 1'000));
    REQUIRE(immediate->note_on(0, 57, 1.0F, 1));

    const auto on_time = make_player();
    REQUIRE(on_time->note_on(1'000, 57, 1.0F, 1));
    static_cast<void>(render(*on_time, 1'000));
    const auto on_time_out = render(*on_time, 2'000);

    REQUIRE_FALSE(silent(on_time_out));
    REQUIRE(late_out == on_time_out);
    REQUIRE(render(*immediate, 2'000) == on_time_out);
}

TEST_CASE("a player writes what its voices sum to", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    REQUIRE(player->note_on(0, 64, 0.5F, 1));
    send_instrument(*player, 1'000, {{InstrumentParam::pan, -0.5F}});
    const auto out = render(*player, 2'000);

    // The same voices, reading the same instrument, which glides to its new pan at frame 1000,
    // each with the noise generator the new song's seed gives it.
    irama::Instrument instrument{static_cast<float>(rate)};
    irama::InstrumentBuffers buffers;
    irama::Pcg32 generator{irama::SongDescription{}.seed, 0};
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
        const auto block = instrument.render(buffers, frames);
        first.render(rest.first(frames * 2), scratch, block);
        second.render(rest.first(frames * 2), scratch, block);
        rest = rest.subspan(frames * 2);
        frame += frames;
    }
    REQUIRE(out == expected);
}

TEST_CASE("an instrument changed at frame 0 renders as if it had held its values from the start",
          "[player]") {
    constexpr std::size_t frames = 24'000;
    const auto loaded = make_empty_player();
    send_instrument(*loaded, 0, other_values());
    send_chords(*loaded);
    const auto expected = render(*loaded, frames);
    REQUIRE_FALSE(silent(expected));

    const auto changed = make_player();
    send_instrument(*changed, 0, other_values());
    send_chords(*changed);
    REQUIRE(render(*changed, frames) == expected);
}

TEST_CASE("a change set applies whole, exactly at its frame", "[player]") {
    constexpr std::size_t frames = 3'000;
    // Instrument 01 changes and instrument 02 arrives in one change set at frame 1000, where a
    // note of each starts. A note of 02 a frame earlier plays nothing.
    const auto together = make_player();
    REQUIRE(together->note_on(0, 48, 1.0F, 1));
    REQUIRE(together->note_on(999, 55, 1.0F, 2));
    auto changes = irama::ChangeSet::create(irama::default_allocator(), {.instruments = 2});
    REQUIRE(changes.has_value());
    (*changes)->replace_instrument(1, instrument_part(other_values()));
    (*changes)->replace_instrument(2, instrument_part({{InstrumentParam::pan, -1.0F}}));
    send(*together, 1'000, std::move(*changes));
    REQUIRE(together->note_on(1'000, 60, 1.0F, 1));
    REQUIRE(together->note_on(1'000, 64, 1.0F, 2));
    const auto out = render_split(*together, frames, {1, 7, 255, 256, 4'096});

    // The same, with each part in a change set of its own at the same frame.
    const auto apart = make_player();
    REQUIRE(apart->note_on(0, 48, 1.0F, 1));
    send_instrument(*apart, 1'000, other_values());
    send_instrument(*apart, 1'000, {{InstrumentParam::pan, -1.0F}}, 2);
    REQUIRE(apart->note_on(1'000, 60, 1.0F, 1));
    REQUIRE(apart->note_on(1'000, 64, 1.0F, 2));
    REQUIRE(render(*apart, frames) == out);

    // Until frame 1000, instrument 01's note sounds as it would with no change.
    const auto unchanged = make_player();
    REQUIRE(unchanged->note_on(0, 48, 1.0F, 1));
    const auto unchanged_out = render(*unchanged, frames);
    REQUIRE(std::ranges::equal(std::span{out}.first(2'000), std::span{unchanged_out}.first(2'000)));
    REQUIRE(out.at(2'000) != unchanged_out.at(2'000));
}

TEST_CASE("each instrument's voices read its own values", "[player]") {
    constexpr std::size_t frames = 4'000;
    const auto both = make_player();
    send_instrument(*both, 0, other_values(), 2);
    REQUIRE(both->note_on(0, 57, 1.0F, 1));
    REQUIRE(both->note_on(0, 61, 0.5F, 2));

    const auto first = make_player();
    REQUIRE(first->note_on(0, 57, 1.0F, 1));
    const auto second = make_empty_player();
    send_instrument(*second, 0, other_values(), 2);
    REQUIRE(second->note_on(0, 61, 0.5F, 2));

    const auto out = render(*both, frames);
    const auto first_out = render(*first, frames);
    const auto second_out = render(*second, frames);
    std::vector<float> sum(out.size());
    std::ranges::transform(first_out, second_out, sum.begin(), std::plus{});
    REQUIRE(out == sum);
}

TEST_CASE("a note on an empty instrument number plays nothing", "[player]") {
    const auto empty = make_empty_player();
    REQUIRE(empty->note_on(0, 57, 1.0F, 1));
    REQUIRE(silent(render(*empty, 1'000)));
    REQUIRE(empty->active_voices() == 0);

    const auto loaded = make_player();
    REQUIRE(loaded->note_on(0, 57, 1.0F, 0xFE));
    REQUIRE(silent(render(*loaded, 1'000)));
    REQUIRE(loaded->active_voices() == 0);
}

TEST_CASE("an instrument emptied while its voices sound fades them out over exactly 5 ms",
          "[player]") {
    constexpr std::uint64_t emptied = 1'000;
    constexpr std::size_t frames = 2'000;
    const auto player = make_player();
    send_instrument(*player, 0, {}, 2);
    REQUIRE(player->note_on(0, 48, 1.0F, 1));
    REQUIRE(player->note_on(0, 55, 0.5F, 2));
    send(*player, emptied, instrument_change(1, nullptr));
    const auto out = render(*player, frames);
    REQUIRE(player->active_voices() == 1);

    // Instrument 02's note carries on as if alone, and 01's fades as a stolen voice does.
    const auto other = make_empty_player();
    send_instrument(*other, 0, {}, 2);
    REQUIRE(other->note_on(0, 55, 0.5F, 2));
    const auto other_out = render(*other, frames);
    std::vector<double> expected(other_out.begin(), other_out.end());
    add_faded(expected, note_alone(0, 48, 1.0F, frames), emptied, emptied + fade_frames);
    require_near(out, expected);
    REQUIRE(std::ranges::equal(std::span{out}.subspan((emptied + fade_frames) * 2),
                               std::span{other_out}.subspan((emptied + fade_frames) * 2)));
}

TEST_CASE("a value an instrument part leaves as it was keeps its glide", "[player]") {
    const auto once = make_player();
    REQUIRE(once->note_on(0, 57, 1.0F, 1));
    send_instrument(*once, 0, {{InstrumentParam::pan, -1.0F}});

    // A second part halfway through the glide changes only the release, which isn't heard before
    // the note is released, so the two sound alike if the pan carries on gliding.
    const auto twice = make_player();
    REQUIRE(twice->note_on(0, 57, 1.0F, 1));
    send_instrument(*twice, 0, {{InstrumentParam::pan, -1.0F}});
    send_instrument(*twice, 480, {{InstrumentParam::pan, -1.0F}, {InstrumentParam::release, 0.5F}});
    REQUIRE(render(*twice, 2'000) == render(*once, 2'000));
}

TEST_CASE("a value set while no voice sounds jumps to it as if set at frame 0", "[player]") {
    constexpr std::size_t frames = 12'000;
    const auto at_start = make_player();
    send_instrument(*at_start, 0, other_values());
    REQUIRE(at_start->note_on(12'000, 57, 0.9F, 1));
    static_cast<void>(render(*at_start, 12'000));
    const auto expected = render(*at_start, frames);

    // A note sounds and ends before the values are set, at the frame of the next note.
    const auto later = make_player();
    REQUIRE(later->note_on(0, 60, 1.0F, 1));
    REQUIRE(later->note_off(100, 60, 1));
    send_instrument(*later, 12'000, other_values());
    REQUIRE(later->note_on(12'000, 57, 0.9F, 1));
    static_cast<void>(render(*later, 12'000));
    // The first note's release has ended.
    REQUIRE(later->active_voices() == 0);
    REQUIRE(render(*later, frames) == expected);
}

TEST_CASE("a value set while a voice of its instrument sounds glides there", "[player]") {
    const auto jumped = make_player();
    send_instrument(*jumped, 0, {{InstrumentParam::pan, -1.0F}});
    REQUIRE(jumped->note_on(0, 57, 1.0F, 1));

    const auto glided = make_player();
    REQUIRE(glided->note_on(0, 57, 1.0F, 1));
    send_instrument(*glided, 0, {{InstrumentParam::pan, -1.0F}});

    // The glide takes 20 ms, 960 frames, to reach the pan the other player jumped to.
    const auto jumped_out = render(*jumped, 2'000);
    const auto glided_out = render(*glided, 2'000);
    constexpr std::size_t arrived = std::size_t{959} * 2;
    REQUIRE(glided_out.at(arrived - 1) != 0.0F);
    REQUIRE(std::ranges::equal(std::span{glided_out}.subspan(arrived),
                               std::span{jumped_out}.subspan(arrived)));
    REQUIRE(jumped_out.at(arrived + 1) == 0.0F);
}

TEST_CASE("a value set while only another instrument's voices sound jumps to it", "[player]") {
    constexpr std::size_t frames = 4'000;
    const auto jumped = make_player();
    send_instrument(*jumped, 0, {}, 2);
    send_instrument(*jumped, 0, {{InstrumentParam::pan, -1.0F}});
    REQUIRE(jumped->note_on(0, 50, 1.0F, 2));
    REQUIRE(jumped->note_on(100, 57, 1.0F, 1));

    const auto from_start = make_empty_player();
    send_instrument(*from_start, 0, {}, 2);
    send_instrument(*from_start, 0, {{InstrumentParam::pan, -1.0F}});
    REQUIRE(from_start->note_on(0, 50, 1.0F, 2));
    REQUIRE(from_start->note_on(100, 57, 1.0F, 1));
    const auto jumped_out = render(*jumped, frames);
    REQUIRE(render(*from_start, frames) == jumped_out);

    // Instrument 02 sounds from frame 0 here, before 01's pan arrives at frame 50.
    const auto later = make_player();
    send_instrument(*later, 0, {}, 2);
    REQUIRE(later->note_on(0, 50, 1.0F, 2));
    send_instrument(*later, 50, {{InstrumentParam::pan, -1.0F}});
    REQUIRE(later->note_on(100, 57, 1.0F, 1));
    REQUIRE(render(*later, frames) == jumped_out);
}

TEST_CASE("a player counts the voices sounding at the end of each render call", "[player]") {
    const auto player = make_player();
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 48, 1.0F, 1));
    REQUIRE(player->note_on(0, 52, 1.0F, 1));
    REQUIRE(player->note_on(0, 55, 1.0F, 1));
    REQUIRE(player->active_voices() == 0);
    static_cast<void>(render(*player, 10));
    REQUIRE(player->active_voices() == 3);

    REQUIRE(player->note_off(100, 52, 1));
    static_cast<void>(render(*player, 90 + release_frames - 1));
    REQUIRE(player->active_voices() == 3);
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 2);

    // Frames never go backwards, so this one is already past, and applies at once.
    REQUIRE(player->release_all(100));
    static_cast<void>(render(*player, release_frames));
    REQUIRE(player->active_voices() == 0);
}

TEST_CASE("a note-off releases every held voice playing its note on its instrument", "[player]") {
    const auto player = make_player();
    send_instrument(*player, 0, {}, 2);
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    REQUIRE(player->note_on(100, 57, 0.5F, 1));
    REQUIRE(player->note_on(100, 60, 0.5F, 1));
    REQUIRE(player->note_on(100, 57, 0.5F, 2));
    REQUIRE(player->note_off(1'000, 57, 1));
    static_cast<void>(render(*player, 1'000 + release_frames));
    REQUIRE(player->active_voices() == 2);
}

TEST_CASE("a release-all releases the held voices of every instrument", "[player]") {
    const auto player = make_player();
    send_instrument(*player, 0, {}, 2);
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    REQUIRE(player->note_on(0, 60, 0.5F, 2));
    REQUIRE(player->release_all(1'000));
    static_cast<void>(render(*player, 1'000 + release_frames));
    REQUIRE(player->active_voices() == 0);
}

TEST_CASE("with no voice free, a note-on steals a voice, which fades out over exactly 5 ms",
          "[player]") {
    constexpr std::uint64_t steal = 1'000;
    constexpr std::size_t frames = 2'000;
    const auto player = make_player(1);
    REQUIRE(player->note_on(0, 48, 1.0F, 1));
    REQUIRE(player->note_on(steal, 55, 0.8F, 1));
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
        REQUIRE(player->note_on(note * spacing, 40 + static_cast<int>(note), 1.0F, 1));
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
        REQUIRE(player->note_on(note * 37, 40 + static_cast<int>(note % 30), 1.0F, 1));
        if (note % 3 == 0) {
            REQUIRE(player->note_off((note * 37) + 20, 40 + static_cast<int>(note % 30), 1));
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
    REQUIRE(player->note_on(0, 48, 1.0F, 1));
    REQUIRE(player->note_off(10, 48, 1));
    REQUIRE(player->note_on(10 + release_frames, 52, 1.0F, 1));
    static_cast<void>(render(*player, 20 + release_frames));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a full queue refuses a message, and takes messages again once rendered", "[player]") {
    const auto player = make_player();
    static_cast<void>(render(*player, 1));
    for (std::size_t index = 0; index < irama::Player::queue_capacity; ++index) {
        REQUIRE(player->release_all(0));
    }
    REQUIRE_FALSE(player->note_on(0, 57, 1.0F, 1));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 0);
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 1);
}

TEST_CASE("a player renders no frames when asked for none", "[player]") {
    const auto player = make_player();
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
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

TEST_CASE("a song part of another seed reseeds the player's random numbers, and one of the same "
          "seed doesn't",
          "[player]") {
    constexpr std::size_t frames = 2'000;
    const auto seeded = make_noise_player(1'234);
    const auto expected = render(*seeded, frames);

    // The noise of a note after a song part of seed 1234 is the noise of a new player of that seed.
    const auto reseeded = make_empty_player();
    send_instrument(*reseeded, 0, noise_alone());
    REQUIRE(reseeded->note_on(0, 48, 1.0F, 1));
    static_cast<void>(render(*reseeded, 100));
    send(*reseeded, 0, seed_change(1'234));
    REQUIRE(reseeded->note_off(0, 48, 1));
    static_cast<void>(render(*reseeded, release_frames));
    REQUIRE(reseeded->active_voices() == 0);
    constexpr std::uint64_t start = 100 + release_frames;
    REQUIRE(reseeded->note_on(start, 57, 1.0F, 1));
    REQUIRE(reseeded->note_on(start + 500, 60, 1.0F, 1));
    REQUIRE(render(*reseeded, frames) == expected);

    // A song part of the seed the player already has, between the notes, leaves the generator
    // where it was.
    const auto kept = make_empty_player();
    send(*kept, 0, seed_change(1'234));
    send_instrument(*kept, 0, noise_alone());
    REQUIRE(kept->note_on(0, 57, 1.0F, 1));
    send(*kept, 250, seed_change(1'234));
    REQUIRE(kept->note_on(500, 60, 1.0F, 1));
    REQUIRE(render(*kept, frames) == expected);
}

TEST_CASE("with nothing taken back, change sets past the return queue's capacity wait in order "
          "with the messages behind them",
          "[player]") {
    irama::test::CountingAllocator counting;
    const irama::Allocator allocator = counting.allocator();
    const auto player = make_empty_player();
    fill_return_queue(*player, allocator);

    // The return queue is full, so the next change set waits, and the note behind it with it.
    send(*player, 0, instrument_change(1, part_of_gain(0.0F, allocator), allocator));
    REQUIRE(player->note_on(0, 57, 1.0F, 1));
    REQUIRE(silent(render(*player, 1'000)));
    REQUIRE(player->active_voices() == 0);

    for (std::size_t index = 0; index < irama::Player::return_capacity; ++index) {
        require_returned(*player, index);
    }
    REQUIRE(player->take_returned() == nullptr);

    // Once taken back, every replaced part has been freed: only the part in the EngineSong and the
    // waiting change set are left.
    REQUIRE(counting.live().size() == 2 + 4);
    static_cast<void>(render(*player, 1));
    REQUIRE(player->active_voices() == 1);
    REQUIRE(drain(*player) == 1);
    REQUIRE(counting.live().size() == 2);
}

TEST_CASE("destroying a player frees every change set in either queue, and the song's parts",
          "[player]") {
    irama::test::CountingAllocator counting;
    const irama::Allocator allocator = counting.allocator();
    {
        auto player = irama::Player::create({.sample_rate = rate, .allocator = allocator});
        REQUIRE(player.has_value());
        for (int number = 1; number <= 3; ++number) {
            send(**player, 0, instrument_change(number, instrument_part({}, allocator), allocator));
        }
        static_cast<void>(render(**player, 10));
        // One change set waits for its frame, popped, and two more in the queue behind it.
        send(**player, 100, instrument_change(4, instrument_part({}, allocator), allocator));
        send(**player, 200, instrument_change(1, nullptr, allocator));
        send(**player, 300, instrument_change(5, instrument_part({}, allocator), allocator));
        static_cast<void>(render(**player, 10));
    }
    REQUIRE(counting.allocations() > 0);
    REQUIRE(counting.frees() == counting.allocations());
    REQUIRE(counting.mismatched_frees() == 0);
    REQUIRE(counting.live().empty());
}

TEST_CASE("a change set queued and refused is left with its sender", "[player]") {
    const auto player = make_empty_player();
    for (std::size_t index = 0; index < irama::Player::queue_capacity; ++index) {
        REQUIRE(player->release_all(0));
    }
    irama::ChangeSetPtr changes = instrument_change(1, instrument_part({}));
    REQUIRE_FALSE(player->change(0, changes));
    REQUIRE(changes != nullptr);
    static_cast<void>(render(*player, 1));
    REQUIRE(player->change(0, changes));
    REQUIRE(changes == nullptr);
}
