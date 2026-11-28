// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/song_editor.hpp"

#include "audio/audio_engine.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/undo_history.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace irama_studio {

SongEditor::SongEditor(Document document, std::size_t budget)
    : document_(std::move(document)), history_(budget) {}

bool SongEditor::apply(const Edit& edit, AudioEngine& engine, const EditViews& views) {
    const std::expected<ChangedParts, Refusal> parts = history_.apply(document_, edit, views);
    if (!parts) {
        refusal_ = parts.error();
        return false;
    }
    refusal_.clear();
    engine.send_changes(document_, *parts);
    return true;
}

std::optional<View> SongEditor::undo(AudioEngine& engine) {
    const std::optional<HistoryStep> step = history_.undo(document_);
    if (!step) {
        return std::nullopt;
    }
    refusal_.clear();
    engine.send_changes(document_, step->parts);
    return step->view;
}

std::optional<View> SongEditor::redo(AudioEngine& engine) {
    const std::optional<HistoryStep> step = history_.redo(document_);
    if (!step) {
        return std::nullopt;
    }
    refusal_.clear();
    engine.send_changes(document_, step->parts);
    return step->view;
}

} // namespace irama_studio
