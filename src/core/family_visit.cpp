#include "infrastructure_internal.hpp"
#include <iterator>

namespace pgp::core {
using namespace infrastructure_detail;
namespace {
const HeadquartersState* headquarters(const SimulationState& state) {
    if(!state.infrastructure || !state.infrastructure->workplace || !state.infrastructure->workplace->headquarters) return nullptr;
    return &*state.infrastructure->workplace->headquarters;
}
const std::vector<FamilyVisit>* visits(const SimulationState& state) {
    if(!state.succession || !state.succession->personal_life) return nullptr;
    return &state.succession->personal_life->family_visits;
}
FamilyVisit itinerary(const SimulationState& state,const HQPremises& origin,OrdinalDay booked,Days stay) {
    const auto& rules=*personal_life_catalog(state).family_visits;
    if(stay<Days{1} || stay>rules.maximum_stay) invalid("Choose a stay within the disclosed visit length.");
    const auto& catalog=state.infrastructure->catalog;
    const auto& from=find(catalog.workplace->headquarters->markets,origin.market).state;
    const auto destination=family_home_state(state);
    const auto& outbound=travel_route(catalog,from,destination);
    const auto& inbound=travel_route(catalog,destination,from);
    const auto departs=advance_days(booked,rules.notice);
    const auto arrives=advance_days(departs,outbound.one_way);
    const auto leaves=advance_days(arrives,stay);
    const auto returns=advance_days(leaves,inbound.one_way);
    if(returns>=state.end_date) invalid("The visit must return before the campaign ends.");
    return {origin.id,stay,booked,departs,arrives,leaves,returns,checked_unit_add(outbound.fare,inbound.fare),{}};
}
}

std::string_view family_home_state(const SimulationState& state) {
    const auto* home=headquarters(state);
    if(!home || home->home_family.empty()) return {};
    return find(state.infrastructure->catalog.workplace->headquarters->markets,home->home_market).state;
}
const FamilyVisit* pending_family_visit(const SimulationState& state) {
    const auto* rows=visits(state);
    if(!rows || rows->empty()) return nullptr;
    const auto& last=rows->back();
    return !last.cancelled_on && state.date<last.returns_on?&last:nullptr;
}
const FamilyVisit* family_visit_on(const SimulationState& state,OrdinalDay day) {
    const auto* rows=visits(state);
    if(!rows) return nullptr;
    const auto after=std::ranges::upper_bound(*rows,day,{},&FamilyVisit::departs_on);
    if(after==rows->begin()) return nullptr;
    const auto& row=*std::prev(after);
    return !row.cancelled_on && day<row.returns_on?&row:nullptr;
}
FamilyVisit family_visit_quote(const SimulationState& state,Days stay) {
    const auto& catalog=personal_life_catalog(state);
    const auto* rows=visits(state);
    if(!catalog.family_visits || !rows || family_home_state(state).empty()) invalid("Family visits are unavailable in this campaign.");
    const auto& premises=current_headquarters(state);
    if(premises.kind=="home") invalid("You already work from the family home.");
    if(pending_family_visit(state)) invalid("Finish or cancel the current family visit first.");
    if(rows->size()>=family_visit_limit) invalid("The personal travel register is full.");
    const auto trip=itinerary(state,premises,state.date,stay);
    for(const auto& appointment:state.infrastructure->appointments)
        if(!appointment.cancelled_on && appointment.mode!=AppointmentMode::delegated &&
            trip.departs_on<appointment.ends_before && appointment.starts_on<trip.returns_on)
            invalid("A company inspection overlaps this visit. Change the inspection or shorten the stay.");
    const auto& owned=*state.succession->personal_life;
    for(const auto id:owned.pending_events) {
        const auto day=owned.events[id-1].day;
        if(day>=trip.departs_on && day<trip.returns_on) invalid("A gathering you are hosting overlaps this visit.");
    }
    return trip;
}
void validate_family_visits(const SimulationState& state) {
    const auto& rows=*visits(state);
    if(rows.empty()) return;
    const auto* hq=headquarters(state);
    if(!personal_life_catalog(state).family_visits || !hq || family_home_state(state).empty())
        invalid("Saved family visits require the authored family home and travel rules.");
    OrdinalDay next_booking=state.accounts->starts_on;
    std::vector<const FamilyVisit*> completed_or_active;
    for(const auto& trip:rows) {
        if(!trip.premises || trip.premises>hq->premises.size() || trip.booked_on<next_booking || trip.booked_on>state.date ||
            (trip.cancelled_on && (*trip.cancelled_on<trip.booked_on || *trip.cancelled_on>state.date || *trip.cancelled_on>=trip.departs_on)))
            invalid("Invalid family visit dates or origin.");
        const auto& premises=hq->premises[trip.premises-1];
        auto expected=itinerary(state,premises,trip.booked_on,trip.stay);
        expected.cancelled_on=trip.cancelled_on;
        const auto until=trip.cancelled_on.value_or(trip.returns_on);
        if(premises.kind=="home" || premises.entered_on>trip.booked_on ||
            (premises.left_on && *premises.left_on<until) || expected!=trip)
            invalid("The saved visit differs from the original office, booked dates or round-trip fare.");
        if(!trip.cancelled_on) completed_or_active.push_back(&trip);
        next_booking=until;
    }
    for(const auto& appointment:state.infrastructure->appointments) {
        if(appointment.cancelled_on || appointment.mode==AppointmentMode::delegated) continue;
        const auto visit=std::ranges::upper_bound(completed_or_active,appointment.starts_on,{},
            [](const auto* trip){return trip->returns_on;});
        if(visit!=completed_or_active.end() && (*visit)->departs_on<appointment.ends_before)
            invalid("A saved company inspection overlaps a family visit.");
    }
}
}
