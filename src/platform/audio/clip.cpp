#include "clip.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace pgp::audio::detail {
namespace {
constexpr std::size_t max_encoded_bytes = 4U * 1024U * 1024U;
constexpr std::uint32_t max_seconds = 30;
constexpr std::uint32_t min_rate = 8000;
constexpr std::uint32_t max_rate = 96000;

bool tag(std::span<const unsigned char> bytes, std::size_t at, const char* text) {
    return at <= bytes.size() && bytes.size() - at >= 4 &&
           std::memcmp(bytes.data() + at, text, 4) == 0;
}

std::uint16_t u16(std::span<const unsigned char> bytes, std::size_t at) {
    return static_cast<std::uint16_t>(bytes[at]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[at + 1]) << 8U);
}

std::uint32_t u32(std::span<const unsigned char> bytes, std::size_t at) {
    return static_cast<std::uint32_t>(u16(bytes, at)) |
           (static_cast<std::uint32_t>(u16(bytes, at + 2)) << 16U);
}

DecodeResult fail(const char* message) { return {{}, message}; }

DecodeResult wav(std::span<const unsigned char> bytes) {
    if (bytes.size() < 12 || !tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE") ||
        u32(bytes, 4) != bytes.size() - 8) {
        return fail("Invalid RIFF/WAVE envelope.");
    }
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::span<const unsigned char> pcm;
    bool has_format = false;
    bool has_data = false;
    for (std::size_t pos = 12; pos < bytes.size();) {
        if (bytes.size() - pos < 8) return fail("Truncated WAV chunk.");
        const std::size_t length = u32(bytes, pos + 4);
        const std::size_t body = pos + 8;
        if (length > bytes.size() - body) return fail("WAV chunk exceeds file.");
        if (tag(bytes, pos, "fmt ")) {
            if (has_format || length != 16 || u16(bytes, body) != 1 ||
                u16(bytes, body + 14) != 16) {
                return fail("Only one PCM16 WAV format chunk is accepted.");
            }
            channels = u16(bytes, body + 2);
            sample_rate = u32(bytes, body + 4);
            if (channels < 1 || channels > 2 || sample_rate < min_rate || sample_rate > max_rate ||
                u16(bytes, body + 12) != channels * 2U ||
                u32(bytes, body + 8) != sample_rate * channels * 2U) {
                return fail("Unsupported WAV channel/rate/alignment metadata.");
            }
            has_format = true;
        } else if (tag(bytes, pos, "data")) {
            if (has_data) return fail("Duplicate WAV data chunk.");
            pcm = bytes.subspan(body, length);
            has_data = true;
        }
        const std::size_t padding = length % 2;
        if (padding > bytes.size() - body - length) return fail("Missing WAV padding.");
        pos = body + length + padding;
    }
    if (!has_format || !has_data || pcm.empty() || pcm.size() % (channels * 2U) != 0) {
        return fail("WAV lacks aligned PCM frames.");
    }
    const std::size_t frames = pcm.size() / (channels * 2U);
    if (frames > static_cast<std::size_t>(sample_rate) * max_seconds) {
        return fail("Audio exceeds the 30 second probe limit.");
    }
    Clip clip{{}, sample_rate, channels};
    clip.samples.reserve(pcm.size() / 2);
    for (std::size_t i = 0; i < pcm.size(); i += 2) {
        const std::uint16_t raw = u16(pcm, i);
        const std::int32_t signed_value = raw < 32768U ? raw : static_cast<std::int32_t>(raw) - 65536;
        clip.samples.push_back(static_cast<float>(signed_value) / 32768.0F);
    }
    return {std::move(clip), {}};
}

DecodeResult vorbis(std::span<const unsigned char> bytes) {
    if (bytes.size() < 4 || !tag(bytes, 0, "OggS")) return fail("Invalid Ogg envelope.");
    // stb receives bounded memory, never a path or stdio stream. Its setup
    // allocator is capped separately from the decoded PCM sample allocation.
    std::vector<char> setup_memory(2U * 1024U * 1024U);
    stb_vorbis_alloc allocator{setup_memory.data(), static_cast<int>(setup_memory.size())};
    int error = 0;
    std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> decoder(
        stb_vorbis_open_memory(bytes.data(), static_cast<int>(bytes.size()), &error, &allocator),
        &stb_vorbis_close);
    if (!decoder) return fail("Invalid or unsupported Ogg Vorbis stream.");
    const auto info = stb_vorbis_get_info(decoder.get());
    if (info.channels < 1 || info.channels > 2 || info.sample_rate < min_rate || info.sample_rate > max_rate) {
        return fail("Unsupported Vorbis channel/rate metadata.");
    }
    const std::size_t channels = static_cast<std::size_t>(info.channels);
    const std::size_t limit = static_cast<std::size_t>(info.sample_rate) * max_seconds * channels;
    const auto declared_frames = stb_vorbis_stream_length_in_samples(decoder.get());
    if (declared_frames == 0 || declared_frames > static_cast<std::size_t>(info.sample_rate) * max_seconds) {
        return fail("Invalid or oversized Vorbis duration.");
    }
    Clip clip{{}, info.sample_rate, static_cast<std::uint16_t>(info.channels)};
    clip.samples.reserve(static_cast<std::size_t>(declared_frames) * channels);
    std::array<float, 4096> chunk{};
    for (;;) {
        const int frames = stb_vorbis_get_samples_float_interleaved(
            decoder.get(), info.channels, chunk.data(), static_cast<int>(chunk.size()));
        if (frames == 0) break;
        const std::size_t count = static_cast<std::size_t>(frames) * channels;
        if (count > limit - clip.samples.size()) return fail("Decoded Vorbis exceeds limit.");
        clip.samples.insert(clip.samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    }
    if (stb_vorbis_get_error(decoder.get()) != VORBIS__no_error || clip.samples.empty() ||
        clip.samples.size() / channels != declared_frames) {
        return fail("Truncated or inconsistent Vorbis PCM stream.");
    }
    return {std::move(clip), {}};
}
} // namespace

DecodeResult decode(std::span<const unsigned char> bytes, bool ogg) {
    if (bytes.empty() || bytes.size() > max_encoded_bytes) return fail("Audio file size is outside probe limits.");
    return ogg ? vorbis(bytes) : wav(bytes);
}

void mix(const Clip& clip, std::size_t& cursor, float gain, std::span<float> output) {
    std::fill(output.begin(), output.end(), 0.0F);
    const auto count = std::min(output.size(), clip.samples.size() - std::min(cursor, clip.samples.size()));
    for (std::size_t i = 0; i < count; ++i) output[i] = clip.samples[cursor + i] * gain;
    cursor += count;
}
} // namespace pgp::audio::detail
