#pragma once
#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime {
struct LegacyEquity {
    std::string company,holder;
    std::uint64_t numerator=0,denominator=10000;
    auto operator<=>(const LegacyEquity&) const = default;
};
struct FounderLegacy {
    std::string institution,holder;
    core::Cents cash{0},liabilities{0},book_equity{0},net_book_wealth{0};
    std::vector<LegacyEquity> positions;
    core::Cents assets{0};
    auto operator<=>(const FounderLegacy&) const = default;
};
// Exact retained beneficial claims, including the founder's share of a pooled
// acquisition consideration. Terminal objective valuation owns its own prices.
[[nodiscard]] FounderLegacy founder_legacy(const WorldState&,std::string_view institution);
}
