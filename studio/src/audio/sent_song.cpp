// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/sent_song.hpp"

#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/engine_song.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace irama_studio {
namespace {

constexpr double hundredths = 100.0;

// Takes a part or change set that was built, or throws as a standard container would.
template <typename Ptr> [[nodiscard]] Ptr built(std::expected<Ptr, irama::AllocationError> made) {
    if (!made) {
        throw std::bad_alloc{};
    }
    return std::move(*made);
}

[[nodiscard]] irama::SongPartPtr song_part(const Document& document) {
    std::vector<std::uint8_t> note_columns;
    for (const Track& track : document.tracks()) {
        note_columns.push_back(track.note_columns);
    }
    const Timing& timing = document.info().timing;
    return built(irama::SongPart::create(
        irama::default_allocator(),
        {
            .bpm100 = irama::narrow<std::uint32_t>(std::lround(timing.bpm * hundredths)),
            .lpb = timing.lpb,
            .beats_per_bar = timing.beats_per_bar,
            .ticks_per_line = timing.ticks_per_line,
            .seed = document.info().seed,
            .note_columns = note_columns,
            .sequence = document.sequence().slots,
        }));
}

[[nodiscard]] irama::SongPartPtr copy_of(const irama::SongPart& part) {
    return built(irama::SongPart::create(irama::default_allocator(),
                                         {
                                             .bpm100 = part.bpm100(),
                                             .lpb = part.lpb(),
                                             .beats_per_bar = part.beats_per_bar(),
                                             .ticks_per_line = part.ticks_per_line(),
                                             .seed = part.seed(),
                                             .note_columns = part.note_columns(),
                                             .sequence = part.sequence(),
                                         }));
}

// The note cells of every track's note columns, line by line, or null for an empty number.
[[nodiscard]] irama::PatternPartPtr pattern_part(const Document& document, std::size_t number) {
    const std::optional<Pattern>& pattern = document.pattern(number);
    if (!pattern) {
        return nullptr;
    }
    std::size_t columns = 0;
    for (const Track& track : document.tracks()) {
        columns += track.note_columns;
    }
    const std::size_t lines = pattern->head.lines;
    std::vector<NoteCell> cells;
    cells.reserve(lines * columns);
    for (std::size_t line = 0; line < lines; ++line) {
        for (std::size_t track = 0; track < document.tracks().size(); ++track) {
            const std::size_t track_columns = document.tracks().at(track).note_columns;
            const std::vector<NoteCell>& notes = pattern->blocks.at(track).notes;
            for (std::size_t column = 0; column < track_columns; ++column) {
                cells.push_back(notes.at((line * track_columns) + column));
            }
        }
    }
    return built(irama::PatternPart::create(irama::default_allocator(), lines, columns, cells));
}

[[nodiscard]] irama::PatternPartPtr copy_of(const irama::PatternPart* part) {
    if (part == nullptr) {
        return nullptr;
    }
    return built(irama::PatternPart::create(irama::default_allocator(), part->lines(),
                                            part->columns(), part->cells()));
}

[[nodiscard]] irama::InstrumentPartPtr instrument_part(const Document& document,
                                                       std::size_t number) {
    const std::optional<Instrument>& instrument = document.instrument(number);
    if (!instrument) {
        return nullptr;
    }
    return built(irama::InstrumentPart::create(irama::default_allocator(), instrument->values));
}

[[nodiscard]] irama::InstrumentPartPtr copy_of(const irama::InstrumentPart* part) {
    if (part == nullptr) {
        return nullptr;
    }
    return built(irama::InstrumentPart::create(irama::default_allocator(), part->values()));
}

// Whether two entries hold equal parts, or are both empty.
template <typename Part> [[nodiscard]] bool same(const Part* left, const Part* right) {
    if (left == nullptr || right == nullptr) {
        return left == right;
    }
    return *left == *right;
}

} // namespace

