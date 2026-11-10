#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
const PersonalEventRules& rules(const SimulationState& state) {
    const auto& catalog=personal_life_catalog(state);
    if(!catalog.events || !state.management->people || !state.infrastructure || !state.succession || !state.succession->personal_life)
        invalid("Hosted gatherings are unavailable in this campaign.");
    return *catalog.events;
}
const PersonalEventOffer& offer(const SimulationState& state,std::string_view id) {
    const auto& offers=rules(state).offers;
    const auto found=std::ranges::find(offers,id,&PersonalEventOffer::id);
    if(found==offers.end()) invalid("Choose an available gathering.");
    return *found;
}
const Employee* worker(const SimulationState& state,std::string_view id) {
    const auto& employees=state.management->people->employees;
    const auto found=std::ranges::find_if(employees,[&](const auto& row){return row.terms.profile.id==id;});
    return found==employees.end()?nullptr:&*found;
}
std::string invitation_reply(const SimulationState& state,std::string_view id,OrdinalDay day) {
    const auto& contacts=state.management->people->contacts;
    const auto found=std::ranges::find(contacts,id,&PeopleContact::person);
    if(found==contacts.end()) invalid("Invite people whose contact details you have exchanged.");
    if(found->opening) return found->opening->outcome=="signed"?"accepted":"declined";
    const auto* employee=worker(state,id);
    return employee && employee->ready_on<=day && !employee->departed_on?"accepted":"unavailable";
}
bool host_present(const SimulationState& state,OrdinalDay day) {
    return !family_visit_on(state,day) && std::ranges::none_of(state.infrastructure->appointments,[&](const auto& row){
        return row.mode==AppointmentMode::onsite && day>=row.starts_on && day<row.ends_before &&
            (!row.cancelled_on || *row.cancelled_on>day);
    });
}
void refund(SimulationState& state,PersonalEvent& event,Cents amount,std::string outcome) {
    event.refund=amount;event.outcome=std::move(outcome);event.resolved_on=state.date;
    state.succession->personal_cash=checked_unit_add(state.succession->personal_cash,amount);
    auto& owned=*state.succession->personal_life;
    owned.spent=checked_unit_subtract(owned.spent,amount);
}
void venue(const SimulationState& state,const PersonalEventOffer& gathering,std::uint32_t property,OrdinalDay booked,bool historical=false) {
    if(gathering.venue=="office") {
        if(property || !state.infrastructure || !state.infrastructure->workplace || !state.infrastructure->workplace->headquarters)
            invalid("An office gathering needs the current physical headquarters.");
        return;
    }
    const auto& owned=state.succession->personal_life->properties;
    if(!property || property>owned.size() || owned[property-1].purchased_on>booked ||
        (owned[property-1].sold_on && (*owned[property-1].sold_on<booked || (!historical && *owned[property-1].sold_on==booked))))
        invalid("Choose a home you own for this gathering.");
}
void label(const std::string& value) {
    if(value.empty() || value.size()>512 || std::ranges::any_of(value,[](unsigned char c){return c<32;}))
        invalid("Gathering descriptions must be bounded single-line text.");
}
}
void validate_personal_event_rules(const PersonalEventRules& catalog) {
    if(catalog.offers.empty() || catalog.offers.size()>8 || catalog.interval<Days{1} || catalog.interval>Days{365} ||
        catalog.maximum_notice<Days{1} || catalog.maximum_notice>Days{365}) invalid("Gathering timing must be bounded.");
    std::set<std::string> ids;
    for(const auto& row:catalog.offers) {
        label(row.id);label(row.name);label(row.description);
        if(!ids.insert(row.id).second || row.id.size()>64 || (row.venue!="home" && row.venue!="office") ||
            row.booking.value()<0 || row.booking.value()>1000000000 || row.per_guest.value()<0 || row.per_guest.value()>100000000 ||
            row.guests<1 || row.guests>personal_event_guest_limit || row.mood>10 || row.notice<Days{1} || row.notice>catalog.maximum_notice ||
            row.duration<Days{1} || row.duration>Days{30} || row.cancellation_refund.value()<0 || row.cancellation_refund.value()>10000)
            invalid("A gathering needs supported costs, capacity, notice and effects.");
    }
}
Cents personal_event_quote(const SimulationState& state,const BookPersonalEvent& request) {
    const auto& gathering=offer(state,request.offer);
    const auto& owned=*state.succession->personal_life;
    if(owned.unpaid.value()>0) invalid("Pay outstanding personal upkeep before booking a gathering.");
    if(owned.events.size()>=personal_event_limit || owned.pending_events.size()>=8) invalid("The gathering register or upcoming calendar is full.");
    if(request.guests.empty() || request.guests.size()>gathering.guests) invalid("Choose a guest list within this gathering's capacity.");
    if(request.day<advance_days(state.date,gathering.notice) || request.day>advance_days(state.date,rules(state).maximum_notice) || request.day>=state.end_date)
        invalid("Allow the gathering's notice period and stay within this campaign.");
    if(family_visit_on(state,request.day)) invalid("Choose a gathering date outside your family visit.");
    venue(state,gathering,request.property,state.date);
    std::set<std::string_view> unique;
    for(const auto& id:request.guests) {
        if(!unique.insert(id).second) invalid("Invite each person once.");
        (void)invitation_reply(state,id,request.day);
    }
    for(const auto& past:owned.events) if(past.outcome!="cancelled" && past.outcome!="venue_changed") {
        const auto gap=std::abs(request.day.value()-past.day.value());
        if(gap<rules(state).interval.value()) invalid("Leave the disclosed interval between your gatherings.");
    }
    return Cents{checked_add(gathering.booking.value(),gathering.per_guest.value()*static_cast<std::int64_t>(request.guests.size()))};
}
void book_personal_event(SimulationState& state,const BookPersonalEvent& request) {
    const auto paid=personal_event_quote(state,request);
    if(paid>state.succession->personal_cash) invalid("Your personal savings cannot cover this gathering.");
    auto& owned=*state.succession->personal_life;
    const auto& gathering=offer(state,request.offer);
    PersonalEvent event;
    event.id=static_cast<std::uint32_t>(owned.events.size()+1);event.property=request.property;
    event.premises=gathering.venue=="office"?current_headquarters(state).id:0;
    event.offer=request.offer;event.booked_on=state.date;event.day=request.day;event.paid=paid;
    for(const auto& id:request.guests) event.guests.push_back({id,invitation_reply(state,id,request.day),false});
    state.succession->personal_cash=checked_unit_subtract(state.succession->personal_cash,paid);
    owned.spent=checked_unit_add(owned.spent,paid);
    owned.pending_events.push_back(event.id);owned.events.push_back(std::move(event));
}
void cancel_personal_event(SimulationState& state,std::uint32_t id) {
    auto& owned=*state.succession->personal_life;
    if(!id || id>owned.events.size()) invalid("Choose a scheduled gathering.");
    auto& event=owned.events[id-1];
    if(event.resolved_on || event.day<=state.date) invalid("Cancel the gathering before its date.");
    refund(state,event,personal_resale(event.paid,offer(state,event.offer).cancellation_refund),"cancelled");
    std::erase(owned.pending_events,id);
}
void prepare_personal_events(SimulationState& state) {
    if(!state.succession || !state.succession->personal_life || !personal_life_catalog(state).events) return;
    auto& owned=*state.succession->personal_life;
    for(const auto id:owned.pending_events) {
        auto& event=owned.events[id-1];
        if(event.day>state.date)continue;
        if(event.premises && event.premises!=current_headquarters(state).id) {
            refund(state,event,event.paid,"venue_changed");continue;
        }
        const bool present=host_present(state,state.date);
        event.outcome=present?"hosted":"host_absent";event.resolved_on=state.date;
        for(auto& guest:event.guests) {
            if(!present || guest.reply!="accepted")continue;
            const auto* employee=worker(state,guest.person);
            guest.attended=!employee || (!employee->departed_on && employee->ready_on<=state.date);
        }
    }
    std::erase_if(owned.pending_events,[&](auto id){return owned.events[id-1].resolved_on.has_value();});
}
std::map<std::string,int> personal_event_moods(const SimulationState& state) {
    std::map<std::string,int> points;
    if(!state.succession || !state.succession->personal_life || !personal_life_catalog(state).events)return points;
    const auto& events=state.succession->personal_life->events;
    for(auto it=events.rbegin();it!=events.rend();++it) {
        if(state.date.value()-it->booked_on.value()>rules(state).maximum_notice.value()+30)break;
        if(it->outcome!="hosted" || it->day>state.date)continue;
        const auto& gathering=offer(state,it->offer);
        if(state.date.value()-it->day.value()>=gathering.duration.value())continue;
        for(const auto& guest:it->guests) if(guest.attended)
            points[guest.person]=std::max(points[guest.person],static_cast<int>(gathering.mood));
    }
    return points;
}
void validate_personal_events(const SimulationState& state) {
    const auto& owned=*state.succession->personal_life;
    if(owned.events.size()>personal_event_limit || owned.pending_events.size()>8) invalid("The gathering register exceeds its supported limits.");
    if(owned.events.empty()) {
        if(!owned.pending_events.empty())invalid("The upcoming calendar refers to missing gatherings.");
        return;
    }
    if(!personal_life_catalog(state).events) {
        if(!owned.events.empty() || !owned.pending_events.empty()) invalid("Gatherings require their authored rules.");
        return;
    }
    (void)rules(state);
    std::map<std::string_view,const PeopleContact*> contacts;
    std::map<std::string_view,const Employee*> employees;
    for(const auto& contact:state.management->people->contacts)contacts.emplace(contact.person,&contact);
    for(const auto& employee:state.management->people->employees)employees.emplace(employee.terms.profile.id,&employee);
    std::vector<std::uint32_t> pending;
    std::vector<OrdinalDay> days;
    std::vector<OrdinalDay> committed_dates;
    OrdinalDay last_booked=state.accounts->starts_on;
    for(const auto& event:owned.events) {
        const auto& gathering=offer(state,event.offer);
        if(event.id!=days.size()+1 || event.booked_on<last_booked || event.booked_on>state.date ||
            event.day<advance_days(event.booked_on,gathering.notice) || event.day>advance_days(event.booked_on,rules(state).maximum_notice) || event.day>=state.end_date ||
            event.guests.empty() || event.guests.size()>gathering.guests ||
            event.paid!=Cents{checked_add(gathering.booking.value(),gathering.per_guest.value()*static_cast<std::int64_t>(event.guests.size()))})
            invalid("A gathering must match its booked date, guests and paid cost.");
        venue(state,gathering,event.property,event.booked_on,true);
        if(event.property) {
            const auto& property=owned.properties[event.property-1];
            if(property.sold_on && (!event.resolved_on || *property.sold_on<*event.resolved_on))
                invalid("The host sold a venue before resolving its gathering.");
        }
        if(gathering.venue=="office") {
            const auto& history=state.infrastructure->workplace->headquarters->premises;
            if(!event.premises || event.premises>history.size()) invalid("An office event needs its actual premises.");
            const auto& place=history[event.premises-1];
            if(place.entered_on>event.booked_on || (place.left_on && *place.left_on<event.booked_on)) invalid("The office was unavailable when the event was booked.");
        } else if(event.premises) invalid("A home gathering cannot use office premises.");
        std::set<std::string_view> unique;
        for(const auto& guest:event.guests) {
            const auto found=contacts.find(guest.person);
            if(found==contacts.end() || found->second->exchanged_on>event.booked_on || !unique.insert(guest.person).second ||
                (guest.reply!="accepted" && guest.reply!="declined" && guest.reply!="unavailable") ||
                (guest.attended && (guest.reply!="accepted" || event.outcome!="hosted"))) invalid("Invitations and attendance need known contacts and a hosted event.");
            const auto* contact=found->second;
            if(contact->opening && guest.reply!=(contact->opening->outcome=="signed"?"accepted":"declined")) invalid("The invitation reply differs from the contact's recorded relationship.");
            const auto person=employees.find(guest.person);
            const auto* employee=person==employees.end()?nullptr:person->second;
            if(!contact->opening && (!employee || (guest.reply=="accepted" && (employee->ready_on>event.day || (employee->departed_on && *employee->departed_on<event.booked_on))) || guest.reply=="declined"))
                invalid("An employee's RSVP must reflect onboarding and employment at booking.");
            if(event.outcome=="hosted" && guest.attended && employee && employee->departed_on && *employee->departed_on<event.day)
                invalid("Attendance must reflect the guest's actual employment at the event.");
        }
        if(!event.resolved_on) {
            if(event.outcome!="scheduled" || event.refund.value()!=0 || event.day<=state.date) invalid("An unresolved gathering must be scheduled in the future.");
            pending.push_back(event.id);
        } else {
            if(*event.resolved_on<event.booked_on || *event.resolved_on>state.date) invalid("Invalid gathering resolution date.");
            if(event.outcome=="cancelled") {
                if(*event.resolved_on>=event.day || event.refund!=personal_resale(event.paid,gathering.cancellation_refund)) invalid("Cancellation must use the disclosed refund before the date.");
            } else if(event.outcome=="venue_changed") {
                if(!event.premises || *event.resolved_on!=event.day || event.refund!=event.paid) invalid("A moved office event requires its full refund.");
                const auto& place=state.infrastructure->workplace->headquarters->premises[event.premises-1];
                if(!place.left_on || *place.left_on>event.day)invalid("A venue refund requires an actual office move.");
            } else if((event.outcome!="hosted" && event.outcome!="host_absent") || *event.resolved_on!=event.day || event.refund.value()!=0)
                invalid("A completed gathering must have its actual date and outcome.");
            if((event.outcome=="hosted" || event.outcome=="host_absent") && (event.outcome=="hosted")!=host_present(state,event.day))
                invalid("The event outcome differs from the host's recorded calendar.");
        }
        if(event.outcome!="cancelled" && event.outcome!="venue_changed")committed_dates.push_back(event.day);
        days.push_back(event.day);last_booked=event.booked_on;
    }
    if(pending!=owned.pending_events) invalid("The upcoming gathering calendar differs from its history.");
    std::ranges::sort(committed_dates);
    for(std::size_t i=1;i<committed_dates.size();++i)
        if(committed_dates[i].value()-committed_dates[i-1].value()<rules(state).interval.value())
            invalid("Gatherings overlap the disclosed calendar interval.");
}
}
