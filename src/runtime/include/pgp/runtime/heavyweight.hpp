#pragma once
#include <pgp/core/simulation.hpp>
#include <array>
namespace pgp::runtime {
struct WorldState;struct CompiledProgram;struct RivalCompany;
enum class HeavyweightPhase { watch,mobilize,compete,consolidate };
[[nodiscard]] std::string_view to_string(HeavyweightPhase phase) noexcept;
struct PressureReview {
    core::OrdinalDay day{0};
    std::array<core::BasisPoints,4> components{core::BasisPoints{0},core::BasisPoints{0},core::BasisPoints{0},core::BasisPoints{0}};
    core::BasisPoints score{0};
    core::Cents substituted_revenue{0};
    std::uint64_t delivered_tasks=0,customer_demand=0;
    std::uint32_t public_services=0,disclosed_rounds=0;
    auto operator<=>(const PressureReview&) const = default;
};
struct HeavyweightTransition {
    core::OrdinalDay day{0};HeavyweightPhase phase=HeavyweightPhase::watch;
    std::string cause;std::optional<core::OrdinalDay> due;
    auto operator<=>(const HeavyweightTransition&) const = default;
};
struct HeavyweightState {
    HeavyweightPhase phase=HeavyweightPhase::watch;
    core::OrdinalDay next_review{0};
    std::uint32_t high_reviews=0,low_reviews=0;
    std::optional<core::OrdinalDay> warned_on,mobilized_on,ready_on,retired_on;
    std::vector<PressureReview> reviews;
    std::vector<HeavyweightTransition> transitions;
    bool offers_issued=false;
    auto operator<=>(const HeavyweightState&) const = default;
};
enum class CompetitiveOfferKind { publication,pilot };
enum class CompetitiveOfferStatus { pending,active,paid,declined,expired,cancelled,failed };
[[nodiscard]] std::string_view to_string(CompetitiveOfferKind kind) noexcept;
[[nodiscard]] std::string_view to_string(CompetitiveOfferStatus status) noexcept;
struct CompetitiveOffer {
    std::string id,company_id,subject;
    CompetitiveOfferKind kind=CompetitiveOfferKind::publication;
    CompetitiveOfferStatus status=CompetitiveOfferStatus::pending;
    core::OrdinalDay issued_on{0},respond_by{0};
    std::optional<core::OrdinalDay> accepted_on,due_on,resolved_on,last_delivery_day;
    core::Cents amount{0};
    std::uint64_t reservation_id=0,required_tasks=0,delivered_tasks=0;
    auto operator<=>(const CompetitiveOffer&) const = default;
};
[[nodiscard]] PressureReview observe_heavyweight_pressure(const WorldState&,const CompiledProgram&,std::string_view,core::OrdinalDay);
void update_heavyweight_responses(WorldState&,const CompiledProgram&);
void update_competitive_offers(WorldState&,const CompiledProgram&);
void respond_to_competitive_offer(WorldState&,const CompiledProgram&,std::string_view,bool);
void validate_heavyweight_responses(const WorldState&,const CompiledProgram&,bool);
}
