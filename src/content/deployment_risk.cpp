#include <pgp/content/deployment_risk.hpp>
#include <pgp/core/lab.hpp>
#include <pgp/core/market.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid() {throw std::invalid_argument("Invalid deployment incident risk rules");}
void fields(const Json& j,std::initializer_list<std::string_view> names) {
    if(!j.is_object() || j.size()!=names.size()) invalid();
    for(const auto name:names) if(!j.contains(name)) invalid();
}
std::string text(const Json& j,std::size_t maximum=4096) {
    if(!j.is_string()) invalid();
    auto result=j.get<std::string>();
    if(result.empty() || result.size()>maximum || result.find('\0')!=std::string::npos) invalid();
    return result;
}
std::uint64_t number(const Json& j,std::uint64_t maximum=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    if(!j.is_number_integer() || (!j.is_number_unsigned() && j.get<std::int64_t>()<0)) invalid();
    const auto result=j.get<std::uint64_t>();
    if(result>maximum) invalid();
    return result;
}
bool boolean(const Json& j) {if(!j.is_boolean()) invalid();return j.get<bool>();}
}
core::DeploymentRiskRules load_deployment_risk_rules(std::string_view bytes) {
    if(bytes.size()>65536) invalid();
    std::vector<std::set<std::string>> keys;
    std::size_t tokens=0;
    const auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if(depth>12 || ++tokens>8192) invalid();
        if(event==Json::parse_event_t::object_start) keys.emplace_back();
        else if(event==Json::parse_event_t::object_end) keys.pop_back();
        else if(event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if(event==Json::parse_event_t::value && value.is_number_float()) invalid();
        return true;
    });
    fields(j,{"assumptions","exposure","severity","domain","control_threshold_bp","large_daily_tasks","elevated_from_bp","high_from_bp",
        "lookup","compensation_cents","response_fee_cents","response_work_milli_CU","response_staff_days"});
    core::DeploymentRiskRules result;
    result.assumptions=text(j.at("assumptions"),1024);result.exposure=text(j.at("exposure"),32);result.severity=text(j.at("severity"),32);
    if(result.exposure!="bounded" && result.exposure!="public" && result.exposure!="physical") invalid();
    if(result.severity!="routine" && result.severity!="consequential" && result.severity!="critical") invalid();
    const auto name=text(j.at("domain"),64);
    const auto domain=std::ranges::find(core::lab_domain_names,name);
    if(domain==core::lab_domain_names.end()) invalid();
    result.domain=static_cast<std::size_t>(domain-core::lab_domain_names.begin());
    result.control_threshold=core::BasisPoints{number(j.at("control_threshold_bp"),10000)};
    result.large_daily_tasks=number(j.at("large_daily_tasks"),core::market_task_limit);
    result.elevated_from=core::BasisPoints{number(j.at("elevated_from_bp"),10000)};
    result.high_from=core::BasisPoints{number(j.at("high_from_bp"),10000)};
    const auto& lookup=j.at("lookup");
    if(!lookup.is_array() || lookup.size()!=48) invalid();
    for(const auto& row:lookup) {
        fields(row,{"large","autonomy","mismatch","supervised","staged","probability_bp"});
        result.lookup.push_back({boolean(row.at("large")),core::parse_agent_tools(text(row.at("autonomy"),32)),boolean(row.at("mismatch")),
            boolean(row.at("supervised")),boolean(row.at("staged")),core::BasisPoints{number(row.at("probability_bp"),10000)}});
    }
    result.compensation=core::Cents{number(j.at("compensation_cents"))};
    result.response_fee=core::Cents{number(j.at("response_fee_cents"))};
    result.response_work=core::MilliCU{number(j.at("response_work_milli_CU"))};
    result.response_staff_days=core::Days{number(j.at("response_staff_days"))};
    core::validate_deployment_risk_rules(result);
    return result;
}
std::string canonical_deployment_risk_rules(const core::DeploymentRiskRules& rules) {
    core::validate_deployment_risk_rules(rules);
    Json lookup=Json::array();
    for(const auto& cell:rules.lookup) lookup.push_back({{"large",cell.large},{"autonomy",core::agent_tools_key(cell.autonomy)},
        {"mismatch",cell.mismatch},{"supervised",cell.supervised},{"staged",cell.staged},{"probability_bp",cell.probability.value()}});
    return Json{{"assumptions",rules.assumptions},{"exposure",rules.exposure},{"severity",rules.severity},{"domain",core::lab_domain_names.at(rules.domain)},
        {"control_threshold_bp",rules.control_threshold.value()},{"large_daily_tasks",rules.large_daily_tasks},{"elevated_from_bp",rules.elevated_from.value()},
        {"high_from_bp",rules.high_from.value()},{"lookup",std::move(lookup)},{"compensation_cents",rules.compensation.value()},
        {"response_fee_cents",rules.response_fee.value()},{"response_work_milli_CU",rules.response_work.value()},
        {"response_staff_days",rules.response_staff_days.value()}}.dump();
}
}
