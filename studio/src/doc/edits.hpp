// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include <irama/engine/param/param_id.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace irama_studio {

/// Sets the song's title.
[[nodiscard]] Edit set_title(std::string title);

/// Sets the song's author.
[[nodiscard]] Edit set_author(std::string author);

/// Sets the song's comments.
[[nodiscard]] Edit set_comments(std::string comments);

/// Sets the seed of every random source.
[[nodiscard]] Edit set_seed(std::uint64_t seed);

/// Adds a song parameter after the others.
[[nodiscard]] Edit add_song_parameter(SongParameter parameter);

/// Replaces the song parameter at `index`: its name, range and default.
[[nodiscard]] Edit change_song_parameter(std::size_t index, SongParameter parameter);

/// Deletes the song parameter at `index`.
[[nodiscard]] Edit delete_song_parameter(std::size_t index);

/// Adds a marker on a line of a pattern that has none.
[[nodiscard]] Edit add_marker(std::size_t pattern, Marker marker);

/// Renames the marker on a line of a pattern.
[[nodiscard]] Edit rename_marker(std::size_t pattern, std::size_t line, std::string name);

/// Deletes the marker on a line of a pattern.
[[nodiscard]] Edit delete_marker(std::size_t pattern, std::size_t line);

/// Puts an instrument with every default and an empty name at an empty instrument number.
[[nodiscard]] Edit new_instrument(std::size_t number);

/// Copies an instrument, name and values, to an empty instrument number.
[[nodiscard]] Edit clone_instrument(std::size_t from, std::size_t to);

/// Empties an instrument number. Pattern data that names it is left alone.
[[nodiscard]] Edit delete_instrument(std::size_t number);

/// Renames an instrument.
[[nodiscard]] Edit rename_instrument(std::size_t number, std::string name);

/// Sets one of an instrument's parameter values, clamped to the parameter's range and rounded to a
/// whole number if it is stepped. It is a step of a gesture on that one value, so the values a
/// slider passes through while dragged merge into one step.
///
/// \param number The instrument number.
/// \param id The ID of one of the instrument's parameters, which is checked.
/// \param value The new plain value.
[[nodiscard]] Edit set_instrument_value(std::size_t number, irama::ParamId id, float value);

} // namespace irama_studio
