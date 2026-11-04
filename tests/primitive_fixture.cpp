#include <pgp/core/calendar.hpp>
#include <pgp/core/checked_arithmetic.hpp>
#include <pgp/core/integer_codec.hpp>
#include <pgp/core/random.hpp>
#include <pgp/core/units.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace pgp::core;
struct FixtureEntityTag {};
using FixtureId = EntityId<FixtureEntityTag>;
struct Entity { FixtureId id; std::uint64_t score; };

template<class Integer>
std::string decimal(Integer value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc{}) { throw std::runtime_error("fixture integer formatting failed"); }
    return {buffer.data(), result.ptr};
}

struct PrimitiveState {
    OrdinalDay day{0};
    Cents cash{120000000};
    MilliCU work{0};
    MilliCUPerDay capacity{100000};
    BasisPoints rate{0};
    PartsPerMillion probability{0};
    RandomStreams random{123456789};
    std::array<Entity, 3> entities{{{FixtureId{11}, 0}, {FixtureId{3}, 0}, {FixtureId{7}, 0}}};
};

// This is a primitive-only script, not provisional economic/gameplay balance.
void advance(PrimitiveState& state) {
    std::array<std::uint64_t, stream_count> draws{};
    for (std::size_t index = 0; index < stream_count; ++index) {
        draws[index] = state.random.stream(static_cast<RandomStream>(index)).bounded(10001);
    }
    const std::array<std::uint64_t, 3> income_factors{draws[0], draws[3], draws[5]};
    const auto income = mul_div_round_half_up(income_factors, std::uint64_t{100000000});
    const std::array<std::uint64_t, 1> charge_factors{100 + draws[4]};
    const auto charge = signed_mul_div_round_half_up(
        -static_cast<std::int64_t>(1000000 + draws[2]), charge_factors, std::uint64_t{10000});
    state.cash = checked_unit_add(state.cash, Cents{income});
    state.cash = checked_unit_add(state.cash, Cents{charge});
    state.capacity = MilliCUPerDay{100000 + draws[1]};
    state.work = checked_unit_add(state.work, integrate(state.capacity, Days{1}));
    state.rate = BasisPoints{draws[4]};
    state.probability = PartsPerMillion{draws[5] * 100};
    for (auto& entity : state.entities) { entity.score = (draws[6] + entity.id.value()) % 3; }
    // Every tie ends in a unique typed ID, independent of prior container order.
    std::sort(state.entities.begin(), state.entities.end(), [](const Entity& left, const Entity& right) {
        if (left.score != right.score) { return left.score > right.score; }
        return left.id < right.id;
    });
    state.day = advance_days(state.day, Days{1});
}

std::string canonical(const PrimitiveState& state) {
    std::string output = "{\"ordinal_day\":" + decimal(state.day.value())
        + ",\"date\":\"" + iso_date(state.day) + "\",\"cash_cents\":" + decimal(state.cash.value())
        + ",\"work_milli_CU\":" + decimal(state.work.value())
        + ",\"capacity_milli_CU_per_day\":" + decimal(state.capacity.value())
        + ",\"rate_bp\":" + decimal(state.rate.value())
        + ",\"probability_ppm\":" + decimal(state.probability.value()) + ",\"rng_states\":[";
    const auto random = state.random.snapshot();
    for (std::size_t index = 0; index < random.size(); ++index) {
        if (index != 0) { output += ','; }
        output += decimal(random[index]);
    }
    output += "],\"entities\":[";
    for (std::size_t index = 0; index < state.entities.size(); ++index) {
        if (index != 0) { output += ','; }
        output += "{\"id\":" + decimal(state.entities[index].id.value())
            + ",\"score\":" + decimal(state.entities[index].score) + '}';
    }
    return output + "]}";
}

std::string hash(const std::string& canonical_state) {
    // FNV-1a64 is a deterministic test checksum, never a security/content hash.
    std::uint64_t result = 14695981039346656037ULL;
    for (const char byte : canonical_state) {
        result ^= static_cast<unsigned char>(byte);
        result *= 1099511628211ULL;
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string output(16, '0');
    for (std::size_t index = 0; index < 16; ++index) {
        output[15 - index] = hex[result & 15U];
        result >>= 4U;
    }
    return output;
}

void round_trip_integers_and_rng(PrimitiveState& state) {
    state.day = OrdinalDay{parse_json_i64(decimal(state.day.value()))};
    state.cash = Cents{parse_json_i64(decimal(state.cash.value()))};
    state.work = MilliCU{parse_json_i64(decimal(state.work.value()))};
    state.capacity = MilliCUPerDay{parse_json_i64(decimal(state.capacity.value()))};
    state.rate = BasisPoints{parse_json_i64(decimal(state.rate.value()))};
    state.probability = PartsPerMillion{parse_json_i64(decimal(state.probability.value()))};
    auto saved = state.random.snapshot();
    for (auto& stream : saved) { stream = parse_json_u64(decimal(stream)); }
    state.random = RandomStreams::restore(saved);
    for (auto& entity : state.entities) {
        entity.id = FixtureId{parse_json_u64(decimal(entity.id.value()))};
        entity.score = parse_json_u64(decimal(entity.score));
    }
}
} // namespace

int main() {
    try {
        PrimitiveState state;
        PrimitiveState resumed;
        std::vector<std::string> hashes;
        for (int day = 0; day < 1000; ++day) {
            const auto live = state.random.snapshot();
            auto preview = state.random;
            (void)preview.stream(RandomStream::research).next();
            if (live != state.random.snapshot()) { throw std::runtime_error("preview mutated live RNG"); }
            if (day == 500) { round_trip_integers_and_rng(resumed); }
            advance(state);
            advance(resumed);
            if (canonical(state) != canonical(resumed)) {
                throw std::runtime_error("primitive integer/RNG restoration changed continuation");
            }
            hashes.push_back(hash(canonical(state)));
        }
        std::string output = "{\"fixture\":\"primitive-1000-days-v1\",\"hash\":\"fnv1a64-canonical-json-v1\",\"seed\":123456789,\"daily_hashes\":[";
        for (std::size_t index = 0; index < hashes.size(); ++index) {
            if (index != 0) { output += ','; }
            output += '"' + hashes[index] + '"';
        }
        std::cout << output << "],\"final\":" << canonical(state) << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
