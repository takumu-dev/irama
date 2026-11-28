// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/limits.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include <irama/engine/param/param_descriptor.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace irama_studio {
namespace {

constexpr std::uint8_t last_note = 119;
constexpr std::uint8_t max_volume = 0x80;
constexpr std::uint8_t max_pan = 0x80;
constexpr std::size_t max_lpb = 64;
constexpr std::size_t max_beats_per_bar = 16;
constexpr std::size_t max_ticks_per_line = 32;

using Broken = std::optional<std::string>;

[[nodiscard]] bool in_range(std::size_t value, std::size_t min, std::size_t max) {
    return value >= min && value <= max;
}

[[nodiscard]] Broken check_info(const SongInfo& info) {
    const Timing& timing = info.timing;
    if (timing.bpm < Document::min_bpm || timing.bpm > Document::max_bpm ||
        std::isnan(timing.bpm)) {
        return "BPM is from 20 to 999";
    }
    if (!in_range(timing.lpb, 1, max_lpb)) {
        return "LPB is from 1 to 64";
    }
    if (!in_range(timing.beats_per_bar, 1, max_beats_per_bar)) {
        return "Beats per bar is from 1 to 16";
    }
    if (!in_range(timing.ticks_per_line, 1, max_ticks_per_line)) {
        return "Ticks per line is from 1 to 32";
    }
    return std::nullopt;
}

[[nodiscard]] Broken check_tracks(const std::vector<Track>& tracks) {
    if (!in_range(tracks.size(), 1, Document::max_tracks)) {
        return "A song has from 1 to 64 tracks";
    }
    for (const Track& track : tracks) {
        if (!in_range(track.note_columns, 1, Document::max_note_columns)) {
            return "A track has from 1 to 12 note columns";
        }
        if (track.effect_columns > Document::max_effect_columns) {
            return "A track has at most 8 effect columns";
        }
    }
    return std::nullopt;
}

[[nodiscard]] Broken check_sequence(const Document& document) {
    const Sequence& sequence = document.sequence();
    if (!in_range(sequence.slots.size(), 1, Document::max_slots)) {
        return "A sequence has from 1 to 1,000 slots";
    }
    for (const std::uint16_t number : sequence.slots) {
        if (number >= Document::pattern_count || !document.pattern(number)) {
            return std::format("Slots hold patterns that exist, and pattern {:03} doesn't", number);
        }
    }
    std::set<std::string_view> names;
    std::optional<std::size_t> previous_start;
    for (const Section& section : sequence.sections) {
        if (!is_lookup_name(section.name)) {
            return "A section's name isn't empty and has no space at either end";
        }
        if (!names.insert(section.name).second) {
            return std::format("There is already a section named {}", section.name);
        }
        if (section.start_slot >= sequence.slots.size() ||
            (previous_start && section.start_slot <= *previous_start)) {
            return "Each section starts on its own slot";
        }
        previous_start = section.start_slot;
    }
    return std::nullopt;
}

[[nodiscard]] Broken check_parameters(const std::vector<SongParameter>& parameters) {
    if (parameters.size() > Document::max_parameters) {
        return "A song has at most 64 song parameters";
    }
    std::set<std::string_view> names;
    for (const SongParameter& parameter : parameters) {
        if (!is_lookup_name(parameter.name)) {
            return "A song parameter's name isn't empty and has no space at either end";
        }
        if (!names.insert(parameter.name).second) {
            return std::format("There is already a song parameter named {}", parameter.name);
        }
        if (!std::isfinite(parameter.min) || !std::isfinite(parameter.max) ||
            !(parameter.min < parameter.max)) {
            return "A song parameter's minimum is below its maximum";
        }
        if (parameter.default_value < parameter.min || parameter.default_value > parameter.max ||
            std::isnan(parameter.default_value)) {
            return "A song parameter's default is in its range";
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool valid_note_cell(const NoteCell& cell) {
    const bool note =
        cell.note <= last_note || cell.note == NoteCell::off || cell.note == NoteCell::empty;
    const bool instrument = cell.instrument >= Document::first_instrument;
    const bool volume = cell.volume <= max_volume || cell.volume == NoteCell::empty;
    const bool pan = cell.pan <= max_pan || cell.pan == NoteCell::empty;
    return note && instrument && volume && pan;
}

[[nodiscard]] bool valid_command_character(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
}

[[nodiscard]] bool valid_effect_cell(const EffectCell& cell) {
    return cell.command == std::array<char, 2>{} ||
           std::ranges::all_of(cell.command, valid_command_character);
}

[[nodiscard]] Broken check_pattern(const Document& document, std::size_t number) {
    const std::optional<Pattern>& pattern = document.pattern(number);
    if (!pattern) {
        if (std::ranges::contains(document.sequence().slots, number)) {
            return std::format("Pattern {:03} is in the sequence", number);
        }
        if (std::ranges::none_of(
                std::views::iota(std::size_t{0}, Document::pattern_count),
                [&document](std::size_t n) { return document.pattern(n).has_value(); })) {
            return "A song has at least one pattern";
        }
        return std::nullopt;
    }
    const std::size_t lines = pattern->head.lines;
    if (!in_range(lines, 1, Document::max_lines)) {
        return "A pattern has from 1 to 512 lines";
    }
    if (pattern->blocks.size() != document.tracks().size()) {
        return "A pattern has cells for every track";
    }
    for (std::size_t track = 0; track < pattern->blocks.size(); ++track) {
        const TrackBlock& block = pattern->blocks.at(track);
        const Track& shape = document.tracks().at(track);
        if (block.notes.size() != lines * shape.note_columns ||
            block.effects.size() != lines * shape.effect_columns) {
            return "A pattern has cells for every line and column";
        }
        if (!std::ranges::all_of(block.notes, valid_note_cell) ||
            !std::ranges::all_of(block.effects, valid_effect_cell)) {
            return "A cell holds a value outside its field's range";
        }
    }
    std::optional<std::size_t> previous_line;
    for (const Marker& marker : pattern->head.markers) {
        if (marker.line >= lines || (previous_line && marker.line <= *previous_line)) {
            return "Each marker is on its own line of its pattern";
        }
        if (marker.name.empty()) {
            return "A marker has a name";
        }
        previous_line = marker.line;
    }
    return std::nullopt;
}

[[nodiscard]] Broken check_instrument(const Document& document, std::size_t number) {
    const std::optional<Instrument>& instrument = document.instrument(number);
    if (!instrument) {
        return std::nullopt;
    }
    const std::span<const irama::ParamDescriptor> table = instrument_table();
    if (instrument->values.size() != table.size()) {
        return "An instrument has a value for every parameter";
    }
    for (const auto& [param, value] : std::views::zip(table, instrument->values)) {
        if (std::isnan(value) || param.constrain(value) != value) {
            return std::format("{} is outside its range", param.name);
        }
    }
    return std::nullopt;
}

} // namespace

bool is_lookup_name(std::string_view name) {
    return !name.empty() && name.front() != ' ' && name.back() != ' ';
}

std::optional<std::string> broken_limit(const Document& document, const ChangedParts& changed) {
    if (changed.song) {
        if (Broken broken = check_info(document.info())) {
            return broken;
        }
        if (Broken broken = check_tracks(document.tracks())) {
            return broken;
        }
        if (Broken broken = check_sequence(document)) {
            return broken;
        }
        if (Broken broken = check_parameters(document.parameters())) {
            return broken;
        }
        for (std::size_t number = 0; number < Document::pattern_count; ++number) {
            if (document.pattern(number)) {
                if (Broken broken = check_pattern(document, number)) {
                    return broken;
                }
            }
        }
    }
    for (const std::size_t number : changed.patterns) {
        if (Broken broken = check_pattern(document, number)) {
            return broken;
        }
    }
    for (const std::size_t number : changed.instruments) {
        if (Broken broken = check_instrument(document, number)) {
            return broken;
        }
    }
    return std::nullopt;
}

} // namespace irama_studio
