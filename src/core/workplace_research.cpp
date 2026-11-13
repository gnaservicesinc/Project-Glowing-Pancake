#include <pgp/core/workplace_research.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
const CapacityProject *project(const SimulationState &state, std::string_view id) {
    const auto &projects = state.capacity->projects;
    const auto found = std::ranges::find(projects, id, &CapacityProject::id);
    return found == projects.end() ? nullptr : &*found;
}
LabAllocation allocation(const ResearchRun &run, const CapacityProject &work) {
    return {work.source_id, work.requested, work.staff, run.researchers};
}
} // namespace

bool workplace_research_enabled(const SimulationState &state) {
    return state.lab && state.lab->catalog.research_workplaces.has_value();
}

std::vector<ResearchWorker> research_workers(const SimulationState &state, std::string_view except_project) {
    std::vector<ResearchWorker> workers;
    if (!workplace_research_enabled(state) || !state.management || !state.management->people)
        return workers;
    std::set<std::string_view> active, committed;
    for (const auto &work : state.capacity->projects)
        if (work.id != except_project && work.status == ProjectStatus::active && work.required_staff_days)
            active.insert(work.id);
    for (const auto &run : state.lab->research)
        if (active.contains(run.project_id))
            committed.insert(run.researchers.begin(), run.researchers.end());
    const auto *manager = operations_manager_on(state, state.date);
    for (const auto &employee : state.management->people->employees) {
        if (employee.departed_on)
            continue;
        const auto &person = employee.terms.profile;
        const auto desk_identity =
            person.role == "Founder / researcher" ? std::string_view{"founder"} : std::string_view{person.id};
        ResearchWorker worker{person.id, person.name, {}, person.research};
        if (employee.ready_on > state.date)
            worker.unavailable = "Onboarding until " + iso_date(employee.ready_on);
        else if (talent_rules(state) && !person_has_skill(state,person.id,"ml_research") &&
            !(person.role=="Founder / researcher" && state.management->people->talent->founder_genius && person_has_skill(state,person.id,"programming")))
            worker.unavailable = "Needs ML Research, or Programming plus Actual Genius";
        else if (talent_rules(state) && !software_worker_blocker(state,person.id,except_project).empty())
            worker.unavailable = software_worker_blocker(state,person.id,except_project);
        else if (manager && manager->person == person.id)
            worker.unavailable = "Assigned to operations management";
        else if (committed.contains(person.id))
            worker.unavailable = "Already working on another research project";
        else if (!assigned_hq_desk(state, desk_identity) && !hq_laptop_ready(state, desk_identity)) {
            worker.unavailable = "Needs an assigned HQ desk";
            worker.needs_workstation = true;
        } else if (!hq_workstation_ready(state, desk_identity)) {
            worker.unavailable = "Needs a powered computer on their desk";
            worker.needs_workstation = true;
        }
        workers.push_back(std::move(worker));
    }
    return workers;
}

std::vector<std::string> research_equipment(const SimulationState &state, std::string_view topic) {
    if (!workplace_research_enabled(state))
        return {};
    const auto &rooms = *state.lab->catalog.research_workplaces;
    const auto found = rooms.find(std::string{topic});
    return found == rooms.end() ? std::vector<std::string>{} : found->second;
}

std::string research_workplace_blocker(const SimulationState &state, std::string_view topic, const LabAllocation &plan,
                                       std::string_view except_project) {
    if (!workplace_research_enabled(state))
        return {};
    if (plan.researchers.empty() || plan.researchers.size() != plan.staff || plan.researchers.size() > 32)
        return "Choose at least one named researcher for this work.";
    const auto workers = research_workers(state, except_project);
    std::set<std::string_view> unique;
    for (const auto &id : plan.researchers) {
        if (!unique.insert(id).second)
            return "A person can only fill one research assignment.";
        const auto worker = std::ranges::find(workers, id, &ResearchWorker::id);
        if (worker == workers.end())
            return "An assigned researcher is no longer available at this company.";
        if (!worker->unavailable.empty())
            return worker->name + ": " + worker->unavailable + ".";
    }
    if(except_project.empty()) {
        const auto threshold=research_creativity_threshold(*state.lab,*state.capacity,topic,state.date);
        const auto team=research_team(state,plan.researchers);
        if(threshold==UINT32_MAX) return "Complete the required technology levels and prerequisites.";
        if(team.creativity<threshold) return "Team creativity is "+std::to_string(team.creativity)+" distinct traits; this research needs "+std::to_string(threshold)+". Add new perspectives or complete more prerequisites.";
    }
    const auto equipment = research_equipment(state, topic);
    if (!hq_has_lab_equipment(state, equipment)) {
        const auto &offers = state.infrastructure->catalog.workplace->headquarters->equipment;
        std::string description;
        for (const auto &id : equipment) {
            const auto model = std::ranges::find(offers, id, &HQEquipmentOffer::id);
            if (!description.empty())
                description += ", ";
            description += model == offers.end() ? id : model->name;
        }
        return "Install the required equipment together in one HQ room: " + description + ".";
    }
    const auto *current = except_project.empty() ? nullptr : project(state, except_project);
    if (!current || current->completed_work < current->total_work) {
        const auto source = std::ranges::find(state.capacity->sources, plan.source_id, &CapacitySource::id);
        if (source == state.capacity->sources.end() || company_capacity(*source).value() == 0)
            return "The selected compute source has no available output. Restore its power or supplier service.";
    }
    return {};
}

