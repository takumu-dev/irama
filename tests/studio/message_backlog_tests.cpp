// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/message_backlog.hpp"

#include "input/note_message.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <utility>
#include <vector>

using irama_studio::MessageBacklog;
using irama_studio::NoteMessage;

namespace {

[[nodiscard]] NoteMessage note_on(int note) {
    return {.kind = NoteMessage::Kind::note_on, .note = note, .velocity = 1.0F};
}

[[nodiscard]] NoteMessage note_off(int note) {
    return {.kind = NoteMessage::Kind::note_off, .note = note};
}

// Stands in for a player's queue, which takes messages until it is full.
class Queue {
  public:
    explicit Queue(std::size_t room) : room_(room) {}

    [[nodiscard]] bool push(const NoteMessage& message) {
        if (taken_.size() == room_) {
            return false;
        }
        taken_.push_back(message);
        return true;
    }

    // Empties the queue, as a render call would.
    [[nodiscard]] std::vector<NoteMessage> drain() { return std::exchange(taken_, {}); }

    [[nodiscard]] auto pusher() {
        return [this](const NoteMessage& message) { return push(message); };
    }

  private:
    std::size_t room_;
    std::vector<NoteMessage> taken_;
};

} // namespace

TEST_CASE("a message the queue takes doesn't wait", "[studio][backlog]") {
    Queue queue{4};
    MessageBacklog backlog;
    backlog.send(note_on(48), queue.pusher());
    CHECK(backlog.empty());
    CHECK(queue.drain() == std::vector{note_on(48)});
}

TEST_CASE("messages the queue refuses wait, and go first, in order, once it has room",
          "[studio][backlog]") {
    Queue queue{1};
    MessageBacklog backlog;
    backlog.send(note_on(48), queue.pusher());
    backlog.send(note_on(52), queue.pusher());
    backlog.send(note_off(48), queue.pusher());
    CHECK(backlog.size() == 2);
    CHECK(queue.drain() == std::vector{note_on(48)});

    // A new message waits behind those before it, though the queue has room for one.
    backlog.send(note_off(52), queue.pusher());
    CHECK(backlog.size() == 2);
    CHECK(queue.drain() == std::vector{note_on(52)});

    backlog.flush(queue.pusher());
    CHECK(queue.drain() == std::vector{note_off(48)});
    backlog.flush(queue.pusher());
    CHECK(queue.drain() == std::vector{note_off(52)});
    CHECK(backlog.empty());
}

TEST_CASE("flushing sends every waiting message the queue has room for", "[studio][backlog]") {
    Queue full{0};
    MessageBacklog backlog;
    backlog.send(note_on(48), full.pusher());
    backlog.send(note_on(52), full.pusher());
    backlog.send(note_on(55), full.pusher());
    REQUIRE(backlog.size() == 3);

    Queue roomy{8};
    backlog.flush(roomy.pusher());
    CHECK(backlog.empty());
    CHECK(roomy.drain() == std::vector{note_on(48), note_on(52), note_on(55)});
}

TEST_CASE("clearing the backlog drops what waits", "[studio][backlog]") {
    Queue full{0};
    MessageBacklog backlog;
    backlog.send(note_on(48), full.pusher());
    backlog.clear();
    CHECK(backlog.empty());
    Queue roomy{8};
    backlog.flush(roomy.pusher());
    CHECK(roomy.drain().empty());
}
