// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/pcg32.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

// Draws the generator's next outputs.
template <std::size_t Count>
[[nodiscard]] constexpr std::array<std::uint32_t, Count> draw(irama::Pcg32& generator) {
    std::array<std::uint32_t, Count> outputs{};
    for (std::uint32_t& output : outputs) {
        output = generator.next();
    }
    return outputs;
}

} // namespace

TEST_CASE("PCG32 gives the paper's outputs for seed 42 on stream 54", "[base]") {
    irama::Pcg32 generator{42, 54};
    constexpr std::array<std::uint32_t, 6> expected{
        0xa15c'02b7, 0x7b47'f409, 0xba1d'3330, 0x83d2'f293, 0xbfa4'784b, 0xcbed'606e,
    };
    REQUIRE(draw<6>(generator) == expected);
}

TEST_CASE("PCG32 runs in a constant expression", "[base]") {
    STATIC_REQUIRE([] {
        irama::Pcg32 generator{42, 54};
        return generator.next();
    }() == 0xa15c'02b7);
}

TEST_CASE("two PCG32 generators with the same seed and stream give the same outputs", "[base]") {
    irama::Pcg32 first{0x1234'5678'9abc'def0, 7};
    irama::Pcg32 second{0x1234'5678'9abc'def0, 7};
    REQUIRE(draw<1'000>(first) == draw<1'000>(second));
}

TEST_CASE("PCG32's seed and stream each choose a different sequence", "[base]") {
    irama::Pcg32 reference{1, 1};
    irama::Pcg32 other_seed{2, 1};
    irama::Pcg32 other_stream{1, 2};
    const auto outputs = draw<16>(reference);
    REQUIRE(draw<16>(other_seed) != outputs);
    REQUIRE(draw<16>(other_stream) != outputs);
}

TEST_CASE("a default PCG32 is seed 0 on stream 0", "[base]") {
    irama::Pcg32 by_default;
    irama::Pcg32 zero{0, 0};
    REQUIRE(draw<16>(by_default) == draw<16>(zero));
}

TEST_CASE("splitting a PCG32 seeds a new generator from four of its outputs", "[base]") {
    irama::Pcg32 generator{42, 54};
    irama::Pcg32 copy = generator;
    const auto outputs = draw<4>(copy);
    irama::Pcg32 expected{(std::uint64_t{outputs.at(0)} << 32U) | outputs.at(1),
                          (std::uint64_t{outputs.at(2)} << 32U) | outputs.at(3)};
    irama::Pcg32 split = generator.split();
    REQUIRE(draw<16>(split) == draw<16>(expected));
    // The generator has moved on past the four outputs.
    REQUIRE(draw<16>(generator) == draw<16>(copy));
}

TEST_CASE("each generator split from a PCG32 gives a different sequence", "[base]") {
    irama::Pcg32 generator{1, 1};
    irama::Pcg32 first = generator.split();
    irama::Pcg32 second = generator.split();
    REQUIRE(draw<16>(first) != draw<16>(second));
}
