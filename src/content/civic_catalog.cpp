#include <pgp/content/collection.hpp>
#include "civic_codec.hpp"
#include <algorithm>
#include <set>

namespace pgp::content {
core::CivicCatalog load_civic_catalog(std::string_view bytes) {
    if (bytes.size() > 128U * 1024U) civic_codec::invalid();
    using Json = civic_codec::Json;
    std::vector<std::set<std::string>> keys;
    std::size_t tokens = 0;
    const auto value = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& item) {
        if (depth > 16 || ++tokens > 32768) civic_codec::invalid();
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(item.get<std::string>()).second) civic_codec::invalid();
        return true;
    });
    auto result = civic_codec::decode<core::CivicCatalog>(value);
    const auto by_id = [](const auto& left, const auto& right) { return left.id < right.id; };
    std::sort(result.constituencies.begin(), result.constituencies.end(), by_id);
    std::sort(result.policies.begin(), result.policies.end(), by_id);
    core::validate_civic_catalog(result);
    return result;
}

std::string canonical_civic_catalog(const core::CivicCatalog& catalog) {
    core::validate_civic_catalog(catalog);
    return civic_codec::encode(catalog).dump();
}

const core::CivicCatalog* civic_catalog(const Collection& collection, const ScenarioDefinition& scenario) {
    if (scenario.civic.empty()) return nullptr;
    const auto* definition = collection.find(scenario.civic);
    const auto* catalog = definition ? std::get_if<core::CivicCatalog>(&definition->data) : nullptr;
    if (!catalog) throw std::invalid_argument("Scenario refers to an unknown US institutions catalog.");
    return catalog;
}
} // namespace pgp::content
