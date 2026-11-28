// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/document.hpp"

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace irama_studio {
namespace {

constexpr std::size_t new_song_tracks = 4;
constexpr std::size_t new_song_lines = 64;

} // namespace

float Instrument::value(irama::ParamId id) const {
    const std::optional<std::size_t> index = irama::find_param(instrument_table(), id);
    IRAMA_ASSERT(index.has_value(), "a parameter's ID is one of the instrument's");
    return values.at(*index);
}

std::span<const irama::ParamDescriptor> instrument_table() { return irama::instrument_params; }

Instrument default_instrument() {
    Instrument instrument;
    instrument.values.reserve(instrument_table().size());
    for (const irama::ParamDescriptor& param : instrument_table()) {
        instrument.values.push_back(param.default_value);
    }
    return instrument;
}

Document::Document() : patterns_(pattern_count), instruments_(instrument_count) {
    for (std::size_t track = 1; track <= new_song_tracks; ++track) {
        tracks_.push_back({.name = std::format("Track {}", track)});
    }
    patterns_.front() = empty_pattern(*this, new_song_lines);
    sequence_.slots.push_back(0);
    instruments_.front() = default_instrument();
}

const std::optional<Pattern>& Document::pattern(std::size_t number) const {
    IRAMA_ASSERT(number < pattern_count, "a pattern number is from 000 to 999");
    return patterns_.at(number);
}

const std::optional<Instrument>& Document::instrument(std::size_t number) const {
    IRAMA_ASSERT(number >= first_instrument && number <= last_instrument,
                 "an instrument number is from 01 to FE");
    return instruments_.at(number - first_instrument);
}

Pattern empty_pattern(const Document& document, std::size_t lines) {
    Pattern pattern{.head = {.lines = lines}};
    for (const Track& track : document.tracks()) {
        pattern.blocks.push_back({
            .notes = std::vector<NoteCell>(lines * track.note_columns),
            .effects = std::vector<EffectCell>(lines * track.effect_columns),
        });
    }
    return pattern;
}

std::optional<std::size_t> lowest_free_pattern(const Document& document) {
    for (std::size_t number = 0; number < Document::pattern_count; ++number) {
        if (!document.pattern(number)) {
            return number;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> lowest_free_instrument(const Document& document) {
    for (std::size_t number = Document::first_instrument; number <= Document::last_instrument;
         ++number) {
        if (!document.instrument(number)) {
            return number;
        }
    }
    return std::nullopt;
}

} // namespace irama_studio
