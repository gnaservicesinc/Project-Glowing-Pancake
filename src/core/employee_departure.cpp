#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace pgp::core {
namespace {
constexpr std::string_view departure_prefix="dismissal/";
const PeopleRules& rules(const SimulationState& state) {
    if(!state.management || !state.management->catalog.people || !state.management->people || !state.accounts || !state.capacity)
        throw std::invalid_argument("This company has no individual employment records.");
    return *state.management->catalog.people;
}
const Employee& employee(const SimulationState& state,std::string_view id) {
    (void)rules(state);
    const auto& roster=state.management->people->employees;
    const auto found=std::ranges::find(roster,id,[](const auto& row){return row.terms.profile.id;});
    if(found==roster.end()) throw std::invalid_argument("Choose an employee of this company.");
    return *found;
}
bool unfinished(const CapacityProject& project) {
    return project.status==ProjectStatus::active || project.status==ProjectStatus::paused;
}
std::vector<const Employee*> opening_workers(const SimulationState& state) {
    std::vector<const Employee*> workers;
    for(const auto& row:state.management->people->employees)
        if(row.opening && row.terms.profile.role!="Founder / researcher") workers.push_back(&row);
    return workers;
}
} // namespace

Cents employee_monthly_salary(const SimulationState& state,const Employee& worker) {
    if(!worker.opening) return worker.terms.salary;
    if(!state.accounts) return Cents{0};
    const auto salary=std::ranges::find(state.accounts->recurring,worker.terms.profile.id,&RecurringObligation::id);
    return salary==state.accounts->recurring.end()?Cents{0}:salary->monthly_amount;
}

void initialize_individual_payroll(SimulationState& state) {
    if(!rules(state).dismissal) return;
    const auto workers=opening_workers(state);
    const auto grouped=std::ranges::find(state.accounts->recurring,std::string_view{"payroll_with_benefits"},&RecurringObligation::id);
    if(workers.empty()) return;
    if(grouped==state.accounts->recurring.end() || grouped->monthly_amount.value()<static_cast<std::int64_t>(workers.size()))
        throw std::invalid_argument("Opening employees need a funded salary allocation.");
    const auto total=grouped->monthly_amount.value();
    end_recurring(*state.accounts,"payroll_with_benefits",state.date);
    for(std::size_t i=0;i<workers.size();++i) {
        const auto& person=workers[i]->terms.profile;
        const auto salary=total/static_cast<std::int64_t>(workers.size())+(i<static_cast<std::size_t>(total%static_cast<std::int64_t>(workers.size()))?1:0);
        add_recurring(*state.accounts,{person.id,Cents{salary},state.date,{},"Employee salary: "+person.name});
    }
}

DismissalQuote dismissal_quote(const SimulationState& state,std::string_view id) {
    const auto& policy=rules(state);
    if(!policy.dismissal) throw std::invalid_argument("Individual dismissal is unavailable in this scenario.");
    const auto& worker=employee(state,id);
    if(worker.departed_on) throw std::invalid_argument("This employee has already left the company.");
    if(worker.terms.profile.role=="Founder / researcher") throw std::invalid_argument("Use the leadership handover to leave your own company.");
    DismissalQuote quote;
    quote.monthly_salary=employee_monthly_salary(state,worker);
    quote.severance=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(quote.monthly_salary.value()),policy.dismissal->severance_months,1)};
    std::set<std::string_view> named_work;
    if(state.lab) for(const auto& run:state.lab->research)
        if(std::ranges::find(run.researchers,id)!=run.researchers.end()) named_work.insert(run.project_id);
    if(talent_rules(state))for(const auto& run:state.management->people->talent->software)
        if(std::ranges::find(run.design.workers,id)!=run.design.workers.end())named_work.insert(run.project);
    const auto* manager=operations_manager_on(state,state.date);
    quote.operations_manager=manager && manager->person==id;
    if(quote.operations_manager) named_work.insert(manager->project);
    std::uint64_t released=0;
    for(const auto& work:state.capacity->projects) if(unfinished(work) && (named_work.contains(work.id) || work.specialist_id==id)) {
        quote.projects.push_back(work.id);
        if(work.status==ProjectStatus::active) released+=work.staff;
    }
    const auto available=state.capacity->available_staff;
    const auto assigned=capacity_report(*state.capacity).assigned_staff;
    if(worker.ready_on<=state.date && (available==0 || assigned-released>available-1))
        throw std::invalid_argument("Release another staff commitment before dismissal; the remaining team cannot cover it.");
    if(state.infrastructure && state.infrastructure->workplace && state.infrastructure->workplace->headquarters)
        for(const auto& item:state.infrastructure->workplace->headquarters->items) if(item.person==id) ++quote.returned_items;
    return quote;
}

