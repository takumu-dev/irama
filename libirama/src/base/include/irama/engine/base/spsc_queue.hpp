// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

/// \file
/// The queue that carries messages from one thread to another, either of them the audio thread.

#pragma once

#include <irama/engine/base/nonblocking.hpp>

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace irama {

/// A lock-free queue of up to `Capacity` messages, from one producer thread to one consumer thread.
///
/// It is how threads talk to the audio thread, and how the audio thread answers. One thread pushes
/// and one other thread pops, for the queue's whole life: two producers, or two consumers, race.
/// Either thread may be the audio thread, since a push or a pop keeps the real-time rules. Neither
/// allocates, takes a lock or waits: a push finds the queue full, or a pop finds it empty, and says
/// so at once, leaving the caller to try again later.
///
/// The messages live inside the queue, so making one allocates nothing either. A message must be
/// trivially copyable: popping one copies it out and leaves its place to be overwritten, so no
/// destructor runs on either thread, and a message can't own memory that the audio thread would
/// then free.
///
/// \tparam T The message, which is trivially copyable and can be made with no arguments.
/// \tparam Capacity How many messages the queue holds at most, at least one.
template <typename T, std::size_t Capacity>
    requires std::is_trivially_copyable_v<T> && std::default_initializable<T> && (Capacity > 0)
class SpscQueue {
  public:
    /// Adds a message at the back, unless the queue is full. Only the producer thread calls this.
    ///
    /// \param message The message to add.
    /// \return Whether the message was added. A full queue leaves what it holds as it was.
    [[nodiscard]] bool try_push(const T& message) IRAMA_NONBLOCKING {
        const std::size_t back = back_.load(std::memory_order_relaxed);
        const std::size_t next_back = next(back);
        // Acquire, so that the consumer has finished reading the slot before it is overwritten.
        if (next_back == front_.load(std::memory_order_acquire)) {
            return false;
        }
        slot(back) = message;
        // Release, so that the consumer sees the whole message once it sees the new back.
        back_.store(next_back, std::memory_order_release);
        return true;
    }

    /// Takes the message at the front, unless the queue is empty. Only the consumer thread calls
    /// this.
    ///
    /// \return The oldest message in the queue, or nothing if it is empty.
    [[nodiscard]] std::optional<T> try_pop() IRAMA_NONBLOCKING {
        const std::size_t front = front_.load(std::memory_order_relaxed);
        // Acquire, pairing with the release in try_push().
        if (front == back_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        const T message = slot(front);
        // Release, so that the producer overwrites the slot only after it has been read.
        front_.store(next(front), std::memory_order_release);
        return message;
    }

  private:
    // An atomic that isn't lock-free takes a lock, which the audio thread mustn't.
    static_assert(std::atomic<std::size_t>::is_always_lock_free);

    // The slot after `index`, wrapping round to the first.
    static constexpr std::size_t next(std::size_t index) {
        return index == Capacity ? 0 : index + 1;
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): next() keeps it in bounds.
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): as above.
    T& slot(std::size_t index) { return slots_[index]; }

    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

    // One slot more than the capacity, which always stays empty, so that a full queue's back sits
    // just behind its front rather than on it, where an empty queue's does.
    std::array<T, Capacity + 1> slots_{};
    // The slot of the oldest message, which only the consumer moves.
    std::atomic<std::size_t> front_ = 0;
    // The slot the next message goes in, which only the producer moves.
    std::atomic<std::size_t> back_ = 0;
};

} // namespace irama
