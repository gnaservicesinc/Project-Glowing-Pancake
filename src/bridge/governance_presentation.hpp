#pragma once
#include "campaign_model.hpp"
#include <pgp/runtime/runtime.hpp>
namespace pgp::bridge {
struct GovernanceModels {
    std::vector<CampaignRow> financing,executives,motions,disputes,targets,charters;
    QString ownership,board;
};
[[nodiscard]] GovernanceModels governance_models(const runtime::Checkpoint&,const runtime::CompiledProgram&);
[[nodiscard]] QString acquisition_terms(const runtime::Checkpoint&,const runtime::CompiledProgram&,std::string_view,bool);
}
