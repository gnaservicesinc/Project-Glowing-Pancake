#include <pgp/core/authority.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
std::string_view authority_key(AuthorityKind kind) {
    constexpr std::array<std::string_view,4> names{"standards","infrastructure","delegation","coercive"};
    return names.at(static_cast<std::size_t>(kind));
}
std::string_view authority_key(PublicPromiseKind kind) {
    constexpr std::array<std::string_view,2> names{"publication","public_work"};
    return names.at(static_cast<std::size_t>(kind));
}
std::string_view authority_key(PublicCaseKind kind) {
    constexpr std::array<std::string_view,3> names{"promise","delivery","resistance"};
    return names.at(static_cast<std::size_t>(kind));
}
void validate_authority_catalog(const AuthorityCatalog& c) {
    const auto invalid=[] {throw std::invalid_argument("Institutional terms need eight named issuers, four funded roles and bounded response/authority rules.");};
    if(c.assumptions.empty() || c.assumptions.size()>2048 || c.regions.size()!=society_regions || c.offers.size()!=4 ||
        c.promise_window.value()<30 || c.promise_window.value()>180 || c.concern_period.value()<3 || c.concern_period.value()>30 ||
        c.response_window<=c.concern_period || c.response_window.value()>90 || c.extension.value()<7 || c.extension.value()>90 ||
        c.sanctions.value()<1 || c.sanctions.value()>90 || c.identity_memory.value()<90 || c.identity_memory.value()>3650 ||
        c.reapplication_delay.value()<30 || c.reapplication_delay.value()>365 || c.directive_spacing.value()<1 || c.directive_spacing.value()>30 ||
        c.remedy_fee.value()<1 || c.remedy_fee.value()>100000000 || c.remedy_compute.value()<1000 || c.remedy_compute.value()>100000 ||
        c.remedy_staff<1 || c.remedy_staff>8 || c.promise_tasks<100 || c.promise_tasks>1000000 ||
        c.minimum_operation.value()<5000 || c.procurement_floor.value()<5000 || c.initial_resistance.value()<1000 ||
        c.resistance_growth.value()<100 || c.resistance_growth.value()>1500 || c.resistance_decay.value()<100 ||
        c.resistance_decay.value()>1000 || c.remedy_relief.value()>2000 || c.resistance_threshold.value()<3000 ||
        c.resistance_threshold.value()>8000) invalid();
    constexpr std::array<std::string_view,8> ids{"region.africa_middle_east","region.asia_pacific","region.canada","region.china","region.europe","region.india","region.latin_america","region.us"};
    for(std::size_t i=0;i<ids.size();++i) if(c.regions[i].id!=ids[i] || c.regions[i].issuer.empty() || c.regions[i].issuer.size()>120) invalid();
    std::set<std::string> offers;std::set<AuthorityKind> kinds;
    for(const auto& o:c.offers) {
        (void)authority_key(o.kind);
        if(o.id.empty() || o.id.size()>96 || !offers.insert(o.id).second || !kinds.insert(o.kind).second ||
            o.name.empty() || o.name.size()>120 || o.rights.empty() || o.rights.size()>512 ||
            o.setup.value()<10000 || o.setup.value()>1000000000 || o.monthly.value()<10000 || o.monthly.value()>100000000 ||
            o.compute.value()<1000 || o.compute.value()>100000 || o.staff<1 || o.staff>8 ||
            o.lead.value()<7 || o.lead.value()>authority_maximum_lead_days || o.term.value()<90 || o.term.value()>authority_maximum_term_days ||
            o.public_work<100 || o.public_work>1000000 || o.points.value()<1000 ||
            (o.kind!=AuthorityKind::coercive && o.points.value()>5000) || o.evidence_floor.value()<5000) invalid();
    }
    for(const auto& effects:c.identity_effects) for(auto value:effects) if(value<-2000 || value>2000) invalid();
}
const AuthorityOffer& authority_offer(const AuthorityCatalog& c,std::string_view id) {
    const auto it=std::find_if(c.offers.begin(),c.offers.end(),[&](const auto& row){return row.id==id;});
    if(it==c.offers.end()) throw std::invalid_argument("Choose an available institutional role.");return *it;
}
std::string_view public_case_phase(const PublicCase& c,OrdinalDay day,const AuthorityCatalog& terms) {
    if(c.resolved_on && *c.resolved_on<=day) return "resolved";
    if(c.disclosed_on && *c.disclosed_on<=day) return "disclosed";
    if(c.negotiated_on && *c.negotiated_on<=day) return "negotiation";
    return day>=advance_days(c.opened_on,terms.concern_period)?"recommendation":"concern";
}
}
