// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/test_support/spectrum.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

// A signal with no pattern an FFT could get right by accident.
[[nodiscard]] std::vector<std::complex<double>> scattered(std::size_t size) {
    std::vector<std::complex<double>> values(size);
    for (std::size_t index = 0; index < size; ++index) {
        const auto n = static_cast<double>(index);
        values.at(index) = {std::sin((n * n * 0.37) + 1.0), std::cos((n * 1.71) - (n * n * 0.05))};
    }
    return values;
}

// A sine of an amplitude at a frequency, sampled at a rate.
[[nodiscard]] std::vector<float> sine(std::size_t size, double amplitude, double frequency,
                                      double sample_rate) {
    std::vector<float> signal(size);
    for (std::size_t index = 0; index < size; ++index) {
        signal.at(index) =
            static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                                    static_cast<double>(index) / sample_rate));
    }
    return signal;
}

// Adds one signal into another.
void add(std::vector<float>& signal, const std::vector<float>& other) {
    for (std::size_t index = 0; index < signal.size(); ++index) {
        signal.at(index) += other.at(index);
    }
}

constexpr std::size_t analysis_size = 65'536;

} // namespace

TEST_CASE("the FFT matches a direct DFT at small sizes", "[test support]") {
    const std::size_t size = GENERATE(1U, 2U, 4U, 8U, 16U, 32U, 256U);
    const auto signal = scattered(size);
    auto transformed = signal;
    irama_test_support::fft(transformed);
    const auto expected = irama_test_support::dft(signal);
    for (std::size_t bin = 0; bin < size; ++bin) {
        REQUIRE_THAT(transformed.at(bin).real(), WithinAbs(expected.at(bin).real(), 1e-9));
        REQUIRE_THAT(transformed.at(bin).imag(), WithinAbs(expected.at(bin).imag(), 1e-9));
    }
}

TEST_CASE("the direct DFT of a cosine on a bin is half its length in that bin and its mirror",
          "[test support]") {
    constexpr std::size_t size = 16;
    std::vector<std::complex<double>> signal(size);
    for (std::size_t index = 0; index < size; ++index) {
        signal.at(index) = std::cos(2.0 * std::numbers::pi * 3.0 * static_cast<double>(index) /
                                    static_cast<double>(size));
    }
    const auto transformed = irama_test_support::dft(signal);
    for (std::size_t bin = 0; bin < size; ++bin) {
        const double expected = (bin == 3 || bin == size - 3) ? 8.0 : 0.0;
        REQUIRE_THAT(std::abs(transformed.at(bin)), WithinAbs(expected, 1e-9));
    }
}

TEST_CASE(
    "the Blackman-Harris window is periodic, symmetric, near 0 at its ends and 1 at its centre",
    "[test support]") {
    constexpr std::size_t size = 64;
    const auto window = irama_test_support::blackman_harris(size);
    REQUIRE(window.size() == size);
    REQUIRE(window.front() < 1e-7);
    REQUIRE_THAT(window.at(size / 2), WithinAbs(1.0, 1e-12));
    for (std::size_t index = 1; index < size; ++index) {
        REQUIRE_THAT(window.at(index), WithinAbs(window.at(size - index), 1e-12));
    }
}

TEST_CASE("a component between the harmonics measures its level below the fundamental",
          "[test support]") {
    // At a rate equal to the length, each frequency in hertz falls exactly on its bin.
    const auto rate = static_cast<double>(analysis_size);
    auto signal = sine(analysis_size, 0.5, 1'000.0, rate);
    add(signal, sine(analysis_size, 0.5e-3, 1'500.0, rate));
    REQUIRE_THAT(irama_test_support::strongest_non_harmonic(signal, 1'000.0, rate, rate / 2.0),
                 WithinAbs(-60.0, 0.01));
}

TEST_CASE("a harmonic, however strong, isn't counted", "[test support]") {
    const auto rate = static_cast<double>(analysis_size);
    auto signal = sine(analysis_size, 0.5, 1'000.0, rate);
    add(signal, sine(analysis_size, 0.25, 2'000.0, rate));
    add(signal, sine(analysis_size, 0.25, 31'000.0, rate));
    add(signal, sine(analysis_size, 0.5e-4, 2'500.0, rate));
    REQUIRE_THAT(irama_test_support::strongest_non_harmonic(signal, 1'000.0, rate, rate / 2.0),
                 WithinAbs(-80.0, 0.01));
}

TEST_CASE("a component off its bin measures within the window's scalloping", "[test support]") {
    auto signal = sine(analysis_size, 0.5, 440.0, 48'000.0);
    add(signal, sine(analysis_size, 0.5e-3, 3'333.3, 48'000.0));
    REQUIRE_THAT(irama_test_support::strongest_non_harmonic(signal, 440.0, 48'000.0, 24'000.0),
                 WithinAbs(-60.0, 1.0));
}

TEST_CASE("a pure sine in float has nothing between its harmonics above 120 dB down",
          "[test support]") {
    const auto signal = sine(analysis_size, 1.0, 440.0, 48'000.0);
    REQUIRE(irama_test_support::strongest_non_harmonic(signal, 440.0, 48'000.0, 24'000.0) < -120.0);
}

TEST_CASE("a component above the highest frequency isn't counted", "[test support]") {
    const auto rate = static_cast<double>(analysis_size);
    auto signal = sine(analysis_size, 0.5, 1'000.0, rate);
    add(signal, sine(analysis_size, 0.5e-2, 9'500.0, rate));
    add(signal, sine(analysis_size, 0.5e-4, 2'500.0, rate));
    REQUIRE_THAT(irama_test_support::strongest_non_harmonic(signal, 1'000.0, rate, 8'000.0),
                 WithinAbs(-80.0, 0.01));
    REQUIRE_THAT(irama_test_support::strongest_non_harmonic(signal, 1'000.0, rate, 10'000.0),
                 WithinAbs(-40.0, 0.01));
}
