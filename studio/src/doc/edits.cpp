// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doc/edits.hpp"

#include "doc/document.hpp"
#include "doc/edit_scope.hpp"
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/param/param_descriptor.hpp>
#include <irama/engine/param/param_id.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace irama_studio {
namespace {

using Outcome = std::expected<void, Refusal>;

[[nodiscard]] bool is_instrument_number(std::size_t number) {
    return number >= Document::first_instrument && number <= Document::last_instrument;
}

// Refuses an instrument number that is out of range or holds no instrument.
[[nodiscard]] Outcome check_filled(const Document& document, std::size_t number) {
    if (!is_instrument_number(number)) {
        return std::unexpected{"Instrument numbers are from 01 to FE"};
    }
    if (!document.instrument(number)) {
        return std::unexpected{std::format("Instrument {:02X} is empty", number)};
    }
    return {};
}

// Refuses an instrument number that is out of range or already holds an instrument.
[[nodiscard]] Outcome check_free(const Document& document, std::size_t number) {
    if (!is_instrument_number(number)) {
        return std::unexpected{"Instrument numbers are from 01 to FE"};
    }
    if (document.instrument(number)) {
        return std::unexpected{std::format("Instrument {:02X} is already filled", number)};
    }
    return {};
}

[[nodiscard]] Outcome check_parameter(const Document& document, std::size_t index) {
    if (index >= document.parameters().size()) {
        return std::unexpected{std::format("There is no song parameter {}", index + 1)};
    }
    return {};
}

[[nodiscard]] Outcome check_pattern(const Document& document, std::size_t pattern) {
    if (pattern >= Document::pattern_count || !document.pattern(pattern)) {
        return std::unexpected{std::format("There is no pattern {:03}", pattern)};
    }
    return {};
}

// The marker on a line of a pattern that exists, if any.
[[nodiscard]] std::optional<std::size_t> marker_index(const PatternHead& head, std::size_t line) {
    const auto found = std::ranges::find(head.markers, line, &Marker::line);
    if (found == head.markers.end()) {
        return std::nullopt;
    }
    return irama::narrow<std::size_t>(std::distance(head.markers.begin(), found));
}

[[nodiscard]] Outcome check_marker(const Document& document, std::size_t pattern,
                                   std::size_t line) {
    if (Outcome checked = check_pattern(document, pattern); !checked) {
        return checked;
    }
    const std::optional<Pattern>& found = document.pattern(pattern);
    if (!found || !marker_index(found->head, line)) {
        return std::unexpected{
            std::format("Pattern {:03} has no marker on line {}", pattern, line)};
    }
    return {};
}

} // namespace

Edit set_title(std::string title) {
    return {
        .name = "Set Title",
        .function = [title = std::move(title)](EditScope& scope) -> Outcome {
            scope.info().metadata.title = title;
            return {};
        },
    };
}

Edit set_author(std::string author) {
    return {
        .name = "Set Author",
        .function = [author = std::move(author)](EditScope& scope) -> Outcome {
            scope.info().metadata.author = author;
            return {};
        },
    };
}

Edit set_comments(std::string comments) {
    return {
        .name = "Set Comments",
        .function = [comments = std::move(comments)](EditScope& scope) -> Outcome {
            scope.info().metadata.comments = comments;
            return {};
        },
    };
}

Edit set_seed(std::uint64_t seed) {
    return {
        .name = "Set Seed",
        .function = [seed](EditScope& scope) -> Outcome {
            scope.info().seed = seed;
            return {};
        },
    };
}

Edit add_song_parameter(SongParameter parameter) {
    return {
        .name = "Add Song Parameter",
        .function = [parameter = std::move(parameter)](EditScope& scope) -> Outcome {
            if (scope.document().parameters().size() >= Document::max_parameters) {
                return std::unexpected{"A song has at most 64 song parameters"};
            }
            scope.parameters().push_back(parameter);
            return {};
        },
    };
}

Edit change_song_parameter(std::size_t index, SongParameter parameter) {
    return {
        .name = "Change Song Parameter",
        .function = [index, parameter = std::move(parameter)](EditScope& scope) -> Outcome {
            if (Outcome checked = check_parameter(scope.document(), index); !checked) {
                return checked;
            }
            scope.parameters().at(index) = parameter;
            return {};
        },
    };
}

Edit delete_song_parameter(std::size_t index) {
    return {
        .name = "Delete Song Parameter",
        .function = [index](EditScope& scope) -> Outcome {
            if (Outcome checked = check_parameter(scope.document(), index); !checked) {
                return checked;
            }
            std::vector<SongParameter>& parameters = scope.parameters();
            parameters.erase(parameters.begin() + irama::narrow<std::ptrdiff_t>(index));
            return {};
        },
    };
}

