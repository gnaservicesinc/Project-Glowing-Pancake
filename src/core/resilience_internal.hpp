#pragma once
#include "infrastructure_internal.hpp"
#include <pgp/core/resilience.hpp>
namespace pgp::core::resilience_detail {
using infrastructure_detail::find;
using infrastructure_detail::invalid;
using infrastructure_detail::site;
using infrastructure_detail::before_end;
inline ResilienceState& risks(SimulationState& s) {
    if (!s.resilience || !s.infrastructure || !s.capacity || !s.accounts)
        invalid("This scenario has no site resilience rules");
    return *s.resilience;
}
inline const ResilienceState& risks(const SimulationState& s) {return risks(const_cast<SimulationState&>(s));}
inline const SiteProtectionOffer& protection_terms(const ResilienceState& r,const SiteProtection& p) {return find(r.catalog.protections,p.offer_id);}
inline const SiteProtectionOffer* protection(const SimulationState& s,std::string_view site_id,SiteProtectionKind kind,OrdinalDay day) {
    if (!s.resilience) return nullptr;
    for (const auto& row:s.resilience->protections) {
        const auto& offer=protection_terms(*s.resilience,row);
        if (row.site_id==site_id && row.active_from<=day && offer.kind==kind) return &offer;
    }
    return nullptr;
}
inline const HazardRule& hazard(const ResilienceCatalog& c,SiteHazard kind) {
    const auto row=std::find_if(c.hazards.begin(),c.hazards.end(),[&](const auto& rule){return rule.kind==kind;});
    if (row==c.hazards.end()) invalid("A site hazard has no authored outcome");
    return *row;
}
inline std::string repair_invoice(std::string_view id) {return "resilience.repair/"+std::string(id);}
inline OrdinalDay recovered_on(const SiteIncident& incident) {return advance_days(*incident.repaired_on,incident.restoration_days);}
[[nodiscard]] const CapacitySource& capacity_source(const SimulationState&,std::string_view);
[[nodiscard]] std::string site_for_source(const SimulationState&,std::string_view);
[[nodiscard]] SiteHazardSample draw_sample(const SimulationState&,std::string_view,OrdinalDay,std::uint64_t before);
[[nodiscard]] SiteIncident incident_terms(const SimulationState&,const SiteHazardSample&,std::size_t index);
[[nodiscard]] std::string incident_description(const SimulationState&,const SiteIncident&);
[[nodiscard]] std::string replica_description(const SimulationState&,const ServiceReplica&,std::string_view destination);
void alert(SimulationState&,std::string id,std::string_view site,OrdinalDay,std::string cause,bool required=false);
void spend(SimulationState&,Cents,Cents protected_cash,std::string key,std::string cause);
[[nodiscard]] MilliCUPerDay free_capacity(const SimulationState&,const CapacitySource&);
void refresh_replica_reservations(SimulationState&);
}
