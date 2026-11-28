// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/engine_song.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/model/change_set.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

// Whether two entries are both empty, or hold equal parts.
template <typename Pointer>
[[nodiscard]] bool same_part(const Pointer& left, const Pointer& right) {
    if (left == nullptr || right == nullptr) {
        return left == right;
    }
    return *left == *right;
}

} // namespace

namespace irama {

const PatternPart* EngineSong::pattern(std::size_t number) const {
    IRAMA_ASSERT(number < pattern_count, "a pattern number is from 0 to 999");
    return patterns_.at(number).get();
}

const InstrumentPart* EngineSong::instrument(std::size_t number) const {
    IRAMA_ASSERT(number >= first_instrument && number <= last_instrument,
                 "an instrument number is from 01 to FE");
    return instruments_.at(number - first_instrument).get();
}

void EngineSong::apply(ChangeSet& changes) IRAMA_NONBLOCKING {
    // Swapping owning pointers moves no part and frees nothing.
    if (changes.replaces_song_) {
        std::swap(song_, changes.song_);
    }
    for (ChangeSet::PatternEntry& entry : changes.patterns_.span().first(changes.pattern_count_)) {
        std::swap(patterns_.at(entry.number), entry.part);
    }
    for (ChangeSet::InstrumentEntry& entry :
         changes.instruments_.span().first(changes.instrument_count_)) {
        std::swap(instruments_.at(entry.number - first_instrument), entry.part);
    }
}

bool operator==(const EngineSong& left, const EngineSong& right) {
    return same_part(left.song_, right.song_) &&
           std::ranges::equal(left.patterns_, right.patterns_, same_part<PatternPartPtr>) &&
           std::ranges::equal(left.instruments_, right.instruments_, same_part<InstrumentPartPtr>);
}

} // namespace irama
