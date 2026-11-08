#pragma once
#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime {
struct PublicIdentityEvent {
    std::size_t effect=0;
    core::OrdinalDay day{0};
    std::string subject,cause;
    std::optional<std::array<std::int32_t,core::society_audiences>> direct_effect;
};
[[nodiscard]] bool submit_authority_action(WorldState&,const core::CommandAction&);
// Returns whether owning company resources changed after the core close.
[[nodiscard]] bool prepare_authority(WorldState&,std::optional<core::OrdinalDay> settled={});
void validate_authority_world(const WorldState&);
[[nodiscard]] core::MarketInfluences regional_market_conditions(const WorldState&,const core::SimulationState&,core::OrdinalDay);
[[nodiscard]] bool institutional_delivery_allowed(const WorldState&,const core::SimulationState&,std::string_view product,
    std::string_view segment,core::OrdinalDay,const core::DeploymentRecord* version=nullptr);
[[nodiscard]] std::vector<const core::DeploymentRecord*> public_service_deployments(const WorldState&,std::string_view region);
[[nodiscard]] core::AuthorityMonth authority_snapshot(const WorldState&,core::OrdinalDay);
[[nodiscard]] std::string_view mandate_status(const WorldState&,const core::AuthorityMandate&,core::OrdinalDay);
[[nodiscard]] core::AudienceScores public_identity(const WorldState&,core::OrdinalDay);
[[nodiscard]] std::vector<PublicIdentityEvent> public_identity_events(const WorldState&,core::OrdinalDay);
[[nodiscard]] std::optional<core::OrdinalDay> next_authority_boundary(const WorldState&);
[[nodiscard]] bool required_public_case(const WorldState&);
} // namespace pgp::runtime
