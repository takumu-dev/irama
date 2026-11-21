// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// An allocator for tests, which draws on the default allocator and keeps a record of every block,
// so that a test can check that each was freed, with the size and alignment it was allocated with.

#pragma once

#include <irama/engine/base/allocator.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace irama::test {

class CountingAllocator {
  public:
    // One block, as it was allocated.
    struct Block {
        void* pointer = nullptr;
        std::size_t size = 0;
        std::size_t alignment = 0;
    };

    CountingAllocator() = default;
    CountingAllocator(const CountingAllocator&) = delete;
    CountingAllocator& operator=(const CountingAllocator&) = delete;
    CountingAllocator(CountingAllocator&&) = delete;
    CountingAllocator& operator=(CountingAllocator&&) = delete;
    ~CountingAllocator() = default;

    // The allocator to hand to the code under test, which records through this one. It must not
    // outlive this one.
    [[nodiscard]] Allocator allocator() {
        return Allocator{.user = this, .allocate = allocate, .free = free};
    }

    // Makes the next allocation return null, as if there were no memory.
    void fail_next() { fail_next_ = true; }

    // How many blocks have been allocated, and how many freed.
    [[nodiscard]] std::size_t allocations() const { return allocations_; }

    [[nodiscard]] std::size_t frees() const { return frees_; }

    // The blocks allocated and not yet freed.
    [[nodiscard]] const std::vector<Block>& live() const { return live_; }

    // The most recent block allocated, freed or not.
    [[nodiscard]] const Block& last() const { return last_; }

    // How many frees named a block that isn't live, or gave a size or alignment other than the
    // block's. Such a free is recorded, but the block isn't freed.
    [[nodiscard]] std::size_t mismatched_frees() const { return mismatched_frees_; }

  private:
    static void* allocate(void* user, std::size_t size, std::size_t alignment) {
        auto& self = *static_cast<CountingAllocator*>(user);
        if (self.fail_next_) {
            self.fail_next_ = false;
            return nullptr;
        }
        const Allocator runtime = default_allocator();
        void* pointer = runtime.allocate(runtime.user, size, alignment);
        if (pointer != nullptr) {
            ++self.allocations_;
            self.last_ = Block{.pointer = pointer, .size = size, .alignment = alignment};
            self.live_.push_back(self.last_);
        }
        return pointer;
    }

    static void free(void* user, void* pointer, std::size_t size, std::size_t alignment) {
        auto& self = *static_cast<CountingAllocator*>(user);
        ++self.frees_;
        const auto block = std::ranges::find(self.live_, pointer, &Block::pointer);
        if (block == self.live_.end() || block->size != size || block->alignment != alignment) {
            ++self.mismatched_frees_;
            return;
        }
        self.live_.erase(block);
        const Allocator runtime = default_allocator();
        runtime.free(runtime.user, pointer, size, alignment);
    }

    std::vector<Block> live_;
    Block last_;
    std::size_t allocations_ = 0;
    std::size_t frees_ = 0;
    std::size_t mismatched_frees_ = 0;
    bool fail_next_ = false;
};

} // namespace irama::test
