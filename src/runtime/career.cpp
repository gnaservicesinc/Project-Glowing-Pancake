#include <pgp/runtime/career.hpp>
#include <pgp/core/workplace_research.hpp>
#include "competition_internal.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
constexpr std::string_view salary_id="career/research-appointment";
constexpr std::string_view salary_prefix="recurring/career/research-appointment/";

bool active(const RivalCompany& employer,OrdinalDay day) {
    return employer.planning && employer.planning->entered_on && employer.simulation.date==day &&
        employer.simulation.phase==DayPhase::work_ready && !employer.response;
}
const RivalCompany* employer_for(const WorldState& world,std::string_view id) {
    const auto& companies=world.competition->companies;
    const auto found=std::ranges::find(companies,id,&RivalCompany::definition_id);
    return found==companies.end()?nullptr:&*found;
}
RecurringObligation salary(const WorldState& world,const content::RivalDefinition& definition,OrdinalDay day) {
    const auto monthly=Cents{definition.monthly_payroll.value()/definition.staff};
    return {std::string(salary_id),monthly,day,world.objectives->deadline,
        "Research appointment: "+world.identity->founder_name};
}
Cents reserve(const SimulationState& employer,const content::RivalPlanning& policy,Cents extra=Cents{0}) {
    auto monthly=extra;
    for(const auto& bill:employer.accounts->recurring)
        if(bill.starts_on<=employer.date && (!bill.ends_before || employer.date<*bill.ends_before))
            monthly=checked_unit_add(monthly,bill.monthly_amount);
    return checked_unit_add(policy.reserve_floor,Cents{mul_div_floor(static_cast<std::uint64_t>(monthly.value()),policy.runway_months,1).quotient});
}
ResearchAppointment& appointment(WorldState& world) {return world.objectives->career->back();}
bool can_defer_training(const CapacityProject& project) {
    return project.kind==ProjectKind::training && project.status==ProjectStatus::active && project.pauses.size()<128;
}
std::vector<std::string> training_to_defer(const SimulationState& state) {
    std::vector<std::string> projects;
    for(const auto& project:state.capacity->projects)
        if(can_defer_training(project)) projects.push_back(project.id);
    return projects;
}
}

