#include "civic_json.hpp"
#include "history_blocks.hpp"
#include "../content/civic_codec.hpp"

namespace pgp::persistence::detail {
Json civic_json(const core::CivicState& state) { return content::civic_codec::encode(state); }
core::CivicState parse_civic(const Json& value) { return content::civic_codec::decode<core::CivicState>(value); }
Json society_json(const core::SocietyState& state) {
    if (state.months.size() <= 12) return content::civic_codec::encode(state);
    const core::SocietyState current{state.catalog, state.starts_on, state.prepared_on, {}, state.campaigns, state.rounds};
    auto result = content::civic_codec::encode(current);
    HistoryRows months{true};
    for (const auto& month : state.months) months.push(content::civic_codec::encode(month));
    result["months"] = months.finish();
    return result;
}
core::SocietyState parse_society(const Json& value) {
    const auto& months = value.at("months");
    if (months.is_array()) return content::civic_codec::decode<core::SocietyState>(value);
    auto current = value;
    current["months"] = Json::array();
    auto result = content::civic_codec::decode<core::SocietyState>(current);
    read_history_rows(months, "society/months", core::society_month_limit, true, [&](const Json& month) {
        result.months.push_back(content::civic_codec::decode<core::SocietyMonth>(month));
    });
    return result;
}
Json authority_json(const core::AuthorityState& value) {return content::civic_codec::encode(value);}
core::AuthorityState parse_authority(const Json& value) {return content::civic_codec::decode<core::AuthorityState>(value);}
Json frontier_json(const core::FrontierState& value) {return content::civic_codec::encode(value);}
core::FrontierState parse_frontier(const Json& value) {return content::civic_codec::decode<core::FrontierState>(value);}
Json succession_json(const core::SuccessionState& value) {return content::civic_codec::encode(value);}
core::SuccessionState parse_succession(const Json& value) {return content::civic_codec::decode<core::SuccessionState>(value);}
}
