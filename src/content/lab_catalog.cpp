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
template<class Profile, class Convert> Profile named_profile(const Json& value, Profile result, Convert convert) {
    if (!value.is_object()) invalid("Lab profiles use named task domains");
    for (std::size_t i=0;i<core::introductory_lab_domains;++i)
        if (!value.contains(core::lab_domain_names[i])) invalid("Lab profiles need the two introductory task domains");
    for (const auto& [key,entry]:value.items()) {
        const auto domain=std::ranges::find(core::lab_domain_names,key);
        if (domain==core::lab_domain_names.end()) invalid("Unsupported Lab task domain");
        result[static_cast<std::size_t>(domain-core::lab_domain_names.begin())]=convert(integer(entry));
    }
    return result;
}
core::LabProfile profile(const Json& value) {
    return named_profile(value,core::empty_lab_profile,[](auto bp) { return core::BasisPoints{bp}; });
}
auto data_scale(const Json& value) {
    return named_profile(value,std::array<std::uint64_t,core::lab_domain_count>{},[](auto units) { return units; });
}
Json profile_json(const core::LabProfile& value) {
    auto result=Json::object();
    for (std::size_t i=0;i<value.size();++i)
        if (i<core::introductory_lab_domains || value[i].value()>0) result[core::lab_domain_names[i]]=value[i].value();
    return result;
}
Json data_scale_json(const std::array<std::uint64_t,core::lab_domain_count>& value) {
    auto result=Json::object();
    for (std::size_t i=0;i<value.size();++i)
        if (i<core::introductory_lab_domains || value[i]>0) result[core::lab_domain_names[i]]=value[i];
    return result;
}
core::DataOffer parse_data(const Json& row) {
    auto shape=row;
    const bool open_release=shape.contains("open_weight_release") && boolean(shape.at("open_weight_release"));
    shape.erase("open_weight_release");
    fields(shape,{"id","name","provenance","rights","price_cents","unique_units","quality_bp","relevance_bp",
        "coverage_bp","contamination_bp","commercial_use"});
    return {text(row.at("id")),text(row.at("name")),text(row.at("provenance")),text(row.at("rights")),
        core::Cents{integer(row.at("price_cents"))},integer(row.at("unique_units")),core::BasisPoints{integer(row.at("quality_bp"))},
        profile(row.at("relevance_bp")),profile(row.at("coverage_bp")),core::BasisPoints{integer(row.at("contamination_bp"))},
        boolean(row.at("commercial_use")),open_release};
}
Json encode_data(const core::DataOffer& row) {
    Json result{{"id",row.id},{"name",row.name},{"provenance",row.provenance},{"rights",row.rights},
        {"price_cents",row.price.value()},{"unique_units",row.unique_units},{"quality_bp",row.quality.value()},
        {"relevance_bp",profile_json(row.relevance)},{"coverage_bp",profile_json(row.coverage)},
        {"contamination_bp",row.contamination.value()},{"commercial_use",row.commercial_use}};
    if(row.open_weight_release) result["open_weight_release"]=true;
    return result;
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
    auto document_shape=doc;
    document_shape.erase("synthesis");
    document_shape.erase("acquisition");
    document_shape.erase("research_workplaces");
    fields(document_shape, {"data", "research", "recipes", "evaluations"});
    core::LabCatalog catalog;
    if(doc.contains("research_workplaces")) {
        const auto& facilities=doc.at("research_workplaces");
        if(!facilities.is_object() || facilities.size()>core::lab_catalog_limit) invalid("Research workplaces exceed the catalog bound");
        catalog.research_workplaces.emplace();
        for(const auto& [id,equipment]:facilities.items())
            catalog.research_workplaces->emplace(text(Json(id)),ids(equipment));
    }
    for (const auto field : {"data", "research", "recipes", "evaluations"}) array(doc.at(field));
    for (const auto& row : doc.at("data")) catalog.data.push_back(parse_data(row));
    for (const auto& row : doc.at("research")) {
        auto shape = row;
        const auto repeats = row.contains("repeat_limit") ? integer(row.at("repeat_limit")) : 1;
        if (repeats<1 || repeats>12) invalid("Research repetitions must be finite and within their bound");
        shape.erase("repeat_limit");
        shape.erase("setback");
        fields(shape, {"id", "all_of", "any_of", "cash_cents", "staff_days", "experiment_milli_CU"});
        core::LabResearch topic{text(row.at("id")), ids(row.at("all_of")), ids(row.at("any_of")),
            core::Cents{integer(row.at("cash_cents"))}, core::Days{integer(row.at("staff_days"))},
            core::MilliCU{integer(row.at("experiment_milli_CU"))},static_cast<std::uint32_t>(repeats)};
        if (row.contains("setback")) {
            const auto& risk = row.at("setback");
            fields(risk, {"failure_bp", "retry_credit_bp", "safeguard", "finding"});
            topic.setback = core::ResearchSetback{core::BasisPoints{integer(risk.at("failure_bp"))},
                core::BasisPoints{integer(risk.at("retry_credit_bp"))}, text(risk.at("safeguard")), text(risk.at("finding"))};
        }
        catalog.research.push_back(std::move(topic));
    }
    for (const auto& row : doc.at("recipes")) {
        fields(row, {"id", "name", "requires_research", "baseline_bp", "gain_bp", "ceiling_bp", "fit_bp", "data_scale", "compute_scale_milli_CU",
            "minimum_work_milli_CU", "maximum_work_milli_CU", "preview_uncertainty_bp", "inference_milli_CU_per_task", "minimum_memory_mib", "interconnect"});
        catalog.recipes.push_back({text(row.at("id")), text(row.at("name")), ids(row.at("requires_research")),
            profile(row.at("baseline_bp")), profile(row.at("gain_bp")), profile(row.at("ceiling_bp")), profile(row.at("fit_bp")),
            data_scale(row.at("data_scale")),
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
    if (doc.contains("synthesis")) {
        array(doc.at("synthesis"));
        for (const auto& row:doc.at("synthesis")) {
            fields(row,{"id","name","requires_research","domain","fee_cents","verification_milli_CU_per_unit",
                "staff_days","maximum_units","diversity_bp","minimum_teacher_bp"});
            const auto domain=text(row.at("domain"));
            const auto axis=std::ranges::find(core::lab_domain_names,domain);
            if (axis==core::lab_domain_names.end()) invalid("Synthetic data requires a supported Lab task domain");
            const auto units=integer(row.at("maximum_units"));
            if (units==0 || units>1'000'000'000'000ULL) invalid("Synthetic data units exceed the finite corpus bound");
            catalog.synthesis.push_back({text(row.at("id")),text(row.at("name")),ids(row.at("requires_research")),
                static_cast<std::size_t>(axis-core::lab_domain_names.begin()),core::Cents{integer(row.at("fee_cents"))},
                core::MilliCU{integer(row.at("verification_milli_CU_per_unit"))},core::Days{integer(row.at("staff_days"))},
                units,core::BasisPoints{integer(row.at("diversity_bp"))},core::BasisPoints{integer(row.at("minimum_teacher_bp"))}});
        }
    }
    if(doc.contains("acquisition")) {
        array(doc.at("acquisition"));
        for(const auto& row:doc.at("acquisition")) {
            auto shape=row;
            shape.erase("feedback");
            fields(shape,{"dataset","collection_days","staff_days","review_work_milli_CU"});
            core::DataAcquisitionOffer offer{parse_data(row.at("dataset")),
                core::Days{integer(row.at("collection_days"))},core::Days{integer(row.at("staff_days"))},
                core::MilliCU{integer(row.at("review_work_milli_CU"))}};
            if(!valid_id(offer.dataset.id)) invalid("Collection needs a valid dataset ID");
            if(row.contains("feedback")) {
                const auto& feedback=row.at("feedback");
                fields(feedback,{"product","segment","opted_in_share_bp"});
                const auto product=text(feedback.at("product"));
                const auto segment=text(feedback.at("segment"));
                if(!valid_id(product) || !valid_id(segment)) invalid("Feedback needs a valid service and customer group ID");
                offer.feedback=core::FeedbackConsent{product,segment,core::BasisPoints{integer(feedback.at("opted_in_share_bp"))}};
            }
            catalog.acquisition.push_back(std::move(offer));
        }
    }
    return catalog;
}
std::string canonical_lab_catalog(const core::LabCatalog& catalog) {
    Json data = Json::array(), research = Json::array(), recipes = Json::array(), evaluations = Json::array();
    for (const auto* row : ordered(catalog.data)) data.push_back(encode_data(*row));
    for (const auto* row : ordered(catalog.research)) {
        research.push_back({{"id",row->id},{"all_of",row->all_of},{"any_of",row->any_of},
            {"cash_cents",row->cash.value()},{"staff_days",row->staff_days.value()},{"experiment_milli_CU",row->experiment_work.value()}});
        if (row->repeat_limit!=1) research.back()["repeat_limit"]=row->repeat_limit;
        if (const auto& risk = row->setback)
            research.back()["setback"] = {{"failure_bp",risk->failure_chance.value()},{"retry_credit_bp",risk->retry_credit.value()},
                {"safeguard",risk->safeguard},{"finding",risk->finding}};
    }
    for (const auto* row : ordered(catalog.recipes)) recipes.push_back({{"id",row->id},{"name",row->name},{"requires_research",row->requires_research},
        {"baseline_bp",profile_json(row->baseline)},{"gain_bp",profile_json(row->gain)},{"ceiling_bp",profile_json(row->ceiling)},{"fit_bp",profile_json(row->fit)},
        {"data_scale",data_scale_json(row->data_scale)},{"compute_scale_milli_CU",row->compute_scale.value()},
        {"minimum_work_milli_CU",row->minimum_work.value()},{"maximum_work_milli_CU",row->maximum_work.value()},
        {"preview_uncertainty_bp",row->preview_uncertainty.value()},{"inference_milli_CU_per_task",row->inference_per_task.value()},
        {"minimum_memory_mib",row->minimum_memory_mib},{"interconnect",row->interconnect}});
    for (const auto* row : ordered(catalog.evaluations)) evaluations.push_back({{"id",row->id},{"name",row->name},{"requires_research",row->requires_research},
        {"coverage_bp",profile_json(row->coverage)},{"required_bp",profile_json(row->required)},{"uncertainty_bp",row->uncertainty.value()},
        {"independent",row->independent},{"work_milli_CU",row->work.value()},{"cash_cents",row->cash.value()}});
    Json result{{"data",std::move(data)},{"research",std::move(research)},{"recipes",std::move(recipes)},{"evaluations",std::move(evaluations)}};
    if (!catalog.synthesis.empty()) {
        auto& rows=result["synthesis"]=Json::array();
        for (const auto* row:ordered(catalog.synthesis)) {
            if (row->domain>=core::lab_domain_count) invalid("Synthetic data has an unsupported Lab task domain");
            rows.push_back({{"id",row->id},{"name",row->name},{"requires_research",row->requires_research},
                {"domain",core::lab_domain_names[row->domain]},{"fee_cents",row->fee.value()},
                {"verification_milli_CU_per_unit",row->verification_per_unit.value()},{"staff_days",row->staff_days.value()},
                {"maximum_units",row->maximum_units},{"diversity_bp",row->diversity.value()},
                {"minimum_teacher_bp",row->minimum_teacher.value()}});
        }
    }
    if(!catalog.acquisition.empty()) {
        auto& rows=result["acquisition"]=Json::array();
        std::vector<const core::DataAcquisitionOffer*> offers;
        for(const auto& offer:catalog.acquisition) offers.push_back(&offer);
        std::ranges::sort(offers,{},[](const auto* offer){return offer->dataset.id;});
        for(const auto* offer:offers) {
            rows.push_back({{"dataset",encode_data(offer->dataset)},{"collection_days",offer->collection_days.value()},
                {"staff_days",offer->staff_days.value()},{"review_work_milli_CU",offer->review_work.value()}});
            if(offer->feedback) rows.back()["feedback"]={{"product",offer->feedback->product_id},
                {"segment",offer->feedback->segment_id},{"opted_in_share_bp",offer->feedback->opted_in_share.value()}};
        }
    }
    if(catalog.research_workplaces) result["research_workplaces"]=*catalog.research_workplaces;
    return result.dump();
}
core::LabCatalog scenario_lab_catalog(const Collection& collection, const ScenarioDefinition& scenario) {
    if (!scenario.lab) throw std::invalid_argument("Scenario has no lab catalog");
    auto result = *scenario.lab;
    if (!result.research.empty()) throw std::invalid_argument("Scenario lab research comes from its referenced research definitions");
    for (const auto& id : scenario.research) {
        const auto* definition = collection.find(id);
        if (!definition || !std::holds_alternative<ResearchDefinition>(definition->data)) invalid("Scenario research reference is not a research node");
        const auto& node = std::get<ResearchDefinition>(definition->data);
        result.research.push_back({id, node.all_of, node.any_of, result.research_workplaces?core::Cents{0}:node.cash, node.staff_days, node.work, node.repeat_limit, node.setback});
    }
    if(result.research_workplaces) {
        if(!scenario.management || !scenario.management->people || !scenario.infrastructure ||
            !scenario.infrastructure->workplace || !scenario.infrastructure->workplace->headquarters)
            invalid("Equipment-backed research requires a physical headquarters");
        const auto& offers=scenario.infrastructure->workplace->headquarters->equipment;
        for(const auto& [id,required]:*result.research_workplaces) {
            if(std::ranges::find(scenario.research,id)==scenario.research.end()) invalid("A research facility must serve a topic in this scenario");
            for(const auto& equipment:required)
                if(std::ranges::find(offers,equipment,&core::HQEquipmentOffer::id)==offers.end()) invalid("Research facility equipment must be sold by an actual supplier");
        }
    }
    // Canonical order is also the saved catalog comparison order.
    result = load_lab_catalog(canonical_lab_catalog(result));
    core::validate_lab_catalog(result);
    return result;
}
} // namespace pgp::content