bool can_continue_career(const WorldState& world) {
    if(!world.founding || !world.identity || !world.competition || !world.competition->setup ||
        !world.objectives || world.objectives->mode!=ObjectiveMode::retrospective || !world.objectives->end ||
        world.objectives->end->day>=world.objectives->deadline) return false;
    switch(world.objectives->end->cause) {
    case CampaignEndCause::insolvency: return world.simulation.phase==DayPhase::closed;
    case CampaignEndCause::institutional_control:
    case CampaignEndCause::final_order: return world.simulation.phase==DayPhase::work_ready;
    default: return false;
    }
}
bool career_open(const WorldState& world) {
    return can_continue_career(world) && campaign_day(world)<world.objectives->deadline;
}
const content::RivalDefinition* career_fellowship(const WorldState& world,const CompiledProgram& program) {
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if(!scenario) return nullptr;
    const auto found=std::ranges::find_if(scenario->rivals,[](const auto& row){
        return row.planning && row.planning->group=="fellowship";
    });
    return found==scenario->rivals.end()?nullptr:&*found;
}
const ResearchAppointment* career_appointment(const WorldState& world,std::string_view employer) {
    if(!world.objectives || !world.objectives->career) return nullptr;
    const auto& history=*world.objectives->career;
    const auto found=std::ranges::find(history,employer,&ResearchAppointment::employer);
    return found==history.end()?nullptr:&*found;
}
const RivalCompany* career_employer(const WorldState& world) {
    if(!career_open(world) || !world.objectives->career || world.objectives->career->empty()) return nullptr;
    const auto* employer=employer_for(world,world.objectives->career->back().employer);
    return employer && active(*employer,campaign_day(world))?employer:nullptr;
}
const RecurringObligation* career_payroll(const RivalCompany& employer) {
    if(!employer.simulation.accounts) return nullptr;
    const auto& rows=employer.simulation.accounts->recurring;
    const auto found=std::ranges::find(rows,salary_id,&RecurringObligation::id);
    return found==rows.end()?nullptr:&*found;
}
std::vector<ResearchJobOffer> research_job_offers(const WorldState& world,const CompiledProgram& program) {
    if(!career_open(world) || career_employer(world)) return {};
    if(world.objectives->career && world.objectives->career->size()>=career_appointment_limit) return {};
    const auto day=campaign_day(world);
    std::vector<ResearchJobOffer> offers;
    for(const auto& employer:world.competition->companies) {
        if(!active(employer,day) || !employer.parent_company.empty() || !employer.simulation.management->people) continue;
        const auto& company=employer.simulation;
        if(company.liquidity->status==LiquidityStatus::workout || company.accounts->recurring.size()>=accounts_terms_limit ||
            career_payroll(employer)) continue;
        const auto& definition=rival_definition(program,employer.definition_id);
        const auto monthly=salary(world,definition,day).monthly_amount;
        if(monthly.value()>0 && uncommitted_cash(company)>=reserve(company,*definition.planning,monthly) &&
            std::ranges::any_of(research_workers(company),[](const auto& worker){return worker.unavailable.empty();}))
            offers.push_back({definition.id,definition.name,monthly});
    }
    if(offers.empty()) {
        const auto* fellowship=career_fellowship(world,program);
        if(fellowship && !employer_for(world,fellowship->id))
            offers.push_back({fellowship->id,fellowship->name,salary(world,*fellowship,day).monthly_amount,true});
    }
    return offers;
}
std::vector<CareerResearchOption> career_research_options(const WorldState& world,const CompiledProgram& program) {
    const auto* employer=career_employer(world);
    if(!employer || world.objectives->career->back().projects.size()>=career_project_limit) return {};
    const auto& state=employer->simulation;
    const auto& definition=rival_definition(program,employer->definition_id);
    const auto report=capacity_report(*state.capacity);
    if(state.liquidity->status==LiquidityStatus::workout) return {};
    auto assigned=report.assigned_staff;
    for(const auto& project:state.capacity->projects)
        if(can_defer_training(project)) assigned-=project.staff;
    if(assigned>=report.available_staff) return {};
    const auto runway=reserve(state,*definition.planning);
    if(uncommitted_cash(state)<runway) return {};
    std::vector<LabAllocation> allocations;
    for(const auto& source:state.capacity->sources) {
        auto spare=company_capacity(source).value();
        for(const auto& project:state.capacity->projects) if(project.status==ProjectStatus::active && project.source_id==source.id) {
            if(can_defer_training(project)) continue;
            const auto rate=project.kind==ProjectKind::serving || project.kind==ProjectKind::replication?
                project.requested.value():std::min(project.requested.value(),project.total_work.value()-project.completed_work.value());
            spare=checked_subtract(spare,rate);
        }
        if(spare>0) allocations.push_back({source.id,MilliCUPerDay{std::min(spare,definition.training_rate.value())},1});
    }
    if(allocations.empty()) return {};
    if(workplace_research_enabled(state)) {
        auto workers=research_workers(state);
        std::erase_if(workers,[](const auto& worker){return !worker.unavailable.empty();});
        std::vector<LabAllocation> named;
        // Form a sequence of teams by adding the person with the most new
        // perspectives. Keep the first assignment small when it meets the brief.
        std::vector<std::vector<std::string>> teams;
        std::vector<std::string> selected;
        const auto limit=std::min<std::size_t>({workers.size(),report.available_staff-assigned,32});
        while(selected.size()<limit) {
            const auto best=std::ranges::max_element(workers,{},[&](const auto& worker) {
                auto candidate=selected;
                candidate.push_back(worker.id);
                const auto traits=research_team(state,candidate);
                return std::pair{traits.creativity,traits.compatibility_bp};
            });
            selected.push_back(best->id);
            workers.erase(best);
            teams.push_back(selected);
        }
        for(const auto& allocation:allocations) for(const auto& team:teams) {
            auto named_allocation=allocation;
            named_allocation.staff=static_cast<std::uint32_t>(team.size());
            named_allocation.researchers=team;
            named.push_back(std::move(named_allocation));
        }
        allocations=std::move(named);
    }
    std::vector<CareerResearchOption> result;
    if(state.lab->research.size()>=lab_record_limit || state.capacity->projects.size()>=capacity_project_limit) return {};
    const auto& current_projects=world.objectives->career->back().projects;
    for(const auto& node:state.lab->catalog.research) {
        if(!frontier_technology_block(state,FrontierTechnology::research,node.id,state.date).empty()) continue;
        const auto fee=research_cash_fee(state,node.id);
        if(fee>checked_unit_subtract(uncommitted_cash(state),runway)) continue;
        const auto& runs=state.lab->research;
        const auto complete=std::ranges::count_if(runs,[&](const auto& run){return run.research_id==node.id && run.completed;});
        if(complete>=node.repeat_limit || std::ranges::any_of(runs,[&](const auto& run) {
            if(run.research_id!=node.id) return false;
            const auto project=std::ranges::find(state.capacity->projects,run.project_id,&CapacityProject::id);
            return project!=state.capacity->projects.end() &&
                (project->status==ProjectStatus::active || project->status==ProjectStatus::paused);
        })) continue;
        // Publication is once per topic. Finish the private result already in the desk first.
        const bool already_public=std::ranges::find(state.lab->publications,node.id,&ResearchPublication::research_id)!=state.lab->publications.end();
        if(!already_public && std::ranges::any_of(current_projects,[&](const auto& job) {
            return job.topic==node.id && std::ranges::any_of(runs,[&](const auto& run){return run.project_id==job.project && run.completed;});
        })) continue;
        const auto threshold=research_creativity_threshold(*state.lab,*state.capacity,node.id,state.date);
        if(threshold==UINT32_MAX) continue;
        std::optional<CareerResearchOption> best;
        for(const auto& allocation:allocations) {
            if(!research_workplace_blocker(state,node.id,allocation).empty()) continue;
            const auto output=staffed_output(static_cast<std::uint64_t>(allocation.daily_compute.value()),staff_output_factor(*state.capacity));
            const auto rate=static_cast<std::int64_t>(std::max<std::uint64_t>(1,output));
            const auto compute_days=(node.experiment_work.value()+rate-1)/rate;
            const auto compatibility=research_team(state,allocation.researchers).compatibility_bp;
            const auto required=coordinated_staff_days(state.lab->catalog,node,compatibility);
            const auto staff_days=(required+allocation.staff-1)/allocation.staff;
            const Days duration{std::max<std::int64_t>(compute_days,staff_days)};
            if(!best || duration<best->estimated_duration)
                best=CareerResearchOption{node.id,fee,allocation,duration};
        }
        if(best && best->estimated_duration.value()<world.objectives->deadline.value()-state.date.value())
            result.push_back(std::move(*best));
    }
    std::ranges::sort(result,[](const auto& a,const auto& b) {
        return std::tie(a.estimated_duration,a.topic)<std::tie(b.estimated_duration,b.topic);
    });
    return result;
}
void accept_research_appointment(WorldState& world,const CompiledProgram& program,std::string_view id) {
    const auto offers=research_job_offers(world,program);
    const auto offer=std::ranges::find(offers,id,&ResearchJobOffer::employer);
    if(offer==offers.end())
        throw std::invalid_argument("Choose an available appointment funded by an operating lab.");
    auto& history=world.objectives->career;
    if(!history) history.emplace();
    const auto day=campaign_day(world);
    const auto& definition=rival_definition(program,id);
    auto& companies=world.competition->companies;
    if(offer->fellowship) {
        RivalCompany employer;
        employer.definition_id=definition.id;
        employer.planning=RivalPlanState{day,day,{}};
        employer.simulation=competition_detail::opening_rival(world,program,definition,day);
        const auto bill=salary(world,definition,day);
        if(bill.monthly_amount.value()<=0 ||
            uncommitted_cash(employer.simulation)<reserve(employer.simulation,*definition.planning,bill.monthly_amount) ||
            std::ranges::none_of(research_workers(employer.simulation),[](const auto& worker){return worker.unavailable.empty();}))
            throw std::invalid_argument("The fellowship endowment does not cover its opening obligations.");
        companies.push_back(std::move(employer));
        world.competition->notices.push_back({definition.id,"entry",day,std::nullopt});
    }
    auto& employer=*std::ranges::find(companies,id,&RivalCompany::definition_id);
    add_recurring(*employer.simulation.accounts,salary(world,definition,day));
    history->push_back({std::string(id),day,{}});
    world.objectives->observer_day=day;
    world.objectives->observer_paused=true;
    if(offer->fellowship) competition_detail::review_rival(world,program,employer);
}
void direct_career_research(WorldState& world,const CompiledProgram& program,std::string_view id) {
    const auto options=career_research_options(world,program);
    const auto selected=std::ranges::find(options,id,&CareerResearchOption::topic);
    if(selected==options.end()) throw std::invalid_argument("This topic needs available lab funding, prerequisites and staff.");
    auto& job=appointment(world);
    auto& employer=*std::ranges::find(world.competition->companies,job.employer,&RivalCompany::definition_id);
    for(const auto& project:training_to_defer(employer.simulation))
        if(!competition_detail::submit_rival(employer,PauseProject{project}))
            throw std::invalid_argument("The lab could not defer its training work to free research resources.");
    if(!competition_detail::submit_rival(employer,StartResearch{selected->topic,selected->allocation}))
        throw std::invalid_argument("The employer could not commit the quoted research resources.");
    job.projects.push_back({selected->topic,employer.simulation.lab->research.back().project_id,{}});
    world.objectives->observer_paused=true;
}
void publish_career_research(WorldState& world,std::string_view id) {
    if(!career_employer(world)) throw std::invalid_argument("An active research appointment is required to publish.");
    auto& job=appointment(world);
    auto& employer=*std::ranges::find(world.competition->companies,job.employer,&RivalCompany::definition_id);
    const auto research=std::ranges::find_if(job.projects,[&](const auto& project) {
        if(project.topic!=id || project.published_on) return false;
        const auto& runs=employer.simulation.lab->research;
        return std::ranges::any_of(runs,[&](const auto& row){return row.project_id==project.project && row.completed;});
    });
    if(research==job.projects.end() || !competition_detail::submit_rival(employer,PublishResearch{research->topic}))
        throw std::invalid_argument("Publish a completed, private result from your appointment.");
    research->published_on=campaign_day(world);
    world.objectives->observer_paused=true;
    competition_detail::collect_public_results(world,core::OrdinalDay{campaign_day(world).value()-1});
}
Cents career_salary_received(const WorldState& world) {
    Cents paid{0};
    if(!world.objectives || !world.objectives->career) return paid;
    for(const auto& job:*world.objectives->career) {
        const auto* employer=employer_for(world,job.employer);
        const auto& accounts=*employer->simulation.accounts;
        if(!accounts.closed_through) continue;
        auto month=from_ordinal(job.accepted_on);
        month.day=1;
        while(to_ordinal(month)<=*accounts.closed_through) {
            const auto invoice=accounts.invoices.find(std::string(salary_prefix)+iso_date(to_ordinal(month)));
            if(invoice) paid=checked_unit_add(paid,accounts.invoices[*invoice].settled);
            if(month.month==12) {++month.year;month.month=1;}
            else ++month.month;
        }
    }
    return paid;
}
void validate_career(const WorldState& world,const CompiledProgram& program,bool deep) {
    if(!world.objectives || !world.objectives->career) {
        if(deep && world.competition &&
            std::ranges::any_of(world.competition->companies,[](const auto& employer){return career_payroll(employer);}))
            throw std::invalid_argument("An employer salary needs its actual career appointment.");
        return;
    }
    const auto& objectives=*world.objectives;
    const auto& history=*objectives.career;
    const auto invalid=[] {throw std::invalid_argument("The saved career must match actual employer payroll, research and dates.");};
    if(!can_continue_career(world) || !objectives.observer_day || history.empty() || history.size()>career_appointment_limit) invalid();
    std::set<std::string_view> employers;
    const RivalCompany* previous=nullptr;
    for(const auto& job:history) {
        const auto* employer=employer_for(world,job.employer);
        if(!employer || employer->response || !employer->parent_company.empty() ||
            !employer->planning || !employer->planning->entered_on || *employer->planning->entered_on>job.accepted_on ||
            !employer->simulation.accounts || !employers.insert(job.employer).second ||
            job.accepted_on<objectives.end->day || job.accepted_on>employer->simulation.date || job.accepted_on>=objectives.deadline ||
            (previous && (previous->simulation.phase!=DayPhase::closed || previous->simulation.date>job.accepted_on)) ||
            job.projects.size()>career_project_limit) invalid();
        previous=employer;
        const auto& state=employer->simulation;
        const auto* bill=career_payroll(*employer);
        if(!bill || *bill!=salary(world,rival_definition(program,job.employer),job.accepted_on)) invalid();
        if(!deep) continue;
        std::set<std::string_view> projects;
        auto earliest=job.accepted_on;
        for(const auto& project:job.projects) {
            const auto run=std::ranges::find(state.lab->research,project.project,&ResearchRun::project_id);
            const auto work=std::ranges::find(state.capacity->projects,project.project,&CapacityProject::id);
            if(!projects.insert(project.project).second || run==state.lab->research.end() || run->research_id!=project.topic ||
                work==state.capacity->projects.end() || work->starts_on<earliest) invalid();
            earliest=work->starts_on;
            if(project.published_on) {
                const auto publication=std::ranges::find(state.lab->publications,project.topic,&ResearchPublication::research_id);
                if(!run->completed || !work->stopped_on || *project.published_on<=*work->stopped_on ||
                    publication==state.lab->publications.end() || publication->day!=project.published_on) invalid();
            }
        }
    }
    if(deep) for(const auto& employer:world.competition->companies)
        if(career_payroll(employer) && !employers.contains(employer.definition_id)) invalid();
}
}
