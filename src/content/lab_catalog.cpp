#include <pgp/content/lab_catalog.hpp>
#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <set>

namespace pgp::content {
namespace {
using Json = nlohmann::json;
[[noreturn]] void invalid(const char* detail) { throw std::invalid_argument(detail); }
void fields(const Json& value, std::initializer_list<std::string_view> required) {
    if (!value.is_object() || value.size() != required.size()) invalid("Lab definition has missing or unsupported fields");
    for (const auto name : required) if (!value.contains(name)) invalid("Lab definition has missing or unsupported fields");
}
std::string text(const Json& value) {
    if (!value.is_string()) invalid("Expected a lab label");
    auto result = value.get<std::string>();
    if (result.empty() || result.size() > 256 || result.find('\0') != std::string::npos) invalid("Invalid lab label length");
    return result;
}
std::uint64_t integer(const Json& value) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
        invalid("Lab values require nonnegative integer tokens");
    const auto result = value.get<std::uint64_t>();
    if (result > static_cast<std::uint64_t>(INT64_MAX)) invalid("Lab integer exceeds its signed unit bound");
    return result;
}
bool boolean(const Json& value) {
    if (!value.is_boolean()) invalid("Lab boolean does not permit numeric coercion");
    return value.get<bool>();
}
void array(const Json& value) {
    if (!value.is_array() || value.size() > core::lab_catalog_limit) invalid("Lab catalog array exceeds its count bound");
}
std::vector<std::string> ids(const Json& value) {
    array(value); std::vector<std::string> result;
    for (const auto& id : value) {
        auto name = text(id);
        if (!valid_id(name)) invalid("Lab reference must be a typed content ID");
        result.push_back(std::move(name));
    }
    return result;
}
core::LabProfile profile(const Json& value) {
    fields(value, {"documents", "scanned_tables"});
    return {core::BasisPoints{integer(value.at("documents"))}, core::BasisPoints{integer(value.at("scanned_tables"))}};
}
Json profile_json(const core::LabProfile& value) {
    return {{"documents", value[0].value()}, {"scanned_tables", value[1].value()}};
}
template<class Rows> auto ordered(const Rows& rows) {
    std::vector<const typename Rows::value_type*> result;
    for (const auto& row : rows) result.push_back(&row);
    std::sort(result.begin(), result.end(), [](auto a, auto b) { return a->id < b->id; });
    return result;
}
}
core::LabCatalog load_lab_catalog(std::string_view bytes) {
    if (bytes.size() > maximum_document_bytes) invalid("Lab catalog exceeds its byte bound");
    std::vector<std::set<std::string>> keys;
    std::size_t tokens = 0;
    const auto callback = [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 32 || ++tokens > 65536) invalid("Lab JSON exceeds depth or token bounds");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        else if (event == Json::parse_event_t::object_end) keys.pop_back();
        else if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
            invalid("Duplicate lab JSON key");
        else if (event == Json::parse_event_t::value && value.is_number_float()) invalid("Lab numbers must be integer tokens");
        return true;
    };
    const auto doc = Json::parse(bytes.begin(), bytes.end(), callback);
    fields(doc, {"data", "research", "recipes", "evaluations"});
    core::LabCatalog catalog;
    for (const auto field : {"data", "research", "recipes", "evaluations"}) array(doc.at(field));
    for (const auto& row : doc.at("data")) {
        auto shape = row;
        const bool open_release = shape.contains("open_weight_release") && boolean(shape.at("open_weight_release"));
        shape.erase("open_weight_release");
        fields(shape, {"id", "name", "provenance", "rights", "price_cents", "unique_units", "quality_bp", "relevance_bp", "coverage_bp", "contamination_bp", "commercial_use"});
        catalog.data.push_back({text(row.at("id")), text(row.at("name")), text(row.at("provenance")), text(row.at("rights")),
            core::Cents{integer(row.at("price_cents"))}, integer(row.at("unique_units")), core::BasisPoints{integer(row.at("quality_bp"))},
            profile(row.at("relevance_bp")), profile(row.at("coverage_bp")), core::BasisPoints{integer(row.at("contamination_bp"))},
            boolean(row.at("commercial_use")), open_release});
    }
    for (const auto& row : doc.at("research")) {
        fields(row, {"id", "all_of", "any_of", "cash_cents", "staff_days", "experiment_milli_CU"});
        catalog.research.push_back({text(row.at("id")), ids(row.at("all_of")), ids(row.at("any_of")),
            core::Cents{integer(row.at("cash_cents"))}, core::Days{integer(row.at("staff_days"))}, core::MilliCU{integer(row.at("experiment_milli_CU"))}});
    }
    for (const auto& row : doc.at("recipes")) {
        fields(row, {"id", "name", "requires_research", "baseline_bp", "gain_bp", "ceiling_bp", "fit_bp", "data_scale", "compute_scale_milli_CU",
            "minimum_work_milli_CU", "maximum_work_milli_CU", "preview_uncertainty_bp", "inference_milli_CU_per_task", "minimum_memory_mib", "interconnect"});
        fields(row.at("data_scale"), {"documents", "scanned_tables"});
        catalog.recipes.push_back({text(row.at("id")), text(row.at("name")), ids(row.at("requires_research")),
            profile(row.at("baseline_bp")), profile(row.at("gain_bp")), profile(row.at("ceiling_bp")), profile(row.at("fit_bp")),
            {integer(row.at("data_scale").at("documents")), integer(row.at("data_scale").at("scanned_tables"))},
            core::MilliCU{integer(row.at("compute_scale_milli_CU"))}, core::MilliCU{integer(row.at("minimum_work_milli_CU"))},
            core::MilliCU{integer(row.at("maximum_work_milli_CU"))}, core::BasisPoints{integer(row.at("preview_uncertainty_bp"))},
            core::MilliCU{integer(row.at("inference_milli_CU_per_task"))}, integer(row.at("minimum_memory_mib")), text(row.at("interconnect"))});
    }
    for (const auto& row : doc.at("evaluations")) {
        fields(row, {"id", "name", "requires_research", "coverage_bp", "required_bp", "uncertainty_bp", "independent", "work_milli_CU", "cash_cents"});
        catalog.evaluations.push_back({text(row.at("id")), text(row.at("name")), ids(row.at("requires_research")),
            profile(row.at("coverage_bp")), profile(row.at("required_bp")), core::BasisPoints{integer(row.at("uncertainty_bp"))},
            boolean(row.at("independent")), core::MilliCU{integer(row.at("work_milli_CU"))}, core::Cents{integer(row.at("cash_cents"))}});
    }
    return catalog;
}
std::string canonical_lab_catalog(const core::LabCatalog& catalog) {
    Json data = Json::array(), research = Json::array(), recipes = Json::array(), evaluations = Json::array();
    for (const auto* row : ordered(catalog.data)) {
        data.push_back({{"id",row->id},{"name",row->name},{"provenance",row->provenance},{"rights",row->rights},
        {"price_cents",row->price.value()},{"unique_units",row->unique_units},{"quality_bp",row->quality.value()},
        {"relevance_bp",profile_json(row->relevance)},{"coverage_bp",profile_json(row->coverage)},
            {"contamination_bp",row->contamination.value()},{"commercial_use",row->commercial_use}});
        if (row->open_weight_release) data.back()["open_weight_release"] = true;
    }
    for (const auto* row : ordered(catalog.research)) research.push_back({{"id",row->id},{"all_of",row->all_of},{"any_of",row->any_of},
        {"cash_cents",row->cash.value()},{"staff_days",row->staff_days.value()},{"experiment_milli_CU",row->experiment_work.value()}});
    for (const auto* row : ordered(catalog.recipes)) recipes.push_back({{"id",row->id},{"name",row->name},{"requires_research",row->requires_research},
        {"baseline_bp",profile_json(row->baseline)},{"gain_bp",profile_json(row->gain)},{"ceiling_bp",profile_json(row->ceiling)},{"fit_bp",profile_json(row->fit)},
        {"data_scale",{{"documents",row->data_scale[0]},{"scanned_tables",row->data_scale[1]}}},{"compute_scale_milli_CU",row->compute_scale.value()},
        {"minimum_work_milli_CU",row->minimum_work.value()},{"maximum_work_milli_CU",row->maximum_work.value()},
        {"preview_uncertainty_bp",row->preview_uncertainty.value()},{"inference_milli_CU_per_task",row->inference_per_task.value()},
        {"minimum_memory_mib",row->minimum_memory_mib},{"interconnect",row->interconnect}});
    for (const auto* row : ordered(catalog.evaluations)) evaluations.push_back({{"id",row->id},{"name",row->name},{"requires_research",row->requires_research},
        {"coverage_bp",profile_json(row->coverage)},{"required_bp",profile_json(row->required)},{"uncertainty_bp",row->uncertainty.value()},
        {"independent",row->independent},{"work_milli_CU",row->work.value()},{"cash_cents",row->cash.value()}});
    return Json{{"data",std::move(data)},{"research",std::move(research)},{"recipes",std::move(recipes)},{"evaluations",std::move(evaluations)}}.dump();
}
core::LabCatalog scenario_lab_catalog(const Collection& collection, const ScenarioDefinition& scenario) {
    if (!scenario.lab) throw std::invalid_argument("Scenario has no lab catalog");
    auto result = *scenario.lab;
    if (!result.research.empty()) throw std::invalid_argument("Scenario lab research comes from its referenced research definitions");
    for (const auto& id : scenario.research) {
        const auto* definition = collection.find(id);
        if (!definition || !std::holds_alternative<ResearchDefinition>(definition->data)) invalid("Scenario research reference is not a research node");
        const auto& node = std::get<ResearchDefinition>(definition->data);
        result.research.push_back({id, node.all_of, node.any_of, node.cash, node.staff_days, node.work});
    }
    // Canonical order is also the saved catalog comparison order.
    result = load_lab_catalog(canonical_lab_catalog(result));
    core::validate_lab_catalog(result);
    return result;
}
} // namespace pgp::content