void require_workplace_research(const SimulationState &state, std::string_view topic, const LabAllocation &plan) {
    const auto blocker = research_workplace_blocker(state, topic, plan);
    if (!blocker.empty())
        throw std::invalid_argument(blocker);
}

void require_workplace_research_resume(const SimulationState &state, std::string_view id) {
    if (!workplace_research_enabled(state))
        return;
    const auto &runs = state.lab->research;
    const auto run = std::ranges::find(runs, id, &ResearchRun::project_id);
    if (run == runs.end())
        return;
    const auto *work = project(state, id);
    if (!work)
        throw std::invalid_argument("Research work is missing its capacity reservation.");
    const auto blocker = research_workplace_blocker(state, run->research_id, allocation(*run, *work), id);
    if (!blocker.empty())
        throw std::invalid_argument(blocker);
}

void reconcile_workplace_research(SimulationState &state) {
    if (!workplace_research_enabled(state) || state.lab->research.empty())
        return;
    std::set<std::string_view> active;
    for (const auto &work : state.capacity->projects)
        if (work.status == ProjectStatus::active && work.required_staff_days)
            active.insert(work.id);
    if (active.empty())
        return;
    // Copy only the IDs that must stop before changing capacity project storage.
    std::vector<std::string> pause;
    for (const auto &run : state.lab->research) {
        if (!active.contains(run.project_id))
            continue;
        const auto *work = project(state, run.project_id);
        if (!research_workplace_blocker(state, run.research_id, allocation(run, *work), work->id).empty())
            pause.push_back(work->id);
    }
    for (const auto &id : pause)
        pause_project(*state.capacity, state.date, id);
    if (!pause.empty())
        state.paused = true;
}
void validate_workplace_research(const SimulationState &state) {
    if (!workplace_research_enabled(state) || state.lab->research.empty())
        return;
    if (!state.management || !state.management->people || !state.infrastructure || !state.infrastructure->workplace ||
        !state.infrastructure->workplace->headquarters)
        throw std::invalid_argument("Equipment-backed research requires a named team and physical headquarters.");
    std::map<std::string_view, const Employee *> roster;
    for (const auto &employee : state.management->people->employees)
        roster.emplace(employee.terms.profile.id, &employee);
    std::map<std::string_view, const CapacityProject *> projects;
    for (const auto &work : state.capacity->projects)
        projects.emplace(work.id, &work);
    for (const auto &run : state.lab->research) {
        const auto *work = projects.at(run.project_id);
        for (const auto &id : run.researchers) {
            const auto worker = roster.find(id);
            if (worker == roster.end() || worker->second->ready_on > work->starts_on ||
                (worker->second->departed_on && *worker->second->departed_on < work->starts_on))
                throw std::invalid_argument("A research assignment must name an onboarded person in the company.");
        }
        if(state.lab->catalog.creative_research.contains(run.research_id)) {
            for(const auto& [id,traits]:run.original_traits) {
                if(std::ranges::find(run.researchers,id)==run.researchers.end() || traits.size()>person_trait_limit ||
                   std::set<std::string_view>{traits.begin(),traits.end()}.size()!=traits.size() ||
                   std::ranges::any_of(traits,[](const auto& value) {
                       return value.empty() || value.size()>80 || value.find('\0')!=value.npos;
                   })) throw std::invalid_argument("Invalid recorded research team traits.");
            }
            const auto team=research_team(state,run.researchers,run.original_traits);
            if(run.creativity!=team.creativity || run.compatibility_bp!=team.compatibility_bp) throw std::invalid_argument("Research team characteristics must match the actual saved people.");
        } else if(run.creativity || run.compatibility_bp || !run.original_traits.empty())
            throw std::invalid_argument("This research has no creative-team rule.");
        if (work->status == ProjectStatus::active &&
            !research_workplace_blocker(state, run.research_id, allocation(run, *work), work->id).empty())
            throw std::invalid_argument("Active research is missing its staff, physical equipment or compute.");
    }
}
} // namespace pgp::core
