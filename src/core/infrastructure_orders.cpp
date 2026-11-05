#include "infrastructure_internal.hpp"
namespace pgp::core {
using namespace infrastructure_detail;
namespace infrastructure_detail {
void add_notice(SimulationState& s,std::string id,std::string kind,std::string subject,OrdinalDay day) {
    auto& records=infrastructure(s).notices;
    if (std::any_of(records.begin(),records.end(),[&](const auto& n){return n.id==id;})) return;
    if (records.size()>=maximum_notices) invalid("The infrastructure notice register is full; no required history was discarded");
    records.push_back({std::move(id),std::move(kind),std::move(subject),day});
}
namespace {
void pay(SimulationState& s,Cents amount,Cents protected_cash,Account offset,std::string key,std::string cause) {
    if (protected_cash.value()<0 || amount.value()<0 || amount>checked_unit_subtract(s.cash,protected_cash)) invalid("The infrastructure commitment exceeds unreserved cash");
    if (amount.value()>0) record_cash(*s.accounts,s.date,Cents{-amount.value()},offset,std::move(cause),std::move(key));
    s.cash=accounts_report(*s.accounts).cash;
}
}
void pay_asset(SimulationState& s,Cents amount,Cents protected_cash,std::string key,std::string cause) {pay(s,amount,protected_cash,Account::assets,std::move(key),std::move(cause));}
void pay_expense(SimulationState& s,Cents amount,Cents protected_cash,std::string key,std::string cause) {pay(s,amount,protected_cash,Account::expenses,std::move(key),std::move(cause));}
}
SiteBuildPreview preview_site_build(const SimulationState& s,const BuildOwnedSite& request) {
    const auto& i=infrastructure(s);const auto& c=i.catalog;const auto& state=find(c.states,request.state_id);
    if (!identifier(request.id) || request.name.empty() || request.name.size()>96 || std::any_of(request.name.begin(),request.name.end(),[](unsigned char ch){return ch<32 || ch==127;}) ||
        std::any_of(i.sites.begin(),i.sites.end(),[&](const auto& row){return row.id==request.id;}) || i.sites.size()>=c.maximum_sites ||
        request.grid_watts<c.minimum_site_watts || request.grid_watts>c.maximum_site_watts || request.grid_watts%1000!=0 ||
        (request.cooling!=CoolingMode::air && request.cooling!=CoolingMode::evaporative)) invalid("Choose a new named site and a bounded whole-kW grid reservation");
    std::uint64_t committed=0;auto connection=s.date;
    for (const auto& row:i.sites) if (row.state_id==request.state_id) {
        if (!row.closed_on) committed+=row.grid_watts;
        // Canceled slots remain on the crew calendar; existing promises never move.
        connection=std::max(connection,row.grid_on);
    }
    if (committed+request.grid_watts>state.grid_watts) invalid("That state has insufficient unreserved grid capacity");
    const auto gross=checked_unit_add(state.construction_base,Cents{mul_div_floor(static_cast<std::uint64_t>(state.construction_per_kw.value()),request.grid_watts/1000,1).quotient});
    const auto incentive=std::min(state.incentive_cap,Cents{mul_div_floor(static_cast<std::uint64_t>(gross.value()),static_cast<std::uint64_t>(state.construction_incentive.value()),10000).quotient});
    OwnedSite planned;planned.cooling=request.cooling;planned.grid_watts=request.grid_watts;
    const auto facility=facility_limit(state,planned,planned.grid_watts);
    return {checked_unit_subtract(gross,incentive),incentive,advance_days(s.date,state.build_lead),advance_days(connection,state.grid_lead),
        facility,mul_div_floor(facility,1000,pue(state,planned.cooling)).quotient};
}
void build_owned_site(SimulationState& s,const BuildOwnedSite& request,Cents protected_cash) {
    const auto preview=preview_site_build(s,request);
    if (preview.built_on>=s.end_date || preview.grid_on>=s.end_date) invalid("This construction and connection cannot finish before the campaign ends");
    pay_asset(s,preview.price,protected_cash,"owned.site/"+request.id,"Owned site construction: "+request.name);
    OwnedSite owned;owned.id=request.id;owned.name=request.name;owned.state_id=request.state_id;owned.cooling=request.cooling;
    owned.ordered_on=s.date;owned.built_on=preview.built_on;owned.grid_on=preview.grid_on;owned.grid_watts=request.grid_watts;owned.paid=preview.price;
    owned.power.push_back({s.date,request.grid_watts});infrastructure(s).sites.push_back(std::move(owned));
    add_notice(s,"ordered/site/"+request.id,"construction_ordered",request.id,s.date);
}
void order_owned_hardware(SimulationState& s,const OrderOwnedHardware& request,Cents protected_cash) {
    auto& i=infrastructure(s);const auto& owned=site(s,request.site_id);const auto& f=find(i.catalog.hardware,request.family_id);const auto& state=profile(s,owned);
    if (!identifier(request.id) || owned.closed_on || i.hardware.size()>=maximum_hardware_orders || std::any_of(i.hardware.begin(),i.hardware.end(),[&](const auto& h){return h.id==request.id;}) ||
        std::count_if(i.hardware.begin(),i.hardware.end(),[&](const auto& h){return h.site_id==owned.id && !h.retired_on;})>=i.catalog.maximum_hardware_per_site ||
        state.connectivity_class<f.connectivity_class) invalid("Hardware needs an open site, a free rack slot and the stated connectivity class");
    for (const auto& research:f.requires_research) if (!s.lab || std::none_of(s.lab->research.begin(),s.lab->research.end(),[&](const auto& r){return r.research_id==research && r.completed;})) invalid("This hardware requires completed integration research");
    const auto delivery=advance_days(s.date,f.delivery_lead);if (delivery>=s.end_date) invalid("Hardware delivery falls after the campaign");
    const auto source="owned/"+request.id;
    if (s.capacity->sources.size()+static_cast<std::size_t>(std::count_if(i.hardware.begin(),i.hardware.end(),[](const auto& h){return !h.commissioned_on && !h.retired_on;}))>=capacity_source_limit ||
        std::any_of(s.capacity->sources.begin(),s.capacity->sources.end(),[&](const auto& row){return row.id==source;})) invalid("The capacity source register has no free unique slot");
    pay_asset(s,f.purchase_cost,protected_cash,"owned.hardware/"+request.id,"Owned hardware purchase: "+f.name);
    i.hardware.push_back({request.id,request.site_id,request.family_id,source,s.date,delivery,std::nullopt,std::nullopt,f.purchase_cost,Cents{0},Cents{0},0});
    add_notice(s,"ordered/hardware/"+request.id,"hardware_ordered",request.id,s.date);
}
void hire_site_operators(SimulationState& s,const HireSiteOperators& request,Cents protected_cash) {
    auto& i=infrastructure(s);const auto& owned=site(s,request.site_id);const auto& state=profile(s,owned);std::uint64_t people=request.people;
    for (const auto& hire:i.hires) if (hire.site_id==owned.id) people+=hire.people;
    if (!identifier(request.id) || request.people==0 || people>i.catalog.maximum_operators_per_site || owned.closed_on || i.hires.size()>=maximum_hires ||
        std::any_of(i.hires.begin(),i.hires.end(),[&](const auto& h){return h.id==request.id;})) invalid("Choose new paid operators within the site's staffing bound");
    const auto start=advance_days(s.date,state.operator_hire_lead);if (start>=s.end_date) invalid("The operators cannot start before the campaign ends");
    pay_expense(s,Cents{mul_div_floor(static_cast<std::uint64_t>(state.operator_signing.value()),request.people,1).quotient},protected_cash,"owned.hire/"+request.id,"Dedicated site operator recruitment");
    i.hires.push_back({request.id,request.site_id,request.people,s.date,start});
    add_notice(s,"ordered/operators/"+request.id,"operators_hired",request.id,s.date);
}
void set_site_power_limit(SimulationState& s,const SetSitePowerLimit& request) {
    auto& owned=site(s,request.site_id);
    if (owned.closed_on || request.watts>owned.grid_watts || request.watts%1000!=0 || owned.power.size()>=40960) invalid("A power budget must be whole kW within an open site's paid grid reservation");
    if (!owned.power.empty() && owned.power.back().day==s.date) owned.power.back().watts=request.watts;
    else owned.power.push_back({s.date,request.watts});
}
void retire_owned_hardware(SimulationState& s,std::string_view id) {
    auto& i=infrastructure(s);auto at=std::find_if(i.hardware.begin(),i.hardware.end(),[&](const auto& h){return h.id==id;});
    if (at==i.hardware.end() || at->retired_on) invalid("This hardware is unavailable or already retired");
    if (std::any_of(s.capacity->sources.begin(), s.capacity->sources.end(), [&](const auto& source) {
        return source.id == at->source_id && (source.wholesale_reserved.value()!=0 || source.replica_reserved.value()!=0);
    })) invalid("Finish the wholesale commitment before retiring its hardware");
    if (s.resilience && std::any_of(s.resilience->replicas.begin(),s.resilience->replicas.end(),[&](const auto& row){
        return !row.stopped_on && (row.primary_source==at->source_id || row.backup_source==at->source_id);
    })) invalid("Stop the replica before retiring either of its physical racks");
    const auto& f=family(s,*at);
    if (s.market) for (const auto& product:s.market->products) {
        const auto p=std::find_if(s.capacity->projects.begin(),s.capacity->projects.end(),[&](const auto& row){return row.id==product.capacity_project_id;});
        if (p==s.capacity->projects.end() || p->source_id!=at->source_id) continue;
        for (const auto& contract:s.market->contracts) if (contract.product_id==product.id && (contract.status==ContractStatus::active || contract.status==ContractStatus::pending)) invalid("Resolve or migrate dependent customer contracts before retiring their hardware");
    }
    std::vector<std::string> projects;for (const auto& p:s.capacity->projects) if (p.source_id==at->source_id && p.status==ProjectStatus::active) projects.push_back(p.id);
    for (const auto& project:projects) {
        const auto p=std::find_if(s.capacity->projects.begin(),s.capacity->projects.end(),[&](const auto& row){return row.id==project;});
        if (p->monthly_support_cost.value()>0) end_recurring(*s.accounts,"support/"+project,s.date);
        cancel_project(*s.capacity,s.date,project);
    }
    // The retirement morning performs no work: depreciation ends yesterday.
    const auto through=OrdinalDay{checked_subtract(s.date.value(),1)};
    const auto target=depreciation_through(at->paid,at->delivered_on,through,f.useful_life);
    record_owned_depreciation(*s.accounts,s.date,"retire.hardware/"+at->id,checked_unit_subtract(target,at->depreciated));at->depreciated=target;
    at->resale=s.date<at->delivered_on?Cents{0}:Cents{mul_div_floor(static_cast<std::uint64_t>(checked_unit_subtract(at->paid,at->depreciated).value()),static_cast<std::uint64_t>(f.resale_fraction.value()),10000).quotient};
    dispose_owned_asset(*s.accounts,s.date,"hardware/"+at->id,at->paid,at->depreciated,at->resale);at->retired_on=s.date;
    s.cash=accounts_report(*s.accounts).cash;for (auto& source:s.capacity->sources) if (source.id==at->source_id) source.effective=MilliCUPerDay{0};
    add_notice(s,"retired/hardware/"+at->id,"hardware_retired",at->id,s.date);
}
void close_owned_site(SimulationState& s,std::string_view id) {
    auto& owned=site(s,id);const auto& i=infrastructure(s);
    if (owned.closed_on || std::any_of(i.hardware.begin(),i.hardware.end(),[&](const auto& h){return h.site_id==id && !h.retired_on;})) invalid("Retire every hardware order before closing its site");
    const auto target=depreciation_through(owned.paid,owned.built_on,OrdinalDay{checked_subtract(s.date.value(),1)},i.catalog.building_life);
    record_owned_depreciation(*s.accounts,s.date,"retire.site/"+owned.id,checked_unit_subtract(target,owned.depreciated));owned.depreciated=target;
    owned.resale=s.date<owned.built_on?Cents{0}:Cents{mul_div_floor(static_cast<std::uint64_t>(checked_unit_subtract(owned.paid,owned.depreciated).value()),static_cast<std::uint64_t>(i.catalog.building_resale.value()),10000).quotient};
    dispose_owned_asset(*s.accounts,s.date,"site/"+owned.id,owned.paid,owned.depreciated,owned.resale);owned.closed_on=s.date;
    s.cash=accounts_report(*s.accounts).cash;add_notice(s,"closed/site/"+owned.id,"site_closed",owned.id,s.date);
}
void notice_initial_lease_termination(SimulationState& s) {
    auto& i=infrastructure(s);
    if (i.initial_lease_noticed_on || std::none_of(s.capacity->sources.begin(),s.capacity->sources.end(),[](const auto& source){return source.id=="lease.initial";}) ||
        std::none_of(s.accounts->recurring.begin(),s.accounts->recurring.end(),[](const auto& term){return term.id=="compute_lease" && !term.ends_before;})) invalid("There is no continuing opening lease to terminate");
    i.initial_lease_noticed_on=s.date;i.initial_lease_ends_before=advance_days(s.date,i.catalog.initial_lease_notice);
    add_notice(s,"lease/notice","initial_lease_notice","lease.initial",s.date);
}
}
