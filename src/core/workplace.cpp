#include <pgp/core/workplace.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <cctype>
#include <set>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
bool id_valid(std::string_view text) {
    return !text.empty() && text.size()<=48 && std::ranges::all_of(text,[](char c){return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_';});
}
template<class T> const auto& find(const T& rows,std::string_view id) {
    const auto it=std::ranges::find(rows,id,[](const auto& row){return row.id;});
    if(it==rows.end()) invalid("The selected room, blueprint, state or hardware is no longer available.");return *it;
}
const WorkplaceRules& rules(const SimulationState& s) {
    if(!s.infrastructure || !s.infrastructure->catalog.workplace || !s.infrastructure->workplace || !s.accounts || !s.capacity)
        invalid("This scenario does not have a headquarters expansion office.");
    return *s.infrastructure->catalog.workplace;
}
Cents multiply(Cents amount,std::uint32_t count) {return Cents{mul_div_round_half_up(static_cast<std::uint64_t>(amount.value()),count,1)};}
void valid_blueprint(const SimulationState& s,const FacilityBlueprint& b) {
    if(!id_valid(b.id) || b.name.empty() || b.name.size()>96 || b.name.find('\0')!=std::string::npos ||
        b.grid_watts<s.infrastructure->catalog.minimum_site_watts || b.grid_watts>s.infrastructure->catalog.maximum_site_watts || b.grid_watts%1000!=0 ||
        b.operators<1 || b.operators>s.infrastructure->catalog.maximum_operators_per_site)
        invalid("Blueprints need a name, whole-kW grid reservation and paid operators within the facility limits.");
    if(b.operators<find(s.infrastructure->catalog.hardware,b.hardware_family).operators)
        invalid("The blueprint needs at least the hardware's required operator headcount.");
}
}
void validate_workplace_rules(const WorkplaceRules& r) {
    const auto text=[](std::string_view value,std::size_t limit) {return !value.empty() && value.size()<=limit && value.find('\0')==std::string_view::npos;};
    if(r.home_seats<1 || r.shared_seats<r.home_seats || r.leased_seats<r.shared_seats || r.leased_seats>128 ||
        r.maximum_rooms<1 || r.maximum_rooms>24 || r.maximum_blueprints<1 || r.maximum_blueprints>16 || r.rooms.empty() || r.rooms.size()>8)
        invalid("Headquarters room rules exceed their bounded catalog.");
    std::set<std::string> ids,places;
    for(const auto& room:r.rooms) {
        if(!id_valid(room.id) || !ids.insert(room.id).second || !places.insert(room.place).second ||
            (room.place!="research_annex" && room.place!="evaluation_suite" && room.place!="collaboration") ||
            !text(room.name,96) || !text(room.description,512) ||
            room.construction<Cents{10000} || room.construction>Cents{100000000} || room.monthly<Cents{0} || room.monthly>Cents{10000000} ||
            room.lead<Days{1} || room.lead>Days{365} || room.seats<1 || room.seats>32)
            invalid("A headquarters room has invalid identity, cost, construction time or workstation capacity.");
    }
}
void initialize_workplace(SimulationState& s,std::string office,bool nonresearch_founder) {
    if(!s.infrastructure || !s.infrastructure->catalog.workplace) return;
    validate_workplace_rules(*s.infrastructure->catalog.workplace);
    if(office!="home" && office!="shared" && office!="leased") invalid("Unsupported opening headquarters office.");
    s.infrastructure->workplace=WorkplaceState{s.date,std::move(office),nonresearch_founder,{},{},{}};
}
std::uint32_t headquarters_seats(const SimulationState& s) {
    const auto& r=rules(s);const auto& w=*s.infrastructure->workplace;
    auto result=w.opening_office=="home"?r.home_seats:w.opening_office=="shared"?r.shared_seats:r.leased_seats;
    for(const auto& room:w.rooms) if(room.ready_on<=s.date) {
        const auto rent=std::ranges::find(s.accounts->recurring,"hq.room/"+room.id,&RecurringObligation::id);
        if(rent!=s.accounts->recurring.end() && (!rent->ends_before || *rent->ends_before>s.date)) result+=find(r.rooms,room.offer).seats;
    }
    return result;
}
std::uint32_t headquarters_seats_used(const SimulationState& s) {
    (void)rules(s);std::uint32_t used=0;
    if(s.management && s.management->people) {
        for(const auto& person:s.management->people->employees) if(!person.departed_on) ++used;
    } else used=s.capacity->available_staff;
    if(s.management) {
        if(s.management->people) for(const auto& hire:s.management->specialists) {
            const auto salary=std::ranges::find(s.accounts->recurring,"specialist/"+hire.offer_id,&RecurringObligation::id);
            if(!hire.withdrawn_on && (salary==s.accounts->recurring.end() || !salary->ends_before || *salary->ends_before>s.date)) ++used;
        }
        if(s.management->people && s.management->governance) for(const auto& hire:s.management->governance->appointments) if(!hire.departed_on) ++used;
    }
    if(s.capacity->available_staff>0 && s.infrastructure->workplace->nonresearch_founder) ++used;
    return used;
}
void expand_headquarters(SimulationState& s,const ExpandHeadquarters& request,Cents protected_cash) {
    const auto& r=rules(s);auto& w=*s.infrastructure->workplace;const auto& offer=find(r.rooms,request.offer);
    if(!id_valid(request.id) || w.rooms.size()>=r.maximum_rooms || std::ranges::any_of(w.rooms,[&](const auto& room){return room.id==request.id;}))
        invalid("Choose a new room within the headquarters construction limit.");
    const auto ready=advance_days(s.date,offer.lead);
    if(ready>=s.end_date || checked_unit_subtract(s.cash,protected_cash)<offer.construction)
        invalid("The room must finish within the campaign and its construction cost must fit available funds.");
    record_cash(*s.accounts,s.date,Cents{-offer.construction.value()},Account::expenses,"HQ fit-out: "+offer.name,"hq.room/"+request.id);
    add_recurring(*s.accounts,{"hq.room/"+request.id,offer.monthly,ready,std::nullopt,"Headquarters room: "+offer.name});
    w.rooms.push_back({request.id,request.offer,s.date,ready});s.cash=accounts_report(*s.accounts).cash;
}
void create_facility_blueprint(SimulationState& s,const FacilityBlueprint& blueprint) {
    const auto& r=rules(s);auto& w=*s.infrastructure->workplace;valid_blueprint(s,blueprint);
    if(w.blueprints.size()>=r.maximum_blueprints || std::ranges::any_of(w.blueprints,[&](const auto& row){return row.id==blueprint.id;}))
        invalid("Choose a new blueprint within the saved blueprint limit.");
    w.blueprints.push_back(blueprint);
}
BlueprintQuote preview_blueprint_build(const SimulationState& s,const BuildFromBlueprint& request) {
    (void)rules(s);const auto& b=find(s.infrastructure->workplace->blueprints,request.blueprint);valid_blueprint(s,b);
    const auto& state=find(s.infrastructure->catalog.states,request.state);const auto& hardware=find(s.infrastructure->catalog.hardware,b.hardware_family);
    const auto site=preview_site_build(s,{request.id,request.name,request.state,b.grid_watts,b.evaporative?CoolingMode::evaporative:CoolingMode::air});
    if(auto blocked=frontier_technology_block(s,FrontierTechnology::hardware,b.hardware_family,s.date);!blocked.empty()) throw std::invalid_argument(blocked);
    if(state.connectivity_class<hardware.connectivity_class || site.it_watts<hardware.active_it_watts)
        invalid("This state's network or the blueprint's usable power cannot support the selected hardware.");
    for(const auto& research:hardware.requires_research) if(!s.lab || std::ranges::none_of(s.lab->research,[&](const auto& row){return row.research_id==research && row.completed;}))
        invalid("Complete the hardware's integration research before building this blueprint.");
    BlueprintQuote q;q.construction=site.price;q.hardware=hardware.purchase_cost;q.recruitment=multiply(state.operator_signing,b.operators);
    q.total=checked_unit_add(checked_unit_add(q.construction,q.hardware),q.recruitment);
    q.monthly=checked_unit_add(checked_unit_add(state.monthly_property,state.monthly_overhead),checked_unit_add(hardware.monthly_maintenance,multiply(state.operator_monthly,b.operators)));
    q.built_on=site.built_on;q.grid_on=site.grid_on;q.delivery_on=advance_days(s.date,hardware.delivery_lead);q.operators_on=advance_days(s.date,state.operator_hire_lead);
    return q;
}
void build_from_blueprint(SimulationState& s,const BuildFromBlueprint& request,Cents protected_cash) {
    const auto quote=preview_blueprint_build(s,request);const auto b=find(s.infrastructure->workplace->blueprints,request.blueprint);
    if(checked_unit_subtract(s.cash,protected_cash)<quote.total) invalid("The complete blueprint exceeds available funds, including hardware and operator recruitment.");
    build_owned_site(s,{request.id,request.name,request.state,b.grid_watts,b.evaporative?CoolingMode::evaporative:CoolingMode::air},protected_cash);
    order_owned_hardware(s,{request.id+".rack",request.id,b.hardware_family},protected_cash);
    hire_site_operators(s,{request.id+".operators",request.id,b.operators},protected_cash);
    s.infrastructure->workplace->builds.push_back({request.blueprint,request.id,s.date});
}
void validate_workplace(const SimulationState& s,bool deep) {
    if(!s.infrastructure) return;
    const auto& i=*s.infrastructure;
    if(i.catalog.workplace.has_value()!=i.workplace.has_value()) invalid("Headquarters plans must match their authored rules.");
    if(!i.workplace) return;
    const auto& r=rules(s);const auto& w=*i.workplace;
    if(w.starts_on!=s.accounts->starts_on || (w.opening_office!="home" && w.opening_office!="shared" && w.opening_office!="leased") ||
        w.rooms.size()>r.maximum_rooms || w.blueprints.size()>r.maximum_blueprints || w.builds.size()>i.catalog.maximum_sites)
        invalid("Headquarters history exceeds its opening or bounded plan limits.");
    if(headquarters_seats_used(s)>headquarters_seats(s)) invalid("HQ has no unreserved paid workstations. Complete a room expansion before hiring more people.");
    if(!deep) return;
    std::set<std::string> ids;
    for(const auto& room:w.rooms) {
        const auto& offer=find(r.rooms,room.offer);const auto id="hq.room/"+room.id;
        if(!id_valid(room.id) || !ids.insert(room.id).second || room.ordered_on<w.starts_on || room.ordered_on>s.date || room.ready_on!=advance_days(room.ordered_on,offer.lead))
            invalid("Headquarters room identity or construction dates are inconsistent.");
        const auto salary=std::ranges::find(s.accounts->recurring,id,&RecurringObligation::id);
        if(salary==s.accounts->recurring.end() || salary->monthly_amount!=offer.monthly || salary->starts_on!=room.ready_on)
            invalid("A headquarters room lost its monthly property obligation.");
        if(salary->ends_before && std::ranges::none_of(s.receipts,[&](const auto& receipt){return receipt.cause=="withdraw_operations" && std::max(receipt.day,room.ready_on)==*salary->ends_before;}))
            invalid("A room's property costs cannot end without a recorded operations withdrawal.");
        const auto posting=std::ranges::find_if(s.accounts->postings,[&](const auto& row){return row.key==id && row.day==room.ordered_on && row.amount==offer.construction && row.debit==Account::expenses && row.credit==Account::cash;});
        if(posting==s.accounts->postings.end()) invalid("A headquarters room has no matching paid fit-out.");
    }
    ids.clear();for(const auto& b:w.blueprints) {valid_blueprint(s,b);if(!ids.insert(b.id).second) invalid("Duplicate saved facility blueprint.");}
    ids.clear();for(const auto& build:w.builds) {
        const auto& b=find(w.blueprints,build.blueprint);const auto& site=find(i.sites,build.site);const auto& hardware=find(i.hardware,build.site+".rack");const auto& operators=find(i.hires,build.site+".operators");
        if(!ids.insert(build.site).second || build.ordered_on!=site.ordered_on || site.grid_watts!=b.grid_watts || (site.cooling==CoolingMode::evaporative)!=b.evaporative ||
            hardware.site_id!=site.id || hardware.family_id!=b.hardware_family || hardware.ordered_on!=build.ordered_on || operators.site_id!=site.id || operators.people!=b.operators || operators.ordered_on!=build.ordered_on)
            invalid("A blueprint build differs from its paid physical site, hardware or operators.");
    }
}
}
