#pragma once
#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <algorithm>

namespace pgp::content::participants_codec {
using Json=nlohmann::json;
inline constexpr std::array<std::string_view,4> kinds{"founder","employee","contact","organization"};
[[noreturn]] inline void invalid() {throw std::invalid_argument("Use up to eight distinct participants with valid source, identity, message references and portrait part IDs.");}
inline std::vector<ParticipantRef> decode(const Json& value) {
    if(!value.is_array() || value.empty() || value.size()>8) invalid();
    std::vector<ParticipantRef> result;std::set<std::pair<ParticipantKind,std::string>> seen;
    for(const auto& row:value) {
        if(!row.is_object() || !row.contains("kind") || !row.contains("id") || !row["kind"].is_string() || !row["id"].is_string()) invalid();
        ParticipantRef p;const auto source=row["kind"].get<std::string>();
        const auto kind=std::ranges::find(kinds,source);if(kind==kinds.end()) invalid();
        p.kind=static_cast<ParticipantKind>(kind-kinds.begin());p.id=row["id"].get<std::string>();
        if(!valid_id(p.id) || !seen.emplace(p.kind,p.id).second) invalid();
        std::size_t fields=2;
        const auto message=[&](const char* key,std::string& destination) {
            if(!row.contains(key)) return;
            if(!row[key].is_string()) invalid();destination=row[key].get<std::string>();
            if(!valid_id(destination)) invalid();++fields;
        };
        message("name_key",p.name_key);message("role_key",p.role_key);
        if(row.contains("portrait")) {
            const auto& portrait=row["portrait"];
            if(!portrait.is_object() || portrait.size()!=2 || !portrait.contains("kit") || !portrait.contains("parts") ||
               !portrait["kit"].is_number_integer() || portrait["kit"]!=1 || !portrait["parts"].is_array() || portrait["parts"].size()!=core::portrait_part_counts.size()) invalid();
            core::PortraitRecipe recipe;
            for(std::size_t i=0;i<recipe.parts.size();++i) {
                const auto& part=portrait["parts"][i];if(!part.is_number_integer() || part<0 || part>=core::portrait_part_counts[i]) invalid();
                recipe.parts[i]=part.get<std::uint8_t>();
            }
            core::validate_portrait(recipe);p.portrait=recipe;++fields;
        }
        if(row.size()!=fields || (p.kind==ParticipantKind::founder && p.id!="founder")) invalid();
        const bool authored=p.kind==ParticipantKind::contact || p.kind==ParticipantKind::organization;
        if(authored?(p.name_key.empty() || p.role_key.empty()):( !p.name_key.empty() || p.portrait.has_value())) invalid();
        if((p.kind==ParticipantKind::contact)!=p.portrait.has_value()) invalid();
        result.push_back(std::move(p));
    }
    return result;
}
inline Json encode(const std::vector<ParticipantRef>& rows) {
    Json result=Json::array();
    for(const auto& p:rows) {
        const auto kind=static_cast<std::size_t>(p.kind);if(kind>=kinds.size()) invalid();
        Json row{{"kind",kinds[kind]},{"id",p.id}};
        if(!p.name_key.empty()) row["name_key"]=p.name_key;
        if(!p.role_key.empty()) row["role_key"]=p.role_key;
        if(p.portrait) row["portrait"]={{"kit",p.portrait->kit},{"parts",p.portrait->parts}};
        result.push_back(std::move(row));
    }
    (void)decode(result);return result;
}
}
