#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pgp::audio::detail {

struct Clip {
    std::vector<float> samples;
    std::uint32_t sample_rate{};
    std::uint16_t channels{};
};

struct DecodeResult {
    Clip clip;
    std::string error;
};

[[nodiscard]] DecodeResult decode(std::span<const unsigned char> bytes, bool ogg);

// Shared by the native output callback and deterministic tests. No allocation.
void mix(const Clip& clip, std::size_t& cursor, float gain,
         std::span<float> output);

} // namespace pgp::audio::detail
