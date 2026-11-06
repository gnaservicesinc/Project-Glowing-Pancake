#include <pgp/content/collection.hpp>
#include "civic_codec.hpp"
#include <set>

namespace pgp::content {
core::FrontierCatalog load_frontier_catalog(std::string_view bytes) {
    if (bytes.size()>65536) throw std::invalid_argument("Frontier catalog exceeds its byte limit.");
    using Json = civic_codec::Json;
    std::vector<std::set<std::string>> keys;
    std::size_t tokens = 0;
    const auto doc = Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json& item) {
        if (depth>16 || ++tokens>16384) throw std::invalid_argument("Frontier catalog exceeds its shape limit.");
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        if (event==Json::parse_event_t::object_end) keys.pop_back();
        if (event==Json::parse_event_t::key && !keys.back().insert(item.get<std::string>()).second)
            throw std::invalid_argument("Duplicate frontier catalog key.");
        return true;
    });
    auto result = civic_codec::decode<core::FrontierCatalog>(doc);
    core::validate_frontier_catalog(result);
    return result;
}
std::string canonical_frontier_catalog(const core::FrontierCatalog& catalog) {
    core::validate_frontier_catalog(catalog);
    return civic_codec::encode(catalog).dump();
}
const core::FrontierCatalog* frontier_catalog(const Collection& collection, const ScenarioDefinition& scenario) {
    if (scenario.frontier.empty()) return nullptr;
    const auto* definition = collection.find(scenario.frontier);
    const auto* catalog = definition ? std::get_if<core::FrontierCatalog>(&definition->data) : nullptr;
    if (!catalog) throw std::invalid_argument("The scenario refers to an unavailable frontier catalog.");
    return catalog;
}
}
