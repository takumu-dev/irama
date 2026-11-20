// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/spsc_queue.hpp>

#include <irama/engine/base/nonblocking.hpp>
#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstddef>
#include <optional>
#include <thread>

namespace {

// Stands in for a render entry point: the real-time guard, where it is compiled in, and
// RealtimeSanitizer, where that is on, stop the test at the first allocation inside it.
template <typename Body> void render(Body body) IRAMA_NONBLOCKING {
    const irama::RenderScope scope;
    body();
}

// Pushes and pops `slots` messages, which leaves the empty queue that many slots further on.
void advance_empty(irama::SpscQueue<int, 3>& queue, std::size_t slots) {
    for (std::size_t slot = 0; slot < slots; ++slot) {
        REQUIRE(queue.try_push(0));
        REQUIRE(queue.try_pop() == 0);
    }
}

// Two words, so that a message torn by a race reads as one that was never sent.
struct Message {
    std::size_t sequence = 0;
    std::size_t complement = 0;
};

// Small, so that the producer often finds it full and the consumer often finds it empty.
using StressQueue = irama::SpscQueue<Message, 16>;

constexpr std::size_t stress_count = 200'000;

// Catch2's checks aren't made for threads, so the threads only count, and the test checks after.
void produce(StressQueue& queue) {
    std::size_t sequence = 0;
    while (sequence < stress_count) {
        if (queue.try_push(Message{.sequence = sequence, .complement = ~sequence})) {
            ++sequence;
        } else {
            std::this_thread::yield();
        }
    }
}

struct Received {
    std::size_t count = 0;
    std::size_t out_of_order_or_torn = 0;
};

Received consume(StressQueue& queue) {
    Received received;
    while (received.count < stress_count) {
        if (const auto message = queue.try_pop()) {
            if (message->sequence != received.count || message->complement != ~received.count) {
                ++received.out_of_order_or_torn;
            }
            ++received.count;
        } else {
            std::this_thread::yield();
        }
    }
    return received;
}

} // namespace

TEST_CASE("a new spsc queue has nothing to pop", "[base]") {
    irama::SpscQueue<int, 4> queue;
    REQUIRE(queue.try_pop() == std::nullopt);
}

TEST_CASE("a full spsc queue refuses a push and keeps what it holds", "[base]") {
    irama::SpscQueue<int, 3> queue;
    REQUIRE(queue.try_push(1));
    REQUIRE(queue.try_push(2));
    REQUIRE(queue.try_push(3));
    REQUIRE_FALSE(queue.try_push(4));
    REQUIRE(queue.try_pop() == 1);
    REQUIRE(queue.try_pop() == 2);
    REQUIRE(queue.try_pop() == 3);
    REQUIRE(queue.try_pop() == std::nullopt);
}

TEST_CASE("an spsc queue keeps its order and its capacity as it wraps around", "[base]") {
    // Starting from each of its four slots in turn, so that what it holds straddles the end of its
    // storage in most cases.
    const std::size_t start = GENERATE(range(std::size_t{0}, std::size_t{4}));
    irama::SpscQueue<int, 3> queue;
    advance_empty(queue, start);
    REQUIRE(queue.try_push(1));
    REQUIRE(queue.try_push(2));
    REQUIRE(queue.try_push(3));
    REQUIRE_FALSE(queue.try_push(0));
    REQUIRE(queue.try_pop() == 1);
    REQUIRE(queue.try_push(4));
    REQUIRE_FALSE(queue.try_push(0));
    REQUIRE(queue.try_pop() == 2);
    REQUIRE(queue.try_pop() == 3);
    REQUIRE(queue.try_pop() == 4);
    REQUIRE(queue.try_pop() == std::nullopt);
}

TEST_CASE("an spsc queue pushes and pops inside a render call without allocating", "[base]") {
    irama::SpscQueue<int, 2> queue;
    // Each result starts as the opposite of what the render call should leave, so that a render
    // call that never ran fails the test.
    bool first_pushed = false;
    bool second_pushed = false;
    bool third_pushed = true;
    std::optional<int> first_popped;
    std::optional<int> second_popped;
    std::optional<int> third_popped = 0;
    // Catch2 may allocate as it checks, so the checks wait until the render call has ended.
    render([&] {
        first_pushed = queue.try_push(1);
        second_pushed = queue.try_push(2);
        third_pushed = queue.try_push(3);
        first_popped = queue.try_pop();
        second_popped = queue.try_pop();
        third_popped = queue.try_pop();
    });
    REQUIRE(first_pushed);
    REQUIRE(second_pushed);
    REQUIRE_FALSE(third_pushed);
    REQUIRE(first_popped == 1);
    REQUIRE(second_popped == 2);
    REQUIRE(third_popped == std::nullopt);
}

TEST_CASE("an spsc queue hands every message from one thread to another, in order", "[base]") {
    StressQueue queue;
    std::thread producer{[&] { produce(queue); }};
    Received received;
    std::thread consumer{[&] { received = consume(queue); }};
    producer.join();
    consumer.join();

    REQUIRE(received.count == stress_count);
    REQUIRE(received.out_of_order_or_torn == 0);
    REQUIRE(queue.try_pop() == std::nullopt);
}
