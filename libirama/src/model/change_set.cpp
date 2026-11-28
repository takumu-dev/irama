// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/change_set.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace {

constexpr std::size_t last_pattern = 999;
constexpr std::size_t last_instrument = 0xFE;

} // namespace

namespace irama {

std::expected<ChangeSetPtr, AllocationError> ChangeSet::create(const Allocator& allocator,
                                                               const ChangeSetSize& size) {
    auto changes = make_owned<ChangeSet>(allocator, Key{}, allocator);
    auto patterns = OwnedArray<PatternEntry>::create(allocator, size.patterns);
    auto instruments = OwnedArray<InstrumentEntry>::create(allocator, size.instruments);
    auto slot_map = OwnedArray<std::uint16_t>::create(allocator, size.slots);
    auto track_map = OwnedArray<std::uint16_t>::create(allocator, size.tracks);
    if (!changes || !patterns || !instruments || !slot_map || !track_map) {
        return std::unexpected(AllocationError::out_of_memory);
    }
    (*changes)->patterns_ = std::move(*patterns);
    (*changes)->instruments_ = std::move(*instruments);
    (*changes)->slot_map_ = std::move(*slot_map);
    (*changes)->track_map_ = std::move(*track_map);
    return std::move(*changes);
}

ChangeSet::ChangeSet(Key /*key*/, const Allocator& allocator) : allocator_{allocator} {}

void ChangeSet::replace_song(SongPartPtr part) {
    IRAMA_ASSERT(part != nullptr, "a song always has a song part");
    IRAMA_ASSERT(!replaces_song_, "a change set holds one song part");
    song_ = std::move(part);
    replaces_song_ = true;
}

void ChangeSet::replace_pattern(std::size_t number, PatternPartPtr part) {
    IRAMA_ASSERT(number <= last_pattern, "a pattern number is from 0 to 999");
    IRAMA_ASSERT(pattern_count_ < patterns_.size(), "a change set has room for each pattern");
    patterns_[pattern_count_++] = {
        .number = narrow<std::uint16_t>(number),
        .part = std::move(part),
    };
}

void ChangeSet::replace_instrument(std::size_t number, InstrumentPartPtr part) {
    IRAMA_ASSERT(number >= 1 && number <= last_instrument, "an instrument number is from 01 to FE");
    IRAMA_ASSERT(instrument_count_ < instruments_.size(),
                 "a change set has room for each instrument");
    instruments_[instrument_count_++] = {
        .number = narrow<std::uint8_t>(number),
        .part = std::move(part),
    };
}

std::span<const ChangeSet::PatternEntry> ChangeSet::patterns() const {
    return patterns_.span().first(pattern_count_);
}

std::span<const ChangeSet::InstrumentEntry> ChangeSet::instruments() const {
    return instruments_.span().first(instrument_count_);
}

bool ChangeSet::empty() const {
    return !replaces_song_ && pattern_count_ == 0 && instrument_count_ == 0 && slot_map_.empty() &&
           track_map_.empty();
}

} // namespace irama
