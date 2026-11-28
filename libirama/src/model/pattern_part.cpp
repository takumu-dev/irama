// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/pattern_part.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace {

using irama::NoteCell;

constexpr std::uint8_t highest_note = 119;
constexpr std::uint8_t last_instrument = 0xFE;
constexpr std::uint8_t loudest = 0x80;
constexpr std::uint8_t hard_right = 0x80;

[[nodiscard]] bool in_range(const NoteCell& cell) {
    const bool note =
        cell.note <= highest_note || cell.note == NoteCell::off || cell.note == NoteCell::empty;
    const bool instrument = (cell.instrument >= 1 && cell.instrument <= last_instrument) ||
                            cell.instrument == NoteCell::empty;
    const bool volume = cell.volume <= loudest || cell.volume == NoteCell::empty;
    const bool pan = cell.pan <= hard_right || cell.pan == NoteCell::empty;
    return note && instrument && volume && pan;
}

} // namespace

namespace irama {

std::expected<PatternPartPtr, AllocationError>
PatternPart::create(const Allocator& allocator, std::size_t lines, std::size_t columns,
                    std::span<const NoteCell> cells) {
    IRAMA_ASSERT(lines >= 1 && lines <= max_lines, "a pattern has from 1 to 512 lines");
    IRAMA_ASSERT(columns >= 1 && columns <= max_columns, "a line has from 1 to 768 note columns");
    IRAMA_ASSERT(cells.size() == lines * columns,
                 "a pattern has a cell for each column of each line");
    IRAMA_ASSERT(std::ranges::all_of(cells, in_range), "each field of a cell is in its range");
    auto storage = OwnedArray<NoteCell>::create(allocator, cells.size());
    if (!storage) {
        return std::unexpected(storage.error());
    }
    std::ranges::copy(cells, storage->begin());
    return make_owned<PatternPart>(allocator, Key{}, lines, columns, std::move(*storage));
}

PatternPart::PatternPart(Key /*key*/, std::size_t lines, std::size_t columns,
                         OwnedArray<NoteCell> cells)
    : lines_{narrow<std::uint16_t>(lines)}, columns_{narrow<std::uint16_t>(columns)},
      cells_{std::move(cells)} {}

std::span<const NoteCell> PatternPart::line(std::size_t line) const {
    IRAMA_DEBUG_ASSERT(line < lines_, "a line is read within its pattern");
    return cells_.span().subspan(line * columns_, columns_);
}

bool operator==(const PatternPart& left, const PatternPart& right) {
    return left.lines_ == right.lines_ && left.columns_ == right.columns_ &&
           std::ranges::equal(left.cells(), right.cells());
}

} // namespace irama
