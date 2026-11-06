#include <pgp/content/collection.hpp>
#include "civic_codec.hpp"
#include <algorithm>
#include <set>

namespace pgp::content {
core::AuthorityCatalog load_authority_catalog(std::string_view bytes) {
    if(bytes.size()>65536) civic_codec::invalid();
    using Json=civic_codec::Json;
    std::vector<std::set<std::string>> keys;std::size_t tokens=0;
    const auto value=Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json& item) {
        if(depth>16 || ++tokens>16384) civic_codec::invalid();
        if(event==Json::parse_event_t::object_start) keys.emplace_back();
        if(event==Json::parse_event_t::object_end) keys.pop_back();
        if(event==Json::parse_event_t::key && !keys.back().insert(item.get<std::string>()).second) civic_codec::invalid();
        return true;
    });
    auto result=civic_codec::decode<core::AuthorityCatalog>(value);
    std::sort(result.regions.begin(),result.regions.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    std::sort(result.offers.begin(),result.offers.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_authority_catalog(result);return result;
}
std::string canonical_authority_catalog(const core::AuthorityCatalog& c) {
    core::validate_authority_catalog(c);return civic_codec::encode(c).dump();
}
const core::AuthorityCatalog* authority_catalog(const Collection& c,const ScenarioDefinition& s) {
    if(s.authority.empty()) return nullptr;
    const auto* definition=c.find(s.authority);
    const auto* result=definition?std::get_if<core::AuthorityCatalog>(&definition->data):nullptr;
    if(!result) throw std::invalid_argument("The scenario refers to an unavailable institutional authority catalog.");return result;
}
}
