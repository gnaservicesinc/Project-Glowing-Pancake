#include <pgp/content/resilience_catalog.hpp>
#include "resilience_codec.hpp"
#include <set>
namespace pgp::content {
core::ResilienceCatalog load_resilience_catalog(std::string_view bytes) {
    if (bytes.size()>131072) resilience_codec::invalid();
    std::vector<std::set<std::string>> keys;
    std::size_t tokens=0;
    const auto json=resilience_codec::Json::parse(bytes,[&](int depth,resilience_codec::Json::parse_event_t event,resilience_codec::Json& value) {
        if (depth>16 || ++tokens>65536) resilience_codec::invalid();
        if (event==resilience_codec::Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==resilience_codec::Json::parse_event_t::object_end) keys.pop_back();
        else if (event==resilience_codec::Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) resilience_codec::invalid();
        else if (event==resilience_codec::Json::parse_event_t::value && value.is_number_float()) resilience_codec::invalid();
        return true;
    });
    auto catalog=resilience_codec::decode<core::ResilienceCatalog>(json);
    core::validate_resilience_catalog(catalog);return catalog;
}
std::string canonical_resilience_catalog(const core::ResilienceCatalog& catalog) {
    core::validate_resilience_catalog(catalog);return resilience_codec::encode(catalog).dump();
}
}
