#pragma once
#include "campaign_model.hpp"
#include <pgp/core/simulation.hpp>
namespace pgp::bridge {
struct ResilienceModels {
    QVariantList sites,protections,insurance,services,sources;
    std::vector<CampaignRow> installed,policies,incidents,replicas;
};
[[nodiscard]] ResilienceModels resilience_models(const core::SimulationState&);
[[nodiscard]] QString resilience_review(const core::SimulationState&,const core::CommandAction&);
[[nodiscard]] QString site_exposure(const core::SimulationState&,std::string_view state);
}
