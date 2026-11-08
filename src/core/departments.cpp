#include <pgp/core/departments.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <array>
#include <set>
#include <unordered_map>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
bool present(const SimulationState& s) {return s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->departments.has_value();}
const DepartmentRules& rules(const SimulationState& s) {
    if(!present(s) || !s.infrastructure->catalog.workplace->departments) invalid("This scenario has no HQ operations management plan.");return *s.infrastructure->catalog.workplace->departments;
}
const Employee& employee(const SimulationState& s,std::string_view id) {
    if(!s.management || !s.management->people) invalid("Operations management requires the individual HQ roster.");
    const auto& rows=s.management->people->employees;const auto it=std::ranges::find(rows,id,[](const auto& row){return row.terms.profile.id;});
    if(it==rows.end()) invalid("Choose a hired HQ employee for operations management.");return *it;
}
const CapacityProject& project(const SimulationState& s,const OperationsAppointment& a) {
    const auto it=std::ranges::find(s.capacity->projects,a.project,&CapacityProject::id);
    if(it==s.capacity->projects.end()) invalid("An operations assignment lost its funded work reservation.");return *it;
}
OrdinalDay project_end(const CapacityProject& p) {
    if(!p.stopped_on) invalid("An ended operations assignment has no recorded project ending.");
    return p.status==ProjectStatus::completed?advance_days(*p.stopped_on,Days{1}):*p.stopped_on;
}
}
void validate_department_rules(const DepartmentRules& r) {
    if(r.operator_skill>100 || r.maximum_assignments<1 || r.maximum_assignments>1024 || r.minimum_availability<BasisPoints{5000} ||
        r.minimum_availability>BasisPoints{10000} || r.operator_skill_credit>BasisPoints{5000} ||
        r.minimum_availability.value()+r.operator_skill_credit.value()>10000 || r.manager_credit>BasisPoints{2500} ||
        r.management_compute<MilliCUPerDay{10} || r.management_compute>MilliCUPerDay{1000}) invalid("Operations skills, availability, assignment history or management compute are out of bounds.");
}
void initialize_departments(SimulationState& s) {
    if(s.infrastructure && s.infrastructure->catalog.workplace && s.infrastructure->catalog.workplace->departments)
        s.infrastructure->workplace->departments=DepartmentsState{};
}
const OperationsAppointment* operations_manager_on(const SimulationState& s,OrdinalDay day) {
    if(!present(s)) return nullptr;const auto& rows=s.infrastructure->workplace->departments->operations;
    for(auto it=rows.rbegin();it!=rows.rend();++it) if(it->starts_on<=day && (!it->ends_before || *it->ends_before>day)) return &*it;
    return nullptr;
}
bool operations_project(const SimulationState& s,std::string_view id) {
    return present(s) && std::ranges::any_of(s.infrastructure->workplace->departments->operations,[&](const auto& a){return a.project==id;});
}
BasisPoints operations_availability(const SimulationState& s,OrdinalDay day) {
    if(!present(s)) return BasisPoints{10000};const auto& r=rules(s);
    auto result=static_cast<std::uint64_t>(r.minimum_availability.value())+mul_div_floor(static_cast<std::uint64_t>(r.operator_skill_credit.value()),r.operator_skill,100).quotient;
    const auto* manager=operations_manager_on(s,day);
    if(manager && day>s.capacity->starts_on) {
        const auto index=static_cast<std::uint64_t>(day.value()-s.capacity->starts_on.value()-1);
        if(index<s.capacity->days.size()) {
            const auto& prior=s.capacity->days[static_cast<std::size_t>(index)];
            const auto work=std::ranges::find(prior.projects,manager->project,&ProjectDayWork::project_id);
            if(prior.day==OrdinalDay{day.value()-1} && work!=prior.projects.end()) {
                const auto delivered=std::min<std::uint64_t>(static_cast<std::uint64_t>(work->work.value()),static_cast<std::uint64_t>(r.management_compute.value()));
                result+=mul_div_floor(std::array<std::uint64_t,3>{static_cast<std::uint64_t>(r.manager_credit.value()),employee(s,manager->person).terms.profile.engineering,delivered},100ULL*static_cast<std::uint64_t>(r.management_compute.value())).quotient;
            }
        }
    }
    return BasisPoints{std::min<std::uint64_t>(10000,result)};
}
void prepare_departments(SimulationState& s) {
    if(!present(s)) return;
    for(auto& a:s.infrastructure->workplace->departments->operations) if(!a.ends_before) {
        if(project(s,a).status==ProjectStatus::active && s.date>=a.planned_until) cancel_project(*s.capacity,s.date,a.project);
        const auto& p=project(s,a);
        if(p.status!=ProjectStatus::active) a.ends_before=project_end(p);
    }
}
void assign_operations_manager(SimulationState& s,const AssignOperationsManager& action) {
    const auto& r=rules(s);prepare_departments(s);auto& rows=s.infrastructure->workplace->departments->operations;
    const auto* current=operations_manager_on(s,s.date);
    if(current && current->person==action.person && current->source==action.source) invalid("This employee already manages operations using that compute allocation.");
    if(!action.person.empty()) {
        const auto& person=employee(s,action.person);
        if(person.ready_on>s.date || person.departed_on) invalid("Wait for onboarding and choose an active HQ employee.");
        if(rows.size()>=r.maximum_assignments) invalid("The saved operations assignment history is full.");
    } else if(!current) invalid("Operations has no assigned HQ manager.");
    if(current) {cancel_project(*s.capacity,s.date,current->project);for(auto& a:rows) if(a.id==current->id) a.ends_before=s.date;}
    if(action.person.empty()) return;
    const auto source=std::ranges::find(s.capacity->sources,action.source,&CapacitySource::id);if(source==s.capacity->sources.end()) invalid("Choose an existing compute source for the manager's daily work.");
    const auto id=rows.size()+1;CapacityProject p;p.id="hq.operations/"+std::to_string(id);p.source_id=source->id;p.kind=ProjectKind::management;
    p.starts_on=s.date;p.requested=r.management_compute;p.staff=1;p.interconnect=source->interconnect;
    p.total_work=integrate(p.requested,Days{s.end_date.value()-s.date.value()});p.cause="HQ operations coordination: "+employee(s,action.person).terms.profile.name;
    reserve_project(*s.capacity,s.date,p);rows.push_back({id,action.person,p.id,p.source_id,s.date,s.end_date,{}});
}
void validate_departments(const SimulationState& s,bool deep) {
    const auto& w=*s.infrastructure->workplace;const auto& catalog=*s.infrastructure->catalog.workplace;
    if(w.departments.has_value()!=catalog.departments.has_value()) invalid("HQ operations management differs from its authored rules.");if(!w.departments) return;
    if(!s.management || !s.management->people || !s.capacity) invalid("Operations managers require individual HQ people and company capacity.");
    const auto& r=rules(s);const auto& rows=w.departments->operations;if(rows.size()>r.maximum_assignments) invalid("Operations assignment history exceeds its bound.");
    std::set<std::string_view> projects;
    std::unordered_map<std::string_view,const CapacityProject*> indexed;
    if(deep) {indexed.reserve(s.capacity->projects.size());for(const auto& p:s.capacity->projects) indexed.emplace(p.id,&p);}
    // Daily advancement can only finish the latest appointment. Earlier records
    // remain immutable; load and non-daily transactions validate the full history.
    const auto first=deep || rows.empty()?std::size_t{0}:rows.size()-1;
    std::optional<OrdinalDay> previous_end=first?rows[first-1].ends_before:std::optional<OrdinalDay>{};
    for(std::size_t i=first;i<rows.size();++i) {
        const auto& a=rows[i];const auto& person=employee(s,a.person);
        const auto found=deep?indexed.find(a.project):indexed.end();
        if(deep && found==indexed.end()) invalid("An operations assignment lost its funded work reservation.");
        const auto& p=deep?*found->second:project(s,a);if(deep) projects.insert(a.project);
        if(a.id!=i+1 || a.project!="hq.operations/"+std::to_string(a.id) || a.starts_on<person.ready_on || a.starts_on<w.starts_on || a.starts_on>s.date || a.planned_until<=a.starts_on || a.planned_until>s.end_date ||
            (a.ends_before && (*a.ends_before<a.starts_on || *a.ends_before>s.date)) || (i && (!previous_end || *previous_end>a.starts_on)) ||
            (person.departed_on && (!a.ends_before || *a.ends_before>*person.departed_on))) invalid("Operations appointments overlap or use unavailable people.");
        if(p.kind!=ProjectKind::management || p.source_id!=a.source || p.starts_on!=a.starts_on || p.staff!=1 || p.requested!=r.management_compute ||
            p.total_work!=integrate(r.management_compute,Days{a.planned_until.value()-a.starts_on.value()}) || !p.pauses.empty() ||
            (a.ends_before?(p.status==ProjectStatus::active || *a.ends_before!=project_end(p)):p.status!=ProjectStatus::active)) invalid("Operations management must retain its real staff, compute and dated project reservation.");
        previous_end=a.ends_before;
    }
    if(deep) for(const auto& p:s.capacity->projects) if(p.kind==ProjectKind::management && !projects.contains(p.id)) invalid("An operations management project has no employee appointment.");
}
}
