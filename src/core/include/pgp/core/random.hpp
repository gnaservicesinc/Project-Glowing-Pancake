#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pgp::core {

// Algorithm/version contract: SplitMix64, increment 0x9e3779b97f4a7c15;
// mix shifts 30,27,31 and multipliers 0xbf58476d1ce4e5b9,
// 0x94d049bb133111eb. Arithmetic deliberately wraps in uint64_t.
// A saved state is the accumulator *before* the next increment and draw.
// This is a simulation PRNG, not a cryptographic generator.
class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t state) noexcept : state_{state} {}
    [[nodiscard]] constexpr std::uint64_t state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t next() noexcept;
    // Uniform [0, exclusive_upper), using rejection; zero is invalid.
    [[nodiscard]] std::uint64_t bounded(std::uint64_t exclusive_upper);

private:
    std::uint64_t state_;
};

// Ordinals and names are persisted ABI: never reorder or reuse an ordinal.
enum class RandomStream : std::size_t {
    research, rivals, markets, geography, politics, hazards, narrative, risk, count
};
inline constexpr std::size_t stream_count = static_cast<std::size_t>(RandomStream::count);
inline constexpr std::array<std::string_view, stream_count> stream_names{
    "research", "rivals", "markets", "geography", "politics", "hazards", "narrative", "risk"
};
using RandomSnapshot = std::array<std::uint64_t, stream_count>;

class RandomStreams {
public:
    // Derive initial stream accumulators as mix(seed + gamma*(ordinal+1)).
    // The fixed bijective mix distinguishes domains before their first draw.
    explicit RandomStreams(std::uint64_t root_seed) noexcept;
    [[nodiscard]] SplitMix64& stream(RandomStream domain);
    [[nodiscard]] const SplitMix64& stream(RandomStream domain) const;
    [[nodiscard]] RandomSnapshot snapshot() const noexcept;
    [[nodiscard]] static RandomStreams restore(const RandomSnapshot& saved) noexcept;

private:
    explicit RandomStreams(const RandomSnapshot& saved) noexcept;
    std::array<SplitMix64, stream_count> streams_;
};

} // namespace pgp::core
