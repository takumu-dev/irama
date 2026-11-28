// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace irama_studio {

/// The environment variables that decide where Studio keeps its settings. An empty string stands
/// for a variable that isn't set.
struct Environment {
    std::string xdg_config_home{};
    std::string home{};
    /// The Roaming application data folder, which Windows gives every user.
    std::string appdata{};
};

/// Reads the environment variables that decide where Studio keeps its settings: `APPDATA` on
/// Windows, and `XDG_CONFIG_HOME` and `HOME` elsewhere.
[[nodiscard]] Environment read_environment();

/// The directory that holds `studio.ini` and `imgui.ini`: the one `--config-dir` gives, or else
/// `irama` in `APPDATA`, or else `$XDG_CONFIG_HOME/irama`, or else `$HOME/.config/irama`. As the
/// XDG base directory specification says, an `XDG_CONFIG_HOME` that isn't an absolute path is
/// ignored. With none of them, there is no directory, and Studio keeps nothing.
[[nodiscard]] std::optional<std::filesystem::path>
config_directory(const std::optional<std::filesystem::path>& option,
                 const Environment& environment);

/// Reads a whole file, or gives nothing if it is missing or can't be read.
[[nodiscard]] std::optional<std::string> read_text_file(const std::filesystem::path& path);

/// Replaces a file's contents with `text`, making its directory first if there is none. The text
/// goes to a temporary file beside it, which is then renamed over it, so the file always holds
/// either its old text or its new text, never part of either. If it can't, the error says why.
[[nodiscard]] std::expected<void, std::string>
write_file_atomically(const std::filesystem::path& path, const std::string& text);

/// A file that is written a while after its text last changed, so that dragging a window writes
/// it once, after the drag, rather than on every frame of it.
class DelayedFile {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long after the last change the file is written.
    static constexpr Clock::duration delay = std::chrono::seconds{1};

    /// A file at `path` that already holds `written`, the text read from it, or an empty string
    /// for a file that doesn't exist yet.
    DelayedFile(std::filesystem::path path, std::string written);

    /// Gives the text the file should hold, as it is at `now`. Text that the file already holds, or
    /// is already waiting to be written, is no change, and starts no new delay.
    void change(std::string text, Clock::time_point now);

    /// Writes the file if its text changed at least `delay` before `now`, and hasn't changed since.
    [[nodiscard]] std::expected<void, std::string> write_if_due(Clock::time_point now);

    /// Writes the file now if its text has changed, as on quitting.
    [[nodiscard]] std::expected<void, std::string> flush();

  private:
    std::filesystem::path path_;
    std::string written_;
    std::optional<std::string> pending_;
    Clock::time_point changed_at_;
};

} // namespace irama_studio
