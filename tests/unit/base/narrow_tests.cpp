// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/narrow.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

TEST_CASE("narrow keeps a value that fits the narrower type", "[base]") {
    STATIC_REQUIRE(irama::narrow<std::uint8_t>(255) == 255);
    STATIC_REQUIRE(irama::narrow<std::int8_t>(-128) == -128);
    STATIC_REQUIRE(irama::narrow<std::int16_t>(std::int64_t{-32768}) == -32768);
    REQUIRE(irama::narrow<std::uint32_t>(std::size_t{4'294'967'295}) == 4'294'967'295U);
}

TEST_CASE("narrow changes signedness when the value fits both types", "[base]") {
    STATIC_REQUIRE(irama::narrow<std::size_t>(0) == 0U);
    STATIC_REQUIRE(irama::narrow<unsigned>(std::numeric_limits<int>::max()) == 2'147'483'647U);
    STATIC_REQUIRE(irama::narrow<int>(2'147'483'647U) == std::numeric_limits<int>::max());
    REQUIRE(irama::narrow<std::ptrdiff_t>(std::size_t{48'000}) == 48'000);
}
