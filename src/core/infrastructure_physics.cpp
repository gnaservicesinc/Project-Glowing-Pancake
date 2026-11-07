#include "infrastructure_internal.hpp"
#include <limits>
namespace pgp::core {
using namespace infrastructure_detail;
namespace infrastructure_detail {
Cents prorated_day(Cents monthly,OrdinalDay starts,OrdinalDay day,std::optional<OrdinalDay> ends) {
    if (day<starts || !before_end(day,ends) || monthly.value()==0) return Cents{0};
    const auto first=std::max(first_of_month(day),starts);const auto civil=from_ordinal(day);
    const auto elapsed=static_cast<std::uint64_t>(day.value()-first.value());const auto denominator=days_in_month(civil.year,civil.month);
    const auto before=mul_div_floor(static_cast<std::uint64_t>(monthly.value()),elapsed,denominator).quotient;
    const auto after=mul_div_floor(static_cast<std::uint64_t>(monthly.value()),elapsed+1,denominator).quotient;
    return Cents{after-before};
}
Cents depreciation_through(Cents cost,OrdinalDay starts,OrdinalDay through,Days life) {
    if (through<starts) return Cents{0};
    const auto elapsed=static_cast<std::uint64_t>(checked_add(checked_subtract(through.value(),starts.value()),1));
    return Cents{mul_div_floor(static_cast<std::uint64_t>(cost.value()),std::min(elapsed,static_cast<std::uint64_t>(life.value())),static_cast<std::uint64_t>(life.value())).quotient};
}
std::uint32_t operators_on(const SimulationState& s,const OwnedSite& owned,OrdinalDay day) {
    if (!before_end(day,owned.closed_on)) return 0;
    std::uint32_t people=0;for (const auto& hire:infrastructure(s).hires) if (hire.site_id==owned.id && hire.starts_on<=day) people+=hire.people;
    return people;
}
std::uint32_t pue(const StateSiteProfile& state,CoolingMode cooling) {return cooling==CoolingMode::air?state.air_pue_milli:state.water_pue_milli;}
std::uint64_t facility_limit(const StateSiteProfile& state,const OwnedSite& owned,std::uint64_t budget) {
    auto limit=std::min(owned.grid_watts,budget);
    if (owned.cooling==CoolingMode::evaporative) {
        const auto water_limit=mul_div_floor(state.water_limit_ml_per_day,1000,state.water_ml_per_kwh*24).quotient;
        limit=std::min(limit,water_limit);
    }
    return limit;
}
SitePowerPreview power_plan(const SimulationState& s,const OwnedSite& owned,OrdinalDay day,bool commissioning) {
    SitePowerPreview result;const auto& state=profile(s,owned);
    if (day<owned.built_on || day<owned.grid_on || !owned.commissioned_on || *owned.commissioned_on>day || !before_end(day,owned.closed_on)) return result;
    std::uint64_t budget=owned.grid_watts;for (const auto& change:owned.power) if (change.day<=day) budget=change.watts;
    result.facility_watts=facility_limit(state,owned,budget);
    const auto disruption=site_disruption(s,owned.id,day);
    if (disruption.emergency_watts) result.facility_watts=std::min(result.facility_watts,*disruption.emergency_watts);
    result.it_watts=mul_div_floor(result.facility_watts,1000,pue(state,owned.cooling)).quotient;
    result.operators=operators_on(s,owned,day);
    std::array<const HardwareOrder*,4> candidates{};std::size_t count=0;
    for (const auto& order:infrastructure(s).hardware) if (order.site_id==owned.id && order.delivered_on<=day && before_end(day,order.retired_on) &&
        (commissioning || (order.commissioned_on && *order.commissioned_on<=day))) {
        if (count==candidates.size()) invalid("Site active hardware exceeds its paid bounded rack slots");candidates[count++]=&order;
    }
    std::sort(candidates.begin(),candidates.begin()+static_cast<std::ptrdiff_t>(count),[](const auto* a,const auto* b){return std::tie(a->delivered_on,a->id)<std::tie(b->delivered_on,b->id);});
    std::uint32_t free=result.operators;std::uint64_t idle=0,delta=0;
    for (std::size_t i=0;i<count;++i) {
        const auto& f=family(s,*candidates[i]);const bool staffed=free>=f.operators;
        if (staffed) {free-=f.operators;idle+=f.idle_it_watts;delta+=f.active_it_watts-f.idle_it_watts;}
        result.hardware.push_back({candidates[i]->id,MilliCUPerDay{0},staffed});
    }
    if (result.it_watts<idle) {for (auto& output:result.hardware) output.powered=false;return result;}
    const auto fraction=delta==0?10000ULL:std::min<std::uint64_t>(10000,mul_div_floor(result.it_watts-idle,10000,delta).quotient);
    for (auto& output:result.hardware) if (output.powered) {
        const auto& order=find(infrastructure(s).hardware,output.order_id);const auto& f=family(s,order);
        output.effective=MilliCUPerDay{mul_div_floor(std::array<std::uint64_t,3>{
            static_cast<std::uint64_t>(f.capacity.value()),fraction,static_cast<std::uint64_t>(disruption.remaining.value())},100'000'000).quotient};
    }
    return result;
}
Cents monthly_fixed_cost(const SimulationState& s,const OwnedSite& owned,OrdinalDay day) {
    if (!before_end(day,owned.closed_on)) return Cents{0};
    const auto& state=profile(s,owned);Cents total{0};
    if (day>=owned.built_on) total=checked_unit_add(state.monthly_property,state.monthly_overhead);
    total=checked_unit_add(total,Cents{mul_div_floor(static_cast<std::uint64_t>(state.operator_monthly.value()),operators_on(s,owned,day),1).quotient});
    for (const auto& h:infrastructure(s).hardware) if (h.site_id==owned.id && day>=h.delivered_on && before_end(day,h.retired_on)) total=checked_unit_add(total,family(s,h).monthly_maintenance);
    return total;
}
Cents fixed_cost_day(const SimulationState& s,const OwnedSite& owned,OrdinalDay day) {
    const auto& state=profile(s,owned);Cents total=prorated_day(checked_unit_add(state.monthly_property,state.monthly_overhead),owned.built_on,day,owned.closed_on);
    for (const auto& hire:infrastructure(s).hires) if (hire.site_id==owned.id) total=checked_unit_add(total,prorated_day(Cents{mul_div_floor(static_cast<std::uint64_t>(state.operator_monthly.value()),hire.people,1).quotient},hire.starts_on,day,owned.closed_on));
    for (const auto& h:infrastructure(s).hardware) if (h.site_id==owned.id) {
        auto end=h.retired_on;if (owned.closed_on && (!end || *owned.closed_on<*end)) end=owned.closed_on;
        total=checked_unit_add(total,prorated_day(family(s,h).monthly_maintenance,h.delivered_on,day,end));
    }
    return total;
}
std::vector<MeteredProjection> forecast_terms(const SimulationState& s) {
    std::vector<MeteredProjection> terms;if (!s.infrastructure) return terms;
    const auto add=[&](OrdinalDay start,std::optional<OrdinalDay> end,Cents monthly) {
        if (monthly.value()==0 || (end && *end<=std::max(start,s.date))) return;
        terms.push_back({start,end,Cents{(static_cast<std::uint64_t>(monthly.value())+27)/28}});
    };
    for (const auto& owned:s.infrastructure->sites) {
        const auto& state=profile(s,owned);add(owned.built_on,owned.closed_on,checked_unit_add(state.monthly_property,state.monthly_overhead));
        for (const auto& hire:s.infrastructure->hires) if (hire.site_id==owned.id) add(hire.starts_on,owned.closed_on,Cents{mul_div_floor(static_cast<std::uint64_t>(state.operator_monthly.value()),hire.people,1).quotient});
        for (const auto& h:s.infrastructure->hardware) if (h.site_id==owned.id) {
            const auto& f=family(s,h);auto end=h.retired_on;if (owned.closed_on && (!end || *owned.closed_on<*end)) end=owned.closed_on;
            add(h.delivered_on,end,f.monthly_maintenance);
        }
        // Conservative full facility draw. It may overstate an unstaffed or
        // idle site, but never hides an already committed operating ceiling.
        auto ready=std::max(owned.built_on,owned.grid_on);
        if (owned.closed_on && *owned.closed_on<=std::max(ready,s.date)) continue;
        std::uint64_t maximum=facility_limit(state,owned,owned.grid_watts);
        const auto wh=maximum*24;const auto water=owned.cooling==CoolingMode::air?0:mul_div_floor(wh,state.water_ml_per_kwh,1000).quotient;
        auto tariff=state.electricity_millicents_per_kwh;
        if (s.resilience) for (const auto& protection:s.resilience->protections) if (protection.site_id==owned.id) {
            const auto& offer=find(s.resilience->catalog.protections,protection.offer_id);
            if (offer.kind==SiteProtectionKind::generator) tariff=std::max(tariff,offer.power_millicents_per_kwh);
        }
        const auto raw=mul_div_floor(wh,tariff,1).quotient+mul_div_floor(water,state.water_cents_per_1000_liters,1).quotient;
        terms.push_back({ready,owned.closed_on,Cents{raw/1000000+(raw%1000000?1:0)}});
    }
    return terms;
}
MeterSample meter_site(const SimulationState& s,const OwnedSite& owned,OrdinalDay day,const CapacityDay& usage,std::vector<HardwareOrder>& orders,OwnedSite& meter) {
    MeterSample sample;const auto& state=profile(s,owned);const auto plan=power_plan(s,owned,day);
    for (const auto& output:plan.hardware) if (output.powered) {
        auto it=std::find_if(orders.begin(),orders.end(),[&](const auto& h){return h.id==output.order_id;});if (it==orders.end()) invalid("Meter has no actual hardware owner");
        const auto& f=family(s,*it);const auto nominal=static_cast<std::uint64_t>(f.capacity.value());std::uint64_t used=0;
        for (const auto& row : usage.sources) {
            if (row.source_id == it->source_id)
                used = static_cast<std::uint64_t>(checked_unit_add(row.used, row.wholesale_used).value());
        }
        if (used>static_cast<std::uint64_t>(output.effective.value())) invalid("Owned work bypassed its grid, cooling or staffing budget");
        const auto factors=std::array<std::uint64_t,4>{f.active_it_watts-f.idle_it_watts,used,24,pue(state,owned.cooling)};
        const auto variable=mul_div_floor(factors,nominal);
        const auto residual=it->energy_remainder+variable.remainder;
        const auto milli_wh=f.idle_it_watts*24*pue(state,owned.cooling)+variable.quotient+residual/nominal;
        sample.wh+=milli_wh/1000;it->energy_remainder=(milli_wh%1000)*nominal+residual%nominal;
    }
    const auto water_raw=sample.wh*(owned.cooling==CoolingMode::air?0:state.water_ml_per_kwh)+meter.water_remainder;
    sample.water=water_raw/1000;meter.water_remainder=water_raw%1000;
    const auto disruption=site_disruption(s,owned.id,day);
    const auto tariff=disruption.emergency_watts?disruption.power_millicents_per_kwh:state.electricity_millicents_per_kwh;
    const auto carbon=disruption.emergency_watts?disruption.power_grams_per_kwh:state.grid_grams_per_kwh;
    const auto emissions_raw=sample.wh*carbon+meter.emissions_remainder;
    sample.emissions=emissions_raw/1000;meter.emissions_remainder=emissions_raw%1000;
    const auto electric=sample.wh*tariff+meter.electricity_remainder;
    const auto water_cost=sample.water*state.water_cents_per_1000_liters+meter.water_charge_remainder;
    sample.electricity=Cents{electric/1000000};meter.electricity_remainder=electric%1000000;
    sample.water_charge=Cents{water_cost/1000000};meter.water_charge_remainder=water_cost%1000000;
    return sample;
}
}
SitePowerPreview preview_site_power(const SimulationState& s,std::string_view id,OrdinalDay day) {return power_plan(s,site(s,id),day);}
Cents wholesale_electricity(const SimulationState& s,std::string_view source_id,MilliCU work) {
    const auto& orders=infrastructure(s).hardware;
    const auto order=std::find_if(orders.begin(),orders.end(),[&](const auto& row){return row.source_id==source_id;});
    if (order==orders.end()) invalid("Wholesale electricity requires an owned hardware order");
    const auto& owned=site(s,order->site_id);
    const auto& terms=profile(s,owned);
    const auto& hardware=family(s,*order);
    if (work.value()>hardware.capacity.value()) invalid("Wholesale electricity exceeds the rack's rated work");
    const auto disruption=site_disruption(s,owned.id,s.date);
    const auto tariff=disruption.emergency_watts?disruption.power_millicents_per_kwh:terms.electricity_millicents_per_kwh;
    const auto factors=std::array<std::uint64_t,5>{hardware.active_it_watts-hardware.idle_it_watts,
        static_cast<std::uint64_t>(work.value()),24,pue(terms,owned.cooling),tariff};
    // Watt-hours with milli-PUE at milli-cents per kWh. Charge only
    // incremental electricity and round down once to whole cents.
    return Cents{mul_div_floor(factors,static_cast<std::uint64_t>(hardware.capacity.value())*1'000'000'000U).quotient};
}
namespace {
AccountsState forecast_lease_terms(const SimulationState& s) {
    if (!s.accounts) invalid("A company forecast requires accounts");
    auto ledger=*s.accounts;
    if (s.infrastructure && s.infrastructure->initial_lease_ends_before && *s.infrastructure->initial_lease_ends_before>s.date) {
        for (auto& term:ledger.recurring) if (term.id=="compute_lease" || term.id=="founder_compute") term.ends_before=s.infrastructure->initial_lease_ends_before;
    }
    return ledger;
}
}
CashProjection project_company_cash(const SimulationState& s,OrdinalDay through,Cents protected_cash) {
    return project_cash(forecast_lease_terms(s),through,protected_cash,forecast_terms(s));
}
CashOutlook project_company_cash_outlook(const SimulationState& s,OrdinalDay near_through,Cents protected_cash,
                                         std::span<const ProjectedCashDay> days) {
    return project_cash_outlook(forecast_lease_terms(s),near_through,protected_cash,forecast_terms(s),days);
}
Cents infrastructure_monthly_fixed_cost(const SimulationState& s,OrdinalDay day) {
    Cents total{0};if (s.infrastructure) for (const auto& owned:s.infrastructure->sites) total=checked_unit_add(total,monthly_fixed_cost(s,owned,day));return total;
}
}
