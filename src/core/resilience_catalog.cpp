#include "resilience_internal.hpp"
#include <set>
namespace pgp::core {
using namespace resilience_detail;
std::string_view hazard_key(SiteHazard kind) {
    static constexpr std::array<std::string_view,site_hazard_count> keys{"grid","heat","storm","flood","fire","earthquake"};
    const auto index=static_cast<std::size_t>(kind);
    if (index>=keys.size()) invalid("Unknown site hazard");
    return keys[index];
}
SiteHazard parse_hazard(std::string_view key) {
    for (std::size_t i=0;i<site_hazard_count;++i) if (hazard_key(static_cast<SiteHazard>(i))==key) return static_cast<SiteHazard>(i);
    invalid("Unknown site hazard");
}
std::string_view protection_key(SiteProtectionKind kind) {
    static constexpr std::array<std::string_view,5> keys{"generator","backup","maintenance","hardening","cooling"};
    const auto index=static_cast<std::size_t>(kind);
    if (index>=keys.size()) invalid("Unknown site protection");
    return keys[index];
}
SiteProtectionKind parse_protection(std::string_view key) {
    for (std::size_t i=0;i<5;++i) if (protection_key(static_cast<SiteProtectionKind>(i))==key) return static_cast<SiteProtectionKind>(i);
    invalid("Unknown site protection");
}
void validate_resilience_catalog(const ResilienceCatalog& c) {
    const auto money=[](Cents value){return value.value()>=0 && value.value()<=100'000'000'000LL;};
    if (c.assumptions.empty() || c.assumptions.size()>2048 || c.hazards.size()!=site_hazard_count ||
        c.exposures.size()!=51 || c.protections.size()!=5 || c.insurance.empty() || c.insurance.size()>8 ||
        !money(c.insurer_capital) || c.insurer_capital.value()==0 ||
        !money(c.replication.setup) || !money(c.replication.monthly) || c.replication.synchronization.value()==0 ||
        c.replication.synchronization.value()>1'000'000 || c.replication.initial_copy.value()==0 ||
        c.replication.initial_copy.value()>1'000'000'000 || c.replication.staff==0 || c.replication.staff>100)
        invalid("Resilience catalogue exceeds its finite budgets or coverage");
    std::set<SiteHazard> kinds;
    for (const auto& row:c.hazards) {
        (void)hazard_key(row.kind);
        if (!kinds.insert(row.kind).second || row.physical_days.value()==0 || row.physical_days.value()>60 ||
            row.restoration_days.value()>30 || !money(row.repair_cost) ||
            (row.kind==SiteHazard::grid && (row.remaining_capacity.value()!=0 || row.restoration_days.value()!=0 || row.repair_cost.value()!=0)))
            invalid("Site hazard needs a unique bounded physical outcome");
    }
    std::set<std::string> states;
    for (const auto& row:c.exposures) {
        if (!states.insert(row.state_id).second || row.state_id.size()!=5 || !row.state_id.starts_with("US-"))
            invalid("Site exposure requires each domestic state exactly once");
        for (std::size_t m=0;m<12;++m) {
            std::uint64_t total=0;
            for (const auto& series:row.monthly_probability) {if (series[m]>9999) invalid("Monthly site probabilities must be basis points");total+=series[m];}
            if (total>=10000) invalid("Every month must retain a possible quiet outcome");
        }
    }
    std::set<std::string> ids;
    std::set<SiteProtectionKind> protections;
    for (const auto& row:c.protections) {
        (void)protection_key(row.kind);
        if (!infrastructure_detail::identifier(row.id) || !ids.insert(row.id).second || !protections.insert(row.kind).second ||
            row.name.empty() || row.name.size()>100 || !money(row.setup) || !money(row.monthly) ||
            row.lead.value()==0 || row.lead.value()>60)
            invalid("Site protections need unique mechanisms and valid paid installation terms");
        if (row.kind==SiteProtectionKind::generator) {
            if (row.emergency_watts<1000 || row.emergency_watts>1'000'000 || row.emergency_days.value()==0 || row.emergency_days.value()>14 ||
                row.power_millicents_per_kwh==0 || row.power_millicents_per_kwh>1'000'000 || row.power_grams_per_kwh>2000)
                invalid("Emergency generation needs finite power, endurance and fuel terms");
        } else if (row.emergency_watts!=0 || row.emergency_days.value()!=0 || row.power_millicents_per_kwh!=0 || row.power_grams_per_kwh!=0)
            invalid("Only backup generation can supply emergency power");
    }
    for (const auto& row:c.insurance) {
        if (!infrastructure_detail::identifier(row.id) || !ids.insert(row.id).second || row.name.empty() || row.name.size()>100 ||
            !money(row.premium) || row.premium.value()==0 || !money(row.deductible) || !money(row.limit) || row.limit.value()==0 ||
            row.coverage.value()==0 || row.term.value()==0 || row.term.value()>366 || row.waiting.value()==0 || row.waiting>=row.term ||
            row.claim_delay.value()==0 || row.claim_delay.value()>60 || row.covered.empty() || row.covered.size()>site_hazard_count)
            invalid("Insurance needs funded limits, a waiting period and explicit covered hazards");
        std::set<SiteHazard> covered;
        for (const auto kind:row.covered) {(void)hazard_key(kind);if (!covered.insert(kind).second) invalid("Repeated insured hazard");}
    }
}
}
