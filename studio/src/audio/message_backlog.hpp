// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/note_message.hpp"
#include <irama/engine/model/change_set.hpp>

#include <concepts>
#include <cstddef>
#include <deque>
#include <utility>
#include <variant>

namespace irama_studio {

/// A message for the player: a note key's, or a change set of the song.
using PlayerMessage = std::variant<NoteMessage, irama::ChangeSetPtr>;

/// The messages a player's queue has refused, waiting on the UI thread to be sent, in order, before
/// any new one. The queue fills only while nothing renders, and a note-off lost there would leave a
/// note stuck, so nothing is dropped. Change sets wait like any other message, each as it was sent,
/// and are never merged.
class MessageBacklog {
  public:
    /// Sends `message` through `deliver` once every waiting message has gone, and otherwise keeps
    /// it waiting behind them.
    ///
    /// \param message The message to send.
    /// \param deliver Sends one message, and returns whether it was taken. It may take a change set
    ///                out of the message it is given only when it returns true.
    template <std::predicate<PlayerMessage&> Deliver>
    void send(PlayerMessage message, Deliver deliver) {
        waiting_.push_back(std::move(message));
        flush(deliver);
    }

    /// Sends the waiting messages through `deliver`, in order, until one is refused.
    ///
    /// \param deliver Sends one message, and returns whether it was taken.
    template <std::predicate<PlayerMessage&> Deliver> void flush(Deliver deliver) {
        while (!waiting_.empty() && deliver(waiting_.front())) {
            waiting_.pop_front();
        }
    }

    /// Drops every waiting message, as when the player they were meant for is gone.
    void clear() { waiting_.clear(); }

    /// \return Whether no message waits.
    [[nodiscard]] bool empty() const { return waiting_.empty(); }

    /// \return How many messages wait.
    [[nodiscard]] std::size_t size() const { return waiting_.size(); }

  private:
    std::deque<PlayerMessage> waiting_;
};

} // namespace irama_studio
