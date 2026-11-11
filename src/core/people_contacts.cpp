#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <set>

namespace pgp::core {
namespace {
constexpr std::size_t message_limit = 32;

const Employee* employee(const SimulationState& state, std::string_view person) {
    if (!state.management || !state.management->people) return nullptr;
    const auto& roster = state.management->people->employees;
    const auto found = std::ranges::find_if(roster, [&](const auto& row) {
        return row.terms.profile.id == person && row.terms.profile.role != "Founder / researcher";
    });
    return found == roster.end() ? nullptr : &*found;
}

bool at_hq(const SimulationState& state) {
    if (!state.infrastructure) return false;
    const auto location = executive_location(state);
    if (location.traveling) return false;
    return !location.appointment || std::ranges::none_of(state.infrastructure->appointments, [&](const auto& appointment) {
        return appointment.id == *location.appointment && appointment.mode == AppointmentMode::onsite;
    });
}

void require_room(const SimulationState& state, std::string_view person, std::uint32_t room) {
    if (!room || employee_contact_room(state, person) != room)
        throw std::invalid_argument("Meet this person at their current headquarters room.");
}

bool valid_reply(const ContactMessage& message) {
    if (message.reply == "former") return message.mood == -1;
    if (message.topic == "check_in") {
        return message.reply == "mood" ? message.mood >= 0 && message.mood <= 100
            : message.reply == "settled" && message.mood == -1;
    }
    return message.mood == -1 &&
        (message.reply == "laptop" || message.reply == "desk" || message.reply == "needs_computer");
}

std::string opening_reply(const SimulationState& state, const PeopleContact& contact, std::string_view topic) {
    if (topic=="opening_terms") return contact.opening->outcome;
    const bool followup=std::ranges::any_of(contact.funding,[](const auto& discussion){return discussion.status=="submitted";});
    if (topic!="board_role" || (contact.opening->outcome!="signed" && !followup))
        throw std::invalid_argument("Choose a topic for this opening conversation.");
    if (!state.management->governance) return "no_seat";
    const auto id=contact.person.substr(opening_contact_prefix.size());
    const auto seat=id=="alder"?"board.cofounder":"board."+id;
    const auto board=current_board(state);
    const bool opening_seat=contact.opening->outcome=="signed" && std::ranges::any_of(board,[&](const auto& row){return row.id==seat;});
    const bool followup_seat=std::ranges::any_of(contact.funding,[&](const auto& discussion) {
        return discussion.status=="submitted" && std::ranges::any_of(board,[&](const auto& row){return row.id=="seat/"+discussion.selected;});
    });
    return opening_seat || followup_seat?"has_seat":"no_seat";
}

bool valid_opening_reply(const PeopleContact& contact, const ContactMessage& message) {
    const bool followup=std::ranges::any_of(contact.funding,[&](const auto& discussion){return discussion.status=="submitted" && discussion.responded_on && *discussion.responded_on<=message.day;});
    return message.mood==-1 && ((message.topic=="opening_terms" && message.reply==contact.opening->outcome) ||
        (message.topic=="board_role" && (contact.opening->outcome=="signed" || followup) && (message.reply=="has_seat" || message.reply=="no_seat")));
}
} // namespace

std::uint32_t employee_contact_room(const SimulationState& state, std::string_view person) {
    const auto* found = employee(state, person);
    if (!found || found->departed_on || found->ready_on > state.date || !at_hq(state)) return 0;
    return assigned_hq_room(state, person);
}

void manage_people_contact(SimulationState& state, const ContactAction& action,Cents protected_cash) {
    if (!state.management || !state.management->people)
        throw std::invalid_argument("This company has no individual contacts.");
    auto& contacts = state.management->people->contacts;
    std::visit([&](const auto& request) {
        const auto contact = std::ranges::find(contacts, request.person, &PeopleContact::person);
        using Action = std::decay_t<decltype(request)>;
        if constexpr (std::is_same_v<Action, ExchangePeopleContact>) {
            require_room(state, request.person, request.room);
            if (contact != contacts.end()) throw std::invalid_argument("You have already exchanged contact details.");
            contacts.push_back({request.person, state.date, {}});
        } else {
            if (contact == contacts.end() && (!std::is_same_v<Action,DismissPeopleContact> || !request.room))
                throw std::invalid_argument("Choose someone whose contact details you have.");
            if (request.room) {
                require_room(state, request.person, request.room);
            } else {
                if (request.device != "phone" && request.device != "tablet" && request.device != "laptop" && request.device != "desktop")
                    throw std::invalid_argument("Choose a powered device that supports People.");
                if (!state.infrastructure || !state.infrastructure->workplace || !state.infrastructure->workplace->devices ||
                    resolve_app_device(state, request.device, "people", at_hq(state)) != request.device)
                    throw std::invalid_argument("The selected device cannot send a message here.");
            }
            if constexpr (std::is_same_v<Action,DismissPeopleContact>) {
                // Keep an employment conversation available after an in-person departure.
                if(contact==contacts.end()) contacts.push_back({request.person,state.date,{}});
                dismiss_employee(state,request.person,protected_cash);
            } else if constexpr (std::is_same_v<Action,RequestContactFunding>) {
                request_contact_funding(state,*contact);
            } else if constexpr (std::is_same_v<Action,RespondContactFunding>) {
                respond_contact_funding(state,*contact,request);
            } else {
                if (!contact->opening && request.topic != "check_in" && request.topic != "workstation")
                    throw std::invalid_argument("Choose a conversation topic.");
                auto& messages = contact->messages;
                if (std::ranges::any_of(messages, [&](const auto& old) { return old.day == state.date && old.topic == request.topic; }))
                    throw std::invalid_argument("You already asked about this today.");
                ContactMessage message{state.date, request.topic, {}, -1};
                const auto* found=employee(state,request.person);
                if (contact->opening) {
                    message.reply=opening_reply(state,*contact,request.topic);
                } else if (!found) {
                    throw std::invalid_argument("Choose an employee contact.");
                } else if (found->departed_on) {
                    message.reply = "former";
                } else if (request.topic == "check_in") {
                    const auto moods = workplace_moods(state);
                    const auto mood = moods.find(request.person);
                    message.reply = mood == moods.end() ? "settled" : "mood";
                    if (mood != moods.end()) message.mood = mood->second.score;
                } else {
                    message.reply = hq_laptop_ready(state, request.person) ? "laptop"
                        : hq_workstation_ready(state, request.person) ? "desk" : "needs_computer";
                }
                if (messages.size() == message_limit) messages.erase(messages.begin());
                messages.push_back(std::move(message));
                if(!contact->opening && request.topic=="check_in")note_social_contact(state,request.person);
            }
        }
    }, action);
}

void validate_people_contacts(const SimulationState& state) {
    const auto& people = *state.management->people;
    if (people.contacts.size() > people.employees.size()+opening_contact_limit)
        throw std::invalid_argument("The contact register exceeds its supported size.");
    std::set<std::string_view> people_seen;
    for (const auto& contact : people.contacts) {
        const auto* found = contact.opening?nullptr:employee(state, contact.person);
        if (!people_seen.insert(contact.person).second || contact.exchanged_on > state.date || contact.messages.size() > message_limit)
            throw std::invalid_argument("The contact has an invalid identity, date or history size.");
        if (contact.opening) {
            const auto& opening=*contact.opening;
            if (!contact.person.starts_with(opening_contact_prefix) || contact.person.size()<=opening_contact_prefix.size() ||
                contact.person.size()>72 || contact.exchanged_on!=people.starts_on ||
                (opening.source!="recruiter" && opening.source!="application" && opening.source!="introduction" && opening.source!="pitch" && opening.source!="partnership") ||
                (opening.outcome!="signed" && opening.outcome!="declined" && opening.outcome!="rejected" && opening.outcome!="unsigned"))
                throw std::invalid_argument("An opening conversation must record a supported source and outcome.");
        } else if (!found || contact.exchanged_on < found->ready_on || (found->departed_on && contact.exchanged_on > *found->departed_on)) {
            throw std::invalid_argument("An employee contact must record a meeting with an onboarded employee.");
        }
        auto previous = contact.exchanged_on;
        std::set<std::pair<OrdinalDay,std::string_view>> asked;
        for (const auto& message : contact.messages) {
            const auto valid=contact.opening?valid_opening_reply(contact,message):
                (message.topic=="check_in" || message.topic=="workstation") && valid_reply(message) &&
                (message.reply!="former" || (found->departed_on && message.day>=*found->departed_on));
            if (message.day < previous || message.day > state.date || !asked.emplace(message.day,message.topic).second || !valid)
                throw std::invalid_argument("A conversation has an invalid date, topic or reply.");
            previous = message.day;
        }
    }
}
} // namespace pgp::core
