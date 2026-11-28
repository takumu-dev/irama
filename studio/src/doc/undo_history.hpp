// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/part_copies.hpp"

#include <cstddef>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace irama_studio {

/// A place in the pattern grid: a line, and a field of a column of a track.
struct GridPosition {
    std::size_t line = 0;
    std::size_t track = 0;
    /// The track's note columns first, then its effect columns.
    std::size_t column = 0;
    std::size_t field = 0;

    friend bool operator==(const GridPosition&, const GridPosition&) = default;
};

/// What the pattern editor shows of an edit: the current slot, the cursor and the selection, which
/// undo and redo put back so the change is in sight.
struct View {
    std::size_t slot = 0;
    GridPosition cursor{};
    /// The corner of the selection opposite the cursor, if there is a selection.
    std::optional<GridPosition> selection_anchor{};

    friend bool operator==(const View&, const View&) = default;
};

/// The views before and after an edit, for an edit made in the pattern editor. An edit made
/// anywhere else has neither, and undo and redo leave the view where it is.
struct EditViews {
    std::optional<View> before{};
    std::optional<View> after{};
};

/// What undo or redo did: the parts it changed, and the view to show.
struct HistoryStep {
    ChangedParts parts{};
    std::optional<View> view{};
};

/// The linear history of edits that undo and redo step through, kept within a memory budget.
///
/// Each step is one edit, or one continuous gesture of edits. The budget counts the copies the
/// steps keep, their text and a fixed amount for each step, and the oldest steps are dropped to
/// stay within it. A step larger than the whole budget is still kept, as the only step.
class UndoHistory {
  public:
    /// Studio's budget.
    static constexpr std::size_t studio_budget = std::size_t{64} * 1024 * 1024;

    /// Makes an empty history.
    ///
    /// \param budget How many bytes the steps may take.
    explicit UndoHistory(std::size_t budget);

    /// Applies an edit to `document` and records it as a step, which clears the steps undone. An
    /// edit that changes nothing, or is refused, records nothing. A step of the gesture that the
    /// latest step holds, while it is still going, merges into that step.
    ///
    /// \param document The Document.
    /// \param edit The edit.
    /// \param views The views before and after the edit, if it was made in the pattern editor.
    /// \return The parts the edit changed, or why it was refused.
    [[nodiscard]] std::expected<ChangedParts, Refusal> apply(Document& document, const Edit& edit,
                                                             const EditViews& views = {});

    /// Ends the gesture going on, if any, so that no later edit merges into its step.
    void end_gesture() { gesture_open_ = false; }

    /// Undoes the latest step, ending its gesture first if it is still going.
    ///
    /// \return What the undo changed, and the view before the step, or nothing if no step is left
    ///         to undo.
    [[nodiscard]] std::optional<HistoryStep> undo(Document& document);

    /// Redoes the step undone last.
    ///
    /// \return What the redo changed, and the view after the step, or nothing if no step is left
    ///         to redo.
    [[nodiscard]] std::optional<HistoryStep> redo(Document& document);

    /// \return The name of the step that undo would undo, if any.
    [[nodiscard]] std::optional<std::string_view> undo_name() const;

    /// \return The name of the step that redo would redo, if any.
    [[nodiscard]] std::optional<std::string_view> redo_name() const;

    /// \return How many steps can be undone.
    [[nodiscard]] std::size_t undo_count() const { return done_.size(); }

    /// \return How many steps can be redone.
    [[nodiscard]] std::size_t redo_count() const { return undone_.size(); }

    /// \return How many bytes the steps take, by the budget's count.
    [[nodiscard]] std::size_t bytes() const { return bytes_; }

    /// \return The budget.
    [[nodiscard]] std::size_t budget() const { return budget_; }

  private:
    struct Step {
        std::string name{};
        PartCopies copies{};
        std::optional<PositionMap> positions{};
        EditViews views{};
        std::optional<std::string> gesture{};
        std::size_t bytes = 0;
    };

    // Counts a step's size afresh.
    static void measure(Step& step);

    // Drops the oldest steps until the history fits its budget, or one step is left.
    void trim();

    // Merges a step of the open gesture into the latest step.
    [[nodiscard]] ChangedParts merge(Document& document, EditRun run, const EditViews& views);

    std::size_t budget_;
    std::size_t bytes_ = 0;
    std::deque<Step> done_;
    std::vector<Step> undone_;
    bool gesture_open_ = false;
};

} // namespace irama_studio
