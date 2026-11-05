#pragma once
#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>

namespace pgp::core {
struct SimulationState;
enum class SiteHazard { grid, heat, storm, flood, fire, earthquake };
inline constexpr std::size_t site_hazard_count=6;
[[nodiscard]] std::string_view hazard_key(SiteHazard);
[[nodiscard]] SiteHazard parse_hazard(std::string_view);
enum class SiteProtectionKind { generator, backup, maintenance, hardening, cooling };
[[nodiscard]] std::string_view protection_key(SiteProtectionKind);
[[nodiscard]] SiteProtectionKind parse_protection(std::string_view);
struct HazardRule {
    SiteHazard kind=SiteHazard::grid;
    Days physical_days{0},restoration_days{0};
    BasisPoints remaining_capacity{0};
    Cents repair_cost{0};
    auto operator<=>(const HazardRule&) const = default;
};
struct SiteExposure {
    std::string state_id;
    std::array<std::array<std::uint32_t,12>,site_hazard_count> monthly_probability{};
    auto operator<=>(const SiteExposure&) const = default;
};
struct SiteProtectionOffer {
    std::string id,name;
    SiteProtectionKind kind=SiteProtectionKind::backup;
    Cents setup{0},monthly{0};
    Days lead{1};
    BasisPoints factor{10000};
    std::uint64_t emergency_watts=0;
    Days emergency_days{0};
    std::uint32_t power_millicents_per_kwh=0,power_grams_per_kwh=0;
    auto operator<=>(const SiteProtectionOffer&) const = default;
};
struct SiteInsuranceOffer {
    std::string id,name;
    Cents premium{0},deductible{0},limit{0};
    BasisPoints coverage{0};
    Days term{0},waiting{1},claim_delay{1};
    std::vector<SiteHazard> covered;
    auto operator<=>(const SiteInsuranceOffer&) const = default;
};
struct ReplicationTerms {
    Cents setup{0},monthly{0};
    MilliCUPerDay synchronization{0};
    MilliCU initial_copy{0};
    std::uint32_t staff=1;
    auto operator<=>(const ReplicationTerms&) const = default;
};
struct ResilienceCatalog {
    std::string assumptions;
    std::vector<HazardRule> hazards;
    std::vector<SiteExposure> exposures;
    std::vector<SiteProtectionOffer> protections;
    std::vector<SiteInsuranceOffer> insurance;
    ReplicationTerms replication;
    Cents insurer_capital{0};
    auto operator<=>(const ResilienceCatalog&) const = default;
};
struct SiteProtection {
    std::string id,site_id,offer_id;
    OrdinalDay purchased_on{0},active_from{0};
    auto operator<=>(const SiteProtection&) const = default;
};
struct SiteInsurance {
    std::string id,site_id,offer_id;
    OrdinalDay purchased_on{0},covers_from{0},ends_before{0};
    auto operator<=>(const SiteInsurance&) const = default;
};
struct SiteHazardSample {
    std::string site_id;
    OrdinalDay sampled_on{0},event_on{0};
    std::uint64_t stream_before=0,stream_after=0;
    // Zero is the quiet outcome. Other values are hazard ordinal plus one.
    std::uint32_t outcome=0;
    auto operator<=>(const SiteHazardSample&) const = default;
};
struct SiteIncident {
    std::string id,site_id;
    std::uint64_t sample_index=0;
    SiteHazard kind=SiteHazard::grid;
    OrdinalDay starts_on{0},repair_due{0};
    std::optional<OrdinalDay> repaired_on;
    Days restoration_days{0};
    BasisPoints remaining_capacity{0};
    Cents repair_cost{0};
    std::string policy_id;
    Cents claim{0};
    OrdinalDay claim_due{0};
    std::optional<OrdinalDay> paid_on;
    bool recovery_reported=false;
    auto operator<=>(const SiteIncident&) const = default;
};
struct ServiceReplica {
    std::string id,service_id,primary_source,backup_source,sync_project,model_id,evidence_id;
    bool human_review=false;
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> ready_on,stopped_on;
    MilliCUPerDay reservation{0};
    std::uint64_t initial_deployment=0,last_deployment=0;
    auto operator<=>(const ServiceReplica&) const = default;
};
struct SiteAlert {
    std::string id,source_id,cause;
    OrdinalDay day{0};
    bool required=false,acknowledged=false;
    auto operator<=>(const SiteAlert&) const = default;
};
struct ResilienceState {
    ResilienceCatalog catalog;
    Cents insurer_cash{0};
    std::vector<SiteProtection> protections;
    std::vector<SiteInsurance> policies;
    std::vector<SiteHazardSample> samples;
    std::vector<SiteIncident> incidents;
    std::vector<ServiceReplica> replicas;
    std::vector<SiteAlert> alerts;
    std::vector<std::uint64_t> open_incidents;
    std::optional<OrdinalDay> prepared_on;
    std::size_t alert_cursor=0;
    auto operator<=>(const ResilienceState&) const = default;
};
inline constexpr std::size_t resilience_protection_limit=250,resilience_policy_limit=8192,
    resilience_sample_limit=300000,resilience_incident_limit=8192,resilience_replica_limit=1024,resilience_alert_limit=32768;
struct BuySiteProtection {std::string site_id,offer_id;};
struct InsureSite {std::string site_id,offer_id;};
struct ReplicateService {std::string service_id,backup_source;};
struct StopServiceReplica {std::string id;};
struct AcknowledgeSiteAlert {std::string id;};
struct SiteDisruption {
    BasisPoints remaining{10000};
    std::optional<std::uint64_t> emergency_watts;
    std::uint32_t power_millicents_per_kwh=0,power_grams_per_kwh=0;
};
void validate_resilience_catalog(const ResilienceCatalog&);
void validate_resilience(const SimulationState&,bool deep=true);
[[nodiscard]] SiteDisruption site_disruption(const SimulationState&,std::string_view site,OrdinalDay);
[[nodiscard]] const SiteAlert* required_site_alert(const SimulationState&);
[[nodiscard]] Cents insurer_available(const SimulationState&);
void buy_site_protection(SimulationState&,const BuySiteProtection&,Cents protected_cash);
void insure_site(SimulationState&,const InsureSite&,Cents protected_cash);
void replicate_service(SimulationState&,const ReplicateService&,Cents protected_cash);
void stop_service_replica(SimulationState&,std::string_view);
void sample_site_hazards(SimulationState&);
void prepare_site_resilience(SimulationState&);
void prepare_service_replicas(SimulationState&);
}