Edit add_marker(std::size_t pattern, Marker marker) {
    return {
        .name = "Add Marker",
        .function = [pattern, marker = std::move(marker)](EditScope& scope) -> Outcome {
            if (Outcome checked = check_pattern(scope.document(), pattern); !checked) {
                return checked;
            }
            const PatternHead& head = scope.document().pattern(pattern)->head;
            if (marker.line >= head.lines) {
                return std::unexpected{
                    std::format("Pattern {:03} has {} lines", pattern, head.lines)};
            }
            if (marker_index(head, marker.line)) {
                return std::unexpected{
                    std::format("There is already a marker on line {}", marker.line)};
            }
            if (marker.name.empty()) {
                return std::unexpected{"A marker has a name"};
            }
            std::vector<Marker>& markers = scope.pattern_head(pattern).markers;
            markers.insert(std::ranges::upper_bound(markers, marker.line, {}, &Marker::line),
                           marker);
            return {};
        },
    };
}

Edit rename_marker(std::size_t pattern, std::size_t line, std::string name) {
    return {
        .name = "Rename Marker",
        .function = [pattern, line, name = std::move(name)](EditScope& scope) -> Outcome {
            if (Outcome checked = check_marker(scope.document(), pattern, line); !checked) {
                return checked;
            }
            if (name.empty()) {
                return std::unexpected{"A marker has a name"};
            }
            PatternHead& head = scope.pattern_head(pattern);
            head.markers.at(marker_index(head, line).value()).name = name;
            return {};
        },
    };
}

Edit delete_marker(std::size_t pattern, std::size_t line) {
    return {
        .name = "Delete Marker",
        .function = [pattern, line](EditScope& scope) -> Outcome {
            if (Outcome checked = check_marker(scope.document(), pattern, line); !checked) {
                return checked;
            }
            PatternHead& head = scope.pattern_head(pattern);
            const std::size_t index = marker_index(head, line).value();
            head.markers.erase(head.markers.begin() + irama::narrow<std::ptrdiff_t>(index));
            return {};
        },
    };
}

Edit new_instrument(std::size_t number) {
    return {
        .name = "New Instrument",
        .function = [number](EditScope& scope) -> Outcome {
            if (Outcome checked = check_free(scope.document(), number); !checked) {
                return checked;
            }
            scope.instrument(number) = default_instrument();
            return {};
        },
    };
}

Edit clone_instrument(std::size_t from, std::size_t to) {
    return {
        .name = "Clone Instrument",
        .function = [from, to](EditScope& scope) -> Outcome {
            if (Outcome checked = check_filled(scope.document(), from); !checked) {
                return checked;
            }
            if (Outcome checked = check_free(scope.document(), to); !checked) {
                return checked;
            }
            scope.instrument(to) = scope.document().instrument(from);
            return {};
        },
    };
}

Edit delete_instrument(std::size_t number) {
    return {
        .name = "Delete Instrument",
        .function = [number](EditScope& scope) -> Outcome {
            if (Outcome checked = check_filled(scope.document(), number); !checked) {
                return checked;
            }
            scope.instrument(number).reset();
            return {};
        },
    };
}

Edit rename_instrument(std::size_t number, std::string name) {
    return {
        .name = "Rename Instrument",
        .function = [number, name = std::move(name)](EditScope& scope) -> Outcome {
            if (Outcome checked = check_filled(scope.document(), number); !checked) {
                return checked;
            }
            scope.instrument(number)->name = name;
            return {};
        },
    };
}

Edit set_instrument_value(std::size_t number, irama::ParamId id, float value) {
    const std::span<const irama::ParamDescriptor> table = instrument_table();
    const auto found = std::ranges::find(table, id, &irama::ParamDescriptor::id);
    IRAMA_ASSERT(found != table.end(), "a parameter's ID is one of the instrument's");
    const irama::ParamDescriptor& param = *found;
    const auto index = irama::narrow<std::size_t>(std::distance(table.begin(), found));
    return {
        .name = std::format("Set {}", param.name),
        .function = [number, index, &param, value](EditScope& scope) -> Outcome {
            if (Outcome checked = check_filled(scope.document(), number); !checked) {
                return checked;
            }
            if (std::isnan(value)) {
                return std::unexpected{std::format("{} takes a number", param.name)};
            }
            scope.instrument(number)->values.at(index) = param.constrain(value);
            return {};
        },
        .gesture = std::format("instrument {:02X} parameter {}", number, std::to_underlying(id)),
    };
}

} // namespace irama_studio
