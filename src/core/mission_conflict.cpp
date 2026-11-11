#include <pgp/core/mission_conflict.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
const Employee* employee(const SimulationState& state,std::string_view person) {
    const auto& employees=state.management->people->employees;
    const auto match=std::ranges::find_if(employees,[person](const Employee& worker){
        return worker.terms.profile.id==person;
    });
    return match==employees.end()?nullptr:&*match;
}

bool mission_valid(std::string_view mission) {
    return mission=="research" || mission=="public benefit" || mission=="commercial";
}

bool contributed_to_a_project(const SimulationState& state,std::string_view person) {
    const auto started_work=[&](std::string_view project_id) {
        if(!state.capacity)return false;
        const auto project=std::ranges::find(state.capacity->projects,project_id,&CapacityProject::id);
        return project!=state.capacity->projects.end() && (project->completed_work>MilliCU{0} || project->completed_staff_days>0);
    };
    if(state.lab && std::ranges::any_of(state.lab->research,[&](const auto& work){
        return std::ranges::find(work.researchers,person)!=work.researchers.end() && started_work(work.project_id);
    }))return true;
    return talent_rules(state) && std::ranges::any_of(state.management->people->talent->software,[&](const auto& work){
        return std::ranges::find(work.design.workers,person)!=work.design.workers.end() && started_work(work.project);
    });
}

SocialLink* founder_link(SimulationState& state,std::string_view person) {
    auto& links=state.management->people->social->links;
    const auto match=std::ranges::find_if(links,[person](const SocialLink& link){
        return (link.first=="founder" && link.second==person) || (link.second=="founder" && link.first==person);
    });
    return match==links.end()?nullptr:&*match;
}
}

const MissionConflictRules* mission_conflict_rules(const SimulationState& state) {
    const auto* rules=social_rules(state);
    return rules && rules->mission_conflict?&*rules->mission_conflict:nullptr;
}

std::string_view committed_company_mission(const SimulationState& state) {
    const auto& people=*state.management->people;
    if(people.social && people.social->mission_conflict)return people.social->mission_conflict->mission;
    return people.mission;
}

const MissionConcern* mission_concern(const SimulationState& state,std::string_view person) {
    if(!mission_conflict_rules(state) || !state.management->people->social->mission_conflict)return nullptr;
    const auto& concerns=state.management->people->social->mission_conflict->concerns;
    const auto match=concerns.find(person);
    return match==concerns.end()?nullptr:&match->second;
}

bool is_personal_rival(const SimulationState& state,std::string_view first,std::string_view second) {
    if(first!="founder" && second!="founder")return false;
    const auto* concern=mission_concern(state,first=="founder"?second:first);
    return concern && concern->rivalry_on && !concern->settled_on;
}

std::vector<std::string> mission_pivot_concerns(const SimulationState& state,std::string_view mission) {
    const auto* rules=mission_conflict_rules(state);
    if(!rules || !mission_valid(mission))return {};
    const auto& policy=*state.management->people->social->mission_conflict;
    if(mission==policy.mission || state.date<advance_days(policy.committed_on,Days{rules->commitment_days}))return {};

    std::vector<std::string> affected;
    for(const auto& worker:state.management->people->employees) {
        const auto& profile=worker.terms.profile;
        if(worker.departed_on || profile.role=="Founder / researcher" || profile.preferred_mission!=policy.mission ||
           state.date<advance_days(worker.ready_on,Days{rules->employee_days}) || !contributed_to_a_project(state,profile.id))continue;
        const auto* concern=mission_concern(state,profile.id);
        if(concern && (concern->frenemy || !concern->settled_on))continue;
        affected.push_back(profile.id);
    }
    return affected;
}

void change_company_mission(SimulationState& state,const ChangeCompanyMission& request) {
    if(!mission_conflict_rules(state) || !mission_valid(request.mission))throw std::invalid_argument("Choose a company mission.");
    auto& people=*state.management->people;
    auto& policy=*people.social->mission_conflict;
    if(request.mission==policy.mission)throw std::invalid_argument("This is already the company mission.");
    const auto affected=mission_pivot_concerns(state,request.mission);
    for(auto& [person,concern]:policy.concerns) {
        (void)person;
        if(!concern.rivalry_on && !concern.settled_on && request.mission==concern.previous_mission)
            concern.settled_on=state.date;
    }
    for(const auto& person:affected) {
        const auto* link=social_link(state,"founder",person);
        policy.concerns.insert_or_assign(person,MissionConcern{policy.mission,request.mission,state.date,link?link->affinity:0,{},{},{},{},false});
    }
    policy.mission=request.mission;
    policy.committed_on=state.date;
    people.mission=request.mission;
}

bool mission_mediator_available(const SimulationState& state,std::string_view person) {
    if(!mission_conflict_rules(state))return false;
    const auto* worker=employee(state,person);
    return worker && !worker->departed_on && worker->ready_on<=state.date &&
        worker->terms.profile.role!="Founder / researcher" && !is_personal_rival(state,"founder",person) &&
        (has_skill(worker->terms.profile,"empathy") || has_skill(worker->terms.profile,"psychology"));
}

