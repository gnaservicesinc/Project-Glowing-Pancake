#include "lab_json.hpp"
#include <pgp/content/lab_catalog.hpp>

namespace pgp::persistence::detail {
namespace {
Json profile(const core::LabProfile& value) { return Json::array({value[0].value(), value[1].value()}); }
core::LabProfile parse_profile(const Json& value) {
    array(value,"lab/profile",core::lab_domain_count);
    if (value.size() != core::lab_domain_count) invalid("lab/profile","Wrong domain count");
    return {core::BasisPoints{i64(value.at(0),"lab/profile")}, core::BasisPoints{i64(value.at(1),"lab/profile")}};
}
std::string label(const Json& value, bool empty = false) { return text(value,"lab/text",256,empty); }
std::vector<std::string> strings(const Json& value, std::size_t bound, std::size_t length = 256) {
    array(value,"lab/strings",bound); std::vector<std::string> result;
    for (const auto& item : value) result.push_back(text(item,"lab/string",length));
    return result;
}
}
Json lab_json(const core::LabState& state) {
    Json licenses=Json::array(), research=Json::array(), models=Json::array(), evaluations=Json::array();
    for (const auto& row : state.licenses) licenses.push_back({{"offer_id",row.offer_id},{"acquired_on",row.acquired_on.value()}});
    for (const auto& row : state.research) research.push_back({{"research_id",row.research_id},{"project_id",row.project_id},{"completed",row.completed}});
    for (const auto& row : state.models) models.push_back({{"id",row.id},{"recipe_id",row.recipe_id},{"parent_id",row.parent_id},{"project_id",row.project_id},
        {"data_ids",row.data_ids},{"created_on",row.created_on.value()},{"inherited_work_milli_CU",row.inherited_work.value()},
        {"consumed_work_milli_CU",row.consumed_work.value()},{"capability_bp",profile(row.capability)}});
    for (const auto& row : state.evaluations) evaluations.push_back({{"id",row.id},{"model_id",row.model_id},{"plan_id",row.plan_id},{"project_id",row.project_id},
        {"model_work_milli_CU",row.model_work.value()},{"completed",row.completed},{"measured_bp",profile(row.measured)},
        {"lower_bp",profile(row.lower)},{"upper_bp",profile(row.upper)},{"findings",row.findings},
        {"stream_before",row.stream_before},{"stream_after",row.stream_after}});
    return {{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",state.closed_through?Json(state.closed_through->value()):Json(nullptr)},
        {"research_stream_origin",state.research_stream_origin},{"catalog",Json::parse(content::canonical_lab_catalog(state.catalog))},
        {"licenses",std::move(licenses)},{"research",std::move(research)},{"models",std::move(models)},{"evaluations",std::move(evaluations)}};
}
core::LabState parse_lab(const Json& value) {
    fields(value,"lab",{"owner","starts_on","closed_through","research_stream_origin","catalog","licenses","research","models","evaluations"});
    core::LabState state;
    state.owner=label(value.at("owner")); state.starts_on=core::OrdinalDay{i64(value.at("starts_on"),"lab/starts_on")};
    if (!value.at("closed_through").is_null()) state.closed_through=core::OrdinalDay{i64(value.at("closed_through"),"lab/closed_through")};
    state.research_stream_origin=u64(value.at("research_stream_origin"),"lab/research_stream_origin");
    try { state.catalog=content::load_lab_catalog(value.at("catalog").dump()); }
    catch (const std::exception& error) { invalid("lab/catalog",error.what()); }
    array(value.at("licenses"),"lab/licenses",core::lab_catalog_limit);
    array(value.at("research"),"lab/research",core::lab_record_limit);
    array(value.at("models"),"lab/models",core::lab_record_limit);
    array(value.at("evaluations"),"lab/evaluations",core::lab_record_limit);
    for (const auto& row : value.at("licenses")) {
        fields(row,"lab/license",{"offer_id","acquired_on"});
        state.licenses.push_back({label(row.at("offer_id")),core::OrdinalDay{i64(row.at("acquired_on"),"lab/license/acquired_on")}});
    }
    for (const auto& row : value.at("research")) {
        fields(row,"lab/research_run",{"research_id","project_id","completed"});
        state.research.push_back({label(row.at("research_id")),label(row.at("project_id")),boolean(row.at("completed"),"lab/research_run/completed")});
    }
    for (const auto& row : value.at("models")) {
        fields(row,"lab/model",{"id","recipe_id","parent_id","project_id","data_ids","created_on","inherited_work_milli_CU","consumed_work_milli_CU","capability_bp"});
        state.models.push_back({label(row.at("id")),label(row.at("recipe_id")),label(row.at("parent_id"),true),label(row.at("project_id"),true),
            strings(row.at("data_ids"),core::lab_catalog_limit),core::OrdinalDay{i64(row.at("created_on"),"lab/model/created_on")},
            core::MilliCU{i64(row.at("inherited_work_milli_CU"),"lab/model/inherited_work")},
            core::MilliCU{i64(row.at("consumed_work_milli_CU"),"lab/model/consumed_work")},parse_profile(row.at("capability_bp"))});
    }
    for (const auto& row : value.at("evaluations")) {
        fields(row,"lab/evidence",{"id","model_id","plan_id","project_id","model_work_milli_CU","completed","measured_bp","lower_bp","upper_bp","findings","stream_before","stream_after"});
        state.evaluations.push_back({label(row.at("id")),label(row.at("model_id")),label(row.at("plan_id")),label(row.at("project_id")),
            core::MilliCU{i64(row.at("model_work_milli_CU"),"lab/evidence/model_work")},boolean(row.at("completed"),"lab/evidence/completed"),
            parse_profile(row.at("measured_bp")),parse_profile(row.at("lower_bp")),parse_profile(row.at("upper_bp")),
            strings(row.at("findings"),core::lab_domain_count,512),u64(row.at("stream_before"),"lab/stream_before"),u64(row.at("stream_after"),"lab/stream_after")});
    }
    return state;
}
}
