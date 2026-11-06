#include <pgp/content/collection.hpp>
#include "civic_codec.hpp"

#include <algorithm>

namespace pgp::content {
core::SocietyCatalog load_society_catalog(std::string_view bytes) {
    if(bytes.size()>65536) civic_codec::invalid();
    auto result=civic_codec::decode<core::SocietyCatalog>(civic_codec::Json::parse(bytes));
    std::sort(result.regions.begin(),result.regions.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_society_catalog(result);
    return result;
}
std::string canonical_society_catalog(const core::SocietyCatalog& catalog) {
    core::validate_society_catalog(catalog);
    return civic_codec::encode(catalog).dump();
}
const core::SocietyCatalog* society_catalog(const Collection& collection,const ScenarioDefinition& scenario) {
    if(scenario.society.empty()) return nullptr;
    const auto* definition=collection.find(scenario.society);
    const auto* catalog=definition?std::get_if<core::SocietyCatalog>(&definition->data):nullptr;
    if(!catalog) throw std::invalid_argument("Scenario refers to an unavailable regional society catalog.");
    return catalog;
}
}
