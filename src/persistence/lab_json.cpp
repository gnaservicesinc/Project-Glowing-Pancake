#include "lab_json.hpp"
#include "lab_profile_json.hpp"
#include <pgp/content/lab_catalog.hpp>

namespace pgp::persistence::detail {
namespace {
std::string label(const Json& value, bool empty = false) { return text(value,"lab/text",256,empty); }
std::vector<std::string> strings(const Json& value, std::size_t bound, std::size_t length = 256) {
    array(value,"lab/strings",bound); std::vector<std::string> result;
    for (const auto& item : value) result.push_back(text(item,"lab/string",length));
    return result;
}
Json data_offer_json(const core::DataOffer& offer) {
    core::LabCatalog catalog;
    catalog.data.push_back(offer);
    return Json::parse(content::canonical_lab_catalog(catalog)).at("data").front();
}
core::DataOffer parse_data_offer(const Json& value) {
    const Json catalog{{"data",Json::array({value})},{"research",Json::array()},
        {"recipes",Json::array()},{"evaluations",Json::array()}};
    try { return content::load_lab_catalog(catalog.dump()).data.front(); }
    catch (const std::exception& error) { invalid("lab/generated/output",error.what()); }
}
}
Json lab_json(const core::LabState& state) {
    Json licenses=Json::array(), research=Json::array(), models=Json::array(), evaluations=Json::array();
    for (const auto& row : state.licenses) licenses.push_back({{"offer_id",row.offer_id},{"acquired_on",row.acquired_on.value()}});
    for (const auto& row : state.research) {
        research.push_back({{"research_id",row.research_id},{"project_id",row.project_id},{"completed",row.completed},{"insight_credit",row.insight_credit.value()}});
        if (row.sponsored_fee.value()!=0) research.back()["sponsored_fee"]=row.sponsored_fee.value();
    }
    for (const auto& row : state.models) {models.push_back({{"id",row.id},{"recipe_id",row.recipe_id},{"parent_id",row.parent_id},{"project_id",row.project_id},
        {"data_ids",row.data_ids},{"created_on",row.created_on.value()},{"inherited_work_milli_CU",row.inherited_work.value()},
        {"consumed_work_milli_CU",row.consumed_work.value()},{"capability_bp",lab_profile_json(row.capability)}});
        if(row.checkpoint_cursor) models.back()["checkpoint_cursor"]=*row.checkpoint_cursor;
    }
    for (const auto& row : state.evaluations) evaluations.push_back({{"id",row.id},{"model_id",row.model_id},{"plan_id",row.plan_id},{"project_id",row.project_id},
        {"model_work_milli_CU",row.model_work.value()},{"completed",row.completed},{"measured_bp",lab_profile_json(row.measured)},
        {"lower_bp",lab_profile_json(row.lower)},{"upper_bp",lab_profile_json(row.upper)},{"findings",row.findings},
        {"stream_before",row.stream_before},{"stream_after",row.stream_after}});
    Json publications=Json::array(),insights=Json::array();
    for (const auto& row:state.publications) publications.push_back({{"research",row.research_id},{"day",row.day.value()}});
    for (const auto& row:state.insights) insights.push_back({{"research",row.research_id},{"publisher",row.publisher},{"published",row.published_on.value()},{"observed",row.observed_on.value()},{"credit",row.credit.value()}});
    Json result{{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",state.closed_through?Json(state.closed_through->value()):Json(nullptr)},
        {"research_stream_origin",state.research_stream_origin},{"catalog",Json::parse(content::canonical_lab_catalog(state.catalog))},
        {"licenses",std::move(licenses)},{"research",std::move(research)},{"models",std::move(models)},{"evaluations",std::move(evaluations)},
        {"publications",std::move(publications)},{"insights",std::move(insights)}};
    if (!state.generated.empty()) {
        auto& rows=result["generated"]=Json::array();
        for (const auto& row:state.generated) {
            rows.push_back({{"id",row.id},{"recipe_id",row.recipe_id},{"teacher_id",row.teacher_id},{"evidence_id",row.evidence_id},
                {"verification_id",row.verification_id},{"project_id",row.project_id},{"started_on",row.started_on.value()},
                {"teacher_work_milli_CU",row.teacher_work.value()},{"attempted_units",row.attempted_units}});
            if (row.output) rows.back()["output"]=data_offer_json(*row.output);
        }
    }
    if(!state.acquisitions.empty()) {
        auto& rows=result["acquisitions"]=Json::array();
        for(const auto& job:state.acquisitions) {
            rows.push_back({{"offer_id",job.offer_id},{"project_id",job.project_id}});
            if(job.settled_deliveries) rows.back()["settled_deliveries"]=*job.settled_deliveries;
            if(job.output) rows.back()["output"]=data_offer_json(*job.output);
        }
    }
    return result;
}
core::LabState parse_lab(const Json& value) {
    fields(value,"lab",{"owner","starts_on","closed_through","research_stream_origin","catalog","licenses","research","models","evaluations"},{"publications","insights","generated","acquisitions"});
    core::LabState state;
    state.owner=label(value.at("owner")); state.starts_on=core::OrdinalDay{i64(value.at("starts_on"),"lab/starts_on")};
    if (!value.at("closed_through").is_null()) state.closed_through=core::OrdinalDay{i64(value.at("closed_through"),"lab/closed_through")};
    state.research_stream_origin=u64(value.at("research_stream_origin"),"lab/research_stream_origin");
    try { state.catalog=content::load_lab_catalog(value.at("catalog").dump()); }
    catch (const std::exception& error) { invalid("lab/catalog",error.what()); }
    if (value.contains("publications")) {
        array(value.at("publications"),"lab/publications",core::lab_catalog_limit);
        for (const auto& row:value.at("publications")) {
            fields(row,"lab/publication",{"research","day"});
            state.publications.push_back({label(row.at("research")),core::OrdinalDay{i64(row.at("day"),"lab/publication/day")}});
        }
    }
    if (value.contains("insights")) {
        array(value.at("insights"),"lab/insights",core::lab_catalog_limit);
        for (const auto& row:value.at("insights")) {
            fields(row,"lab/insight",{"research","publisher","published","observed","credit"});
            state.insights.push_back({label(row.at("research")),label(row.at("publisher")),core::OrdinalDay{i64(row.at("published"),"lab/insight/published")},
                core::OrdinalDay{i64(row.at("observed"),"lab/insight/observed")},core::BasisPoints{i64(row.at("credit"),"lab/insight/credit")}});
        }
    }
    array(value.at("licenses"),"lab/licenses",core::lab_catalog_limit);
    array(value.at("research"),"lab/research",core::lab_record_limit);
    array(value.at("models"),"lab/models",core::lab_record_limit);
    array(value.at("evaluations"),"lab/evaluations",core::lab_record_limit);
    for (const auto& row : value.at("licenses")) {
        fields(row,"lab/license",{"offer_id","acquired_on"});
        state.licenses.push_back({label(row.at("offer_id")),core::OrdinalDay{i64(row.at("acquired_on"),"lab/license/acquired_on")}});
    }
    for (const auto& row : value.at("research")) {
        fields(row,"lab/research_run",{"research_id","project_id","completed"},{"insight_credit","sponsored_fee"});
        state.research.push_back({label(row.at("research_id")),label(row.at("project_id")),boolean(row.at("completed"),"lab/research_run/completed"),
            core::BasisPoints{row.contains("insight_credit")?i64(row.at("insight_credit"),"lab/research_run/insight_credit"):0},
            core::Cents{row.contains("sponsored_fee")?i64(row.at("sponsored_fee"),"lab/research_run/sponsored_fee"):0}});
    }
    for (const auto& row : value.at("models")) {
        fields(row,"lab/model",{"id","recipe_id","parent_id","project_id","data_ids","created_on","inherited_work_milli_CU","consumed_work_milli_CU","capability_bp"},{"checkpoint_cursor"});
        state.models.push_back({label(row.at("id")),label(row.at("recipe_id")),label(row.at("parent_id"),true),label(row.at("project_id"),true),
            strings(row.at("data_ids"),core::lab_catalog_limit),core::OrdinalDay{i64(row.at("created_on"),"lab/model/created_on")},
            core::MilliCU{i64(row.at("inherited_work_milli_CU"),"lab/model/inherited_work")},
            core::MilliCU{i64(row.at("consumed_work_milli_CU"),"lab/model/consumed_work")},parse_lab_profile(row.at("capability_bp"))});
        if(row.contains("checkpoint_cursor")) state.models.back().checkpoint_cursor=u64(row.at("checkpoint_cursor"),"lab/model/checkpoint_cursor",core::capacity_day_limit);
    }
    for (const auto& row : value.at("evaluations")) {
        fields(row,"lab/evidence",{"id","model_id","plan_id","project_id","model_work_milli_CU","completed","measured_bp","lower_bp","upper_bp","findings","stream_before","stream_after"});
        state.evaluations.push_back({label(row.at("id")),label(row.at("model_id")),label(row.at("plan_id")),label(row.at("project_id")),
            core::MilliCU{i64(row.at("model_work_milli_CU"),"lab/evidence/model_work")},boolean(row.at("completed"),"lab/evidence/completed"),
            parse_lab_profile(row.at("measured_bp")),parse_lab_profile(row.at("lower_bp")),parse_lab_profile(row.at("upper_bp")),
            strings(row.at("findings"),core::lab_domain_count,512),u64(row.at("stream_before"),"lab/stream_before"),u64(row.at("stream_after"),"lab/stream_after")});
    }
    if (value.contains("generated")) {
        array(value.at("generated"),"lab/generated",core::lab_record_limit);
        for (const auto& row:value.at("generated")) {
            fields(row,"lab/generated/dataset",{"id","recipe_id","teacher_id","evidence_id","verification_id","project_id",
                "started_on","teacher_work_milli_CU","attempted_units"},{"output"});
            state.generated.push_back({label(row.at("id")),label(row.at("recipe_id")),label(row.at("teacher_id")),
                label(row.at("evidence_id")),label(row.at("verification_id")),label(row.at("project_id")),
                core::OrdinalDay{i64(row.at("started_on"),"lab/generated/started_on")},
                core::MilliCU{i64(row.at("teacher_work_milli_CU"),"lab/generated/teacher_work")},
                u64(row.at("attempted_units"),"lab/generated/attempted_units",1'000'000'000'000ULL),
                row.contains("output")?std::optional{parse_data_offer(row.at("output"))}:std::nullopt});
        }
    }
    if(value.contains("acquisitions")) {
        array(value.at("acquisitions"),"lab/acquisitions",core::lab_catalog_limit);
        for(const auto& row:value.at("acquisitions")) {
            fields(row,"lab/acquisition",{"offer_id","project_id"},{"settled_deliveries","output"});
            core::DataAcquisition job{label(row.at("offer_id")),label(row.at("project_id"))};
            if(row.contains("settled_deliveries")) job.settled_deliveries=u64(row.at("settled_deliveries"),"lab/acquisition/settled_deliveries");
            if(row.contains("output")) job.output=parse_data_offer(row.at("output"));
            state.acquisitions.push_back(std::move(job));
        }
    }
    return state;
}
}
