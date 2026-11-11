#pragma once
#include <pgp/core/social.hpp>
#include <nlohmann/json.hpp>
namespace pgp::core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SocialRules,interval_days,weekly_gain,shared_gain,friction,interaction_gain,outgoing_threshold,private_threshold,remote_decay,mood_cap,thresholds,city_chance_bp,state_chance_bp,remote_chance_bp,remote_ceiling,city_gain,state_gain,remote_gain,remote_breakthrough_bp,referral_days)
}
namespace pgp::content::social_codec {
inline constexpr std::array geography_fields{"city_chance_bp","state_chance_bp","remote_chance_bp","remote_ceiling","city_gain","state_gain","remote_gain","remote_breakthrough_bp","referral_days"};
inline nlohmann::json encode(const core::SocialRules& r){
    nlohmann::json j=r;const nlohmann::json defaults=core::SocialRules{};bool implicit=true;
    for(const auto* key:geography_fields)if(j.at(key)!=defaults.at(key))implicit=false;
    if(implicit)for(const auto* key:geography_fields)j.erase(key);return j;
}
inline core::SocialRules decode(const nlohmann::json& source){
    auto j=source;const nlohmann::json defaults=core::SocialRules{};
    bool any=false;for(const auto* key:geography_fields)any|=j.contains(key);
    if(!any)for(const auto* key:geography_fields)j[key]=defaults.at(key);
    const auto r=j.get<core::SocialRules>();if(nlohmann::json(r)!=j)throw std::invalid_argument("Unsupported social rules.");core::validate_social_rules(r);return r;
}
}
