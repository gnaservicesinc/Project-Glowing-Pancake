#pragma once
#include "json_support.hpp"
#include <pgp/core/social.hpp>
namespace pgp::persistence::detail {
Json social_json(const core::SocialState&);
core::SocialState parse_social(const Json&);
}
