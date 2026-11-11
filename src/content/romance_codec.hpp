#pragma once
#include <pgp/core/romance.hpp>
#include <nlohmann/json.hpp>
#include <climits>
namespace pgp::core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RomanceRules,interval_days,friendship,demisexual_friendship,dating_days,engagement_days,engagement_affection,marriage_affection,initial_affection,date_gain,date_cooldown,proposal_cooldown,minimum_dates,ask_acceptance_bp,proposal_acceptance_bp,npc_start_bp,npc_progress_bp,city_gain,state_gain,remote_decay,neglect_days,strain_affection,strain_grace_days,interested_chance_bp,public_orientation_chance_bp)
}
namespace pgp::content::romance_codec {
inline nlohmann::json encode(const core::RomanceRules& r){return r;}
inline core::RomanceRules decode(const nlohmann::json& j){if(!j.is_object())throw std::invalid_argument("Romance rules require an object.");for(const auto& [key,value]:j.items()){(void)key;if(!value.is_number_integer() || value.get<std::int64_t>()<0 || value.get<std::uint64_t>()>static_cast<std::uint64_t>(INT_MAX))throw std::invalid_argument("Romance rules require bounded integer tokens.");}const auto r=j.get<core::RomanceRules>();if(nlohmann::json(r)!=j)throw std::invalid_argument("Unsupported romance rules.");core::validate_romance_rules(r);return r;}
}
