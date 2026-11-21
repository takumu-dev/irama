// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "ui/param_text.hpp"

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

using irama::ParamDescriptor;
using irama::ParamScale;
using irama::ParamUnit;
using irama_studio::format_param_value;
using irama_studio::parse_param_value;

namespace {

constexpr std::array<std::string_view, 3> shapes{"Sine", "Saw", "Pulse"};

[[nodiscard]] constexpr ParamDescriptor param(ParamUnit unit, std::uint8_t precision, float min,
                                              float max, ParamScale scale = ParamScale::linear) {
    return ParamDescriptor{
        .unit = unit,
        .precision = precision,
        .min = min,
        .max = max,
        .default_value = min,
        .scale = scale,
    };
}

constexpr ParamDescriptor plain = param(ParamUnit::none, 2, -1.0F, 1.0F);
constexpr ParamDescriptor frequency =
    param(ParamUnit::hertz, 0, 20.0F, 20'000.0F, ParamScale::logarithmic);
constexpr ParamDescriptor level = param(ParamUnit::decibels, 1, -60.0F, 12.0F);
constexpr ParamDescriptor duration = param(ParamUnit::seconds, 3, 0.001F, 10.0F);
constexpr ParamDescriptor delay = param(ParamUnit::milliseconds, 0, 0.0F, 500.0F);
constexpr ParamDescriptor share = param(ParamUnit::percent, 1, 0.0F, 100.0F);
constexpr ParamDescriptor transpose =
    param(ParamUnit::semitones, 0, -24.0F, 24.0F, ParamScale::stepped);
constexpr ParamDescriptor detune = param(ParamUnit::cents, 1, -100.0F, 100.0F);
constexpr ParamDescriptor shape{
    .min = 0.0F,
    .max = 2.0F,
    .default_value = 0.0F,
    .scale = ParamScale::stepped,
    .labels = shapes,
};

} // namespace

TEST_CASE("a value is written to its parameter's precision, with its unit's symbol",
          "[studio][param-text]") {
    CHECK(format_param_value(plain, 0.5F) == "0.50");
    CHECK(format_param_value(plain, -0.25F) == "-0.25");
    CHECK(format_param_value(frequency, 440.0F) == "440 Hz");
    CHECK(format_param_value(frequency, 1'234.6F) == "1235 Hz");
    CHECK(format_param_value(level, -12.0F) == "-12.0 dB");
    CHECK(format_param_value(level, 3.25F) == "3.2 dB");
    CHECK(format_param_value(duration, 0.3F) == "0.300 s");
    CHECK(format_param_value(delay, 25.4F) == "25 ms");
    CHECK(format_param_value(share, 50.0F) == "50.0%");
    CHECK(format_param_value(transpose, -12.0F) == "-12 st");
    CHECK(format_param_value(detune, -100.0F) == "-100.0 ct");
}

TEST_CASE("semitones and cents above 0 take a plus sign", "[studio][param-text]") {
    CHECK(format_param_value(transpose, 7.0F) == "+7 st");
    CHECK(format_param_value(detune, 5.5F) == "+5.5 ct");
    CHECK(format_param_value(transpose, 0.0F) == "0 st");
    CHECK(format_param_value(detune, 0.0F) == "0.0 ct");
    CHECK(format_param_value(plain, 0.5F) == "0.50");
}

TEST_CASE("a value that rounds to 0 isn't written with a minus sign", "[studio][param-text]") {
    CHECK(format_param_value(plain, -0.0F) == "0.00");
    CHECK(format_param_value(plain, -0.001F) == "0.00");
    CHECK(format_param_value(level, -0.04F) == "0.0 dB");
    CHECK(format_param_value(detune, -0.01F) == "0.0 ct");
}

TEST_CASE("a stepped value with labels is written as its label", "[studio][param-text]") {
    CHECK(format_param_value(shape, 0.0F) == "Sine");
    CHECK(format_param_value(shape, 2.0F) == "Pulse");
}

TEST_CASE("every instrument parameter's default writes as text", "[studio][param-text]") {
    for (const ParamDescriptor& descriptor : irama::instrument_params) {
        INFO(descriptor.name);
        CHECK_FALSE(format_param_value(descriptor, descriptor.default_value).empty());
    }
}

TEST_CASE("a typed value reads with or without its unit, and is constrained",
          "[studio][param-text]") {
    CHECK(parse_param_value(frequency, "440") == 440.0F);
    CHECK(parse_param_value(frequency, " 1000 Hz ") == 1'000.0F);
    CHECK(parse_param_value(frequency, "1000hz") == 1'000.0F);
    CHECK(parse_param_value(level, "-6.5 dB") == -6.5F);
    CHECK(parse_param_value(share, "25%") == 25.0F);
    CHECK(parse_param_value(transpose, "+7") == 7.0F);
    CHECK(parse_param_value(transpose, "7.6 st") == 8.0F);
    CHECK(parse_param_value(frequency, "1e6") == 20'000.0F);
    CHECK(parse_param_value(level, "-1000") == -60.0F);
}

TEST_CASE("a parameter with labels reads a label in any case, or its number",
          "[studio][param-text]") {
    CHECK(parse_param_value(shape, "saw") == 1.0F);
    CHECK(parse_param_value(shape, " PULSE ") == 2.0F);
    CHECK(parse_param_value(shape, "0") == 0.0F);
}

TEST_CASE("text that isn't a value of the parameter reads as nothing", "[studio][param-text]") {
    CHECK_FALSE(parse_param_value(frequency, "").has_value());
    CHECK_FALSE(parse_param_value(frequency, "   ").has_value());
    CHECK_FALSE(parse_param_value(frequency, "loud").has_value());
    CHECK_FALSE(parse_param_value(frequency, "440 dB").has_value());
    CHECK_FALSE(parse_param_value(frequency, "nan").has_value());
    CHECK_FALSE(parse_param_value(frequency, "+").has_value());
    CHECK_FALSE(parse_param_value(shape, "square").has_value());
}
