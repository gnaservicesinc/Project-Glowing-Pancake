#include <pgp/content/management_catalog.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <set>
#include <stdexcept>
namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid() {throw std::invalid_argument("Management catalog has missing, unsupported or invalid typed fields");}
void fields(const Json& j,std::initializer_list<std::string_view> keys) {
    if (!j.is_object() || j.size()!=keys.size()) invalid();for (const auto key:keys) if (!j.contains(key)) invalid();
}
std::int64_t integer(const Json& j) {
    if (!j.is_number_integer() || (j.is_number_unsigned() && j.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) invalid();
    return j.get<std::int64_t>();
}
std::string text(const Json& j) {if (!j.is_string()) invalid();const auto value=j.get<std::string>();if (value.empty() || value.size()>128 || value.find('\0')!=std::string::npos) invalid();return value;}
void array(const Json& j,std::size_t maximum) {if (!j.is_array() || j.size()>maximum) invalid();}
std::uint32_t count(const Json& j,std::uint32_t maximum) {const auto v=integer(j);if (v<0 || v>maximum) invalid();return static_cast<std::uint32_t>(v);}
}
core::ManagementCatalog load_management_catalog(std::string_view bytes) {
    if (bytes.size()>32768) invalid();std::vector<std::set<std::string>> keys;std::size_t tokens=0;
    const auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>16 || ++tokens>8192) invalid();
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid();
        return true;
    });
    fields(j,{"financing","specialists","setbacks","scoring","minimum_evaluation_interval_days","maximum_policy_runs","default_policy_budget_cents","default_reserve_floor_cents","default_monthly_ceiling_cents","default_policy_runs"});
    core::ManagementCatalog c;
    array(j.at("financing"),8);array(j.at("specialists"),8);array(j.at("setbacks"),8);
    for (const auto& row:j.at("financing")) {
        fields(row,{"id","name","proceeds_cents","acquired_bp","monthly_interest_bp","term_days"});
        c.financing.push_back({text(row.at("id")),text(row.at("name")),core::Cents{integer(row.at("proceeds_cents"))},core::BasisPoints{integer(row.at("acquired_bp"))},
            core::BasisPoints{integer(row.at("monthly_interest_bp"))},core::Days{integer(row.at("term_days"))}});
    }
    for (const auto& row:j.at("specialists")) {
        fields(row,{"id","name","signing_cost_cents","monthly_salary_cents"});
        c.specialists.push_back({text(row.at("id")),text(row.at("name")),core::Cents{integer(row.at("signing_cost_cents"))},core::Cents{integer(row.at("monthly_salary_cents"))}});
    }
    for (const auto& row:j.at("setbacks")) {
        fields(row,{"id","name","source_id","announced_on","starts_on","ends_before","reduced_milli_CU_per_day"});
        c.setbacks.push_back({text(row.at("id")),text(row.at("name")),text(row.at("source_id")),core::parse_iso_date(text(row.at("announced_on"))),
            core::parse_iso_date(text(row.at("starts_on"))),core::parse_iso_date(text(row.at("ends_before"))),core::MilliCUPerDay{integer(row.at("reduced_milli_CU_per_day"))}});
    }
    const auto& scoring=j.at("scoring");fields(scoring,{"task_target","weights","founder","opening_founder_share_bp"});
    c.scoring.task_target=static_cast<std::uint64_t>(integer(scoring.at("task_target")));
    array(scoring.at("weights"),5);if (scoring.at("weights").size()!=5) invalid();
    for (std::size_t i=0;i<5;++i) c.scoring.weights[i]=count(scoring.at("weights")[i],100);
    c.scoring.founder=text(scoring.at("founder"));c.scoring.opening_founder_share=core::BasisPoints{integer(scoring.at("opening_founder_share_bp"))};
    c.minimum_evaluation_interval=core::Days{integer(j.at("minimum_evaluation_interval_days"))};c.maximum_policy_runs=count(j.at("maximum_policy_runs"),12);
    c.default_policy_budget=core::Cents{integer(j.at("default_policy_budget_cents"))};c.default_reserve_floor=core::Cents{integer(j.at("default_reserve_floor_cents"))};
    c.default_monthly_ceiling=core::Cents{integer(j.at("default_monthly_ceiling_cents"))};c.default_policy_runs=count(j.at("default_policy_runs"),12);
    core::validate_management_catalog(c);return c;
}
std::string canonical_management_catalog(const core::ManagementCatalog& c) {
    Json financing=Json::array(),specialists=Json::array(),setbacks=Json::array();
    for (const auto& row:c.financing) financing.push_back({{"id",row.id},{"name",row.name},{"proceeds_cents",row.proceeds.value()},
        {"acquired_bp",row.acquired.value()},{"monthly_interest_bp",row.monthly_interest.value()},{"term_days",row.term.value()}});
    for (const auto& row:c.specialists) specialists.push_back({{"id",row.id},{"name",row.name},{"signing_cost_cents",row.signing_cost.value()},{"monthly_salary_cents",row.monthly_salary.value()}});
    for (const auto& row:c.setbacks) setbacks.push_back({{"id",row.id},{"name",row.name},{"source_id",row.source_id},{"announced_on",core::iso_date(row.announced_on)},
        {"starts_on",core::iso_date(row.starts_on)},{"ends_before",core::iso_date(row.ends_before)},{"reduced_milli_CU_per_day",row.reduced_capacity.value()}});
    return Json{{"financing",std::move(financing)},{"specialists",std::move(specialists)},{"setbacks",std::move(setbacks)},
        {"scoring",{{"task_target",c.scoring.task_target},{"weights",c.scoring.weights},{"founder",c.scoring.founder},{"opening_founder_share_bp",c.scoring.opening_founder_share.value()}}},
        {"minimum_evaluation_interval_days",c.minimum_evaluation_interval.value()},{"maximum_policy_runs",c.maximum_policy_runs},{"default_policy_budget_cents",c.default_policy_budget.value()},
        {"default_reserve_floor_cents",c.default_reserve_floor.value()},{"default_monthly_ceiling_cents",c.default_monthly_ceiling.value()},{"default_policy_runs",c.default_policy_runs}}.dump();
}
}
