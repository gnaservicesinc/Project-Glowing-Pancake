#pragma once
#include <pgp/core/social.hpp>
#include <nlohmann/json.hpp>
namespace pgp::core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SocialRules,interval_days,weekly_gain,shared_gain,friction,interaction_gain,outgoing_threshold,private_threshold,remote_decay,mood_cap,thresholds)
}
namespace pgp::content::social_codec {
inline nlohmann::json encode(const core::SocialRules& r){return r;}
inline core::SocialRules decode(const nlohmann::json& j){const auto r=j.get<core::SocialRules>();if(nlohmann::json(r)!=j)throw std::invalid_argument("Unsupported social rules.");core::validate_social_rules(r);return r;}
}
