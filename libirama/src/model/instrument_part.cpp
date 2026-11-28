// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/model/instrument_part.hpp>

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <expected>
#include <span>
#include <utility>

namespace irama {

std::expected<InstrumentPartPtr, AllocationError>
InstrumentPart::create(const Allocator& allocator, std::span<const float> values) {
    IRAMA_ASSERT(!values.empty(), "an instrument has at least one parameter");
    IRAMA_ASSERT(std::ranges::none_of(values, [](float value) { return std::isnan(value); }),
                 "a parameter's value is a number");
    auto storage = OwnedArray<float>::create(allocator, values.size());
    if (!storage) {
        return std::unexpected(storage.error());
    }
    std::ranges::copy(values, storage->begin());
    return make_owned<InstrumentPart>(allocator, Key{}, std::move(*storage));
}

InstrumentPart::InstrumentPart(Key /*key*/, OwnedArray<float> values)
    : values_{std::move(values)} {}

bool operator==(const InstrumentPart& left, const InstrumentPart& right) {
    return std::ranges::equal(left.values(), right.values());
}

} // namespace irama