void dismiss_employee(SimulationState& state,std::string_view id,Cents protected_cash) {
    const auto quote=dismissal_quote(state,id);
    if(checked_unit_subtract(state.cash,protected_cash)<quote.severance)
        throw std::invalid_argument("Uncommitted company cash does not cover this employee's severance.");
    std::string manager_project;
    if(quote.operations_manager) {
        manager_project=operations_manager_on(state,state.date)->project;
        assign_operations_manager(state,{"",""});
    }
    for(const auto& project:quote.projects) if(project!=manager_project) cancel_project(*state.capacity,state.date,project);
    auto& roster=state.management->people->employees;
    auto& worker=*std::ranges::find(roster,id,[](const auto& row){return row.terms.profile.id;});
    const auto key=std::string{departure_prefix}+std::string{id};
    if(worker.ready_on<=state.date) remove_staff_capacity(*state.capacity,state.date,key,1);
    end_recurring(*state.accounts,id,state.date);
    if(quote.severance>Cents{0}) record_cash(*state.accounts,state.date,Cents{-quote.severance.value()},Account::expenses,
        "Employee severance: "+worker.terms.profile.name,key);
    if(state.infrastructure && state.infrastructure->workplace && state.infrastructure->workplace->headquarters)
        for(auto& item:state.infrastructure->workplace->headquarters->items) if(item.person==id) item.person.clear();
    worker.departed_on=state.date;
    worker.dismissal_severance=quote.severance;
    state.cash=accounts_report(*state.accounts).cash;
}

void validate_employee_departures(const SimulationState& state) {
    const auto& policy=rules(state);
    const auto& roster=state.management->people->employees;
    std::map<std::string_view,const RecurringObligation*> salaries;
    for(const auto& row:state.accounts->recurring) salaries.emplace(row.id,&row);
    std::map<std::string_view,const StaffAddition*> departures;
    for(const auto& row:state.capacity->staff_departures) if(row.id.starts_with(departure_prefix)) departures.emplace(row.id,&row);
    std::map<std::string_view,const Posting*> severances;
    for(const auto& row:state.accounts->postings) if(row.key.starts_with(departure_prefix))
        if(!severances.emplace(row.key,&row).second) throw std::invalid_argument("Employee severance cannot be paid twice.");
    std::size_t expected_departures=0,expected_payments=0;
    std::set<std::string_view> dismissed;
    for(const auto& worker:roster) {
        if(worker.opening && policy.dismissal && worker.terms.profile.role!="Founder / researcher") {
            const auto salary=salaries.find(worker.terms.profile.id);
            if(salary==salaries.end() || salary->second->starts_on!=worker.joined_on || salary->second->ends_before!=worker.departed_on)
                throw std::invalid_argument("Inherited employee payroll must end on their departure date.");
        }
        if(!worker.dismissal_severance) continue;
        dismissed.insert(worker.terms.profile.id);
        if(!policy.dismissal || !worker.departed_on || worker.terms.profile.role=="Founder / researcher" ||
            *worker.dismissal_severance!=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(employee_monthly_salary(state,worker).value()),policy.dismissal->severance_months,1)})
            throw std::invalid_argument("A dismissal must retain its dated employment and authored severance.");
        const auto key=std::string{departure_prefix}+worker.terms.profile.id;
        if(worker.ready_on<=*worker.departed_on) {
            ++expected_departures;
            const auto departure=departures.find(key);
            if(departure==departures.end() || departure->second->day!=*worker.departed_on || departure->second->people!=1)
                throw std::invalid_argument("A dismissal must release exactly one onboarded employee.");
        }
        if(*worker.dismissal_severance>Cents{0}) {
            ++expected_payments;
            const auto posting=severances.find(key);
            if(posting==severances.end() || posting->second->day!=*worker.departed_on || posting->second->amount!=*worker.dismissal_severance ||
                posting->second->debit!=Account::expenses || posting->second->credit!=Account::cash)
                throw std::invalid_argument("Dismissal severance differs from its company payment.");
        }
    }
    if(departures.size()!=expected_departures || severances.size()!=expected_payments)
        throw std::invalid_argument("A departure or severance payment has no dismissed employee.");
    if(!dismissed.empty()) {
        if(state.infrastructure && state.infrastructure->workplace && state.infrastructure->workplace->headquarters)
            for(const auto& item:state.infrastructure->workplace->headquarters->items)
                if(dismissed.contains(item.person)) throw std::invalid_argument("Dismissed employees must return company equipment assignments.");
        std::set<std::string_view> unfinished_work;
        for(const auto& project:state.capacity->projects) if(unfinished(project)) {
            unfinished_work.insert(project.id);
            if(dismissed.contains(project.specialist_id)) throw std::invalid_argument("A departed specialist cannot retain unfinished work.");
        }
        if(state.lab) for(const auto& run:state.lab->research) if(unfinished_work.contains(run.project_id))
            for(const auto& id:run.researchers) if(dismissed.contains(id))
                throw std::invalid_argument("Dismissed researchers must be released from unfinished projects.");
    }
    if(policy.dismissal) {
        const auto workers=opening_workers(state);
        if(!workers.empty()) {
            const auto grouped=salaries.find("payroll_with_benefits");
            if(grouped==salaries.end() || grouped->second->ends_before!=state.management->people->starts_on)
                throw std::invalid_argument("The inherited payroll must be allocated once at company opening.");
            const auto total=grouped->second->monthly_amount.value();
            for(std::size_t i=0;i<workers.size();++i) {
                const auto expected=total/static_cast<std::int64_t>(workers.size())+(i<static_cast<std::size_t>(total%static_cast<std::int64_t>(workers.size()))?1:0);
                if(employee_monthly_salary(state,*workers[i])!=Cents{expected})
                    throw std::invalid_argument("Inherited employee salaries must reconcile to their original payroll.");
            }
        }
    }
}
} // namespace pgp::core
