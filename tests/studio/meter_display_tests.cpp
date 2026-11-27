// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/meter_display.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using irama_studio::MeterDisplay;

namespace {

// The time `milliseconds` after the clock's start.
[[nodiscard]] MeterDisplay::Clock::time_point at(int milliseconds) {
    return MeterDisplay::Clock::time_point{} + std::chrono::milliseconds{milliseconds};
}

// Stands in for the audio engine's CPU peak, counting how often it is taken.
struct PeakSource {
    float next = 0.0F;
    int taken = 0;

    [[nodiscard]] auto taker() {
        return [this] {
            ++taken;
            return next;
        };
    }
};

} // namespace

TEST_CASE("the CPU peak is taken every half second, and shown until the next", "[studio][meter]") {
    MeterDisplay meters;
    PeakSource source{.next = 0.25F};

    meters.update(at(0), source.taker(), false);
    CHECK(source.taken == 1);
    CHECK(meters.cpu_peak() == 0.25F);

    source.next = 0.75F;
    meters.update(at(100), source.taker(), false);
    meters.update(at(499), source.taker(), false);
    CHECK(source.taken == 1);
    CHECK(meters.cpu_peak() == 0.25F);

    meters.update(at(500), source.taker(), false);
    CHECK(source.taken == 2);
    CHECK(meters.cpu_peak() == 0.75F);

    source.next = 0.125F;
    meters.update(at(999), source.taker(), false);
    CHECK(source.taken == 2);
    meters.update(at(1'000), source.taker(), false);
    CHECK(source.taken == 3);
    CHECK(meters.cpu_peak() == 0.125F);
}

TEST_CASE("CLIP shows for a second after the last clip", "[studio][meter]") {
    MeterDisplay meters;
    PeakSource source;

    meters.update(at(0), source.taker(), false);
    CHECK_FALSE(meters.clipping());

    meters.update(at(10), source.taker(), true);
    CHECK(meters.clipping());
    meters.update(at(600), source.taker(), true);
    meters.update(at(1'500), source.taker(), false);
    CHECK(meters.clipping());
    meters.update(at(1'599), source.taker(), false);
    CHECK(meters.clipping());
    meters.update(at(1'600), source.taker(), false);
    CHECK_FALSE(meters.clipping());
}
