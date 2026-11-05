#pragma once
#include <pgp/core/simulation.hpp>
namespace pgp::runtime {
struct WorldState;struct CompiledProgram;
struct SellerSettlement {
    std::string holder;
    core::Cents cash{0};
    core::BasisPoints shares{0};
    auto operator<=>(const SellerSettlement&) const = default;
};
struct AcquisitionRecord {
    struct Transfer {
        core::OrdinalDay day{0};core::Cents amount{0};bool funding=false;
        auto operator<=>(const Transfer&) const = default;
    };
    std::string motion_id,target;
    core::OrdinalDay quoted_on{0};
    std::optional<core::OrdinalDay> completed_on;
    core::Cents value{0},cash{0};
    core::BasisPoints shares{0};
    std::uint64_t buyer_posting_cursor=0,target_posting_cursor=0;
    std::uint64_t acquired_posting_cursor=0;
    core::Cents acquired_profit{0};
    std::vector<core::EquityHolding> sellers;
    std::vector<SellerSettlement> settlements;
    std::vector<Transfer> transfers;
    auto operator<=>(const AcquisitionRecord&) const = default;
};
[[nodiscard]] AcquisitionRecord quote_acquisition(const WorldState&,const CompiledProgram&,std::string_view,bool equity);
void propose_acquisition(WorldState&,const CompiledProgram&,std::string_view,bool equity);
void complete_approved_acquisitions(WorldState&,const CompiledProgram&);
void transfer_subsidiary_funds(WorldState&,std::string_view,core::Cents,bool funding);
struct OwnedGroupReport {core::Cents cash{0},net_assets{0},operating_profit{0};};
[[nodiscard]] OwnedGroupReport owned_group_report(const WorldState&);
void validate_acquisitions(const WorldState&,const CompiledProgram&,bool deep);
}
