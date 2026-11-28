// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/undo_history.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace irama_studio {

class AudioEngine;

/// Studio's song: the Document, the undo history of its edits, and the path from each change to
/// the player. Views call its edits and never write the Document themselves.
///
/// Every edit, undo and redo sends the audio engine the parts it changed. An edit that is refused
/// leaves its reason for the status bar until the next change.
class SongEditor {
  public:
    /// Starts a song, the new song unless another is given, with an empty history.
    ///
    /// \param document The song.
    /// \param budget How many bytes the undo history may take.
    explicit SongEditor(Document document = Document{},
                        std::size_t budget = UndoHistory::studio_budget);

    /// \return The Document.
    [[nodiscard]] const Document& document() const { return document_; }

    /// \return The undo history.
    [[nodiscard]] const UndoHistory& history() const { return history_; }

    /// Applies an edit, records it, and sends the engine the parts it changed.
    ///
    /// \param edit The edit.
    /// \param engine The audio engine.
    /// \param views The views before and after the edit, if it was made in the pattern editor.
    /// \return Whether the edit was made, rather than refused.
    bool apply(const Edit& edit, AudioEngine& engine, const EditViews& views = {});

    /// Ends the gesture going on, such as a slider drag when the mouse is let go.
    void end_gesture() { history_.end_gesture(); }

    /// Undoes the latest step, and sends the engine the parts it changed.
    ///
    /// \return The view to show, if the step has one.
    std::optional<View> undo(AudioEngine& engine);

    /// Redoes the step undone last, and sends the engine the parts it changed.
    ///
    /// \return The view to show, if the step has one.
    std::optional<View> redo(AudioEngine& engine);

    /// \return Why the latest edit was refused, or nothing since a change.
    [[nodiscard]] const std::string& refusal() const { return refusal_; }

  private:
    Document document_;
    UndoHistory history_;
    std::string refusal_;
};

} // namespace irama_studio
