#pragma once
#include <nlohmann/json.hpp>
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
}
