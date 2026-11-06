#include <pgp/content/succession_catalog.hpp>
#include "civic_codec.hpp"
#include <set>

namespace pgp::content {
core::SuccessionCatalog load_succession_catalog(std::string_view bytes) {
    if(bytes.size()>16384) throw std::invalid_argument("Leadership charter exceeds its content budget.");
    std::vector<std::set<std::string>> keys;
    const auto value=civic_codec::Json::parse(bytes.begin(),bytes.end(),[&](int depth,auto event,auto& item) {
        if(depth>12) throw std::invalid_argument("Leadership charter exceeds its nesting limit.");
        using Json=civic_codec::Json;
        if(event==Json::parse_event_t::object_start) keys.emplace_back();
        else if(event==Json::parse_event_t::object_end) keys.pop_back();
        else if(event==Json::parse_event_t::key && !keys.back().insert(item.template get<std::string>()).second)
            throw std::invalid_argument("Leadership charter repeats a field.");
        return true;
    });
    auto result=civic_codec::decode<core::SuccessionCatalog>(value);
    core::validate_succession_catalog(result);return result;
}
std::string canonical_succession_catalog(const core::SuccessionCatalog& value) {
    core::validate_succession_catalog(value);return civic_codec::encode(value).dump();
}
}
