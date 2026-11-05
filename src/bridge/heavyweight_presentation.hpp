#pragma once
#include "campaign_model.hpp"
#include <pgp/runtime/runtime.hpp>
namespace pgp::bridge {
struct HeavyweightModels {std::vector<CampaignRow> responses,offers;};
[[nodiscard]] HeavyweightModels heavyweight_models(const runtime::Checkpoint&,const runtime::CompiledProgram&);
[[nodiscard]] QString heavyweight_notice_detail(const runtime::WorldState&,const runtime::CompiledProgram&,const runtime::RivalNotice&);
}
