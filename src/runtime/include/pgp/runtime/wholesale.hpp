#pragma once
#include <pgp/content/market_setup.hpp>
#include <pgp/core/capacity.hpp>
#include <span>

namespace pgp::runtime {
struct CompiledProgram;
struct WorldState;

struct WholesaleDay {
    core::MilliCUPerDay available{0};
    core::MilliCU used{0};
    core::Cents service_credit{0}, electricity{0};
    std::uint64_t seller_posting=0, buyer_posting=0;
    auto operator<=>(const WholesaleDay&) const = default;
};

struct WholesaleContract {
    std::string id, plan_id, source_id, buyer_id;
    core::OrdinalDay starts_on{0}, ends_before{0};
    std::optional<core::OrdinalDay> terminated_on;
    std::uint64_t reservation_id=0;
    core::Cents daily_ceiling{0};
    std::vector<WholesaleDay> days;
    auto operator<=>(const WholesaleContract&) const = default;
};

inline constexpr std::size_t wholesale_contract_limit=1024;
inline constexpr std::size_t wholesale_day_limit=366;
struct WholesaleQuote {
    core::OrdinalDay ends_before{0};
    core::Cents funded_ceiling{0};
};

[[nodiscard]] std::span<const content::WholesalePlan> wholesale_plans(const WorldState&,const CompiledProgram&);
[[nodiscard]] WholesaleQuote wholesale_quote(const WorldState&,const CompiledProgram&,std::string_view plan,
    std::string_view source,std::string_view buyer);
void sign_wholesale(WorldState&,const CompiledProgram&,std::string_view plan,std::string_view source,std::string_view buyer);
}
