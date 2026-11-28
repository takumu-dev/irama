// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/config_files.hpp"

#include <irama/engine/base/narrow.hpp>

#include <SDL3/SDL_stdinc.h>

#include <cerrno>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace irama_studio {
namespace {

// The subdirectory of the user's configuration directory that is Studio's.
constexpr const char* directory_name = "irama";

[[nodiscard]] std::string environment_variable(const char* name) {
    const char* const value = SDL_getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
}

// Why the last file operation of the C++ library failed, which it leaves only in errno.
[[nodiscard]] std::string last_error() { return std::generic_category().message(errno); }

} // namespace

Environment read_environment() {
#ifdef _WIN32
    return Environment{.appdata = environment_variable("APPDATA")};
#else
    return Environment{
        .xdg_config_home = environment_variable("XDG_CONFIG_HOME"),
        .home = environment_variable("HOME"),
    };
#endif
}

std::optional<std::filesystem::path>
config_directory(const std::optional<std::filesystem::path>& option,
                 const Environment& environment) {
    if (option) {
        return option;
    }
    if (!environment.appdata.empty()) {
        return std::filesystem::path{environment.appdata} / directory_name;
    }
    // An absolute path in the XDG specification's sense, which is a POSIX one, starts at the root.
    const std::filesystem::path config_home{environment.xdg_config_home};
    if (config_home.has_root_directory()) {
        return config_home / directory_name;
    }
    if (!environment.home.empty()) {
        return std::filesystem::path{environment.home} / ".config" / directory_name;
    }
    return std::nullopt;
}

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if (file.bad()) {
        return std::nullopt;
    }
    return text;
}

std::expected<void, std::string> write_file_atomically(const std::filesystem::path& path,
                                                       const std::string& text) {
    std::error_code error;
    const std::filesystem::path directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
        if (error) {
            return std::unexpected{
                std::format("couldn't make {}: {}", directory.string(), error.message())};
        }
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
    if (!file) {
        return std::unexpected{
            std::format("couldn't write {}: {}", temporary.string(), last_error())};
    }
    file.write(text.data(), irama::narrow<std::streamsize>(text.size()));
    file.close();
    if (!file) {
        const std::string why = last_error();
        std::filesystem::remove(temporary, error);
        return std::unexpected{std::format("couldn't write {}: {}", temporary.string(), why)};
    }

    std::filesystem::rename(temporary, path, error);
    if (error) {
        const std::string why = error.message();
        std::filesystem::remove(temporary, error);
        return std::unexpected{std::format("couldn't replace {}: {}", path.string(), why)};
    }
    return {};
}

DelayedFile::DelayedFile(std::filesystem::path path, std::string written)
    : path_(std::move(path)), written_(std::move(written)) {}

void DelayedFile::change(std::string text, Clock::time_point now) {
    if (pending_ ? text == *pending_ : text == written_) {
        return;
    }
    if (text == written_) {
        // Changed back before it was written.
        pending_.reset();
        return;
    }
    pending_ = std::move(text);
    changed_at_ = now;
}

std::expected<void, std::string> DelayedFile::write_if_due(Clock::time_point now) {
    if (!pending_ || now - changed_at_ < delay) {
        return {};
    }
    return flush();
}

std::expected<void, std::string> DelayedFile::flush() {
    if (!pending_) {
        return {};
    }
    std::string text = std::move(*pending_);
    pending_.reset();
    // A file that can't be written is tried again on its next change, not on every frame.
    auto written = write_file_atomically(path_, text);
    if (written) {
        written_ = std::move(text);
    }
    return written;
}

} // namespace irama_studio
