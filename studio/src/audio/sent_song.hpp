// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>

namespace irama_studio {

/// Builds the EngineSong of a Document from scratch: its song part, and a part for every pattern
/// and instrument it holds.
///
/// \return The EngineSong. Running out of memory throws `std::bad_alloc`, as a standard container
///         would.
[[nodiscard]] irama::EngineSong build_engine_song(const Document& document);

/// The song as Studio last sent it to the player, part by part, which an edit's change set is
/// built against.
///
/// After each edit, the parts it reports are rebuilt from the Document. A rebuilt part equal to the
/// one last sent for its entry is left out, so an edit of what the engine doesn't hold, such as the
/// title, sends nothing. The rest replace their entries here, and go to the player as one change
/// set, with the edit's position map.
class SentSong {
  public:
    /// Starts from the whole of a Document, as if it had all been sent.
    explicit SentSong(const Document& document);

    /// Rebuilds the parts an edit reported, and keeps those that differ from the parts last sent.
    ///
    /// \param document The Document after the edit, its undo or its redo.
    /// \param parts What the edit, undo or redo reported.
    /// \return A change set of the parts that differ and the position map, or null if it would
    ///         change nothing. Running out of memory throws `std::bad_alloc`.
    [[nodiscard]] irama::ChangeSetPtr update(const Document& document, const ChangedParts& parts);

    /// \return A change set of the whole song as last sent, every part a copy, to load a new
    ///         player with. Running out of memory throws `std::bad_alloc`.
    [[nodiscard]] irama::ChangeSetPtr whole_song() const;

    /// \return The song as last sent.
    [[nodiscard]] const irama::EngineSong& song() const { return song_; }

  private:
    irama::EngineSong song_;
};

} // namespace irama_studio
