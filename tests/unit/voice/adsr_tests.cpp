// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/voice/adsr.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

constexpr float rate = 48'000.0F;

// At 48 kHz: an attack of 48 frames, a decay of 96 and a release of 72, with a sustain level of
// 0.5.
constexpr irama::AdsrSettings short_settings{
    .attack_seconds = 0.001F,
    .decay_seconds = 0.002F,
    .release_seconds = 0.0015F,
    .attack_curve = 0.0F,
    .decay_curve = 0.0F,
    .release_curve = 0.0F,
};
constexpr std::size_t attack_frames = 48;
constexpr std::size_t decay_frames = 96;
constexpr std::size_t release_frames = 72;
constexpr float sustain_level = 0.5F;

// The levels' error from the formula, which the envelope computes in float.
constexpr double tolerance = 2e-6;

// Renders as a voice does, inside a render scope, so the real-time checks watch the envelope.
void render(irama::Adsr& envelope, std::span<float> levels, std::span<const float> sustain,
            const irama::AdsrSettings& settings) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    envelope.render(levels, sustain, settings);
}

// Renders the envelope's next levels, with the sustain level of each frame given by `sustain`.
[[nodiscard]] std::vector<float> render(irama::Adsr& envelope, const std::vector<float>& sustain,
                                        const irama::AdsrSettings& settings = short_settings) {
    std::vector<float> levels(sustain.size());
    render(envelope, levels, sustain, settings);
    return levels;
}

[[nodiscard]] std::vector<float> render(irama::Adsr& envelope, std::size_t frames,
                                        const irama::AdsrSettings& settings = short_settings) {
    return render(envelope, std::vector<float>(frames, sustain_level), settings);
}

// A segment's shape at its frame `index`, counting from 0, of `length`, for the curve control.
[[nodiscard]] double shape(std::size_t index, std::size_t length, double curve) {
    const double p = static_cast<double>(index + 1) / static_cast<double>(length);
    if (curve == 0.0) {
        return p;
    }
    const double k = 6.0 * curve;
    return (1.0 - std::exp(-k * p)) / (1.0 - std::exp(-k));
}

} // namespace

TEST_CASE("each segment of an ADSR lasts exactly its time", "[voice]") {
    irama::Adsr envelope;
    envelope.start(short_settings, rate);
    const auto held = render(envelope, attack_frames + decay_frames + 100);

    // The attack reaches full level on its last frame, and the decay the sustain level on its own.
    REQUIRE(held.at(attack_frames - 2) < 1.0F);
    REQUIRE(held.at(attack_frames - 1) == 1.0F);
    REQUIRE(held.at(attack_frames + decay_frames - 2) > 0.5F);
    REQUIRE(held.at(attack_frames + decay_frames - 1) == 0.5F);
    REQUIRE(std::ranges::all_of(std::span{held}.subspan(attack_frames + decay_frames),
                                [](float level) { return level == 0.5F; }));

    envelope.release(short_settings);
    const auto released = render(envelope, release_frames - 1);
    REQUIRE(released.back() > 0.0F);
    REQUIRE(envelope.active());
    const auto last = render(envelope, 1);
    REQUIRE(last.front() == 0.0F);
    REQUIRE_FALSE(envelope.active());
}

TEST_CASE("an ADSR ends on the frame its release ends, and stays at 0", "[voice]") {
    irama::Adsr envelope;
    envelope.start(short_settings, rate);
    static_cast<void>(render(envelope, 10));
    envelope.release(short_settings);
    const auto levels = render(envelope, release_frames + 50);
    REQUIRE(levels.at(release_frames - 2) > 0.0F);
    REQUIRE(std::ranges::all_of(std::span{levels}.subspan(release_frames - 1),
                                [](float level) { return level == 0.0F; }));
    REQUIRE_FALSE(envelope.active());
}

TEST_CASE("an ADSR's segments follow the curve formula", "[voice]") {
    const double curve = GENERATE(-1.0, 0.0, 1.0);
    irama::AdsrSettings settings = short_settings;
    settings.attack_curve = static_cast<float>(curve);
    settings.decay_curve = static_cast<float>(curve);
    settings.release_curve = static_cast<float>(curve);
    irama::Adsr envelope;
    envelope.start(settings, rate);
    const auto held = render(envelope, attack_frames + decay_frames, settings);
    envelope.release(settings);
    const auto released = render(envelope, release_frames, settings);

    for (std::size_t index = 0; index < attack_frames; ++index) {
        REQUIRE_THAT(held.at(index), WithinAbs(shape(index, attack_frames, curve), tolerance));
    }
    for (std::size_t index = 0; index < decay_frames; ++index) {
        const double expected = 1.0 - (0.5 * shape(index, decay_frames, curve));
        REQUIRE_THAT(held.at(attack_frames + index), WithinAbs(expected, tolerance));
    }
    for (std::size_t index = 0; index < release_frames; ++index) {
        const double expected = 0.5 * (1.0 - shape(index, release_frames, curve));
        REQUIRE_THAT(released.at(index), WithinAbs(expected, tolerance));
    }
}

