// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Seeded runs of random edits, undos and redos over a test song: after every step the song as
// sent, updated by what the step reported, equals the song built from scratch, and undoing every
// step brings back the Document the run started from.

#include "audio/sent_song.hpp"
#include "doc/changed_parts.hpp"
#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include "doc/edits.hpp"
#include "doc/undo_history.hpp"
#include "document_builder.hpp"
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <string_view>

using irama_studio::ChangedParts;
using irama_studio::Document;
using irama_studio::Edit;
using irama_studio::HistoryStep;
using irama_studio::Refusal;
using irama_studio::SongParameter;
using irama_studio::UndoHistory;

namespace {

constexpr std::uint64_t seed = 0x5EED'0071;
constexpr std::size_t steps = 400;
// Few enough names and numbers that edits often meet each other's parts, and are refused.
constexpr std::array<std::string_view, 4> names{"A", "B", "Intro", ""};
constexpr std::size_t instruments = 6;
constexpr std::array<std::size_t, 4> patterns{0, 1, 2, 7};
constexpr std::size_t lines = 18;

class RandomEdits {
  public:
    explicit RandomEdits(std::uint64_t run_seed) : random_(run_seed) {}

    [[nodiscard]] std::size_t below(std::size_t count) {
        return std::uniform_int_distribution<std::size_t>{0, count - 1}(random_);
    }

    [[nodiscard]] std::string name() { return std::string{names.at(below(names.size()))}; }

    [[nodiscard]] std::size_t instrument() { return 1 + below(instruments); }

    [[nodiscard]] SongParameter parameter() {
        const auto min = static_cast<float>(below(3));
        const auto max = static_cast<float>(below(3));
        return {.name = name(), .min = min, .max = max, .default_value = min};
    }

    [[nodiscard]] Edit edit() {
        switch (below(15)) {
        case 0:
            return irama_studio::set_title(name());
        case 1:
            return irama_studio::set_author(name());
        case 2:
            return irama_studio::set_comments(name());
        case 3:
            return irama_studio::set_seed(below(3));
        case 4:
            return irama_studio::add_song_parameter(parameter());
        case 5:
            return irama_studio::change_song_parameter(below(3), parameter());
        case 6:
            return irama_studio::delete_song_parameter(below(3));
        case 7:
            return irama_studio::add_marker(patterns.at(below(patterns.size())),
                                            {.line = below(lines), .name = name()});
        case 8:
            return irama_studio::rename_marker(patterns.at(below(patterns.size())), below(lines),
                                               name());
        case 9:
            return irama_studio::delete_marker(patterns.at(below(patterns.size())), below(lines));
        case 10:
            return irama_studio::new_instrument(instrument());
        case 11:
            return irama_studio::clone_instrument(instrument(), instrument());
        case 12:
            return irama_studio::delete_instrument(instrument());
        case 13:
            return irama_studio::rename_instrument(instrument(), name());
        default: {
            const irama::ParamDescriptor& param =
                irama::instrument_params.at(below(irama::instrument_params.size()));
            const auto share = static_cast<float>(below(5)) / 4.0F;
            return irama_studio::set_instrument_value(
                instrument(), param.id, param.min + ((param.max - param.min) * share));
        }
        }
    }

  private:
    std::mt19937_64 random_;
};

[[nodiscard]] std::optional<ChangedParts> parts_of(const std::optional<HistoryStep>& step) {
    if (!step) {
        return std::nullopt;
    }
    return step->parts;
}

// Takes one random step, an edit, undo, redo or end of a gesture, and returns what it changed. A
// refused edit leaves the Document as it was.
[[nodiscard]] std::optional<ChangedParts> random_step(RandomEdits& random, UndoHistory& history,
                                                      Document& document) {
    const std::size_t action = random.below(10);
    if (action < 2) {
        return parts_of(history.undo(document));
    }
    if (action < 3) {
        return parts_of(history.redo(document));
    }
    if (action < 4) {
        history.end_gesture();
        return std::nullopt;
    }
    const Edit edit = random.edit();
    INFO("the edit " << edit.name);
    const Document before = document;
    const std::expected<ChangedParts, Refusal> applied = history.apply(document, edit);
    if (!applied) {
        REQUIRE(document == before);
        return std::nullopt;
    }
    return *applied;
}

} // namespace

TEST_CASE("random edits, undos and redos keep incremental equal to whole, and undo back to the "
          "start",
          "[studio][history]") {
    const Document start = irama_studio::test::test_song();
    Document document = start;
    UndoHistory history{UndoHistory::studio_budget};
    irama_studio::SentSong sent{document};
    RandomEdits random{seed};
    for (std::size_t step = 0; step < steps; ++step) {
        INFO("seed " << seed << ", step " << step);
        if (const std::optional<ChangedParts> parts = random_step(random, history, document)) {
            static_cast<void>(sent.update(document, *parts));
        }
        REQUIRE(sent.song() == irama_studio::build_engine_song(document));
    }
    INFO("seed " << seed);
    CHECK(history.undo_count() > 0);
    while (const std::optional<HistoryStep> undone = history.undo(document)) {
        static_cast<void>(sent.update(document, undone->parts));
    }
    CHECK(document == start);
    CHECK(sent.song() == irama_studio::build_engine_song(start));
}
