// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/undo_history.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include <irama/engine/base/assertion.hpp>

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <utility>

namespace irama_studio {
namespace {

// What each step costs beside its copies: its name, views and place in the history.
constexpr std::size_t step_overhead = 256;

} // namespace

UndoHistory::UndoHistory(std::size_t budget) : budget_(budget) {}

std::expected<ChangedParts, Refusal> UndoHistory::apply(Document& document, const Edit& edit,
                                                        const EditViews& views) {
    std::expected<EditRun, Refusal> run = EditRun::run(document, edit.function);
    if (!run) {
        return std::unexpected{std::move(run).error()};
    }
    if (edit.gesture && gesture_open_ && done_.back().gesture == edit.gesture) {
        return merge(document, std::move(*run), views);
    }
    gesture_open_ = false;
    ChangedParts parts = run->parts();
    if (parts.empty()) {
        return parts;
    }
    for (const Step& undone : undone_) {
        bytes_ -= undone.bytes;
    }
    undone_.clear();
    Step step{
        .name = edit.name,
        .copies = std::move(run->copies),
        .positions = std::move(run->positions),
        .views = views,
        .gesture = edit.gesture,
    };
    measure(step);
    bytes_ += step.bytes;
    done_.push_back(std::move(step));
    gesture_open_ = edit.gesture.has_value();
    trim();
    return parts;
}

ChangedParts UndoHistory::merge(Document& document, EditRun run, const EditViews& views) {
    IRAMA_ASSERT(!run.positions, "a gesture moves no slot or track");
    ChangedParts parts = run.parts();
    Step& step = done_.back();
    bytes_ -= step.bytes;
    step.copies.merge(std::move(run.copies));
    // A gesture brought back to where it started leaves nothing to undo.
    step.copies.drop_unchanged(document);
    if (step.copies.empty()) {
        done_.pop_back();
        gesture_open_ = false;
        return parts;
    }
    if (views.after) {
        step.views.after = views.after;
    }
    measure(step);
    bytes_ += step.bytes;
    trim();
    return parts;
}

std::optional<HistoryStep> UndoHistory::undo(Document& document) {
    end_gesture();
    if (done_.empty()) {
        return std::nullopt;
    }
    Step step = std::move(done_.back());
    done_.pop_back();
    const std::size_t slots_after = document.sequence().slots.size();
    const std::size_t tracks_after = document.tracks().size();
    step.copies.swap_with(document);
    HistoryStep result{.parts = step.copies.parts(), .view = step.views.before};
    if (step.positions) {
        result.parts.positions = step.positions->inverse(slots_after, tracks_after);
    }
    undone_.push_back(std::move(step));
    return result;
}

std::optional<HistoryStep> UndoHistory::redo(Document& document) {
    if (undone_.empty()) {
        return std::nullopt;
    }
    Step step = std::move(undone_.back());
    undone_.pop_back();
    step.copies.swap_with(document);
    HistoryStep result{.parts = step.copies.parts(), .view = step.views.after};
    result.parts.positions = step.positions;
    done_.push_back(std::move(step));
    return result;
}

std::optional<std::string_view> UndoHistory::undo_name() const {
    if (done_.empty()) {
        return std::nullopt;
    }
    return done_.back().name;
}

std::optional<std::string_view> UndoHistory::redo_name() const {
    if (undone_.empty()) {
        return std::nullopt;
    }
    return undone_.back().name;
}

void UndoHistory::measure(Step& step) {
    step.bytes =
        step_overhead + step.name.size() + step.gesture.value_or("").size() + step.copies.bytes();
}

void UndoHistory::trim() {
    while (bytes_ > budget_ && done_.size() > 1) {
        bytes_ -= done_.front().bytes;
        done_.pop_front();
    }
}

} // namespace irama_studio
