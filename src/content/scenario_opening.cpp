#include <pgp/content/collection.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::content {
namespace {
core::Cents scaled(core::Cents value,std::uint32_t percent) {
    return core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(value.value()),percent,100).quotient};
}
}
const CampaignDifficulty* scenario_difficulty(const ScenarioDefinition& s,std::string_view id) {
    if(id.empty()) return nullptr;
    const auto it=std::find_if(s.difficulties.begin(),s.difficulties.end(),[&](const auto& d){return d.id==id;});
    if(it==s.difficulties.end()) throw std::invalid_argument("This scenario does not offer that difficulty");
    return &*it;
}
std::optional<core::ManagementCatalog> scenario_management(const ScenarioDefinition& s,std::string_view difficulty) {
    auto result=s.management;const auto* d=scenario_difficulty(s,difficulty);
    if(!result || !d) return result;
    for(auto& offer:result->financing) offer.proceeds=scaled(offer.proceeds,d->financing_percent);
    if(result->governance) for(auto& offer:result->governance->financing) offer.proceeds=scaled(offer.proceeds,d->financing_percent);
    core::validate_management_catalog(*result);
    return result;
}
std::optional<CompetitionSetup> scenario_competition(const ScenarioDefinition& s,std::string_view difficulty) {
    auto result=s.competition;const auto* d=scenario_difficulty(s,difficulty);
    if(result && d) result->review_interval=d->rival_review_interval;
    return result;
}
ScenarioOpening scenario_opening(const ScenarioDefinition& s,std::string_view background,std::string_view owner,std::string_view difficulty) {
    const auto* d=scenario_difficulty(s,difficulty);
    ScenarioOpening result{s.cash,s.team_people,s.monthly_commitments,s.business_offers,{}};
    if(s.leased_capacity.value()>0) result.capacity_sources.push_back({"lease.initial","cloud.pool.initial",std::string(owner),
        "supplier.compute.initial",core::CapacitySourceKind::leased,s.leased_capacity,s.leased_capacity,0,"general",s.lease_energy_included});
    if(background.empty()) {if(d) result.cash=scaled(result.cash,d->starting_cash_percent);return result;}
    const auto it=std::find_if(s.backgrounds.begin(),s.backgrounds.end(),[&](const auto& b){return b.id==background;});
    if(it==s.backgrounds.end()) throw std::invalid_argument("This scenario does not offer that founder background");
    const auto& b=*it;
    result.cash=core::checked_unit_add(result.cash,b.cash_adjustment);
    const auto staff=core::checked_add(result.team_people,b.staff_adjustment);
    if(result.cash.value()<0 || staff<1 || staff>1000000) throw std::invalid_argument("Founder package exceeds opening cash or staff bounds");
    result.team_people=static_cast<std::uint32_t>(staff);
    if(b.monthly_payroll_adjustment.value()!=0) {
        const auto base=result.monthly_commitments.find("payroll_with_benefits");
        const auto payroll=core::checked_unit_add(base==result.monthly_commitments.end()?core::Cents{0}:base->second,b.monthly_payroll_adjustment);
        if(payroll.value()<0) throw std::invalid_argument("Founder package makes opening payroll negative");
        result.monthly_commitments.insert_or_assign("payroll_with_benefits",payroll);
    }
    if(b.independent_capacity.value()>0) {
        result.capacity_sources.push_back({"lease.founder","cloud.pool.founder",std::string(owner),"supplier.founder_contact",
            core::CapacitySourceKind::leased,b.independent_capacity,b.independent_capacity,0,"general",true});
        result.monthly_commitments.insert_or_assign("founder_compute",b.monthly_compute_cost);
    }
    for(auto& offer:result.business_offers) if(offer.kind!=core::BusinessOfferKind::compute_lease && offer.available_from==s.starts_on) {
        offer.available_from=core::advance_days(offer.available_from,b.commercial_delay);
        if(offer.available_from>=offer.available_before) throw std::invalid_argument("Founder contact delay consumes an offer window");
    }
    for(const auto& offer:b.contacts) {
        if(offer.available_from<s.starts_on || offer.available_before>s.ends_before) throw std::invalid_argument("Founder contact window is outside the campaign");
        result.business_offers.push_back(offer);
    }
    std::sort(result.business_offers.begin(),result.business_offers.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_business_offer_catalog(result.business_offers);
    (void)core::opening_capacity(result.capacity_sources,result.team_people,s.starts_on,std::string(owner));
    if(d) result.cash=scaled(result.cash,d->starting_cash_percent);
    return result;
}
}
