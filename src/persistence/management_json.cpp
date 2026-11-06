#include "management_json.hpp"
#include "governance_json.hpp"
#include <pgp/content/management_catalog.hpp>
#include <algorithm>
#include <array>
namespace pgp::persistence::detail {
namespace {
std::string label(const Json& value,bool empty=false) {return text(value,"management/text",1024,empty);}
core::OrdinalDay day(const Json& value) {return core::OrdinalDay{i64(value,"management/day")};}
core::Cents cents(const Json& value) {return core::Cents{i64(value,"management/cents")};}
constexpr std::array policy_kinds{
    std::string_view{"evaluation"},std::string_view{"lease_renewal"},std::string_view{"pricing"},
    std::string_view{"operating_reserve"},std::string_view{"hardware_replacement"}};
core::DelegationKind policy_kind(const Json& value) {
    const auto name=label(value);
    const auto it=std::ranges::find(policy_kinds,name);
    if (it==policy_kinds.end()) invalid("management/kind","Unsupported policy kind");
    return static_cast<core::DelegationKind>(it-policy_kinds.begin());
}
}
Json management_json(const core::ManagementState& m) {
    Json financing=Json::array(),specialists=Json::array(),policies=Json::array(),decisions=Json::array(),notices=Json::array();
    for (const auto& row:m.financing) financing.push_back({{"offer",row.offer_id},{"day",row.day.value()}});
    for (const auto& row:m.specialists) specialists.push_back({{"offer",row.offer_id},{"day",row.day.value()}});
    for (const auto& p:m.policies) {
        Json row{{"id",p.id},{"target",p.target_id},{"plan",p.plan_id},{"kind",policy_kinds.at(static_cast<std::size_t>(p.kind))},
        {"source",p.allocation.source_id},{"daily_compute",p.allocation.daily_compute.value()},{"staff",p.allocation.staff},
        {"budget",p.budget.value()},{"reserve",p.reserve_floor.value()},{"monthly_ceiling",p.monthly_ceiling.value()},{"interval",p.interval.value()},{"maximum_runs",p.maximum_runs},
        {"enabled",p.enabled},{"created_on",p.created_on.value()},{"next_review",p.next_review.value()},{"runs",p.runs},{"committed",p.committed.value()},{"blocked",p.blocked}};
        if (p.pricing) row["pricing"]={{"floor",p.pricing->floor.value()},{"ceiling",p.pricing->ceiling.value()},{"step",p.pricing->step.value()},
            {"low",p.pricing->low_utilization.value()},{"high",p.pricing->high_utilization.value()}};
        policies.push_back(std::move(row));
    }
    for (const auto& d:m.decisions) {
        Json row{{"policy",d.policy_id},{"result",d.result_id},{"explanation",d.explanation},{"day",d.day.value()},{"commitment",d.commitment.value()},{"accepted",d.accepted}};
        if (d.price_event_cursor) row["price_event_cursor"]=*d.price_event_cursor;
        if (d.overridden) row["overridden"]=true;
        decisions.push_back(std::move(row));
    }
    for (const auto& n:m.notices) notices.push_back({{"id",n.id},{"kind",n.kind},{"subject",n.subject},{"day",n.day.value()},
        {"deadline",n.deadline?Json(n.deadline->value()):Json(nullptr)},{"required",n.required},{"acknowledged",n.acknowledged}});
    Json result{{"catalog",Json::parse(content::canonical_management_catalog(m.catalog))},{"financing",std::move(financing)},{"specialists",std::move(specialists)},
        {"policies",std::move(policies)},{"decisions",std::move(decisions)},{"notices",std::move(notices)},{"reviewed_on",m.reviewed_on?Json(m.reviewed_on->value()):Json(nullptr)}};
    if (m.governance) result["governance"]=governance_json(*m.governance);
    return result;
}
core::ManagementState parse_management(const Json& source) {
    auto j=source;std::optional<core::GovernanceState> governance;
    if (j.contains("governance")) {governance=parse_governance(j.at("governance"));j.erase("governance");}
    fields(j,"management",{"catalog","financing","specialists","policies","decisions","notices","reviewed_on"});
    core::ManagementState m;m.governance=std::move(governance);m.catalog=content::load_management_catalog(j.at("catalog").dump());
    if (!j.at("reviewed_on").is_null()) m.reviewed_on=day(j.at("reviewed_on"));
    array(j.at("financing"),"management/financing",8);array(j.at("specialists"),"management/specialists",8);
    array(j.at("policies"),"management/policies",core::management_policy_limit);array(j.at("decisions"),"management/decisions",core::management_decision_limit);array(j.at("notices"),"management/notices",256);
    for (const auto& r:j.at("financing")) {fields(r,"management/acceptance",{"offer","day"});m.financing.push_back({label(r.at("offer")),day(r.at("day"))});}
    for (const auto& r:j.at("specialists")) {fields(r,"management/acceptance",{"offer","day"});m.specialists.push_back({label(r.at("offer")),day(r.at("day"))});}
    for (const auto& p:j.at("policies")) {
        fields(p,"management/policy",{"id","target","plan","kind","source","daily_compute","staff","budget","reserve","monthly_ceiling","interval","maximum_runs","enabled","created_on","next_review","runs","committed","blocked"},{"pricing"});
        core::DelegationPolicy value;value.id=label(p.at("id"));value.target_id=label(p.at("target"),true);value.plan_id=label(p.at("plan"),true);
        value.kind=policy_kind(p.at("kind"));
        if (p.contains("pricing")) {
            const auto& b=p.at("pricing");fields(b,"management/pricing",{"floor","ceiling","step","low","high"});
            value.pricing=core::PricingBounds{cents(b.at("floor")),cents(b.at("ceiling")),cents(b.at("step")),
                core::BasisPoints{u64(b.at("low"),"management/pricing/low",10000)},core::BasisPoints{u64(b.at("high"),"management/pricing/high",10000)}};
        }
        value.allocation={label(p.at("source"),true),core::MilliCUPerDay{i64(p.at("daily_compute"),"management/rate")},static_cast<std::uint32_t>(u64(p.at("staff"),"management/staff",1'000'000))};
        value.budget=cents(p.at("budget"));value.reserve_floor=cents(p.at("reserve"));value.monthly_ceiling=cents(p.at("monthly_ceiling"));value.interval=core::Days{i64(p.at("interval"),"management/interval")};
        const auto limit=value.kind==core::DelegationKind::pricing?core::pricing_review_limit:12;
        value.maximum_runs=static_cast<std::uint32_t>(u64(p.at("maximum_runs"),"management/max_runs",limit));value.enabled=boolean(p.at("enabled"),"management/enabled");
        value.created_on=day(p.at("created_on"));value.next_review=day(p.at("next_review"));value.runs=static_cast<std::uint32_t>(u64(p.at("runs"),"management/runs",limit));
        value.committed=cents(p.at("committed"));value.blocked=label(p.at("blocked"),true);m.policies.push_back(std::move(value));
    }
    for (const auto& d:j.at("decisions")) {
        fields(d,"management/decision",{"policy","result","explanation","day","commitment","accepted"},{"price_event_cursor","overridden"});
        core::DelegationDecision decision{label(d.at("policy")),label(d.at("result"),true),label(d.at("explanation")),day(d.at("day")),cents(d.at("commitment")),boolean(d.at("accepted"),"management/accepted")};
        if (d.contains("price_event_cursor")) decision.price_event_cursor=u64(d.at("price_event_cursor"),"management/price_event_cursor",core::market_price_history_limit);
        if (d.contains("overridden")) decision.overridden=boolean(d.at("overridden"),"management/overridden");
        m.decisions.push_back(std::move(decision));
    }
    for (const auto& n:j.at("notices")) {
        fields(n,"management/notice",{"id","kind","subject","day","deadline","required","acknowledged"});
        m.notices.push_back({label(n.at("id")),label(n.at("kind")),label(n.at("subject")),day(n.at("day")),n.at("deadline").is_null()?std::nullopt:std::optional{day(n.at("deadline"))},
            boolean(n.at("required"),"management/required"),boolean(n.at("acknowledged"),"management/acknowledged")});
    }
    return m;
}
}
