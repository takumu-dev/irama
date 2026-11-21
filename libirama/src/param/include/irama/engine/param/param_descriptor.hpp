// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The descriptor of a parameter, and the checks of a table of them.

#pragma once

#include <irama/engine/param/param_id.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace irama {

/// The unit a parameter's plain value is in.
enum class ParamUnit : std::uint8_t {
    none,
    hertz,
    decibels,
    seconds,
    milliseconds,
    /// From 0 to 100, not from 0 to 1.
    percent,
    semitones,
    cents,
};

/// How a parameter's plain value maps to its normalised value, from 0 to 1.
enum class ParamScale : std::uint8_t {
    /// Evenly from the minimum to the maximum.
    linear,
    /// Evenly in octaves from the minimum, which is above 0, to the maximum.
    logarithmic,
    /// As `linear`, but only whole numbers, each one a step.
    stepped,
};

/// What a parameter is: its ID, how to show it, its range and default, and how it is smoothed.
///
/// A parameter's value is held as a **plain value**, in its own unit: the player's value table,
/// set-parameter messages and files hold plain values. A **normalised value**, from 0 to 1, exists
/// only at the edges, for a slider, through `to_normalized()` and `from_normalized()`. An
/// instrument's descriptors are a `constexpr` table with static storage.
struct ParamDescriptor {
    /// The parameter's ID, unique within its table.
    ParamId id{};
    /// The name shown for it.
    std::string_view name{};
    /// The unit of its plain value.
    ParamUnit unit = ParamUnit::none;
    /// How many decimal places to show its value with.
    std::uint8_t precision = 0;
    /// The lowest plain value, above 0 for a logarithmic parameter.
    float min = 0.0F;
    /// The highest plain value, above `min`.
    float max = 1.0F;
    /// The plain value a player starts with, from `min` to `max`.
    float default_value = 0.0F;
    /// How the plain value maps to the normalised value.
    ParamScale scale = ParamScale::linear;
    /// For a stepped parameter, one name for each step from `min` to `max`, or none to show the
    /// number itself.
    std::span<const std::string_view> labels{};
    /// How long a new value takes to reach while a note sounds, in seconds, or 0 to switch at its
    /// frame. A stepped parameter always switches.
    float smoothing_seconds = 0.0F;
    /// Whether automation may change it.
    bool automatable = true;
    /// Whether modulation may change it.
    bool modulatable = true;

    /// Maps a plain value to its normalised value, `(v − min) / (max − min)` for a linear or
    /// stepped parameter, and `log(v / min) / log(max / min)` for a logarithmic one.
    ///
    /// \param value A plain value, from `min` to `max`.
    /// \return Its normalised value, from 0 to 1.
    [[nodiscard]] float to_normalized(float value) const;

    /// Maps a normalised value back to its plain value, undoing `to_normalized()`. A stepped value
    /// comes back as it is, unrounded.
    ///
    /// \param normalized A normalised value, from 0 to 1.
    /// \return Its plain value, from `min` to `max`.
    [[nodiscard]] float from_normalized(float normalized) const;

    /// Makes any value one the parameter can hold: clamped to `[min, max]`, and for a stepped
    /// parameter rounded to the nearest whole number, halves away from 0.
    ///
    /// \param value Any value but NaN, infinities included.
    /// \return The value the parameter takes.
    [[nodiscard]] float constrain(float value) const;
};

/// Finds a parameter in a table by its ID.
///
/// \param table The table to look in.
/// \param id The ID to look for.
/// \return The parameter's index in the table, or nothing if no parameter has the ID.
[[nodiscard]] constexpr std::optional<std::size_t>
find_param(std::span<const ParamDescriptor> table, ParamId id) {
    for (std::size_t index = 0; const ParamDescriptor& param : table) {
        if (param.id == id) {
            return index;
        }
        ++index;
    }
    return std::nullopt;
}

/// What is wrong with a table of descriptors.
enum class TableFault : std::uint8_t {
    /// Nothing.
    none,
    /// Two parameters share an ID.
    duplicate_id,
    /// A minimum isn't below its maximum.
    empty_range,
    /// A default is outside its range.
    default_out_of_range,
    /// A logarithmic parameter's minimum isn't above 0.
    logarithmic_from_zero,
    /// A stepped parameter's minimum, maximum or default isn't a whole number.
    fractional_step,
    /// A parameter has labels, but not one for each step, or isn't stepped.
    label_count,
    /// A smoothing time is below 0.
    negative_smoothing,
    /// A stepped parameter has a smoothing time.
    smoothed_step,
};

/// Checks a table of descriptors, so that a test can check every table.
///
/// \param table The table to check.
/// \return The first fault found, parameter by parameter, or `TableFault::none`.
[[nodiscard]] constexpr TableFault find_table_fault(std::span<const ParamDescriptor> table) {
    const auto whole = [](float value) {
        return value == static_cast<float>(static_cast<std::int64_t>(value));
    };
    for (std::size_t index = 0; const ParamDescriptor& param : table) {
        if (find_param(table.first(index++), param.id)) {
            return TableFault::duplicate_id;
        }
        if (!(param.min < param.max)) {
            return TableFault::empty_range;
        }
        if (param.default_value < param.min || param.default_value > param.max) {
            return TableFault::default_out_of_range;
        }
        if (param.scale == ParamScale::logarithmic && !(param.min > 0.0F)) {
            return TableFault::logarithmic_from_zero;
        }
        const bool stepped = param.scale == ParamScale::stepped;
        if (stepped && !(whole(param.min) && whole(param.max) && whole(param.default_value))) {
            return TableFault::fractional_step;
        }
        if (!param.labels.empty() &&
            (!stepped || static_cast<float>(param.labels.size()) != param.max - param.min + 1.0F)) {
            return TableFault::label_count;
        }
        if (!(param.smoothing_seconds >= 0.0F)) {
            return TableFault::negative_smoothing;
        }
        if (stepped && param.smoothing_seconds != 0.0F) {
            return TableFault::smoothed_step;
        }
    }
    return TableFault::none;
}

} // namespace irama
