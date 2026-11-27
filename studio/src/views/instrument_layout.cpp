// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/instrument_layout.hpp"

#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace irama_studio {
namespace {

// The groups, in the order the window shows them.
constexpr std::array<std::string_view, 5> group_titles{
    "Oscillator", "Filter", "Amplifier", "Envelope", "Other",
};
constexpr std::size_t oscillator_group = 0;
constexpr std::size_t other_group = group_titles.size() - 1;

// The first ID of the source's blocks. Every block below it is a thousand IDs wide.
constexpr std::uint32_t first_source_id = 10'000;
constexpr std::uint32_t block_width = 1'000;

// The group a parameter's ID falls in: the source's blocks, then the filter's, the amplifier's and
// the envelope's, which are the thousands from 1000 to 3999 in the order of their groups.
[[nodiscard]] std::size_t group_of(irama::ParamId id) {
    const std::uint32_t value = std::to_underlying(id);
    if (value >= first_source_id) {
        return oscillator_group;
    }
    const std::size_t block = value / block_width;
    return block >= 1 && block < other_group ? block : other_group;
}

} // namespace

ParamWidget widget_for(const irama::ParamDescriptor& param) {
    if (param.scale != irama::ParamScale::stepped) {
        return ParamWidget::slider;
    }
    return param.labels.empty() ? ParamWidget::integer_slider : ParamWidget::combo;
}

std::vector<ParamGroup> group_params(std::span<const irama::ParamDescriptor> table) {
    std::array<std::vector<const irama::ParamDescriptor*>, group_titles.size()> members;
    for (const irama::ParamDescriptor& param : table) {
        members.at(group_of(param.id)).push_back(&param);
    }
    std::vector<ParamGroup> groups;
    for (std::size_t group = 0; group < group_titles.size(); ++group) {
        if (!members.at(group).empty()) {
            groups.push_back(
                {.title = group_titles.at(group), .params = std::move(members.at(group))});
        }
    }
    return groups;
}

} // namespace irama_studio
