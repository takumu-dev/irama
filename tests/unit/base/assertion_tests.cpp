// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/assertion.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a passing IRAMA_ASSERT evaluates its condition once", "[base]") {
    int evaluations = 0;
    IRAMA_ASSERT(++evaluations > 0, "a count only grows");
    REQUIRE(evaluations == 1);
}
