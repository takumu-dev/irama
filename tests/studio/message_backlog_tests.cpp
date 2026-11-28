// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include "audio/message_backlog.hpp"

#include "input/note_message.hpp"
#include <irama/engine/base/allocator.hpp>
#include <irama/engine/model/change_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

using irama_studio::MessageBacklog;
using irama_studio::NoteMessage;
using irama_studio::PlayerMessage;

namespace {

[[nodiscard]] NoteMessage note_on(int note) {
    return {.kind = NoteMessage::Kind::note_on, .note = note, .velocity = 1.0F};
}

[[nodiscard]] NoteMessage note_off(int note) {
    return {.kind = NoteMessage::Kind::note_off, .note = note};
}

[[nodiscard]] irama::ChangeSetPtr change_set() {
    auto changes = irama::ChangeSet::create(irama::default_allocator());
    REQUIRE(changes.has_value());
    return std::move(*changes);
}

// What the queue took: a note key's message, or which change set.
using Taken = std::variant<NoteMessage, const irama::ChangeSet*>;

// Stands in for a player's queue, which takes messages until it is full, and takes a change set out
// of its message as the player does.
class Queue {
  public:
    explicit Queue(std::size_t room) : room_(room) {}

    [[nodiscard]] bool push(PlayerMessage& message) {
        if (taken_.size() == room_) {
            return false;
        }
        if (auto* changes = std::get_if<irama::ChangeSetPtr>(&message)) {
            taken_.emplace_back(changes->get());
            owned_.push_back(std::move(*changes));
        } else {
            taken_.emplace_back(std::get<NoteMessage>(message));
        }
        return true;
    }

    // Empties the queue, as a render call would.
    [[nodiscard]] std::vector<Taken> drain() { return std::exchange(taken_, {}); }

    [[nodiscard]] auto pusher() {
        return [this](PlayerMessage& message) { return push(message); };
    }

  private:
    std::size_t room_;
    std::vector<Taken> taken_;
    std::vector<irama::ChangeSetPtr> owned_;
};

} // namespace

TEST_CASE("a message the queue takes doesn't wait", "[studio][backlog]") {
    Queue queue{4};
    MessageBacklog backlog;
    backlog.send(note_on(48), queue.pusher());
    CHECK(backlog.empty());
    CHECK(queue.drain() == std::vector<Taken>{note_on(48)});
}

TEST_CASE("messages the queue refuses wait, and go first, in order, once it has room",
          "[studio][backlog]") {
    Queue queue{1};
    MessageBacklog backlog;
    backlog.send(note_on(48), queue.pusher());
    backlog.send(note_on(52), queue.pusher());
    backlog.send(note_off(48), queue.pusher());
    CHECK(backlog.size() == 2);
    CHECK(queue.drain() == std::vector<Taken>{note_on(48)});

    // A new message waits behind those before it, though the queue has room for one.
    backlog.send(note_off(52), queue.pusher());
    CHECK(backlog.size() == 2);
    CHECK(queue.drain() == std::vector<Taken>{note_on(52)});

    backlog.flush(queue.pusher());
    CHECK(queue.drain() == std::vector<Taken>{note_off(48)});
    backlog.flush(queue.pusher());
    CHECK(queue.drain() == std::vector<Taken>{note_off(52)});
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
    CHECK(roomy.drain() == std::vector<Taken>{note_on(48), note_on(52), note_on(55)});
}

TEST_CASE("a refused change set waits in order between other messages, each one apart",
          "[studio][backlog]") {
    Queue full{0};
    MessageBacklog backlog;
    irama::ChangeSetPtr first = change_set();
    irama::ChangeSetPtr second = change_set();
    const irama::ChangeSet* first_sent = first.get();
    const irama::ChangeSet* second_sent = second.get();
    backlog.send(note_on(48), full.pusher());
    backlog.send(std::move(first), full.pusher());
    backlog.send(std::move(second), full.pusher());
    backlog.send(note_off(48), full.pusher());
    // Two change sets in a row stay two.
    REQUIRE(backlog.size() == 4);

    // Once there is room, they go first, in order, before a new message.
    Queue two{2};
    backlog.send(note_on(60), two.pusher());
    CHECK(two.drain() == std::vector<Taken>{note_on(48), first_sent});
    backlog.flush(two.pusher());
    CHECK(two.drain() == std::vector<Taken>{second_sent, note_off(48)});
    backlog.flush(two.pusher());
    CHECK(two.drain() == std::vector<Taken>{note_on(60)});
    CHECK(backlog.empty());
}

TEST_CASE("clearing the backlog drops what waits", "[studio][backlog]") {
    Queue full{0};
    MessageBacklog backlog;
    backlog.send(note_on(48), full.pusher());
    backlog.send(change_set(), full.pusher());
    backlog.clear();
    CHECK(backlog.empty());
    Queue roomy{8};
    backlog.flush(roomy.pusher());
    CHECK(roomy.drain().empty());
}
