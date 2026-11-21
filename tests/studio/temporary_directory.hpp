// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#pragma once

#include <filesystem>
#include <format>
#include <random>

namespace irama_studio_tests {

/// A new, empty directory of the test's own, removed with everything in it when the test ends.
/// CTest runs test cases side by side, so each takes a random name.
class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::random_device random;
        path_ = std::filesystem::temp_directory_path() /
                std::format("irama-studio-tests-{:08x}{:08x}", random(), random());
        std::filesystem::create_directories(path_);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace irama_studio_tests
