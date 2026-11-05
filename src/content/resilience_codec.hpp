#pragma once
#include <pgp/core/resilience.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace pgp::content::resilience_codec {
using Json=nlohmann::json;
[[noreturn]] inline void invalid() {throw std::invalid_argument("Site resilience has missing, unsupported or invalid typed fields");}
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type {};
template<class T,class F> void visit(T& row,F&& f) {
    using V=std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V,core::ResilienceCatalog>) {
        f("assumptions",row.assumptions);f("hazards",row.hazards);f("exposures",row.exposures);f("protections",row.protections);
        f("insurance",row.insurance);f("replication",row.replication);f("insurer_capital_cents",row.insurer_capital);
    } else if constexpr (std::is_same_v<V,core::HazardRule>) {
        f("kind",row.kind);f("physical_days",row.physical_days);f("restoration_days",row.restoration_days);
        f("remaining_capacity_bp",row.remaining_capacity);f("repair_cost_cents",row.repair_cost);
    } else if constexpr (std::is_same_v<V,core::SiteExposure>) {f("state",row.state_id);f("monthly_probability_bp",row.monthly_probability);}
    else if constexpr (std::is_same_v<V,core::SiteProtectionOffer>) {
        f("id",row.id);f("name",row.name);f("kind",row.kind);f("setup_cents",row.setup);f("monthly_cents",row.monthly);f("lead_days",row.lead);
        f("factor_bp",row.factor);f("emergency_watts",row.emergency_watts);f("emergency_days",row.emergency_days);
        f("power_millicents_per_kwh",row.power_millicents_per_kwh);f("power_grams_per_kwh",row.power_grams_per_kwh);
    } else if constexpr (std::is_same_v<V,core::SiteInsuranceOffer>) {
        f("id",row.id);f("name",row.name);f("premium_cents",row.premium);f("deductible_cents",row.deductible);f("limit_cents",row.limit);
        f("coverage_bp",row.coverage);f("term_days",row.term);f("waiting_days",row.waiting);f("claim_delay_days",row.claim_delay);f("covered",row.covered);
    } else if constexpr (std::is_same_v<V,core::ReplicationTerms>) {
        f("setup_cents",row.setup);f("monthly_cents",row.monthly);f("synchronization_milli_CU_per_day",row.synchronization);
        f("initial_copy_milli_CU",row.initial_copy);f("staff",row.staff);
    } else if constexpr (std::is_same_v<V,core::ResilienceState>) {
        f("catalog",row.catalog);f("insurer_cash_cents",row.insurer_cash);f("protections",row.protections);f("policies",row.policies);
        f("samples",row.samples);f("incidents",row.incidents);f("replicas",row.replicas);f("alerts",row.alerts);
        f("open_incidents",row.open_incidents);f("prepared_on",row.prepared_on);f("alert_cursor",row.alert_cursor);
    } else if constexpr (std::is_same_v<V,core::SiteProtection>) {
        f("id",row.id);f("site",row.site_id);f("offer",row.offer_id);f("purchased_on",row.purchased_on);f("active_from",row.active_from);
    } else if constexpr (std::is_same_v<V,core::SiteInsurance>) {
        f("id",row.id);f("site",row.site_id);f("offer",row.offer_id);f("purchased_on",row.purchased_on);f("covers_from",row.covers_from);f("ends_before",row.ends_before);
    } else if constexpr (std::is_same_v<V,core::SiteHazardSample>) {
        f("site",row.site_id);f("sampled_on",row.sampled_on);f("event_on",row.event_on);f("stream_before",row.stream_before);f("stream_after",row.stream_after);f("outcome",row.outcome);
    } else if constexpr (std::is_same_v<V,core::SiteIncident>) {
        f("id",row.id);f("site",row.site_id);f("sample_index",row.sample_index);f("kind",row.kind);f("starts_on",row.starts_on);f("repair_due",row.repair_due);
        f("repaired_on",row.repaired_on);f("restoration_days",row.restoration_days);f("remaining_capacity_bp",row.remaining_capacity);f("repair_cost_cents",row.repair_cost);
        f("policy",row.policy_id);f("claim_cents",row.claim);f("claim_due",row.claim_due);f("paid_on",row.paid_on);
        f("recovery_reported",row.recovery_reported);
    } else if constexpr (std::is_same_v<V,core::ServiceReplica>) {
        f("id",row.id);f("service",row.service_id);f("primary_source",row.primary_source);f("backup_source",row.backup_source);
        f("sync_project",row.sync_project);f("model",row.model_id);f("evidence",row.evidence_id);f("human_review",row.human_review);
        f("starts_on",row.starts_on);f("ready_on",row.ready_on);f("stopped_on",row.stopped_on);f("reservation_milli_CU_per_day",row.reservation);
        f("initial_deployment",row.initial_deployment);f("last_deployment",row.last_deployment);
    } else if constexpr (std::is_same_v<V,core::SiteAlert>) {
        f("id",row.id);f("source",row.source_id);f("cause",row.cause);f("day",row.day);f("required",row.required);f("acknowledged",row.acknowledged);
    }
}
template<class T> constexpr std::size_t record_limit() {
    if constexpr (std::is_same_v<T,core::SiteHazardSample>) return core::resilience_sample_limit;
    else if constexpr (std::is_same_v<T,core::SiteAlert>) return core::resilience_alert_limit;
    else if constexpr (std::is_same_v<T,core::SiteIncident>) return core::resilience_incident_limit;
    else if constexpr (std::is_same_v<T,core::SiteInsurance>) return core::resilience_policy_limit;
    else if constexpr (std::is_same_v<T,core::ServiceReplica>) return core::resilience_replica_limit;
    else if constexpr (std::is_same_v<T,core::SiteProtection>) return core::resilience_protection_limit;
    else if constexpr (std::is_same_v<T,std::uint64_t>) return core::resilience_incident_limit;
    else return 64;
}
template<class T> T decode(const Json& j) {
    if constexpr (std::is_same_v<T,std::string>) {
        if (!j.is_string() || j.get_ref<const std::string&>().size()>4096) invalid();
        return j.get<std::string>();
    } else if constexpr (std::is_same_v<T,bool>) {if (!j.is_boolean()) invalid();return j.get<bool>();}
    else if constexpr (std::is_same_v<T,core::SiteHazard>) return core::parse_hazard(decode<std::string>(j));
    else if constexpr (std::is_same_v<T,core::SiteProtectionKind>) return core::parse_protection(decode<std::string>(j));
    else if constexpr (std::is_integral_v<T>) {
        if (!j.is_number_integer() || (!j.is_number_unsigned() && j.get<std::int64_t>()<0)) invalid();
        const auto value=j.get<std::uint64_t>();if (value>std::numeric_limits<T>::max()) invalid();
        return static_cast<T>(value);
    } else if constexpr (Optional<T>::value) {if (j.is_null()) return {};return decode<typename T::value_type>(j);}
    else if constexpr (requires(T v){v.value();}) return T{decode<std::int64_t>(j)};
    else if constexpr (requires {std::tuple_size<T>::value;}) {
        if (!j.is_array() || j.size()!=std::tuple_size<T>::value) invalid();
        T row{};for (std::size_t i=0;i<row.size();++i) row[i]=decode<typename T::value_type>(j.at(i));return row;
    } else if constexpr (requires(T v){v.push_back(typename T::value_type{});}) {
        if (!j.is_array() || j.size()>record_limit<typename T::value_type>()) invalid();
        T rows;for (const auto& item:j) rows.push_back(decode<typename T::value_type>(item));return rows;
    } else {
        if (!j.is_object()) invalid();
        T row;std::size_t count=0;
        visit(row,[&](const char* name,auto& value){if (!j.contains(name)) invalid();value=decode<std::remove_cvref_t<decltype(value)>>(j.at(name));++count;});
        if (count!=j.size()) invalid();return row;
    }
}
template<class T> Json encode(const T& value) {
    if constexpr (std::is_same_v<T,std::string> || std::is_integral_v<T>) return value;
    else if constexpr (std::is_same_v<T,core::SiteHazard>) return core::hazard_key(value);
    else if constexpr (std::is_same_v<T,core::SiteProtectionKind>) return core::protection_key(value);
    else if constexpr (Optional<T>::value) return value?encode(*value):Json(nullptr);
    else if constexpr (requires {value.value();}) return value.value();
    else if constexpr (requires {value.begin();value.end();}) {auto result=Json::array();for (const auto& row:value) result.push_back(encode(row));return result;}
    else {auto result=Json::object();visit(value,[&](const char* name,const auto& field){result[name]=encode(field);});return result;}
}
}
