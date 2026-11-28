// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/part_copies.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include <irama/engine/base/assertion.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace irama_studio {
namespace {

// What each copy costs beside its contents: its place in the map and its key.
constexpr std::size_t copy_overhead = 64;

[[nodiscard]] bool is_song(PartKind kind) {
    return kind == PartKind::info || kind == PartKind::tracks || kind == PartKind::sequence ||
           kind == PartKind::parameters;
}

[[nodiscard]] bool is_pattern(PartKind kind) {
    return kind == PartKind::pattern || kind == PartKind::pattern_head || kind == PartKind::block;
}

[[nodiscard]] std::size_t bytes_of(const std::string& text) { return text.size(); }

[[nodiscard]] std::size_t bytes_of(const SongInfo& info) {
    return sizeof(SongInfo) + bytes_of(info.metadata.title) + bytes_of(info.metadata.author) +
           bytes_of(info.metadata.comments);
}

[[nodiscard]] std::size_t bytes_of(const std::vector<Track>& tracks) {
    std::size_t bytes = 0;
    for (const Track& track : tracks) {
        bytes += sizeof(Track) + bytes_of(track.name);
    }
    return bytes;
}

[[nodiscard]] std::size_t bytes_of(const Sequence& sequence) {
    std::size_t bytes = sequence.slots.size() * sizeof(std::uint16_t);
    for (const Section& section : sequence.sections) {
        bytes += sizeof(Section) + bytes_of(section.name);
    }
    return bytes;
}

[[nodiscard]] std::size_t bytes_of(const std::vector<SongParameter>& parameters) {
    std::size_t bytes = 0;
    for (const SongParameter& parameter : parameters) {
        bytes += sizeof(SongParameter) + bytes_of(parameter.name);
    }
    return bytes;
}

[[nodiscard]] std::size_t bytes_of(const PatternHead& head) {
    std::size_t bytes = sizeof(PatternHead);
    for (const Marker& marker : head.markers) {
        bytes += sizeof(Marker) + bytes_of(marker.name);
    }
    return bytes;
}

[[nodiscard]] std::size_t bytes_of(const TrackBlock& block) {
    return (block.notes.size() * sizeof(NoteCell)) + (block.effects.size() * sizeof(EffectCell));
}

[[nodiscard]] std::size_t bytes_of(const std::optional<Pattern>& pattern) {
    if (!pattern) {
        return 0;
    }
    std::size_t bytes = bytes_of(pattern->head);
    for (const TrackBlock& block : pattern->blocks) {
        bytes += bytes_of(block);
    }
    return bytes;
}

[[nodiscard]] std::size_t bytes_of(const std::optional<Instrument>& instrument) {
    if (!instrument) {
        return 0;
    }
    return bytes_of(instrument->name) + (instrument->values.size() * sizeof(float));
}

// Whether a copy equals its part in the document.
[[nodiscard]] bool equals_part(const Document& document, const PartKey& key,
                               const PartValue& value) {
    switch (key.kind) {
    case PartKind::info:
        return std::get<SongInfo>(value) == document.info();
    case PartKind::tracks:
        return std::get<std::vector<Track>>(value) == document.tracks();
    case PartKind::sequence:
        return std::get<Sequence>(value) == document.sequence();
    case PartKind::parameters:
        return std::get<std::vector<SongParameter>>(value) == document.parameters();
    case PartKind::pattern:
        return std::get<std::optional<Pattern>>(value) == document.pattern(key.number);
    case PartKind::pattern_head: {
        const std::optional<Pattern>& pattern = document.pattern(key.number);
        return pattern && std::get<PatternHead>(value) == pattern->head;
    }
    case PartKind::block: {
        const std::optional<Pattern>& pattern = document.pattern(key.number);
        return pattern && key.track < pattern->blocks.size() &&
               std::get<TrackBlock>(value) == pattern->blocks.at(key.track);
    }
    case PartKind::instrument:
        return std::get<std::optional<Instrument>>(value) == document.instrument(key.number);
    }
    return false;
}

} // namespace