irama::EngineSong build_engine_song(const Document& document) {
    irama::EngineSong song;
    std::vector<std::size_t> patterns;
    for (std::size_t number = 0; number < Document::pattern_count; ++number) {
        if (document.pattern(number)) {
            patterns.push_back(number);
        }
    }
    std::vector<std::size_t> instruments;
    for (std::size_t number = Document::first_instrument; number <= Document::last_instrument;
         ++number) {
        if (document.instrument(number)) {
            instruments.push_back(number);
        }
    }
    irama::ChangeSetPtr changes = built(
        irama::ChangeSet::create(irama::default_allocator(),
                                 {.patterns = patterns.size(), .instruments = instruments.size()}));
    changes->replace_song(song_part(document));
    for (const std::size_t number : patterns) {
        changes->replace_pattern(number, pattern_part(document, number));
    }
    for (const std::size_t number : instruments) {
        changes->replace_instrument(number, instrument_part(document, number));
    }
    song.apply(*changes);
    return song;
}

SentSong::SentSong(const Document& document) : song_(build_engine_song(document)) {}

irama::ChangeSetPtr SentSong::update(const Document& document, const ChangedParts& parts) {
    irama::SongPartPtr song;
    if (parts.song) {
        song = song_part(document);
        if (*song == *song_.song()) {
            song.reset();
        }
    }
    std::vector<std::pair<std::size_t, irama::PatternPartPtr>> patterns;
    for (const std::size_t number : parts.patterns) {
        irama::PatternPartPtr part = pattern_part(document, number);
        if (!same(part.get(), song_.pattern(number))) {
            patterns.emplace_back(number, std::move(part));
        }
    }
    std::vector<std::pair<std::size_t, irama::InstrumentPartPtr>> instruments;
    for (const std::size_t number : parts.instruments) {
        irama::InstrumentPartPtr part = instrument_part(document, number);
        if (!same(part.get(), song_.instrument(number))) {
            instruments.emplace_back(number, std::move(part));
        }
    }
    if (song == nullptr && patterns.empty() && instruments.empty() && !parts.positions) {
        return nullptr;
    }

    const irama::ChangeSetSize size{.patterns = patterns.size(), .instruments = instruments.size()};
    irama::ChangeSetPtr kept = built(irama::ChangeSet::create(irama::default_allocator(), size));
    const bool replaces_song = song != nullptr;
    if (replaces_song) {
        kept->replace_song(std::move(song));
    }
    for (auto& [number, part] : patterns) {
        kept->replace_pattern(number, std::move(part));
    }
    for (auto& [number, part] : instruments) {
        kept->replace_instrument(number, std::move(part));
    }
    // Dropping the change set frees the parts it took the place of.
    song_.apply(*kept);

    irama::ChangeSetSize sent_size = size;
    if (parts.positions) {
        sent_size.slots = parts.positions->slots.size();
        sent_size.tracks = parts.positions->tracks.size();
    }
    irama::ChangeSetPtr sent =
        built(irama::ChangeSet::create(irama::default_allocator(), sent_size));
    if (replaces_song) {
        sent->replace_song(copy_of(*song_.song()));
    }
    for (const auto& [number, part] : patterns) {
        sent->replace_pattern(number, copy_of(song_.pattern(number)));
    }
    for (const auto& [number, part] : instruments) {
        sent->replace_instrument(number, copy_of(song_.instrument(number)));
    }
    if (parts.positions) {
        std::ranges::copy(parts.positions->slots, sent->slot_map().begin());
        std::ranges::copy(parts.positions->tracks, sent->track_map().begin());
    }
    return sent;
}

irama::ChangeSetPtr SentSong::whole_song() const {
    std::vector<std::size_t> patterns;
    for (std::size_t number = 0; number < irama::EngineSong::pattern_count; ++number) {
        if (song_.pattern(number) != nullptr) {
            patterns.push_back(number);
        }
    }
    std::vector<std::size_t> instruments;
    for (std::size_t number = irama::EngineSong::first_instrument;
         number <= irama::EngineSong::last_instrument; ++number) {
        if (song_.instrument(number) != nullptr) {
            instruments.push_back(number);
        }
    }
    irama::ChangeSetPtr changes = built(
        irama::ChangeSet::create(irama::default_allocator(),
                                 {.patterns = patterns.size(), .instruments = instruments.size()}));
    changes->replace_song(copy_of(*song_.song()));
    for (const std::size_t number : patterns) {
        changes->replace_pattern(number, copy_of(song_.pattern(number)));
    }
    for (const std::size_t number : instruments) {
        changes->replace_instrument(number, copy_of(song_.instrument(number)));
    }
    return changes;
}

} // namespace irama_studio
