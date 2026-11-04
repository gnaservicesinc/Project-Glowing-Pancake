#include <pgp/core/random.hpp>

#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::uint64_t gamma = 0x9e3779b97f4a7c15ULL;
constexpr std::uint64_t mix(std::uint64_t value) noexcept {
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

RandomSnapshot initial_states(std::uint64_t seed) noexcept {
    RandomSnapshot states{};
    for (std::size_t index = 0; index < states.size(); ++index) {
        states[index] = mix(seed + gamma * (static_cast<std::uint64_t>(index) + 1U));
    }
    return states;
}
} // namespace

std::uint64_t SplitMix64::next() noexcept {
    state_ += gamma;
    return mix(state_);
}

std::uint64_t SplitMix64::bounded(std::uint64_t exclusive_upper) {
    if (exclusive_upper == 0) {
        throw std::invalid_argument("Random bound must be positive");
    }
    // Unsigned negation is defined modulo 2^64, giving 2^64 % bound without
    // representing 2^64. Rejecting the short prefix avoids modulo bias.
    const auto threshold = (std::uint64_t{0} - exclusive_upper) % exclusive_upper;
    for (;;) {
        const auto draw = next();
        if (draw >= threshold) {
            return draw % exclusive_upper;
        }
    }
}

RandomStreams::RandomStreams(std::uint64_t root_seed) noexcept
    : RandomStreams{initial_states(root_seed)} {}

RandomStreams::RandomStreams(const RandomSnapshot& saved) noexcept
    : streams_{SplitMix64{saved[0]}, SplitMix64{saved[1]}, SplitMix64{saved[2]},
               SplitMix64{saved[3]}, SplitMix64{saved[4]}, SplitMix64{saved[5]},
               SplitMix64{saved[6]}} {}

SplitMix64& RandomStreams::stream(RandomStream domain) {
    return streams_.at(static_cast<std::size_t>(domain));
}

const SplitMix64& RandomStreams::stream(RandomStream domain) const {
    return streams_.at(static_cast<std::size_t>(domain));
}

RandomSnapshot RandomStreams::snapshot() const noexcept {
    RandomSnapshot result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = streams_[index].state();
    }
    return result;
}

RandomStreams RandomStreams::restore(const RandomSnapshot& saved) noexcept {
    return RandomStreams{saved};
}

} // namespace pgp::core
