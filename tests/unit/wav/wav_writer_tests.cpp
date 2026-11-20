// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/wav/wav_writer.hpp>

#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace {

// A path in the temporary folder, named after the test that writes it.
std::filesystem::path temporary_wav(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("irama_wav_tests_" + name + ".wav");
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

// The little-endian 32-bit header field at `offset`.
std::uint32_t field(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t index = 4; index > 0; --index) {
        value = (value << 8U) | bytes.at(offset + index - 1);
    }
    return value;
}

// Where the header holds the size of what follows the RIFF size, the frame count and the size of
// the samples.
constexpr std::size_t riff_size_offset = 4;
constexpr std::size_t frames_offset = 46;
constexpr std::size_t data_size_offset = 54;

} // namespace

TEST_CASE("a WAV writer writes a float32 header that describes its format", "[wav]") {
    const auto path = temporary_wav("header");
    auto writer =
        irama::WavWriter::create(path.string().c_str(), {.sample_rate = 44'100, .channels = 3});
    REQUIRE(writer.has_value());
    const std::array frames{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    REQUIRE(writer->write(frames).has_value());
    REQUIRE(writer->finish().has_value());

    const std::vector<std::uint8_t> header = {
        'R',  'I',  'F',  'F', 74, 0, 0, 0, // the size of what follows
        'W',  'A',  'V',  'E',              //
        'f',  'm',  't',  ' ', 18, 0, 0, 0, // the format, 18 bytes
        3,    0,                            // IEEE float
        3,    0,                            // three channels
        0x44, 0xac, 0,    0,                // 44,100 frames a second
        0x30, 0x13, 0x08, 0,                // 529,200 bytes a second
        12,   0,                            // 12 bytes a frame
        32,   0,                            // 32 bits a sample
        0,    0,                            // no extension
        'f',  'a',  'c',  't', 4,  0, 0, 0, // the frame count, 4 bytes
        2,    0,    0,    0,                // two frames
        'd',  'a',  't',  'a', 24, 0, 0, 0, // the samples, 24 bytes
    };
    const auto bytes = read_bytes(path);
    REQUIRE(bytes.size() == header.size() + 24);
    CHECK(std::vector(bytes.begin(), bytes.begin() + 58) == header);
}

TEST_CASE("a WAV writer writes each call's samples after the last as little-endian float32",
          "[wav]") {
    const auto path = temporary_wav("samples");
    auto writer = irama::WavWriter::create(path.string().c_str(), {});
    REQUIRE(writer.has_value());
    const std::array first{0.5F, -1.0F};
    const std::array second{0.25F, 1.0F, 0.0F, -0.5F};
    REQUIRE(writer->write(first).has_value());
    REQUIRE(writer->write(second).has_value());
    REQUIRE(writer->finish().has_value());

    const auto bytes = read_bytes(path);
    REQUIRE(bytes.size() == 58 + 24);
    CHECK(field(bytes, frames_offset) == 3);
    CHECK(field(bytes, data_size_offset) == 24);
    const std::vector<std::uint8_t> samples = {
        0, 0, 0,    0x3f, // 0.5
        0, 0, 0x80, 0xbf, // -1
        0, 0, 0x80, 0x3e, // 0.25
        0, 0, 0x80, 0x3f, // 1
        0, 0, 0,    0,    // 0
        0, 0, 0,    0xbf, // -0.5
    };
    CHECK(std::vector(bytes.begin() + 58, bytes.end()) == samples);
}

TEST_CASE("a WAV writer given no samples writes an empty WAV file", "[wav]") {
    const auto path = temporary_wav("empty");
    auto writer = irama::WavWriter::create(path.string().c_str(), {});
    REQUIRE(writer.has_value());
    REQUIRE(writer->finish().has_value());

    const auto bytes = read_bytes(path);
    REQUIRE(bytes.size() == 58);
    CHECK(field(bytes, riff_size_offset) == 50); // the rest of the header
    CHECK(field(bytes, frames_offset) == 0);
    CHECK(field(bytes, data_size_offset) == 0);
}

TEST_CASE("creating a WAV writer reports a file it can't open", "[wav]") {
    const auto path =
        std::filesystem::temp_directory_path() / "irama_wav_tests_no_such_folder" / "unopened.wav";
    const auto writer = irama::WavWriter::create(path.string().c_str(), {});
    REQUIRE_FALSE(writer.has_value());
    CHECK(writer.error() == irama::WavError::open_failed);
}

TEST_CASE("a WAV writer writes a file without allocating through operator new", "[wav]") {
    const auto path = temporary_wav("render_scope");
    const auto path_string = path.string();
    const std::array frames{0.5F, 0.5F};
    // The real-time guard, where it is compiled in, stops the test at the first allocation inside
    // the scope.
    const irama::RenderScope scope;
    auto writer = irama::WavWriter::create(path_string.c_str(), {});
    REQUIRE(writer.has_value());
    REQUIRE(writer->write(frames).has_value());
    REQUIRE(writer->finish().has_value());
}
