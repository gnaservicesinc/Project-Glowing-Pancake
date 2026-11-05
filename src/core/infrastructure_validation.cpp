#include "infrastructure_internal.hpp"
#include <map>
#include <set>
namespace pgp::core {
using namespace infrastructure_detail;
namespace {
struct ExpectedPosting {OrdinalDay day{0};Account debit=Account::cash,credit=Account::equity;Cents amount{0};};
using ExpectedLedger=std::map<std::string,ExpectedPosting>;
void expected(ExpectedLedger& ledger,std::string key,OrdinalDay day,Account debit,Account credit,Cents amount) {
    if (amount.value()<0) invalid("Owned ledger replay produced a negative amount");
    if (amount.value()>0 && !ledger.emplace(std::move(key),ExpectedPosting{day,debit,credit,amount}).second) invalid("Owned ledger replay repeated a transaction");
}
void depreciation_post(ExpectedLedger& ledger,std::string reference,OrdinalDay day,Cents amount) {expected(ledger,"owned.depreciation/"+reference+"/"+iso_date(day),day,Account::expenses,Account::accumulated_depreciation,amount);}
void disposal_posts(ExpectedLedger& ledger,std::string reference,OrdinalDay day,Cents cost,Cents depreciation,Cents proceeds) {
    const auto key="owned.disposal/"+reference;
    expected(ledger,key+"/depreciation",day,Account::accumulated_depreciation,Account::assets,depreciation);
    expected(ledger,key+"/cash",day,Account::cash,Account::assets,proceeds);
    expected(ledger,key+"/loss",day,Account::expenses,Account::assets,checked_unit_subtract(checked_unit_subtract(cost,depreciation),proceeds));
}
void add_meter(OwnedSite& replay,OrdinalDay day,const MeterSample& sample,Cents operating) {
    replay.last_energy_wh=sample.wh;replay.last_water_ml=sample.water;replay.last_emissions_grams=sample.emissions;
    replay.last_utility_cost=checked_unit_add(sample.electricity,sample.water_charge);
    if (sample.wh==0 && checked_unit_add(replay.last_utility_cost,operating).value()==0) return;
    const auto month=first_of_month(day);if (replay.meters.empty() || replay.meters.back().month!=month) replay.meters.push_back({month});
    auto& record=replay.meters.back();record.energy_wh+=sample.wh;record.water_ml+=sample.water;record.emissions_grams+=sample.emissions;
    record.electricity=checked_unit_add(record.electricity,sample.electricity);record.water=checked_unit_add(record.water,sample.water_charge);record.operating=checked_unit_add(record.operating,operating);
}
}
void validate_infrastructure(const SimulationState& s,bool deep) {
    if (!s.infrastructure) return;
    const auto& i=infrastructure(s);const auto& c=i.catalog;
    if (i.closed_through!=s.accounts->closed_through || i.sites.size()>c.maximum_sites || i.hardware.size()>maximum_hardware_orders || i.hires.size()>maximum_hires ||
        i.appointments.size()>maximum_appointments || i.notices.size()>maximum_notices) invalid("Owned infrastructure differs from its bounded accounts cursor");
    if (!deep) return;
    validate_infrastructure_catalog(c);(void)find(c.states,i.executive_base);
    const auto dated=[&](OrdinalDay day) {return day>=s.accounts->starts_on && day<=s.date;};
    const auto end_valid=[&](const std::optional<OrdinalDay>& end,OrdinalDay start) {return !end || (dated(*end) && *end>=start);};
    ExpectedLedger ledger;std::set<std::string> ids;
    for (std::size_t at=0;at<i.sites.size();++at) {
        const auto& owned=i.sites[at];const auto& state=profile(s,owned);
        if (!identifier(owned.id) || !ids.insert(owned.id).second || owned.name.empty() || owned.name.size()>96 || std::any_of(owned.name.begin(),owned.name.end(),[](unsigned char ch){return ch<32 || ch==127;}) ||
            !dated(owned.ordered_on) || (at>0 && i.sites[at-1].ordered_on>owned.ordered_on) || owned.built_on!=advance_days(owned.ordered_on,state.build_lead) ||
            owned.built_on>=s.end_date || owned.grid_on>=s.end_date || !end_valid(owned.closed_on,owned.ordered_on) ||
            (owned.cooling!=CoolingMode::air && owned.cooling!=CoolingMode::evaporative) || owned.grid_watts<c.minimum_site_watts || owned.grid_watts>c.maximum_site_watts || owned.grid_watts%1000!=0 ||
            owned.meters.size()>maximum_months_per_site || owned.power.empty() || owned.power.size()>40960) invalid("Invalid owned site identity, construction, grid, cooling or bounded history");
        std::uint64_t committed=owned.grid_watts;auto connection=owned.ordered_on;
        for (std::size_t previous=0;previous<at;++previous) if (i.sites[previous].state_id==owned.state_id) {
            const auto& prior=i.sites[previous];connection=std::max(connection,prior.grid_on);
            if (before_end(owned.ordered_on,prior.closed_on)) committed+=prior.grid_watts;
        }
        const auto gross=checked_unit_add(state.construction_base,Cents{mul_div_floor(static_cast<std::uint64_t>(state.construction_per_kw.value()),owned.grid_watts/1000,1).quotient});
        const auto incentive=std::min(state.incentive_cap,Cents{mul_div_floor(static_cast<std::uint64_t>(gross.value()),static_cast<std::uint64_t>(state.construction_incentive.value()),10000).quotient});
        if (owned.grid_on!=advance_days(connection,state.grid_lead) || committed>state.grid_watts || owned.paid!=checked_unit_subtract(gross,incentive) || owned.paid.value()<=0 ||
            owned.power.front().day!=owned.ordered_on || (owned.commissioned_on && (!dated(*owned.commissioned_on) || *owned.commissioned_on<std::max(owned.built_on,owned.grid_on) || (owned.closed_on && *owned.commissioned_on>*owned.closed_on)))) invalid("Owned site bypasses its paid cost, finite state queue or inspection lead");
        std::optional<OrdinalDay> prior_day;for (const auto& power:owned.power) {
            if (!dated(power.day) || power.day<owned.ordered_on || (owned.closed_on && power.day>*owned.closed_on) || (prior_day && power.day<=*prior_day) || power.watts>owned.grid_watts || power.watts%1000!=0) invalid("Invalid dated site power budget");prior_day=power.day;
        }
        expected(ledger,"owned.site/"+owned.id,owned.ordered_on,Account::assets,Account::cash,owned.paid);
    }
    ids.clear();std::set<std::string> sources;
    for (const auto& h:i.hardware) {
        const auto& owned=site(s,h.site_id);const auto& f=family(s,h);
        if (!identifier(h.id) || !ids.insert(h.id).second || !sources.insert(h.source_id).second || h.source_id!="owned/"+h.id || !dated(h.ordered_on) || h.ordered_on<owned.ordered_on ||
            (owned.closed_on && h.ordered_on>*owned.closed_on) || h.delivered_on!=advance_days(h.ordered_on,f.delivery_lead) || h.delivered_on>=s.end_date || h.paid!=f.purchase_cost ||
            !end_valid(h.retired_on,h.ordered_on) || (owned.closed_on && (!h.retired_on || *h.retired_on>*owned.closed_on)) || profile(s,owned).connectivity_class<f.connectivity_class ||
            h.energy_remainder>=static_cast<std::uint64_t>(f.capacity.value())*1000) invalid("Owned hardware differs from its finite purchase, physical identity, compatibility or delivery");
        for (const auto& research:f.requires_research) {
            if (!s.lab) invalid("Hardware integration has no lab evidence");
            const auto run=std::find_if(s.lab->research.begin(),s.lab->research.end(),[&](const auto& r){return r.research_id==research && r.completed;});
            if (run==s.lab->research.end()) invalid("Owned hardware lacks its completed integration research");
            const auto project=std::find_if(s.capacity->projects.begin(),s.capacity->projects.end(),[&](const auto& p){return p.id==run->project_id;});
            if (project==s.capacity->projects.end() || !project->stopped_on || *project->stopped_on>h.ordered_on) invalid("Hardware was purchased before its integration research completed");
        }
        const auto found=std::find_if(s.capacity->sources.begin(),s.capacity->sources.end(),[&](const auto& source){return source.id==h.source_id;});
        if (h.commissioned_on) {
            if (!dated(*h.commissioned_on) || *h.commissioned_on<h.delivered_on || !owned.commissioned_on || *h.commissioned_on<*owned.commissioned_on || (h.retired_on && *h.commissioned_on>*h.retired_on) ||
                found==s.capacity->sources.end() || found->physical_id!="physical/"+s.accounts->owner+"/"+h.id || found->owner!=s.accounts->owner || found->provider!=s.accounts->owner || found->kind!=CapacitySourceKind::owned ||
                found->nominal!=f.capacity || found->memory_mib!=f.memory_mib || found->interconnect!=f.interconnect || found->energy_included || found->available_from!=h.commissioned_on) invalid("Owned capacity was created without its paid, delivered and commissioned physical bundle");
        } else if (found!=s.capacity->sources.end() || h.energy_remainder!=0) invalid("Undelivered or uncommissioned hardware cannot supply capacity or energy");
        expected(ledger,"owned.hardware/"+h.id,h.ordered_on,Account::assets,Account::cash,h.paid);
    }
    for (const auto& source:s.capacity->sources) if (source.kind==CapacitySourceKind::owned && !sources.contains(source.id)) invalid("An owned source has no conserved hardware owner");
    for (const auto& owned:i.sites) {
        std::vector<OrdinalDay> dates{owned.ordered_on};for (const auto& h:i.hardware) if (h.site_id==owned.id) dates.push_back(h.ordered_on);
        for (const auto day:dates) {
            const auto live=std::count_if(i.hardware.begin(),i.hardware.end(),[&](const auto& h){return h.site_id==owned.id && h.ordered_on<=day && before_end(day,h.retired_on);});
            if (live>c.maximum_hardware_per_site) invalid("The site oversells its bounded physical rack slots");
        }
    }
    ids.clear();for (const auto& hire:i.hires) {
        const auto& owned=site(s,hire.site_id);const auto& state=profile(s,owned);
        if (!identifier(hire.id) || !ids.insert(hire.id).second || !dated(hire.ordered_on) || hire.ordered_on<owned.ordered_on || (owned.closed_on && hire.ordered_on>*owned.closed_on) ||
            hire.starts_on!=advance_days(hire.ordered_on,state.operator_hire_lead) || hire.starts_on>=s.end_date || hire.people==0 || hire.people>c.maximum_operators_per_site) invalid("Site operators lack their paid hiring lead or bounded site assignment");
        expected(ledger,"owned.hire/"+hire.id,hire.ordered_on,Account::expenses,Account::cash,Cents{mul_div_floor(static_cast<std::uint64_t>(state.operator_signing.value()),hire.people,1).quotient});
    }
    for (const auto& owned:i.sites) {std::uint32_t people=0;for (const auto& hire:i.hires) if (hire.site_id==owned.id) people+=hire.people;if (people>c.maximum_operators_per_site) invalid("The site has too many dedicated paid operators");}
    std::map<std::pair<std::string,OrdinalDay>,std::uint64_t> observations;
    ids.clear();for (std::size_t at=0;at<i.appointments.size();++at) {
        const auto& a=i.appointments[at];const auto& owned=site(s,a.site_id);Days lead{0},travel{0};Cents price{0};
        if (a.mode==AppointmentMode::onsite) {
            const auto& from=find(c.states,i.executive_base);const auto& to=profile(s,owned);
            const auto route=std::find_if(c.travel.begin(),c.travel.end(),[&](const auto& r){return r.from==from.travel_zone && r.to==to.travel_zone;});
            if (route==c.travel.end()) invalid("A trip has no authored route");travel=route->one_way;lead=c.onsite_booking_lead;price=checked_unit_add(c.onsite_inspection,Cents{mul_div_floor(static_cast<std::uint64_t>(route->fare.value()),2,1).quotient});
        } else if (a.mode==AppointmentMode::remote) {lead=c.remote_booking_lead;price=c.remote_inspection;}
        else if (a.mode==AppointmentMode::delegated) {lead=c.delegated_booking_lead;price=c.delegated_inspection;}
        else invalid("Unknown appointment mode");
        if (!identifier(a.id) || !ids.insert(a.id).second || !dated(a.booked_on) || a.booked_on<owned.ordered_on || (owned.closed_on && a.booked_on>*owned.closed_on) ||
            a.starts_on!=OrdinalDay{checked_subtract(a.visit_on.value(),travel.value())} || a.ends_before!=advance_days(a.visit_on,checked_unit_add(travel,Days{1})) ||
            a.starts_on<advance_days(a.booked_on,lead) || a.visit_on<std::max(owned.built_on,owned.grid_on) || a.ends_before>s.end_date || a.paid!=price ||
            (a.cancelled_on && (!dated(*a.cancelled_on) || *a.cancelled_on<a.booked_on || *a.cancelled_on>=a.starts_on)) || (a.cancelled_on && a.inspection) ||
            (a.inspection.has_value()!=(!a.cancelled_on && a.visit_on<=s.date))) invalid("An appointment differs from its paid booking, travel, cancellation or completed inspection");
        if (a.mode!=AppointmentMode::delegated) for (std::size_t prior=0;prior<at;++prior) {
            const auto& b=i.appointments[prior];
            if (b.mode!=AppointmentMode::delegated && (!b.cancelled_on || *b.cancelled_on>a.booked_on) && a.starts_on<b.ends_before && b.starts_on<a.ends_before) invalid("The executive has overlapping paid appointments");
        }
        if (a.inspection) {
            const auto& report=*a.inspection;std::uint32_t maximum=0,operators=0;MilliCUPerDay capacity{0};
            const bool open=!owned.closed_on || *owned.closed_on>=a.visit_on;
            for (const auto& h:i.hardware) if (h.site_id==owned.id && h.delivered_on<=a.visit_on && (!h.retired_on || *h.retired_on>=a.visit_on)) {++maximum;capacity=checked_unit_add(capacity,family(s,h).capacity);}
            if (open) for (const auto& hire:i.hires) if (hire.site_id==owned.id && hire.starts_on<=a.visit_on) operators+=hire.people;
            if (report.day!=a.visit_on || report.built!=open || report.connected!=open || report.delivered_bundles!=maximum ||
                report.operators!=operators || report.effective>capacity) invalid("An inspection claims unavailable facilities, hardware or capability");
            observations.emplace(std::pair{owned.id,a.visit_on},0);
        }
        expected(ledger,"owned.appointment/"+a.id,a.booked_on,Account::expenses,Account::cash,a.paid);
    }
    for (const auto& owned:i.sites) {
        std::optional<OrdinalDay> first;
        for (const auto& a:i.appointments) if (a.site_id==owned.id && a.inspection && a.inspection->built && a.inspection->connected) first=first?std::min(*first,a.visit_on):a.visit_on;
        if (first!=owned.commissioned_on) invalid("The site commission date lacks its actual completed inspection");
    }
    if (i.initial_lease_noticed_on.has_value()!=i.initial_lease_ends_before.has_value()) invalid("Lease termination has no notice or effective date");
    if (i.initial_lease_noticed_on) {
        const auto source=std::find_if(s.capacity->sources.begin(),s.capacity->sources.end(),[](const auto& row){return row.id=="lease.initial";});
        const auto term=std::find_if(s.accounts->recurring.begin(),s.accounts->recurring.end(),[](const auto& row){return row.id=="compute_lease";});
        if (!dated(*i.initial_lease_noticed_on) || *i.initial_lease_ends_before!=advance_days(*i.initial_lease_noticed_on,c.initial_lease_notice) || source==s.capacity->sources.end() || term==s.accounts->recurring.end() ||
            (s.date>=*i.initial_lease_ends_before ? (source->effective.value()!=0 || term->ends_before!=i.initial_lease_ends_before) : term->ends_before.has_value())) invalid("Opening lease termination bypasses its notice, future bill or physical end");
        for (const auto& day:s.capacity->days) if (day.day>=*i.initial_lease_ends_before)
            for (const auto& use:day.sources) if (use.source_id==source->id && (use.effective.value()!=0 || use.used.value()!=0)) invalid("Work continued on a terminated opening lease");
    }
    auto replay_sites=i.sites;auto replay_hardware=i.hardware;
    for (auto& owned:replay_sites) {owned.meters.clear();owned.electricity_remainder=0;owned.water_charge_remainder=0;owned.water_remainder=0;owned.emissions_remainder=0;owned.last_energy_wh=0;owned.last_water_ml=0;owned.last_emissions_grams=0;owned.last_utility_cost=Cents{0};owned.depreciated=Cents{0};}
    for (auto& h:replay_hardware) {h.energy_remainder=0;h.depreciated=Cents{0};}
    for (const auto& day:s.capacity->days) {
        Cents cost{0},monthly_depreciation{0};
        for (std::size_t at=0;at<i.sites.size();++at) {
            const auto& owned=i.sites[at];if (day.day<owned.ordered_on) continue;
            const auto sample=meter_site(s,owned,day.day,day,replay_hardware,replay_sites[at]);const auto operating=fixed_cost_day(s,owned,day.day);add_meter(replay_sites[at],day.day,sample,operating);
            cost=checked_unit_add(cost,checked_unit_add(checked_unit_add(sample.electricity,sample.water_charge),operating));
            const auto observation=observations.find({owned.id,advance_days(day.day,Days{1})});if (observation!=observations.end()) observation->second=sample.wh;
            const auto plan=power_plan(s,owned,day.day,true);
            for (const auto& h:i.hardware) if (h.site_id==owned.id && h.commissioned_on && *h.commissioned_on<=day.day) {
                const auto output=std::find_if(plan.hardware.begin(),plan.hardware.end(),[&](const auto& row){return row.order_id==h.id;});const auto actual=std::find_if(day.sources.begin(),day.sources.end(),[&](const auto& row){return row.source_id==h.source_id;});
                const auto effective=output==plan.hardware.end()?MilliCUPerDay{0}:output->effective;
                if (actual==day.sources.end() || actual->effective!=effective) invalid("Historical owned capacity bypasses actual grid, cooling or operators");
            }
            for (const auto& output:plan.hardware) if (output.powered) {const auto& h=find(i.hardware,output.order_id);if (!h.commissioned_on || *h.commissioned_on>day.day) invalid("Paid ready hardware was not commissioned at its first feasible morning");}
            if (month_end(day.day) && before_end(day.day,owned.closed_on)) {
                auto& replay=replay_sites[at];const auto target=depreciation_through(owned.paid,owned.built_on,day.day,c.building_life);
                monthly_depreciation=checked_unit_add(monthly_depreciation,checked_unit_subtract(target,replay.depreciated));replay.depreciated=target;
            }
        }
        if (month_end(day.day)) for (std::size_t at=0;at<i.hardware.size();++at) if (before_end(day.day,i.hardware[at].retired_on)) {
            const auto& h=i.hardware[at];auto& replay=replay_hardware[at];const auto target=depreciation_through(h.paid,h.delivered_on,day.day,family(s,h).useful_life);
            monthly_depreciation=checked_unit_add(monthly_depreciation,checked_unit_subtract(target,replay.depreciated));replay.depreciated=target;
        }
        if (monthly_depreciation.value()>0) depreciation_post(ledger,"monthly",day.day,monthly_depreciation);
        if (cost.value()>0) expected(ledger,"accrue/metered/owned_operations/"+iso_date(first_of_month(day.day))+"/"+iso_date(day.day),day.day,Account::expenses,Account::payables,cost);
    }
    for (std::size_t at=0;at<i.hardware.size();++at) {
        const auto& h=i.hardware[at];const auto& f=family(s,h);auto& replay=replay_hardware[at];
        if (h.retired_on) {
            const auto target=depreciation_through(h.paid,h.delivered_on,OrdinalDay{checked_subtract(h.retired_on->value(),1)},f.useful_life);
            depreciation_post(ledger,"retire.hardware/"+h.id,*h.retired_on,checked_unit_subtract(target,replay.depreciated));replay.depreciated=target;
            const auto proceeds=*h.retired_on<h.delivered_on?Cents{0}:Cents{mul_div_floor(static_cast<std::uint64_t>(checked_unit_subtract(h.paid,target).value()),static_cast<std::uint64_t>(f.resale_fraction.value()),10000).quotient};
            if (h.resale!=proceeds) invalid("Hardware resale exceeds its actual remaining book value");disposal_posts(ledger,"hardware/"+h.id,*h.retired_on,h.paid,target,proceeds);
        } else if (h.resale.value()!=0) invalid("Unretired hardware cannot pay resale proceeds");
        if (h.depreciated!=replay.depreciated || h.energy_remainder!=replay.energy_remainder) invalid("Owned hardware depreciation or fractional energy fails replay");
    }
    for (std::size_t at=0;at<i.sites.size();++at) {
        const auto& owned=i.sites[at];auto& replay=replay_sites[at];
        if (owned.closed_on) {
            const auto target=depreciation_through(owned.paid,owned.built_on,OrdinalDay{checked_subtract(owned.closed_on->value(),1)},c.building_life);
            depreciation_post(ledger,"retire.site/"+owned.id,*owned.closed_on,checked_unit_subtract(target,replay.depreciated));replay.depreciated=target;
            const auto proceeds=*owned.closed_on<owned.built_on?Cents{0}:Cents{mul_div_floor(static_cast<std::uint64_t>(checked_unit_subtract(owned.paid,target).value()),static_cast<std::uint64_t>(c.building_resale.value()),10000).quotient};
            if (owned.resale!=proceeds) invalid("Site resale exceeds its actual remaining book value");disposal_posts(ledger,"site/"+owned.id,*owned.closed_on,owned.paid,target,proceeds);
        } else if (owned.resale.value()!=0) invalid("An open site cannot pay resale proceeds");
        if (owned.depreciated!=replay.depreciated || owned.meters!=replay.meters || owned.electricity_remainder!=replay.electricity_remainder || owned.water_charge_remainder!=replay.water_charge_remainder ||
            owned.water_remainder!=replay.water_remainder || owned.emissions_remainder!=replay.emissions_remainder || owned.last_energy_wh!=replay.last_energy_wh || owned.last_water_ml!=replay.last_water_ml ||
            owned.last_emissions_grams!=replay.last_emissions_grams || owned.last_utility_cost!=replay.last_utility_cost) invalid("Site energy, cooling water, emissions, costs or depreciation fail actual-work replay");
        const auto plan=power_plan(s,owned,s.date,true);
        for (const auto& h:i.hardware) if (h.site_id==owned.id) {
            const auto output=std::find_if(plan.hardware.begin(),plan.hardware.end(),[&](const auto& row){return row.order_id==h.id;});
            if (output!=plan.hardware.end() && output->powered && !h.commissioned_on) invalid("Ready owned hardware lacks its actual commissioned source");
            if (h.commissioned_on) {
                const auto& source=find(s.capacity->sources,h.source_id);const auto effective=output==plan.hardware.end()?MilliCUPerDay{0}:output->effective;
                if (source.effective!=effective) invalid("Current owned compute bypasses its physical power, staffing or availability");
            }
        }
    }
    for (const auto& a:i.appointments) if (a.inspection && a.inspection->observed_energy_wh!=observations.at({a.site_id,a.visit_on})) invalid("Inspection meter evidence differs from the prior actual work interval");
    for (const auto& posting:s.accounts->postings) if (posting.key.starts_with("owned.") || posting.key.starts_with("accrue/metered/owned_operations/")) {
        const auto match=ledger.find(posting.key);
        if (match==ledger.end() || match->second.day!=posting.day || match->second.debit!=posting.debit || match->second.credit!=posting.credit || match->second.amount!=posting.amount) invalid("Owned purchases, metered obligations or disposals fail their conserving ledger replay");ledger.erase(match);
    }
    if (!ledger.empty()) invalid("Owned infrastructure is missing an actual purchase, expense or depreciation posting");
    std::map<std::string,InfrastructureNotice> notices;
    const auto remember=[&](std::string id,std::string kind,const std::string& subject,OrdinalDay day) {notices.emplace(id,InfrastructureNotice{id,std::move(kind),subject,day});};
    const auto occurred=[&](OrdinalDay day,const std::optional<OrdinalDay>& ends) {return day<=s.date && (!ends || day<=*ends);};
    for (const auto& owned:i.sites) {
        remember("ordered/site/"+owned.id,"construction_ordered",owned.id,owned.ordered_on);
        if (occurred(owned.built_on,owned.closed_on)) remember("built/"+owned.id,"construction_complete",owned.id,owned.built_on);
        if (occurred(owned.grid_on,owned.closed_on)) remember("grid/"+owned.id,"grid_connected",owned.id,owned.grid_on);
        if (owned.closed_on) remember("closed/site/"+owned.id,"site_closed",owned.id,*owned.closed_on);
    }
    for (const auto& h:i.hardware) {
        remember("ordered/hardware/"+h.id,"hardware_ordered",h.id,h.ordered_on);
        if (occurred(h.delivered_on,h.retired_on)) remember("delivered/"+h.id,"hardware_delivered",h.id,h.delivered_on);
        if (h.commissioned_on) remember("commissioned/"+h.id,"hardware_commissioned",h.id,*h.commissioned_on);
        if (h.retired_on) remember("retired/hardware/"+h.id,"hardware_retired",h.id,*h.retired_on);
    }
    for (const auto& hire:i.hires) {
        remember("ordered/operators/"+hire.id,"operators_hired",hire.id,hire.ordered_on);
        if (occurred(hire.starts_on,site(s,hire.site_id).closed_on)) remember("ready/operators/"+hire.id,"operators_started",hire.site_id,hire.starts_on);
    }
    for (const auto& a:i.appointments) {
        remember("booked/"+a.id,"inspection_booked",a.site_id,a.booked_on);
        if (a.cancelled_on) remember("cancelled/"+a.id,"inspection_cancelled",a.site_id,*a.cancelled_on);
        if (a.inspection) remember("inspection/"+a.id,"inspection_complete",a.site_id,a.visit_on);
    }
    if (i.initial_lease_noticed_on) {
        remember("lease/notice","initial_lease_notice","lease.initial",*i.initial_lease_noticed_on);
        if (*i.initial_lease_ends_before<=s.date) remember("lease/ended","initial_lease_ended","lease.initial",*i.initial_lease_ends_before);
    }
    for (const auto& n:i.notices) {
        const auto wanted=notices.find(n.id);if (wanted==notices.end() || wanted->second!=n) invalid("Infrastructure notice lacks its actual causal transaction or date");notices.erase(wanted);
    }
    if (!notices.empty()) invalid("A required infrastructure transition has no causal notice");
}
}