TEST_CASE("a curve bends its segment as its sign says", "[voice]") {
    // Halfway through a rising segment, a positive curve is above the straight line and a negative
    // one below it.
    irama::AdsrSettings settings = short_settings;
    settings.attack_curve = 1.0F;
    irama::Adsr fast;
    fast.start(settings, rate);
    settings.attack_curve = -1.0F;
    irama::Adsr slow;
    slow.start(settings, rate);
    REQUIRE(render(fast, attack_frames / 2).back() > 0.5F);
    REQUIRE(render(slow, attack_frames / 2).back() < 0.5F);
}

TEST_CASE("a note-off mid-attack releases from the current level", "[voice]") {
    irama::Adsr envelope;
    envelope.start(short_settings, rate);
    const auto attack = render(envelope, 20);
    const auto level = static_cast<double>(attack.back());
    REQUIRE_THAT(level, WithinAbs(20.0 / 48.0, tolerance));

    envelope.release(short_settings);
    REQUIRE(envelope.releasing());
    const auto released = render(envelope, release_frames);
    for (std::size_t index = 0; index < release_frames; ++index) {
        const double expected = level * (1.0 - shape(index, release_frames, 0.0));
        REQUIRE_THAT(released.at(index), WithinAbs(expected, tolerance));
    }
    REQUIRE_FALSE(envelope.active());
}

TEST_CASE("releasing an ADSR again doesn't start its release over", "[voice]") {
    irama::Adsr once;
    irama::Adsr twice;
    once.start(short_settings, rate);
    twice.start(short_settings, rate);
    static_cast<void>(render(once, 100));
    static_cast<void>(render(twice, 100));
    once.release(short_settings);
    twice.release(short_settings);
    const auto first = render(once, 30);
    REQUIRE(render(twice, 30) == first);
    twice.release(short_settings);
    REQUIRE(render(once, 30) == render(twice, 30));
}

TEST_CASE("an ADSR's default settings are the amplifier envelope's", "[voice]") {
    constexpr irama::AdsrSettings defaults;
    REQUIRE(defaults.attack_seconds == 0.002F);
    REQUIRE(defaults.decay_seconds == 0.3F);
    REQUIRE(defaults.release_seconds == 0.2F);
    REQUIRE(defaults.attack_curve == 0.0F);
    REQUIRE(defaults.decay_curve == 0.5F);
    REQUIRE(defaults.release_curve == 0.5F);
}

TEST_CASE("each segment latches its time and curve when it starts", "[voice]") {
    // The decay starts with the attack's last frame, so it takes the settings given then, and the
    // release the settings given to release().
    irama::AdsrSettings longer = short_settings;
    longer.attack_seconds = 0.01F;
    longer.decay_seconds = 0.004F;
    longer.release_seconds = 0.003F;
    irama::Adsr envelope;
    envelope.start(short_settings, rate);
    const auto attack = render(envelope, attack_frames, longer);
    REQUIRE(attack.back() == 1.0F);
    const auto decay = render(envelope, 2 * decay_frames, short_settings);
    REQUIRE(decay.at((2 * decay_frames) - 2) > sustain_level);
    REQUIRE(decay.back() == sustain_level);

    envelope.release(longer);
    const auto released = render(envelope, 2 * release_frames, short_settings);
    REQUIRE(released.at((2 * release_frames) - 2) > 0.0F);
    REQUIRE(released.back() == 0.0F);
    REQUIRE_FALSE(envelope.active());
}

TEST_CASE("the decay heads for the sustain level of each frame, and the sustain follows it",
          "[voice]") {
    irama::Adsr envelope;
    envelope.start(short_settings, rate);
    static_cast<void>(render(envelope, attack_frames));
    std::vector<float> sustain(decay_frames + 100);
    for (std::size_t index = 0; index < sustain.size(); ++index) {
        sustain.at(index) = 0.5F - (0.002F * static_cast<float>(index));
    }
    const auto levels = render(envelope, sustain);
    for (std::size_t index = 0; index < decay_frames; ++index) {
        const auto target = static_cast<double>(sustain.at(index));
        const double expected = 1.0 + ((target - 1.0) * shape(index, decay_frames, 0.0));
        REQUIRE_THAT(levels.at(index), WithinAbs(expected, tolerance));
    }
    for (std::size_t index = decay_frames - 1; index < sustain.size(); ++index) {
        REQUIRE(levels.at(index) == sustain.at(index));
    }

    // A release from the sustain starts from its latest level.
    envelope.release(short_settings);
    const auto released = render(envelope, 1);
    const auto from = static_cast<double>(sustain.back());
    REQUIRE_THAT(released.front(),
                 WithinAbs(from * (1.0 - shape(0, release_frames, 0.0)), tolerance));
}
