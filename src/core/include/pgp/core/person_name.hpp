#pragma once
#include <pgp/core/random.hpp>
#include <string>
#include <vector>

namespace pgp::core {
// WTP Story's separate first-name and surname draws, using the campaign's name pools.
inline std::string draw_name_part(const std::vector<std::string>& pool, SplitMix64& random,
                                  std::string_view previous = {}) {
    if (pool.empty()) return {};
    const auto first = random.bounded(pool.size());
    for (std::size_t offset = 0; offset < pool.size(); ++offset) {
        const auto& name = pool[(first + offset) % pool.size()];
        if (name != previous) return name;
    }
    return pool[first];
}
}
