// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "views/instrument_layout.hpp"

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using irama::ParamDescriptor;
using irama::ParamId;
using irama::ParamScale;
using irama_studio::group_params;
using irama_studio::ParamGroup;
using irama_studio::ParamWidget;
using irama_studio::widget_for;

namespace {

constexpr std::array<std::string_view, 2> labels{"Off", "On"};

[[nodiscard]] constexpr ParamDescriptor param(std::uint32_t id,
                                              ParamScale scale = ParamScale::linear,
                                              std::span<const std::string_view> names = {}) {
    return ParamDescriptor{
        .id = ParamId{id},
        .min = scale == ParamScale::logarithmic ? 1.0F : 0.0F,
        .max = static_cast<float>(names.empty() ? 4 : names.size() - 1),
        .default_value = 1.0F,
        .scale = scale,
        .labels = names,
    };
}

// The title of the group a parameter's ID falls in, written out block by block.
[[nodiscard]] std::string_view expected_title(ParamId id) {
    const std::uint32_t value = std::to_underlying(id);
    if (value >= 10'000) {
        return "Oscillator";
    }
    switch (value / 1'000) {
    case 1:
        return "Filter";
    case 2:
        return "Amplifier";
    case 3:
        return "Envelope";
    default:
        return "Other";
    }
}

// The widget a parameter's descriptor asks for, written out case by case.
[[nodiscard]] ParamWidget expected_widget(const ParamDescriptor& descriptor) {
    if (descriptor.scale != ParamScale::stepped) {
        return ParamWidget::slider;
    }
    return descriptor.labels.empty() ? ParamWidget::integer_slider : ParamWidget::combo;
}

[[nodiscard]] std::vector<std::string_view> titles(const std::vector<ParamGroup>& groups) {
    std::vector<std::string_view> result;
    result.reserve(groups.size());
    for (const ParamGroup& group : groups) {
        result.push_back(group.title);
    }
    return result;
}

// The titles of the groups that hold the parameter, once for each time a group holds it.
[[nodiscard]] std::vector<std::string_view> titles_holding(const std::vector<ParamGroup>& groups,
                                                           const ParamDescriptor& descriptor) {
    std::vector<std::string_view> result;
    for (const ParamGroup& group : groups) {
        const auto count = std::ranges::count(group.params, &descriptor);
        result.insert(result.end(), static_cast<std::size_t>(count), group.title);
    }
    return result;
}

} // namespace

TEST_CASE("a parameter's widget follows its scale and labels", "[studio][instrument]") {
    CHECK(widget_for(param(1, ParamScale::linear)) == ParamWidget::slider);
    CHECK(widget_for(param(1, ParamScale::logarithmic)) == ParamWidget::slider);
    CHECK(widget_for(param(1, ParamScale::stepped, labels)) == ParamWidget::combo);
    CHECK(widget_for(param(1, ParamScale::stepped)) == ParamWidget::integer_slider);
}

TEST_CASE("the instrument's parameters each appear once, in their block's group, with the widget "
          "their descriptor asks for",
          "[studio][instrument]") {
    const std::vector<ParamGroup> groups = group_params(irama::instrument_params);
    CHECK(titles(groups) ==
          std::vector<std::string_view>{"Oscillator", "Filter", "Amplifier", "Envelope"});

    for (const ParamDescriptor& descriptor : irama::instrument_params) {
        INFO(descriptor.name);
        CHECK(titles_holding(groups, descriptor) ==
              std::vector<std::string_view>{expected_title(descriptor.id)});
        CHECK(widget_for(descriptor) == expected_widget(descriptor));
    }
    std::size_t grouped = 0;
    for (const ParamGroup& group : groups) {
        grouped += group.params.size();
    }
    CHECK(grouped == irama::instrument_params.size());
}

TEST_CASE("groups come in a fixed order, keep their table's order, and leave none out",
          "[studio][instrument]") {
    static constexpr std::array table{
        param(2'001),  param(10'090), param(7'000), param(3'000),
        param(10'000), param(2'000),  param(1'500), param(20'000),
    };
    const std::vector<ParamGroup> groups = group_params(table);
    const std::vector<ParamGroup> expected{
        {.title = "Oscillator", .params = {&table.at(1), &table.at(4), &table.at(7)}},
        {.title = "Filter", .params = {&table.at(6)}},
        {.title = "Amplifier", .params = {&table.at(0), &table.at(5)}},
        {.title = "Envelope", .params = {&table.at(3)}},
        {.title = "Other", .params = {&table.at(2)}},
    };
    CHECK(groups == expected);
}

TEST_CASE("a group with no parameters isn't shown", "[studio][instrument]") {
    static constexpr std::array table{param(1'000), param(3'001)};
    CHECK(titles(group_params(table)) == std::vector<std::string_view>{"Filter", "Envelope"});
    CHECK(group_params({}).empty());
}
