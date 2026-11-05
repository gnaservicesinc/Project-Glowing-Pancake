#include "infrastructure_internal.hpp"
namespace pgp::core {
using namespace infrastructure_detail;
void prepare_infrastructure(SimulationState& s) {
    if (!s.infrastructure) return;
    auto& i=infrastructure(s);
    if (i.initial_lease_ends_before && s.date>=*i.initial_lease_ends_before) {
        for (auto& source:s.capacity->sources) if (source.id=="lease.initial") source.effective=MilliCUPerDay{0};
        const auto term=std::find_if(s.accounts->recurring.begin(),s.accounts->recurring.end(),[](const auto& t){return t.id=="compute_lease";});
        if (term!=s.accounts->recurring.end() && !term->ends_before) end_recurring(*s.accounts,"compute_lease",s.date);
        if (s.date==*i.initial_lease_ends_before) add_notice(s,"lease/ended","initial_lease_ended","lease.initial",s.date);
    }
    prepare_site_appointments(s);
    for (const auto& owned:i.sites) {
        if (!owned.closed_on && owned.built_on==s.date) add_notice(s,"built/"+owned.id,"construction_complete",owned.id,owned.built_on);
        if (!owned.closed_on && owned.grid_on==s.date) add_notice(s,"grid/"+owned.id,"grid_connected",owned.id,owned.grid_on);
        const auto plan=power_plan(s,owned,s.date,true);
        for (auto& h:i.hardware) if (h.site_id==owned.id) {
            if (!h.retired_on && h.delivered_on==s.date) add_notice(s,"delivered/"+h.id,"hardware_delivered",h.id,h.delivered_on);
            const auto output=std::find_if(plan.hardware.begin(),plan.hardware.end(),[&](const auto& p){return p.order_id==h.id;});
            // Delivery alone cannot create a source. Paid operators, inspected
            // site, connection and enough idle power must all be available.
            if (!h.commissioned_on && !h.retired_on && output!=plan.hardware.end() && output->powered) {
                if (s.capacity->sources.size()>=capacity_source_limit) invalid("The paid capacity register is full");
                const auto& f=family(s,h);const auto& owner=s.accounts->owner;h.commissioned_on=s.date;
                s.capacity->sources.push_back({h.source_id,"physical/"+owner+"/"+h.id,owner,owner,CapacitySourceKind::owned,f.capacity,output->effective,f.memory_mib,f.interconnect,false,s.date});
                add_notice(s,"commissioned/"+h.id,"hardware_commissioned",h.id,s.date);
            }
            if (h.commissioned_on) {
                auto source=std::find_if(s.capacity->sources.begin(),s.capacity->sources.end(),[&](const auto& row){return row.id==h.source_id;});
                if (source==s.capacity->sources.end()) invalid("Commissioned hardware lost its unique capacity source");
                source->effective=h.retired_on || owned.closed_on || output==plan.hardware.end()?MilliCUPerDay{0}:output->effective;
            }
        }
    }
    for (const auto& hire:i.hires) if (hire.starts_on==s.date && !site(s,hire.site_id).closed_on)
        add_notice(s,"ready/operators/"+hire.id,"operators_started",hire.site_id,s.date);
    std::sort(s.capacity->sources.begin(),s.capacity->sources.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    finish_site_inspections(s);
}
void close_infrastructure_day(SimulationState& s) {
    if (!s.infrastructure) return;
    auto& i=infrastructure(s);
    if (i.closed_through!=s.accounts->closed_through || !s.capacity->closed_through || *s.capacity->closed_through!=s.date || s.capacity->days.empty()) invalid("Owned metering must follow actual work and precede accounts close exactly once");
    const auto& usage=s.capacity->days.back();if (usage.day!=s.date) invalid("Owned metering requires today's actual capacity interval");
    Cents costs{0},depreciation{0};
    for (auto& owned:i.sites) {
        const auto sample=meter_site(s,owned,s.date,usage,i.hardware,owned);
        owned.last_energy_wh=sample.wh;owned.last_water_ml=sample.water;owned.last_emissions_grams=sample.emissions;
        owned.last_utility_cost=checked_unit_add(sample.electricity,sample.water_charge);
        const auto operating=fixed_cost_day(s,owned,s.date);const auto total=checked_unit_add(owned.last_utility_cost,operating);costs=checked_unit_add(costs,total);
        if (sample.wh>0 || total.value()>0) {
            const auto month=first_of_month(s.date);
            if (owned.meters.empty() || owned.meters.back().month!=month) {
                if (owned.meters.size()>=maximum_months_per_site) invalid("The bounded monthly site meter is full");owned.meters.push_back({month});
            }
            auto& record=owned.meters.back();record.energy_wh+=sample.wh;record.water_ml+=sample.water;record.emissions_grams+=sample.emissions;
            record.electricity=checked_unit_add(record.electricity,sample.electricity);record.water=checked_unit_add(record.water,sample.water_charge);record.operating=checked_unit_add(record.operating,operating);
        }
        if (month_end(s.date) && !owned.closed_on) {
            const auto target=depreciation_through(owned.paid,owned.built_on,s.date,i.catalog.building_life);
            depreciation=checked_unit_add(depreciation,checked_unit_subtract(target,owned.depreciated));owned.depreciated=target;
        }
    }
    if (month_end(s.date)) for (auto& h:i.hardware) if (!h.retired_on) {
        const auto target=depreciation_through(h.paid,h.delivered_on,s.date,family(s,h).useful_life);
        depreciation=checked_unit_add(depreciation,checked_unit_subtract(target,h.depreciated));h.depreciated=target;
    }
    accrue_metered_expense(*s.accounts,s.date,"owned_operations",costs,"Owned infrastructure utilities and operating costs",false);
    record_owned_depreciation(*s.accounts,s.date,"monthly",depreciation,false);i.closed_through=s.date;
}
}
