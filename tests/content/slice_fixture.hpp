#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>
namespace pgp_test {
// Definition array order is authoring presentation, not a runtime contract.
// Canonical reviewed snapshots sort IDs, so mutate named definitions explicitly.
template<class Rows> auto& named_row(Rows& rows,std::string_view id) {
    for (auto& row:rows) if (row.at("id")==id) return row;
    throw std::runtime_error("Missing named test definition: "+std::string{id});
}
template<class Document> auto& definition(Document& document,std::string_view id) {return named_row(document.at("definitions"),id);}
template<class Document> auto& scenario_doc(Document& document) {return definition(document,"scenario.first_playable");}

// A fixture that removes scenarios or their roots must also remove unused
// flows whose typed effects refer to the discarded scenario catalogs.
inline void prune_unused_flows(nlohmann::json& document) {
    auto& rows = document.at("definitions");
    std::set<std::string, std::less<>> live;
    const auto keep = [&](const auto& self, const std::string& id) -> void {
        if (!live.insert(id).second) return;
        for (const auto& node : named_row(rows,id).at("nodes"))
            if (node.at("kind") == "subflow") self(self,node.at("flow_id").template get<std::string>());
    };
    for (const auto& row : rows) if (row.at("kind") == "scenario")
        for (const auto& id : row.at("flows")) keep(keep,id.get<std::string>());
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row) {
        return row.at("kind") == "flow" && !live.contains(row.at("id").template get<std::string>());
    }),rows.end());
}
}