std::string mission_response_blocker(const SimulationState& state,const RespondToMissionConcern& response) {
    const auto* rules=mission_conflict_rules(state);
    const auto* concern=mission_concern(state,response.person);
    if(!rules || !concern || concern->settled_on)return "There is no open conversation.";
    const auto& social=*state.management->people->social;
    if(social.last_outing==state.date)return "You already made plans today.";
    if(!response.mediator.empty()) {
        if(concern->rivalry_on)return "Ask to speak with them directly.";
        if(response.person==response.mediator || !mission_mediator_available(state,response.mediator))
            return "Choose an available mediator with Empathetic Listening or Psychology.";
        return {};
    }
    const bool empathetic=person_has_skill(state,"founder","empathy");
    if(concern->rivalry_on && !empathetic)return "Study Empathetic Listening to reopen this conversation.";
    if(!empathetic && concern->previous_affinity<social_rules(state)->thresholds[2])
        return "Empathetic Listening or a prior friendship could open this conversation.";
    if(concern->heard_on && state.date<advance_days(*concern->heard_on,Days{rules->repair_days}))
        return "Give them time until "+iso_date(advance_days(*concern->heard_on,Days{rules->repair_days}))+".";
    return {};
}

void respond_to_mission_concern(SimulationState& state,const RespondToMissionConcern& response) {
    const auto blocked=mission_response_blocker(state,response);
    if(!blocked.empty())throw std::invalid_argument(blocked);
    auto& social=*state.management->people->social;
    auto& concern=social.mission_conflict->concerns.at(response.person);
    social.last_outing=state.date;
    if(concern.rivalry_on && !concern.heard_on) {
        concern.heard_on=state.date;
        return;
    }
    if(!concern.heard_on)concern.heard_on=state.date;
    concern.mediator=response.mediator;
    concern.settled_on=state.date;
    if(auto* link=founder_link(state,response.person)) {
        link->memory=concern.rivalry_on?"Talked through the broken commitment":"Talked through the change of direction";
        link->last_contact=state.date;
    }
}

void prepare_mission_conflicts(SimulationState& state) {
    const auto* rules=mission_conflict_rules(state);
    if(!rules)return;
    auto& concerns=state.management->people->social->mission_conflict->concerns;
    for(auto& [person,concern]:concerns) {
        if(concern.settled_on) {
            if(concern.rivalry_on && !concern.frenemy) {
                const auto* link=social_link(state,"founder",person);
                if(link && link->affinity>=social_rules(state)->thresholds[2])concern.frenemy=true;
            }
            continue;
        }
        if(!concern.rivalry_on && state.date>=advance_days(concern.raised_on,Days{rules->concern_days})) {
            concern.rivalry_on=state.date;
            if(auto* link=founder_link(state,person)) {
                link->affinity=std::max(0,std::min(rules->trust_ceiling,link->affinity-rules->trust_loss));
                link->memory="Felt betrayed by the company's change of mission";
            }
        }
        if(!concern.rivalry_on || state.date<advance_days(concern.heard_on.value_or(*concern.rivalry_on),Days{rules->notice_days}))continue;
        const auto* worker=employee(state,person);
        if(!worker || worker->departed_on)continue;
        // Existing staffing commitments must be reconciled before releasing
        // the employee. Try again when the remaining roster has room.
        try {
            (void)dismissal_quote(state,person);
        } catch(const std::invalid_argument&) {
            continue;
        }
        resign_employee(state,person);
    }
}

void validate_mission_conflict_rules(const MissionConflictRules& rules) {
    if(rules.commitment_days<30 || rules.commitment_days>365 || rules.employee_days<7 || rules.employee_days>365 ||
       rules.concern_days<3 || rules.concern_days>90 || rules.notice_days<3 || rules.notice_days>90 || rules.repair_days<3 || rules.repair_days>90 ||
       rules.trust_loss<100 || rules.trust_loss>1000 || rules.trust_ceiling<0 || rules.trust_ceiling>300)
        throw std::invalid_argument("Mission conflict rules exceed their supported limits.");
}

void validate_mission_conflicts(const SimulationState& state) {
    const auto* rules=mission_conflict_rules(state);
    const auto& people=*state.management->people;
    const auto& policy=people.social->mission_conflict;
    if(static_cast<bool>(rules)!=policy.has_value())throw std::invalid_argument("Mission conflict state and rules disagree.");
    if(!rules)return;
    if(!mission_valid(policy->mission) || policy->committed_on<people.starts_on || policy->committed_on>state.date || policy->concerns.size()>people.employees.size())
        throw std::invalid_argument("Invalid company mission history.");
    for(const auto& [person,concern]:policy->concerns) {
        const auto* worker=employee(state,person);
        if(!worker || worker->terms.profile.role=="Founder / researcher" || concern.previous_mission!=worker->terms.profile.preferred_mission ||
           !mission_valid(concern.new_mission) || concern.previous_mission==concern.new_mission || concern.raised_on<worker->ready_on ||
           concern.raised_on>state.date || concern.previous_affinity<0 || concern.previous_affinity>1000 ||
           (concern.rivalry_on && (*concern.rivalry_on<advance_days(concern.raised_on,Days{rules->concern_days}) || *concern.rivalry_on>state.date)) ||
           (concern.heard_on && (*concern.heard_on<concern.rivalry_on.value_or(concern.raised_on) || *concern.heard_on>state.date)) ||
           (concern.settled_on && (*concern.settled_on<concern.raised_on || *concern.settled_on>state.date ||
               (concern.heard_on && *concern.settled_on<advance_days(*concern.heard_on,Days{concern.rivalry_on?rules->repair_days:0})) || (concern.rivalry_on && !concern.heard_on))) ||
           (!concern.mediator.empty() && (!employee(state,concern.mediator) || concern.mediator==person || !concern.settled_on || concern.rivalry_on)) ||
           (concern.frenemy && (!concern.rivalry_on || !concern.settled_on)))
            throw std::invalid_argument("Invalid mission concern participants or dates.");
    }
}
}
