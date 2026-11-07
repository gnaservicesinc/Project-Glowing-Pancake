#include <pgp/content/management_catalog.hpp>
#include <pgp/content/governance_catalog.hpp>
#include <pgp/content/succession_catalog.hpp>
#include "corporate_tax_codec.hpp"
#include "staff_workload_codec.hpp"
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
    auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>16 || ++tokens>8192) invalid();
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid();
        return true;
    });
    std::optional<core::GovernanceCatalog> governance;
    std::optional<core::SuccessionCatalog> succession;
    if(j.contains("succession")) {succession=load_succession_catalog(j.at("succession").dump());j.erase("succession");}
    if (j.contains("governance")) {governance=load_governance_catalog(j.at("governance").dump());j.erase("governance");}
    core::ManagementCatalog c;c.governance=std::move(governance);c.succession=std::move(succession);
    if (j.contains("corporate_tax")) {
        c.corporate_tax=civic_codec::decode<core::CorporateTaxPolicy>(j.at("corporate_tax"));
        j.erase("corporate_tax");
    }
    if (j.contains("staff_workload")) {
        c.staff_workload=staff_workload_codec::decode(j.at("staff_workload"));
        j.erase("staff_workload");
    }
    if (j.contains("service_monitoring")) {
        const auto& monitor=j.at("service_monitoring");
        fields(monitor,{"window_days","minimum_attempts","warning_drop_bp"});
        c.service_monitoring=core::ServiceMonitoringRules{core::Days{integer(monitor.at("window_days"))},
            count(monitor.at("minimum_attempts"),static_cast<std::uint32_t>(core::market_task_limit)),
            core::BasisPoints{integer(monitor.at("warning_drop_bp"))}};
        j.erase("service_monitoring");
    }
    if (j.contains("support_offers")) {
        array(j.at("support_offers"),core::funding_offer_limit);
        for (auto row:j.at("support_offers")) {
            core::FundingSupportOffer offer;
            const auto kind=text(row.at("kind"));
            if (kind=="research_grant") {
                offer.kind=core::FundingSupportKind::research_grant;
                array(row.at("research"),8);
                for (const auto& research:row.at("research")) offer.targets.push_back(text(research));
                row.erase("research");
            } else if (kind=="cloud_credit") {
                offer.kind=core::FundingSupportKind::cloud_credit;
                if (row.contains("lease_offer_id")) {
                    offer.targets.push_back("offer.lease/"+text(row.at("lease_offer_id")));
                    row.erase("lease_offer_id");
                } else {
                    const auto bill=text(row.at("bill"));
                    if (bill!="compute_lease" && bill!="founder_compute") invalid();
                    offer.targets.push_back(bill);
                    row.erase("bill");
                }
            } else invalid();
            fields(row,{"id","name","kind","limit_cents","coverage_bp","opens_after_days","claim_days","use_days"});
            offer.id=text(row.at("id"));
            offer.name=text(row.at("name"));
            offer.limit=core::Cents{integer(row.at("limit_cents"))};
            offer.coverage=core::BasisPoints{integer(row.at("coverage_bp"))};
            offer.opens_after=core::Days{integer(row.at("opens_after_days"))};
            offer.claim_days=core::Days{integer(row.at("claim_days"))};
            offer.use_days=core::Days{integer(row.at("use_days"))};
            c.support_offers.push_back(std::move(offer));
        }
        j.erase("support_offers");
    }
    fields(j,{"financing","specialists","setbacks","scoring","minimum_evaluation_interval_days","maximum_policy_runs","default_policy_budget_cents","default_reserve_floor_cents","default_monthly_ceiling_cents","default_policy_runs"});
    array(j.at("financing"),8);array(j.at("specialists"),8);array(j.at("setbacks"),8);
    for (auto row:j.at("financing")) {
        bool announced=false;
        if (row.contains("public_announcement")) {
            if (!row.at("public_announcement").is_boolean()) invalid();announced=row.at("public_announcement").get<bool>();row.erase("public_announcement");
        }
        fields(row,{"id","name","proceeds_cents","acquired_bp","monthly_interest_bp","term_days"});
        c.financing.push_back({text(row.at("id")),text(row.at("name")),core::Cents{integer(row.at("proceeds_cents"))},core::BasisPoints{integer(row.at("acquired_bp"))},
            core::BasisPoints{integer(row.at("monthly_interest_bp"))},core::Days{integer(row.at("term_days"))},announced});
    }
    for (auto row:j.at("specialists")) {
        const auto lead=row.contains("start_lead_days")?integer(row.at("start_lead_days")):0;
        const auto onboarding=row.contains("onboarding_days")?integer(row.at("onboarding_days")):0;
        row.erase("start_lead_days");
        row.erase("onboarding_days");
        fields(row,{"id","name","signing_cost_cents","monthly_salary_cents"});
        c.specialists.push_back({text(row.at("id")),text(row.at("name")),core::Cents{integer(row.at("signing_cost_cents"))},
            core::Cents{integer(row.at("monthly_salary_cents"))},core::Days{lead},core::Days{onboarding}});
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
    for (std::size_t i=0;i<c.financing.size();++i) if (c.financing[i].public_announcement) financing[i]["public_announcement"]=true;
    for (const auto& row:c.specialists) {
        specialists.push_back({{"id",row.id},{"name",row.name},{"signing_cost_cents",row.signing_cost.value()},{"monthly_salary_cents",row.monthly_salary.value()}});
        if (row.start_lead.value()>0) specialists.back()["start_lead_days"]=row.start_lead.value();
        if (row.onboarding.value()>0) specialists.back()["onboarding_days"]=row.onboarding.value();
    }
    for (const auto& row:c.setbacks) setbacks.push_back({{"id",row.id},{"name",row.name},{"source_id",row.source_id},{"announced_on",core::iso_date(row.announced_on)},
        {"starts_on",core::iso_date(row.starts_on)},{"ends_before",core::iso_date(row.ends_before)},{"reduced_milli_CU_per_day",row.reduced_capacity.value()}});
    Json result{{"financing",std::move(financing)},{"specialists",std::move(specialists)},{"setbacks",std::move(setbacks)},
        {"scoring",{{"task_target",c.scoring.task_target},{"weights",c.scoring.weights},{"founder",c.scoring.founder},{"opening_founder_share_bp",c.scoring.opening_founder_share.value()}}},
        {"minimum_evaluation_interval_days",c.minimum_evaluation_interval.value()},{"maximum_policy_runs",c.maximum_policy_runs},{"default_policy_budget_cents",c.default_policy_budget.value()},
        {"default_reserve_floor_cents",c.default_reserve_floor.value()},{"default_monthly_ceiling_cents",c.default_monthly_ceiling.value()},{"default_policy_runs",c.default_policy_runs}};
    if (c.governance) result["governance"]=Json::parse(canonical_governance_catalog(*c.governance));
    if (c.corporate_tax) result["corporate_tax"]=civic_codec::encode(*c.corporate_tax);
    if (c.succession) result["succession"]=Json::parse(canonical_succession_catalog(*c.succession));
    if (c.staff_workload) result["staff_workload"]=staff_workload_codec::encode(*c.staff_workload);
    if (const auto& monitor=c.service_monitoring;monitor) result["service_monitoring"]={
        {"window_days",monitor->window.value()},{"minimum_attempts",monitor->minimum_attempts},
        {"warning_drop_bp",monitor->warning_drop.value()}};
    if (!c.support_offers.empty()) {
        result["support_offers"]=Json::array();
        for (const auto& offer:c.support_offers) {
            const bool grant=offer.kind==core::FundingSupportKind::research_grant;
            Json row{{"id",offer.id},{"name",offer.name},{"kind",grant?"research_grant":"cloud_credit"},
                {"limit_cents",offer.limit.value()},{"coverage_bp",offer.coverage.value()},{"opens_after_days",offer.opens_after.value()},
                {"claim_days",offer.claim_days.value()},{"use_days",offer.use_days.value()}};
            if (grant) row["research"]=offer.targets;
            else if (offer.targets.front().starts_with("offer.lease/"))
                row["lease_offer_id"]=offer.targets.front().substr(std::string_view{"offer.lease/"}.size());
            else row["bill"]=offer.targets.front();
            result["support_offers"].push_back(std::move(row));
        }
    }
    return result.dump();
}
}
