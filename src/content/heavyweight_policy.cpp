#include <pgp/content/world_setup.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid() {throw std::invalid_argument("Heavyweight policy has unsupported fields, units, references or bounds");}
std::uint64_t number(const Json& value) {if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>()<0)) invalid();const auto result=value.get<std::uint64_t>();if (result>static_cast<std::uint64_t>(INT64_MAX)) invalid();return result;}
}
HeavyweightPolicy load_heavyweight_policy(std::string_view bytes) {
    if (bytes.size()>8192) invalid();std::vector<std::set<std::string>> keys;std::size_t tokens=0;
    const auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>4 || ++tokens>256) invalid();
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid();return true;
    });
    const std::set<std::string> fields{"review_days","warning_days","delivery_days","cooldown_days","entry_pressure_bp","exit_pressure_bp","weights","revenue_at_risk_cents","capability_target","financed_target","specialist_id","lease_offer_id","publication_node","pilot_product","publication_fee_cents","pilot_fee_cents","offer_response_days","offer_term_days","pilot_tasks"};
    if (!j.is_object() || j.size()!=fields.size()) invalid();for (const auto& name:fields) if (!j.contains(name)) invalid();
    HeavyweightPolicy p;
    p.review_interval=core::Days{number(j.at("review_days"))};p.warning_lead=core::Days{number(j.at("warning_days"))};p.delivery_lead=core::Days{number(j.at("delivery_days"))};p.cooldown=core::Days{number(j.at("cooldown_days"))};
    p.entry_pressure=core::BasisPoints{number(j.at("entry_pressure_bp"))};p.exit_pressure=core::BasisPoints{number(j.at("exit_pressure_bp"))};
    if (!j.at("weights").is_array() || j.at("weights").size()!=4) invalid();std::uint64_t sum=0;
    for (std::size_t i=0;i<4;++i) {const auto weight=number(j.at("weights")[i]);if (weight>10000) invalid();p.weights[i]=static_cast<std::uint32_t>(weight);sum+=weight;}
    p.revenue_at_risk=core::Cents{number(j.at("revenue_at_risk_cents"))};
    const auto capability=number(j.at("capability_target")),financed=number(j.at("financed_target"));
    if (capability==0 || capability>64 || financed==0 || financed>256) invalid();p.capability_target=static_cast<std::uint32_t>(capability);p.financed_target=static_cast<std::uint32_t>(financed);
    const auto id=[&](const char* field){if (!j.at(field).is_string()) invalid();auto result=j.at(field).get<std::string>();if (result.empty() || result.size()>100 || result.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/")!=std::string::npos) invalid();return result;};
    p.specialist_id=id("specialist_id");p.lease_offer_id=id("lease_offer_id");p.publication_node=id("publication_node");p.pilot_product=id("pilot_product");
    p.publication_fee=core::Cents{number(j.at("publication_fee_cents"))};p.pilot_fee=core::Cents{number(j.at("pilot_fee_cents"))};p.offer_response=core::Days{number(j.at("offer_response_days"))};p.offer_term=core::Days{number(j.at("offer_term_days"))};p.pilot_tasks=number(j.at("pilot_tasks"));
    if (sum!=10000 || p.review_interval.value()<7 || p.review_interval.value()>90 || p.warning_lead.value()<90 || p.warning_lead.value()>365 || p.delivery_lead.value()<7 || p.delivery_lead.value()>180 || p.cooldown.value()<2*p.review_interval.value() || p.cooldown.value()>1096 ||
        p.exit_pressure>=p.entry_pressure || p.entry_pressure.value()==0 || p.revenue_at_risk.value()==0 || p.revenue_at_risk.value()>1'000'000'000'000 || p.publication_fee.value()>100'000'000 || p.pilot_fee.value()>100'000'000 ||
        p.offer_response.value()<7 || p.offer_response.value()>60 || p.offer_term.value()<30 || p.offer_term.value()>365 || p.pilot_tasks==0 || p.pilot_tasks>365'000'000) invalid();
    return p;
}
std::string canonical_heavyweight_policy(const HeavyweightPolicy& p) {
    return Json{{"review_days",p.review_interval.value()},{"warning_days",p.warning_lead.value()},{"delivery_days",p.delivery_lead.value()},{"cooldown_days",p.cooldown.value()},
        {"entry_pressure_bp",p.entry_pressure.value()},{"exit_pressure_bp",p.exit_pressure.value()},{"weights",p.weights},{"revenue_at_risk_cents",p.revenue_at_risk.value()},{"capability_target",p.capability_target},{"financed_target",p.financed_target},
        {"specialist_id",p.specialist_id},{"lease_offer_id",p.lease_offer_id},{"publication_node",p.publication_node},{"pilot_product",p.pilot_product},{"publication_fee_cents",p.publication_fee.value()},{"pilot_fee_cents",p.pilot_fee.value()},
        {"offer_response_days",p.offer_response.value()},{"offer_term_days",p.offer_term.value()},{"pilot_tasks",p.pilot_tasks}}.dump();
}
}
