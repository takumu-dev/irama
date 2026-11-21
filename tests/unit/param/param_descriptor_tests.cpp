// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/param/param_descriptor.hpp>

#include <irama/engine/param/param_id.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>

namespace {

using Catch::Matchers::WithinAbs;

constexpr std::array<std::string_view, 3> shape_labels{"Round", "Square", "Spiky"};

constexpr irama::ParamDescriptor level{
    .id = irama::ParamId{1},
    .name = "Level",
    .unit = irama::ParamUnit::percent,
    .precision = 1,
    .min = 0.0F,
    .max = 100.0F,
    .default_value = 50.0F,
    .scale = irama::ParamScale::linear,
    .smoothing_seconds = 0.02F,
};

constexpr irama::ParamDescriptor frequency{
    .id = irama::ParamId{2},
    .name = "Frequency",
    .unit = irama::ParamUnit::hertz,
    .min = 20.0F,
    .max = 20'000.0F,
    .default_value = 1'000.0F,
    .scale = irama::ParamScale::logarithmic,
    .smoothing_seconds = 0.02F,
};

constexpr irama::ParamDescriptor shape{
    .id = irama::ParamId{3},
    .name = "Shape",
    .min = 0.0F,
    .max = 2.0F,
    .default_value = 1.0F,
    .scale = irama::ParamScale::stepped,
    .labels = shape_labels,
    .modulatable = false,
};

constexpr irama::ParamDescriptor transpose{
    .id = irama::ParamId{4},
    .name = "Transpose",
    .unit = irama::ParamUnit::semitones,
    .min = -12.0F,
    .max = 12.0F,
    .default_value = 0.0F,
    .scale = irama::ParamScale::stepped,
    .modulatable = false,
};

constexpr std::array table{level, frequency, shape, transpose};

[[nodiscard]] constexpr irama::TableFault fault_with(irama::ParamDescriptor changed) {
    std::array<irama::ParamDescriptor, 4> faulty = table;
    faulty.at(1) = changed;
    return irama::find_table_fault(faulty);
}

} // namespace

TEST_CASE("a linear parameter's normalised value is its place between its bounds", "[param]") {
    REQUIRE(level.to_normalized(0.0F) == 0.0F);
    REQUIRE(level.to_normalized(25.0F) == 0.25F);
    REQUIRE(level.to_normalized(100.0F) == 1.0F);
    REQUIRE(transpose.to_normalized(6.0F) == 0.75F);
    REQUIRE(level.from_normalized(0.25F) == 25.0F);
    REQUIRE(transpose.from_normalized(0.25F) == -6.0F);
}

TEST_CASE("a logarithmic parameter's normalised value moves evenly in octaves", "[param]") {
    // 20 Hz to 20 kHz is log2(1000) octaves, so each octave above 20 Hz is the same share of it.
    const double octave = 1.0 / std::log2(1'000.0);
    REQUIRE(frequency.to_normalized(20.0F) == 0.0F);
    REQUIRE_THAT(frequency.to_normalized(40.0F), WithinAbs(octave, 1e-6));
    REQUIRE_THAT(frequency.to_normalized(640.0F), WithinAbs(5.0 * octave, 1e-6));
    REQUIRE_THAT(frequency.to_normalized(20'000.0F), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(frequency.from_normalized(static_cast<float>(octave)), WithinAbs(40.0, 1e-3));
    REQUIRE_THAT(frequency.from_normalized(0.5F), WithinAbs(20.0 * std::sqrt(1'000.0), 1e-2));
}

TEST_CASE("a value goes to its normalised value and back", "[param]") {
    const float value = GENERATE(20.0F, 55.5F, 440.0F, 1'234.5F, 19'999.0F);
    REQUIRE_THAT(frequency.from_normalized(frequency.to_normalized(value)),
                 WithinAbs(static_cast<double>(value), static_cast<double>(value) * 1e-5));
}

TEST_CASE("constraining a value clamps it to its range", "[param]") {
    REQUIRE(level.constrain(42.5F) == 42.5F);
    REQUIRE(level.constrain(100.0001F) == 100.0F);
    REQUIRE(level.constrain(-3.0F) == 0.0F);
    REQUIRE(level.constrain(std::numeric_limits<float>::infinity()) == 100.0F);
    REQUIRE(frequency.constrain(-std::numeric_limits<float>::infinity()) == 20.0F);
}

TEST_CASE("constraining a stepped value rounds it to the nearest whole number", "[param]") {
    REQUIRE(transpose.constrain(2.4F) == 2.0F);
    REQUIRE(transpose.constrain(2.5F) == 3.0F);
    REQUIRE(transpose.constrain(-2.5F) == -3.0F);
    REQUIRE(transpose.constrain(-0.4F) == 0.0F);
    REQUIRE(shape.constrain(7.0F) == 2.0F);
    // A continuous value is never rounded.
    REQUIRE(level.constrain(2.4F) == 2.4F);
}

TEST_CASE("a parameter is found in its table by its ID", "[param]") {
    REQUIRE(irama::find_param(table, irama::ParamId{1}) == std::size_t{0});
    REQUIRE(irama::find_param(table, irama::ParamId{4}) == std::size_t{3});
    REQUIRE_FALSE(irama::find_param(table, irama::ParamId{5}).has_value());
    STATIC_REQUIRE(irama::find_param(table, irama::ParamId{3}) == std::size_t{2});
}

TEST_CASE("a well-formed table has no fault", "[param]") {
    STATIC_REQUIRE(irama::find_table_fault(table) == irama::TableFault::none);
}

TEST_CASE("each fault of a table is found", "[param]") {
    using irama::TableFault;
    auto changed = frequency;
    changed.id = irama::ParamId{1};
    REQUIRE(fault_with(changed) == TableFault::duplicate_id);

    changed = frequency;
    changed.min = 20'000.0F;
    REQUIRE(fault_with(changed) == TableFault::empty_range);

    changed = frequency;
    changed.default_value = 10.0F;
    REQUIRE(fault_with(changed) == TableFault::default_out_of_range);
    changed.default_value = 20'001.0F;
    REQUIRE(fault_with(changed) == TableFault::default_out_of_range);

    changed = level;
    changed.id = irama::ParamId{2};
    changed.scale = irama::ParamScale::logarithmic;
    REQUIRE(fault_with(changed) == TableFault::logarithmic_from_zero);

    changed = transpose;
    changed.id = irama::ParamId{2};
    changed.max = 12.5F;
    REQUIRE(fault_with(changed) == TableFault::fractional_step);
    changed.max = 12.0F;
    changed.default_value = 0.5F;
    REQUIRE(fault_with(changed) == TableFault::fractional_step);

    changed = shape;
    changed.id = irama::ParamId{2};
    changed.max = 3.0F;
    REQUIRE(fault_with(changed) == TableFault::label_count);
    changed = level;
    changed.id = irama::ParamId{2};
    changed.labels = shape_labels;
    REQUIRE(fault_with(changed) == TableFault::label_count);

    changed = frequency;
    changed.smoothing_seconds = -0.02F;
    REQUIRE(fault_with(changed) == TableFault::negative_smoothing);

    changed = transpose;
    changed.id = irama::ParamId{2};
    changed.smoothing_seconds = 0.02F;
    REQUIRE(fault_with(changed) == TableFault::smoothed_step);
}
