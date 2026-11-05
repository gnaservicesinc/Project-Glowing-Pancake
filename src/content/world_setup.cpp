#include <pgp/content/world_setup.hpp>
#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
Json parse(std::string_view bytes) {
    if (bytes.size()>maximum_document_bytes) invalid("World setup exceeds its byte bound");
    std::size_t tokens=0; std::vector<std::set<std::string>> keys;
    return Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>16 || ++tokens>32768) invalid("World setup exceeds its JSON budget");
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid("Duplicate world setup key");
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid("World setup uses integer tokens only");
        return true;
    });
}
void fields(const Json& row,std::initializer_list<std::string_view> names) {
    if (!row.is_object() || row.size()!=names.size()) invalid("World setup has missing or unsupported fields");
    for (auto name:names) if (!row.contains(name)) invalid("World setup has missing or unsupported fields");
}
std::string text(const Json& value,bool empty=false) {
    if (!value.is_string()) invalid("World setup needs a label");
    auto result=value.get<std::string>();
    if ((!empty && result.empty()) || result.size()>128 || std::any_of(result.begin(),result.end(),[](unsigned char c){return c<32 || c==127;}))
        invalid("World setup label is empty, oversized or contains control characters");
    return result;
}
std::uint64_t unsigned_number(const Json& value) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>()<0)) invalid("World setup requires nonnegative integer tokens");
    return value.get<std::uint64_t>();
}
std::uint64_t number(const Json& value) {
    const auto result=unsigned_number(value);
    if (result>static_cast<std::uint64_t>(INT64_MAX)) invalid("World setup integer exceeds its signed unit bound");
    return result;
}
core::OrdinalDay date(const Json& value) { return core::parse_iso_date(text(value)); }
void array(const Json& value,std::size_t maximum) {
    if (!value.is_array() || value.size()>maximum) invalid("World setup count exceeds its bound");
}
}
std::vector<core::BusinessOffer> load_business_offer_catalog(std::string_view bytes) {
    const auto doc=parse(bytes); array(doc,32); std::vector<core::BusinessOffer> result;
    for (const auto& row:doc) {
        fields(row,{"id","name","kind","available_from","available_before","term_days","setup_cost_cents","monthly_amount_cents",
            "capacity_milli_CU_per_day","segment_id","product_id","tasks_per_day"});
        const auto kind=text(row.at("kind"));
        if (kind!="compute_lease" && kind!="recovery_pilot" && kind!="enterprise_contract") invalid("Unsupported business offer kind");
        const auto type=kind=="compute_lease"?core::BusinessOfferKind::compute_lease:
            kind=="recovery_pilot"?core::BusinessOfferKind::recovery_pilot:core::BusinessOfferKind::enterprise_contract;
        result.push_back({text(row.at("id")),text(row.at("name")),type,
            date(row.at("available_from")),date(row.at("available_before")),core::Days{number(row.at("term_days"))},
            core::Cents{number(row.at("setup_cost_cents"))},core::Cents{number(row.at("monthly_amount_cents"))},
            core::MilliCUPerDay{number(row.at("capacity_milli_CU_per_day"))},text(row.at("segment_id"),true),text(row.at("product_id"),true),number(row.at("tasks_per_day"))});
        if (!valid_id(result.back().id)) invalid("Business offer needs a stable content ID");
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_business_offer_catalog(result); return result;
}
std::string canonical_business_offer_catalog(const std::vector<core::BusinessOffer>& offers) {
    Json result=Json::array();
    for (const auto& row:offers) result.push_back({{"id",row.id},{"name",row.name},{"kind",row.kind==core::BusinessOfferKind::compute_lease?"compute_lease":
        row.kind==core::BusinessOfferKind::recovery_pilot?"recovery_pilot":"enterprise_contract"},
        {"available_from",core::iso_date(row.available_from)},{"available_before",core::iso_date(row.available_before)},
        {"term_days",row.term.value()},{"setup_cost_cents",row.setup_cost.value()},{"monthly_amount_cents",row.monthly_amount.value()},
        {"capacity_milli_CU_per_day",row.capacity.value()},{"segment_id",row.segment_id},{"product_id",row.product_id},{"tasks_per_day",row.tasks_per_day}});
    return result.dump();
}
std::vector<RivalDefinition> load_rival_definitions(std::string_view bytes) {
    const auto doc=parse(bytes); array(doc,32); std::vector<RivalDefinition> result; std::set<std::string> ids;
    for (const auto& row:doc) {
        fields(row,{"id","name","seed","cash_cents","monthly_payroll_cents","monthly_lease_cents","capacity_milli_CU_per_day",
            "training_milli_CU_per_day","staff","start_after_days","warning_lead_days","player_deployment_threshold","recipe_id","data_id","training_work_milli_CU"});
        const auto staff=number(row.at("staff")), threshold=number(row.at("player_deployment_threshold"));
        if (staff==0 || staff>1000 || threshold>1024) invalid("Rival staff or trigger count exceeds its bound");
        result.push_back({text(row.at("id")),text(row.at("name")),unsigned_number(row.at("seed")),core::Cents{number(row.at("cash_cents"))},
            core::Cents{number(row.at("monthly_payroll_cents"))},core::Cents{number(row.at("monthly_lease_cents"))},
            core::MilliCUPerDay{number(row.at("capacity_milli_CU_per_day"))},core::MilliCUPerDay{number(row.at("training_milli_CU_per_day"))},
            static_cast<std::uint32_t>(staff),core::Days{number(row.at("start_after_days"))},core::Days{number(row.at("warning_lead_days"))},
            static_cast<std::uint32_t>(threshold),text(row.at("recipe_id")),text(row.at("data_id")),core::MilliCU{number(row.at("training_work_milli_CU"))}});
        const auto& rival=result.back();
        if (!valid_id(rival.id) || rival.id.size()>64 || !ids.insert(rival.id).second || rival.cash.value()==0 ||
            rival.monthly_payroll.value()==0 || rival.monthly_lease.value()==0 || rival.capacity.value()==0 || rival.training_rate.value()==0 ||
            rival.cash.value()>1'000'000'000'000LL || rival.monthly_payroll.value()>1'000'000'000'000LL ||
            rival.monthly_lease.value()>1'000'000'000'000LL || rival.capacity.value()>1'000'000'000 ||
            rival.training_rate>rival.capacity || rival.start_after.value()>1096 || rival.warning_lead.value()>365 ||
            (rival.player_deployment_threshold==0 && rival.warning_lead.value()!=0) ||
            ((rival.player_deployment_threshold>0)!=(rival.warning_lead.value()>=90))) invalid("Rival budget, warning or resource terms are invalid");
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;}); return result;
}
std::string canonical_rival_definitions(const std::vector<RivalDefinition>& rivals) {
    Json result=Json::array();
    for (const auto& row:rivals) result.push_back({{"id",row.id},{"name",row.name},{"seed",row.seed},{"cash_cents",row.cash.value()},
        {"monthly_payroll_cents",row.monthly_payroll.value()},{"monthly_lease_cents",row.monthly_lease.value()},
        {"capacity_milli_CU_per_day",row.capacity.value()},{"training_milli_CU_per_day",row.training_rate.value()},{"staff",row.staff},
        {"start_after_days",row.start_after.value()},{"warning_lead_days",row.warning_lead.value()},{"player_deployment_threshold",row.player_deployment_threshold},
        {"recipe_id",row.recipe_id},{"data_id",row.data_id},{"training_work_milli_CU",row.training_work.value()}});
    return result.dump();
}
}
