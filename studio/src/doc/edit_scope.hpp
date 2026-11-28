// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/part_copies.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace irama_studio {

/// The only way to write the Document: an edit asks its scope for each part it writes, and the
/// scope copies the part the first time, so the edit can be undone by swapping the copies back.
class EditScope {
  public:
    EditScope(const EditScope&) = delete;
    EditScope& operator=(const EditScope&) = delete;
    EditScope(EditScope&&) = delete;
    EditScope& operator=(EditScope&&) = delete;
    ~EditScope() = default;

    /// \return The Document, as the edit has left it so far.
    [[nodiscard]] const Document& document() const { return document_; }

    /// \return The metadata, timing and seed, to write.
    [[nodiscard]] SongInfo& info();

    /// \return The track list, to write.
    [[nodiscard]] std::vector<Track>& tracks();

    /// \return The sequence and its sections, to write.
    [[nodiscard]] Sequence& sequence();

    /// \return The song parameters, to write.
    [[nodiscard]] std::vector<SongParameter>& parameters();

    /// \param number The pattern number, from 0 to 999, which is checked.
    /// \return The whole pattern, or its absence, to write.
    [[nodiscard]] std::optional<Pattern>& pattern(std::size_t number);

    /// \param number The number of a pattern that exists, which is checked.
    /// \return The pattern's line count and markers, to write.
    [[nodiscard]] PatternHead& pattern_head(std::size_t number);

    /// \param number The number of a pattern that exists, which is checked.
    /// \param track The position of a track, which is checked.
    /// \return The pattern's cells in the track, to write.
    [[nodiscard]] TrackBlock& block(std::size_t number, std::size_t track);

    /// \param number The instrument number, from 0x01 to 0xFE, which is checked.
    /// \return The instrument, or its absence, to write.
    [[nodiscard]] std::optional<Instrument>& instrument(std::size_t number);

    /// Reports where the edit moved slots and tracks.
    void set_positions(PositionMap positions) { positions_ = std::move(positions); }

  private:
    friend struct EditRun;

    EditScope(Document& document, PartCopies& copies) : document_(document), copies_(copies) {}

    // Copies a part, unless a copy covers it already.
    template <typename Part> void copy_before_write(const PartKey& key, const Part& part);

    Document& document_;
    PartCopies& copies_;
    std::optional<PositionMap> positions_;
};

/// Why an edit wasn't made, as the status bar shows it.
using Refusal = std::string;

/// An edit's body: it checks its arguments, then writes the Document through its scope, or refuses.
using EditFunction = std::function<std::expected<void, Refusal>(EditScope&)>;

/// One kind of change to the Document, ready to apply.
struct Edit {
    /// The name the Edit menu gives its step, as in "Undo Set Title".
    std::string name{};
    EditFunction function{};
    /// What a continuous gesture changes, such as one slider, if the edit is a step of one. Steps
    /// of the same gesture merge into one step of the undo history until it ends.
    std::optional<std::string> gesture{};
};

/// What an edit changed: a copy of each part it changed, as it was before, and where it moved slots
/// and tracks.
struct EditRun {
    PartCopies copies{};
    std::optional<PositionMap> positions{};

    /// Runs an edit on `document`. It is all or nothing: if the edit refuses, or leaves a limit of
    /// the Document broken, every part it wrote is put back. Parts it wrote but left as they were
    /// keep no copy.
    ///
    /// \return What the edit changed, or why it was refused.
    [[nodiscard]] static std::expected<EditRun, Refusal> run(Document& document,
                                                             const EditFunction& function);

    /// \return The parts the edit changed, with its position map.
    [[nodiscard]] ChangedParts parts() const;
};

} // namespace irama_studio
