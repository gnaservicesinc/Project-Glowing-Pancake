#pragma once
#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime {
[[nodiscard]] const core::EvaluationEvidence* society_evidence(const core::SimulationState&,
    std::string_view id, std::size_t axis, core::OrdinalDay);
[[nodiscard]] std::string society_launch_block(const core::SocietyState*, const core::SimulationState&,
    const core::DeployModel&, core::OrdinalDay);
[[nodiscard]] core::MarketInfluences society_market_conditions(const WorldState&, const core::SimulationState&,
    core::OrdinalDay);
[[nodiscard]] core::MediaCampaign preview_media_campaign(const WorldState&,const core::BuyMediaCampaign&);
void buy_media_campaign(WorldState&, const core::BuyMediaCampaign&);
void endorse_forum_standard(WorldState&, const core::EndorseForumStandard&);
void advance_society(WorldState&, core::OrdinalDay settled_on);
void validate_society_world(const WorldState&);
} // namespace pgp::runtime
