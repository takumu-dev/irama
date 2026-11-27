// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/heartbeat_watch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using irama_studio::HeartbeatWatch;

namespace {

// The time `milliseconds` after the clock's start.
[[nodiscard]] HeartbeatWatch::Clock::time_point at(int milliseconds) {
    return HeartbeatWatch::Clock::time_point{} + std::chrono::milliseconds{milliseconds};
}

} // namespace

TEST_CASE("a device is lost once its heartbeat hasn't moved for a second, and not before",
          "[studio][audio]") {
    HeartbeatWatch watch;
    CHECK_FALSE(watch.lost(at(0), 10));
    CHECK_FALSE(watch.lost(at(500), 10));
    CHECK_FALSE(watch.lost(at(999), 10));
    CHECK(watch.lost(at(1000), 10));
    CHECK(watch.lost(at(5000), 10));
}

TEST_CASE("each move of the heartbeat starts the second again", "[studio][audio]") {
    HeartbeatWatch watch;
    CHECK_FALSE(watch.lost(at(0), 10));
    CHECK_FALSE(watch.lost(at(900), 11));
    CHECK_FALSE(watch.lost(at(1800), 11));
    CHECK_FALSE(watch.lost(at(1899), 11));
    CHECK(watch.lost(at(1900), 11));
}

TEST_CASE("a heartbeat that moves only rarely still counts as moving", "[studio][audio]") {
    HeartbeatWatch watch;
    CHECK_FALSE(watch.lost(at(0), 1));
    // The UI thread may itself be held up, and finds the heartbeat moved when it looks again.
    CHECK_FALSE(watch.lost(at(3000), 2));
    CHECK_FALSE(watch.lost(at(3500), 2));
}

TEST_CASE("a reset watch starts its second from the next look", "[studio][audio]") {
    HeartbeatWatch watch;
    CHECK_FALSE(watch.lost(at(0), 10));
    CHECK(watch.lost(at(1000), 10));

    // A device opened again keeps the heartbeat's count, which may not have moved yet.
    watch.reset();
    CHECK_FALSE(watch.lost(at(1200), 10));
    CHECK_FALSE(watch.lost(at(2199), 10));
    CHECK(watch.lost(at(2200), 10));
}
