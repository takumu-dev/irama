// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/wav/wav_writer.hpp>

#include <irama/engine/base/assertion.hpp>
#include <irama/engine/base/narrow.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <limits>
#include <span>
#include <string_view>

namespace irama {
namespace {

static_assert(std::numeric_limits<float>::is_iec559, "a WAV file's float samples are IEEE 754");
// WAV is little-endian, as is every platform Irama builds on, so the samples are written as they
// lie in memory.
static_assert(std::endian::native == std::endian::little, "a WAV file is little-endian");

constexpr std::uint16_t ieee_float = 3;
constexpr auto bytes_per_sample = narrow<std::uint32_t>(sizeof(float));

// The RIFF header, then the fmt chunk with an empty extension, as every format but PCM has, then
// the fact chunk with the frame count, which every format but PCM needs, then the data chunk's
// header.
constexpr std::size_t header_bytes = 12 + 26 + 12 + 8;
// The RIFF chunk's size counts everything after its own size field.
constexpr auto riff_bytes_before_data = narrow<std::uint32_t>(header_bytes - 8);
constexpr std::uint32_t max_data_bytes =
    std::numeric_limits<std::uint32_t>::max() - riff_bytes_before_data;

// The header of a file holding `data_bytes` bytes of samples in `format`.
class Header {
  public:
    Header(WavFormat format, std::uint32_t data_bytes) {
        const std::uint32_t frame_bytes = bytes_per_sample * format.channels;
        tag("RIFF");
        u32(riff_bytes_before_data + data_bytes);
        tag("WAVE");
        tag("fmt ");
        u32(18);
        u16(ieee_float);
        u16(format.channels);
        u32(format.sample_rate);
        u32(narrow<std::uint32_t>(std::uint64_t{format.sample_rate} * frame_bytes));
        u16(narrow<std::uint16_t>(frame_bytes));
        u16(narrow<std::uint16_t>(bytes_per_sample * 8));
        u16(0);
        tag("fact");
        u32(4);
        u32(data_bytes / frame_bytes);
        tag("data");
        u32(data_bytes);
        IRAMA_DEBUG_ASSERT(size_ == bytes_.size(), "the header fills its bytes exactly");
    }

    [[nodiscard]] bool write_to(std::FILE* file) const {
        return std::fwrite(bytes_.data(), 1, bytes_.size(), file) == bytes_.size();
    }

  private:
    // Checked, since the fields' sizes must add up to the header's.
    void byte(std::uint8_t value) { bytes_.at(size_++) = value; }

    void tag(std::string_view name) {
        IRAMA_DEBUG_ASSERT(name.size() == 4, "a chunk's tag is four characters");
        for (const char character : name) {
            byte(std::bit_cast<std::uint8_t>(character));
        }
    }

    void u16(std::uint16_t value) {
        byte(static_cast<std::uint8_t>(value));
        byte(static_cast<std::uint8_t>(value >> 8U));
    }

    void u32(std::uint32_t value) {
        u16(static_cast<std::uint16_t>(value));
        u16(static_cast<std::uint16_t>(value >> 16U));
    }

    std::array<std::uint8_t, header_bytes> bytes_{};
    std::size_t size_ = 0;
};

} // namespace

void WavWriter::Closer::operator()(std::FILE* file) const {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): file_ owns it, through this deleter.
    std::fclose(file);
}

WavWriter::WavWriter(std::FILE* file, WavFormat format) : file_{file}, format_{format} {}

std::expected<WavWriter, WavError> WavWriter::create(const char* path, WavFormat format) {
    IRAMA_ASSERT(path != nullptr, "a WAV file has a path");
    IRAMA_ASSERT(format.sample_rate > 0, "a WAV file plays some frames a second");
    IRAMA_ASSERT(format.channels > 0, "a WAV file has a channel");
    WavWriter writer{std::fopen(path, "wb"), format};
    if (writer.file_ == nullptr || !Header{format, 0}.write_to(writer.file_.get())) {
        return std::unexpected{WavError::open_failed};
    }
    return writer;
}

std::expected<void, WavError> WavWriter::write(std::span<const float> samples) {
    IRAMA_ASSERT(file_ != nullptr, "a finished writer writes no more");
    IRAMA_ASSERT(samples.size() % format_.channels == 0, "the samples make whole frames");
    if (samples.size() > (max_data_bytes - data_bytes_) / bytes_per_sample) {
        return std::unexpected{WavError::too_long};
    }
    if (std::fwrite(samples.data(), bytes_per_sample, samples.size(), file_.get()) !=
        samples.size()) {
        return std::unexpected{WavError::write_failed};
    }
    data_bytes_ += narrow<std::uint32_t>(samples.size() * bytes_per_sample);
    return {};
}

std::expected<void, WavError> WavWriter::finish() {
    IRAMA_ASSERT(file_ != nullptr, "a writer finishes once");
    // Closing flushes what is still buffered, so it can fail too.
    std::FILE* file = file_.release();
    const bool written =
        std::fseek(file, 0, SEEK_SET) == 0 && Header{format_, data_bytes_}.write_to(file);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): released from file_ just above.
    const bool closed = std::fclose(file) == 0;
    if (!written || !closed) {
        return std::unexpected{WavError::write_failed};
    }
    return {};
}

} // namespace irama
