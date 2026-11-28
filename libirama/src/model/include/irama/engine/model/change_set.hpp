// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The change set: every part rebuilt for one edit, which replaces its entries in an EngineSong
/// together.

#pragma once

#include <irama/engine/base/allocator.hpp>
#include <irama/engine/base/owned_array.hpp>
#include <irama/engine/base/owned_ptr.hpp>
#include <irama/engine/model/instrument_part.hpp>
#include <irama/engine/model/pattern_part.hpp>
#include <irama/engine/model/song_part.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace irama {

/// How much a change set has room for.
struct ChangeSetSize {
    /// How many pattern entries it replaces, at most.
    std::size_t patterns = 0;
    /// How many instrument entries it replaces, at most.
    std::size_t instruments = 0;
    /// The length of its slot map: the slot count before the edit, or 0 for no map.
    std::size_t slots = 0;
    /// The length of its track map: the track count before the edit, or 0 for no map.
    std::size_t tracks = 0;
};

class ChangeSet;

/// The single owner of a change set.
using ChangeSetPtr = OwnedPtr<ChangeSet>;

/// Every part rebuilt for one edit, and the edit's position maps, which an EngineSong takes whole.
///
/// A control thread builds a change set and fills it with parts. `EngineSong::apply()` then puts
/// each part in its entry, and the part the entry held in its place in the change set, so the
/// change set ends up holding the parts it replaced, to be freed where it is dropped. A null part
/// empties its entry.
///
/// A **position map** says where each slot or track of the song before the edit went: for each old
/// position, its new one, or `gone`. A change set whose edit moves no slot or track has no maps.
class ChangeSet {
    // Lets create() alone construct a change set, through make_owned().
    struct Key {
        explicit Key() = default;
    };

  public:
    /// A position map's entry for a slot or a track the edit removed.
    static constexpr std::uint16_t gone = 0xFFFF;

    /// A pattern entry the change set replaces, and the part it holds for it.
    struct PatternEntry {
        /// The pattern number, from 0 to 999.
        std::uint16_t number = 0;
        /// The part, or null for an empty entry.
        PatternPartPtr part;
    };

    /// An instrument entry the change set replaces, and the part it holds for it.
    struct InstrumentEntry {
        /// The instrument number, from 0x01 to 0xFE.
        std::uint8_t number = 0;
        /// The part, or null for an empty entry.
        InstrumentPartPtr part;
    };

    /// Makes an empty change set, on a control thread, with room for the entries and maps `size`
    /// asks for.
    ///
    /// \param allocator The allocator to draw on, which the change set is freed to.
    /// \param size How many entries of each kind it has room for, and how long its maps are.
    /// \return The change set, or `AllocationError::out_of_memory` when the allocator has no
    ///         memory.
    [[nodiscard]] static std::expected<ChangeSetPtr, AllocationError>
    create(const Allocator& allocator, const ChangeSetSize& size = {});

    /// Makes an empty change set with no room, for `create()` alone.
    ///
    /// \param key Which only `create()` can make.
    /// \param allocator The allocator it was made from.
    ChangeSet(Key key, const Allocator& allocator);

    /// Sets the song part, which the change set doesn't hold yet, which is checked.
    ///
    /// \param part The part, never null, which is checked.
    void replace_song(SongPartPtr part);

    /// Adds a pattern entry, while the change set has room for one, which is checked.
    ///
    /// \param number The pattern number, from 0 to 999, which is checked.
    /// \param part The part, or null to empty the entry.
    void replace_pattern(std::size_t number, PatternPartPtr part);

    /// Adds an instrument entry, while the change set has room for one, which is checked.
    ///
    /// \param number The instrument number, from 0x01 to 0xFE, which is checked.
    /// \param part The part, or null to empty the entry.
    void replace_instrument(std::size_t number, InstrumentPartPtr part);

    /// \return Whether the change set replaces the song part.
    [[nodiscard]] bool replaces_song() const { return replaces_song_; }

    /// \return The song part it holds, if it replaces the song part: the new one until it is
    ///         applied, the old one after.
    [[nodiscard]] const SongPart* song() const { return song_.get(); }

    /// \return The pattern entries it replaces, in the order they were added.
    [[nodiscard]] std::span<const PatternEntry> patterns() const;

    /// \return The instrument entries it replaces, in the order they were added.
    [[nodiscard]] std::span<const InstrumentEntry> instruments() const;

    /// \return The slot map, empty if it has none, for a control thread to fill.
    [[nodiscard]] std::span<std::uint16_t> slot_map() { return slot_map_.span(); }

    /// \return The slot map, empty if it has none.
    [[nodiscard]] std::span<const std::uint16_t> slot_map() const { return slot_map_.span(); }

    /// \return The track map, empty if it has none, for a control thread to fill.
    [[nodiscard]] std::span<std::uint16_t> track_map() { return track_map_.span(); }

    /// \return The track map, empty if it has none.
    [[nodiscard]] std::span<const std::uint16_t> track_map() const { return track_map_.span(); }

    /// \return Whether it replaces nothing and has no maps, so that sending it would change
    ///         nothing.
    [[nodiscard]] bool empty() const;

    /// \return The allocator the change set was made from, which frees it.
    [[nodiscard]] const Allocator& allocator() const { return allocator_; }

  private:
    // Swaps the parts, the one place that changes them once they are added.
    friend class EngineSong;

    Allocator allocator_;
    bool replaces_song_ = false;
    SongPartPtr song_;
    OwnedArray<PatternEntry> patterns_;
    std::size_t pattern_count_ = 0;
    OwnedArray<InstrumentEntry> instruments_;
    std::size_t instrument_count_ = 0;
    OwnedArray<std::uint16_t> slot_map_;
    OwnedArray<std::uint16_t> track_map_;
};

} // namespace irama
