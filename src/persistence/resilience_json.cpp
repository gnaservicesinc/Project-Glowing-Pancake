#include "resilience_json.hpp"
#include "../content/resilience_codec.hpp"
namespace pgp::persistence::detail {
Json resilience_json(const core::ResilienceState& state) {return content::resilience_codec::encode(state);}
core::ResilienceState parse_resilience(const Json& json) {return content::resilience_codec::decode<core::ResilienceState>(json);}
}
