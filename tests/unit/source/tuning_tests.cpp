// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/tuning.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinRel;

TEST_CASE("A-4 is 440 Hz", "[source]") { REQUIRE(irama::note_frequency(57) == 440.0F); }

TEST_CASE("each octave doubles a note's frequency", "[source]") {
    REQUIRE(irama::note_frequency(45) == 220.0F);
    REQUIRE(irama::note_frequency(69) == 880.0F);
}

TEST_CASE("notes are tuned in equal temperament from A-4", "[source]") {
    REQUIRE_THAT(irama::note_frequency(48), WithinRel(261.6256F, 1e-6F));
    REQUIRE_THAT(irama::note_frequency(irama::lowest_note), WithinRel(16.351598F, 1e-6F));
    REQUIRE_THAT(irama::note_frequency(irama::highest_note), WithinRel(15804.266F, 1e-6F));
}
