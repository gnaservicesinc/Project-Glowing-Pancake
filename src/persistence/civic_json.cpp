#include "civic_json.hpp"
#include "../content/civic_codec.hpp"

namespace pgp::persistence::detail {
Json civic_json(const core::CivicState& state) { return content::civic_codec::encode(state); }
core::CivicState parse_civic(const Json& value) { return content::civic_codec::decode<core::CivicState>(value); }
Json society_json(const core::SocietyState& state) { return content::civic_codec::encode(state); }
core::SocietyState parse_society(const Json& value) { return content::civic_codec::decode<core::SocietyState>(value); }
}
