// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/edit_scope.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/limits.hpp"
#include "doc/part_copies.hpp"
#include <irama/engine/base/assertion.hpp>

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace irama_studio {

template <typename Part> void EditScope::copy_before_write(const PartKey& key, const Part& part) {
    if (!copies_.covers(key)) {
        copies_.keep(key, PartValue{std::in_place_type<Part>, part});
    }
}

SongInfo& EditScope::info() {
    copy_before_write({.kind = PartKind::info}, document_.info_);
    return document_.info_;
}

std::vector<Track>& EditScope::tracks() {
    copy_before_write({.kind = PartKind::tracks}, document_.tracks_);
    return document_.tracks_;
}

Sequence& EditScope::sequence() {
    copy_before_write({.kind = PartKind::sequence}, document_.sequence_);
    return document_.sequence_;
}

std::vector<SongParameter>& EditScope::parameters() {
    copy_before_write({.kind = PartKind::parameters}, document_.parameters_);
    return document_.parameters_;
}

std::optional<Pattern>& EditScope::pattern(std::size_t number) {
    IRAMA_ASSERT(number < Document::pattern_count, "a pattern number is from 000 to 999");
    std::optional<Pattern>& pattern = document_.patterns_.at(number);
    copy_before_write({.kind = PartKind::pattern, .number = number}, pattern);
    return pattern;
}

PatternHead& EditScope::pattern_head(std::size_t number) {
    IRAMA_ASSERT(number < Document::pattern_count, "a pattern number is from 000 to 999");
    std::optional<Pattern>& pattern = document_.patterns_.at(number);
    IRAMA_ASSERT(pattern.has_value(), "an edit writes the head of a pattern that exists");
    PatternHead& head = pattern->head;
    copy_before_write({.kind = PartKind::pattern_head, .number = number}, head);
    return head;
}

TrackBlock& EditScope::block(std::size_t number, std::size_t track) {
    IRAMA_ASSERT(number < Document::pattern_count, "a pattern number is from 000 to 999");
    std::optional<Pattern>& pattern = document_.patterns_.at(number);
    IRAMA_ASSERT(pattern.has_value(), "an edit writes the cells of a pattern that exists");
    std::vector<TrackBlock>& blocks = pattern->blocks;
    IRAMA_ASSERT(track < blocks.size(), "an edit writes the cells of a track that exists");
    copy_before_write({.kind = PartKind::block, .number = number, .track = track},
                      blocks.at(track));
    return blocks.at(track);
}

std::optional<Instrument>& EditScope::instrument(std::size_t number) {
    IRAMA_ASSERT(number >= Document::first_instrument && number <= Document::last_instrument,
                 "an instrument number is from 01 to FE");
    std::optional<Instrument>& instrument =
        document_.instruments_.at(number - Document::first_instrument);
    copy_before_write({.kind = PartKind::instrument, .number = number}, instrument);
    return instrument;
}

std::expected<EditRun, Refusal> EditRun::run(Document& document, const EditFunction& function) {
    EditRun run;
    EditScope scope{document, run.copies};
    std::expected<void, Refusal> outcome = function(scope);
    if (outcome) {
        if (std::optional<std::string> broken = broken_limit(document, run.copies.parts())) {
            outcome = std::unexpected{std::move(*broken)};
        }
    }
    if (!outcome) {
        run.copies.swap_with(document);
        return std::unexpected{std::move(outcome).error()};
    }
    run.copies.drop_unchanged(document);
    run.positions = std::move(scope.positions_);
    return run;
}

ChangedParts EditRun::parts() const {
    ChangedParts parts = copies.parts();
    parts.positions = positions;
    return parts;
}

} // namespace irama_studio