bool PartCopies::covers(const PartKey& key) const {
    if (copies_.contains(key)) {
        return true;
    }
    return (key.kind == PartKind::pattern_head || key.kind == PartKind::block) &&
           copies_.contains({.kind = PartKind::pattern, .number = key.number});
}

void PartCopies::keep(const PartKey& key, PartValue value) {
    IRAMA_ASSERT(!covers(key), "a part is copied before its first write only");
    if (key.kind == PartKind::pattern) {
        auto& pattern = std::get<std::optional<Pattern>>(value);
        for (auto it = copies_.begin(); it != copies_.end();) {
            const bool inside =
                it->first.number == key.number &&
                (it->first.kind == PartKind::pattern_head || it->first.kind == PartKind::block);
            if (!inside) {
                ++it;
                continue;
            }
            IRAMA_ASSERT(pattern.has_value(), "a pattern whose parts were copied existed then");
            if (it->first.kind == PartKind::pattern_head) {
                pattern->head = std::get<PatternHead>(std::move(it->second));
            } else {
                pattern->blocks.at(it->first.track) = std::get<TrackBlock>(std::move(it->second));
            }
            it = copies_.erase(it);
        }
    }
    copies_.emplace(key, std::move(value));
}

void PartCopies::merge(PartCopies later) {
    for (auto& [key, value] : later.copies_) {
        if (!covers(key)) {
            keep(key, std::move(value));
        }
    }
}

void PartCopies::swap_with(Document& document) {
    for (auto& [key, value] : copies_) {
        swap_part(document, key, value);
    }
}

void PartCopies::drop_unchanged(const Document& document) {
    std::erase_if(copies_, [&document](const auto& copy) {
        return equals_part(document, copy.first, copy.second);
    });
}

ChangedParts PartCopies::parts() const {
    ChangedParts parts;
    for (const auto& [key, value] : copies_) {
        if (is_song(key.kind)) {
            parts.song = true;
        } else if (is_pattern(key.kind)) {
            parts.patterns.push_back(key.number);
        } else {
            parts.instruments.push_back(key.number);
        }
    }
    // The map orders the kinds of pattern part apart, so one number can come more than once.
    std::ranges::sort(parts.patterns);
    const auto [first, last] = std::ranges::unique(parts.patterns);
    parts.patterns.erase(first, last);
    return parts;
}

bool PartCopies::touches_song() const {
    return std::ranges::any_of(copies_, [](const auto& copy) { return is_song(copy.first.kind); });
}

std::size_t PartCopies::bytes() const {
    std::size_t bytes = 0;
    for (const auto& [key, value] : copies_) {
        bytes += copy_overhead + std::visit([](const auto& part) { return bytes_of(part); }, value);
    }
    return bytes;
}

void PartCopies::swap_part(Document& document, const PartKey& key, PartValue& value) {
    switch (key.kind) {
    case PartKind::info:
        std::swap(document.info_, std::get<SongInfo>(value));
        break;
    case PartKind::tracks:
        std::swap(document.tracks_, std::get<std::vector<Track>>(value));
        break;
    case PartKind::sequence:
        std::swap(document.sequence_, std::get<Sequence>(value));
        break;
    case PartKind::parameters:
        std::swap(document.parameters_, std::get<std::vector<SongParameter>>(value));
        break;
    case PartKind::pattern:
        std::swap(document.patterns_.at(key.number), std::get<std::optional<Pattern>>(value));
        break;
    case PartKind::pattern_head: {
        std::optional<Pattern>& pattern = document.patterns_.at(key.number);
        IRAMA_ASSERT(pattern.has_value(), "a head is kept only for a pattern that exists");
        std::swap(pattern->head, std::get<PatternHead>(value));
        break;
    }
    case PartKind::block: {
        std::optional<Pattern>& pattern = document.patterns_.at(key.number);
        IRAMA_ASSERT(pattern.has_value(), "a block is kept only for a pattern that exists");
        std::swap(pattern->blocks.at(key.track), std::get<TrackBlock>(value));
        break;
    }
    case PartKind::instrument:
        std::swap(document.instruments_.at(key.number - Document::first_instrument),
                  std::get<std::optional<Instrument>>(value));
        break;
    }
}

} // namespace irama_studio
