// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/source/noise.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/pcg32.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

// Renders as a voice does, inside a render scope, so the real-time checks watch the noise.
void render(irama::Noise& noise, std::span<float> out) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    noise.render(out);
}

[[nodiscard]] std::vector<float> render(irama::Noise& noise, std::size_t frames) {
    std::vector<float> out(frames);
    render(noise, out);
    return out;
}

} // namespace

TEST_CASE("each noise sample is the top 24 bits of the generator's next output, from -1 to 1",
          "[source]") {
    irama::Noise noise;
    noise.start(irama::Pcg32{42, 54});
    irama::Pcg32 generator{42, 54};
    for (const float sample : render(noise, 100)) {
        const auto top_bits = static_cast<float>(generator.next() >> 8U);
        REQUIRE(sample == (top_bits / 8'388'608.0F) - 1.0F);
    }
}

TEST_CASE("noise stays from -1 up to below 1, around 0, at the level of uniform noise",
          "[source]") {
    irama::Noise noise;
    noise.start(irama::Pcg32{7, 3});
    const auto out = render(noise, 65'536);
    REQUIRE(
        std::ranges::all_of(out, [](float sample) { return sample >= -1.0F && sample < 1.0F; }));
    double sum = 0.0;
    double squares = 0.0;
    for (const float sample : out) {
        sum += static_cast<double>(sample);
        squares += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const auto count = static_cast<double>(out.size());
    REQUIRE_THAT(sum / count, WithinAbs(0.0, 0.01));
    // Uniform noise from −1 to 1 has an RMS level of 1/√3.
    REQUIRE_THAT(std::sqrt(squares / count), WithinAbs(1.0 / std::sqrt(3.0), 0.01));
}

TEST_CASE("noise carries on from where its last render ended", "[source]") {
    irama::Noise whole;
    irama::Noise split;
    whole.start(irama::Pcg32{1, 2});
    split.start(irama::Pcg32{1, 2});
    const auto once = render(whole, 500);
    std::vector<float> twice(500);
    render(split, std::span{twice}.first(123));
    render(split, std::span{twice}.subspan(123));
    REQUIRE(once == twice);
}

TEST_CASE("noise from different streams differs", "[source]") {
    irama::Noise first;
    irama::Noise second;
    first.start(irama::Pcg32{1, 1});
    second.start(irama::Pcg32{1, 2});
    REQUIRE(render(first, 64) != render(second, 64));
}
