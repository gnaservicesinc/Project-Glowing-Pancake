#include <pgp/core/talent.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const std::string& why) {throw std::invalid_argument(why);}
template<class Rows,class Id> auto& find(Rows& rows,const Id& id) {
    const auto it=std::ranges::find(rows,id,[](const auto& row){return row.id;});
    if(it==rows.end()) invalid("That skill, person or software project is unavailable.");return *it;
}
void label(std::string_view value,std::size_t size=128) {
    if(value.empty() || value.size()>size || std::ranges::any_of(value,[](unsigned char c){return c<32 || c==127;})) invalid("Use bounded, nonempty text without control characters.");
}
TalentState& talent(SimulationState& s) {
    if(!talent_rules(s) || !s.management->people->talent) invalid("Skills and software are unavailable in this company.");
    return *s.management->people->talent;
}
const Employee* employee(const SimulationState& s,std::string_view id) {
    if(!s.management || !s.management->people)return nullptr;
    const auto& rows=s.management->people->employees;
    const auto it=std::ranges::find_if(rows,[&](const auto& e){return id=="founder"?e.terms.profile.role=="Founder / researcher":e.terms.profile.id==id;});
    return it==rows.end()?nullptr:&*it;
}
std::string canonical_person(const SimulationState& s,std::string_view id) {
    const auto* e=employee(s,id);
    return e && e->terms.profile.role=="Founder / researcher"?"founder":std::string(id);
}
void pay_company(SimulationState& s,Cents cost,Cents protected_cash,const std::string& cause,const std::string& key) {
    if(!s.accounts || checked_unit_subtract(s.cash,protected_cash)<cost) invalid("Uncommitted company cash cannot cover this payment.");
    if(cost.value())record_cash(*s.accounts,s.date,Cents{-cost.value()},Account::expenses,cause,key);
    s.cash=accounts_report(*s.accounts).cash;
}
bool active_work(const SimulationState& s,std::string_view project) {
    const auto& rows=s.capacity->projects;const auto it=std::ranges::find(rows,project,&CapacityProject::id);
    return it!=rows.end() && it->status==ProjectStatus::active;
}
}
const TalentRules* talent_rules(const SimulationState& s) {
    return s.management && s.management->people && s.management->catalog.people && s.management->catalog.people->talent?&*s.management->catalog.people->talent:nullptr;
}
void validate_talent_rules(const TalentRules& r) {
    if(r.skills.empty() || r.skills.size()>96 || r.software.empty() || r.software.size()>12 || r.roles.size()>24 || r.market_bonus_max_bp<0 || r.market_bonus_max_bp>2000 || r.extra_skills<0 || r.extra_skills>4 ||
       r.employee_study_share_bp<0 || r.employee_study_share_bp>5000 || r.employee_study_interval_days<7 || r.employee_study_interval_days>365 ||
       r.design_bonus<0 || r.design_bonus>20 || r.focus_bonus<0 || r.focus_bonus>30 || r.focus_penalty<0 || r.focus_penalty>20)invalid("Talent catalogue exceeds its supported limits.");
    if(r.ai_architecture_floor<7500 || r.ai_programming_floor<r.ai_architecture_floor || r.ai_programming_floor>10000 ||
       r.ai_reliability_floor<7500 || r.ai_reliability_floor>10000 || r.ai_workflow_floor<7500 || r.ai_workflow_floor>10000 ||
       r.ai_compute_milli<1000 || r.ai_compute_milli>1000000 || r.ai_work_percent<25 || r.ai_work_percent>100)invalid("AI development needs bounded evidence, work and compute terms.");
    std::set<std::string> ids;
    for(const auto& skill:r.skills) {
        label(skill.id,64);label(skill.name);label(skill.description,512);
        if(!ids.insert(skill.id).second || skill.course_days<7 || skill.course_days>730 || skill.tuition_cents<0 || skill.tuition_cents>10000000 ||
           skill.engineering<0 || skill.engineering>20 || skill.research<0 || skill.research>20 || skill.evaluation<0 || skill.evaluation>20)invalid("Invalid skill or course terms.");
    }
    for(const auto& skill:r.skills) {
        std::set<std::string> seen{skill.id};auto dependency=skill.prerequisite;
        while(!dependency.empty()) {if(!ids.contains(dependency) || !seen.insert(dependency).second)invalid("Skill prerequisites must exist without cycles.");dependency=find(r.skills,dependency).prerequisite;}
    }
    for(const auto& [role,skill]:r.roles) {label(role,64);if(!ids.contains(skill))invalid("A job must name a defined base skill.");}
    for(const auto* needed:{"programming","architecture","ml_research","ui_design"})if(!ids.contains(needed))invalid("Software needs programming, architecture, ML and UI skills.");
    ids.clear();
    for(const auto& brief:r.software) {
        label(brief.id,64);label(brief.name);label(brief.description,512);
        if(!ids.insert(brief.id).second || brief.work_days<7 || brief.work_days>365 || brief.setup_cents<0 || brief.setup_cents>100000000 || brief.features.empty() || brief.features.size()>8)invalid("Invalid software brief.");
        for(const auto& feature:brief.features)label(feature);
    }
}
bool has_skill(const PersonProfile& p,std::string_view id) {return std::ranges::find(p.skills,id)!=p.skills.end();}
std::vector<std::string> person_skills(const SimulationState& s,std::string_view id) {
    if(!talent_rules(s))return {};
    if(canonical_person(s,id)=="founder")return s.management->people->talent->founder_skills;
    const auto* e=employee(s,id);return e?e->terms.profile.skills:std::vector<std::string>{};
}
bool person_has_skill(const SimulationState& s,std::string_view person,std::string_view id) {
    const auto skills=person_skills(s,person);return std::ranges::find(skills,id)!=skills.end();
}
std::string software_worker_blocker(const SimulationState& s,std::string_view id,std::string_view except) {
    const auto* e=employee(s,id);
    if(!e || e->departed_on)return "Needs a current employee or a qualified working founder.";
    if(e->ready_on>s.date)return "Still onboarding.";
    const auto key=canonical_person(s,id);
    if(!assigned_hq_desk(s,key) && !hq_laptop_ready(s,key))return "Needs an assigned HQ desk.";
    if(!hq_workstation_ready(s,key))return "Needs a powered computer on their desk.";
    const auto* manager=operations_manager_on(s,s.date);
    if(manager && manager->person==e->terms.profile.id)return "Already assigned to operations management.";
    if(s.lab)for(const auto& r:s.lab->research)
        if(r.project_id!=except && active_work(s,r.project_id) && std::ranges::find(r.researchers,e->terms.profile.id)!=r.researchers.end())return "Already assigned to research.";
    if(talent_rules(s))for(const auto& p:s.management->people->talent->software)
        if(p.project!=except && active_work(s,p.project) && std::ranges::any_of(p.design.workers,[&](const auto& who){return canonical_person(s,who)==key;}))return "Already assigned to software development.";
    return {};
}
int software_ai_level(const SimulationState& s) {
    const auto* r=talent_rules(s);if(!r || !s.frontier || !s.deployments || !s.market)return 0;
    int level=0;
    for(const auto& system:s.frontier->systems) {
        const auto evidence=frontier_qualification(*s.frontier,system.id);
        if(!evidence.agi_on || evidence.conservative[7]<static_cast<unsigned>(r->ai_reliability_floor) || evidence.conservative[8]<static_cast<unsigned>(r->ai_workflow_floor))continue;
        const bool available=std::ranges::all_of(system.deployments,[&](auto index){
            if(index==0 || index>s.deployments->records.size())return false;
            const auto& release=s.deployments->records[static_cast<std::size_t>(index-1)];
            const auto product=std::ranges::find(s.market->products,release.service_id,&MarketProduct::id);
            return product!=s.market->products.end() && product->available && product->capacity_project_id==release.project_id && active_work(s,release.project_id);
        });
        if(!available || system.deployments.empty())continue;
        if(evidence.conservative[3]>=static_cast<unsigned>(r->ai_programming_floor))level=2;
        else if(evidence.conservative[3]>=static_cast<unsigned>(r->ai_architecture_floor))level=std::max(level,1);
    }
    return level;
}
SoftwareQuote software_quote(const SimulationState& s,const SoftwareDesign& d,std::string_view except) {
    SoftwareQuote q;
    try {
        const auto* rules=talent_rules(s);if(!rules)invalid("Software Studio is unavailable.");
        const auto& r=*rules;const auto& brief=find(r.software,d.brief);label(d.name,80);
        if(d.aesthetic!="calm" && d.aesthetic!="dense" && d.aesthetic!="expressive")invalid("Choose a supported visual aesthetic.");
        if(d.navigation!="sidebar" && d.navigation!="top" && d.navigation!="command")invalid("Choose sidebar, top navigation or command-first navigation.");
        if(d.focus!="balanced" && d.focus!="usability" && d.focus!="reliability" && d.focus!="capability")invalid("Choose a development focus.");
        if(d.workers.empty() || d.workers.size()>8)invalid("Assign one to eight people with computers.");
        bool programmer=false,architect=false,designer=false;int engineering=0;
        std::set<std::string> unique;
        for(const auto& id:d.workers) {
            if(!unique.insert(canonical_person(s,id)).second)invalid("Each person may fill only one team place.");
            const auto why=software_worker_blocker(s,id,except);if(!why.empty())invalid(why);
            programmer|=person_has_skill(s,id,"programming");architect|=person_has_skill(s,id,"architecture");designer|=person_has_skill(s,id,"ui_design");
            engineering+=static_cast<int>(employee(s,id)->terms.profile.engineering);
        }
        if(!d.ai_source.empty()) {
            q.ai_level=software_ai_level(s);
            if(!q.ai_level)invalid("AI assistance requires independently qualified AGI, verified software, reliability and workflow evidence, and an active deployment.");
            const auto& source=find(s.capacity->sources,d.ai_source);
            if(company_capacity(source).value()<r.ai_compute_milli)invalid("Restore sufficient compute for AI-assisted development.");
        }
        programmer|=q.ai_level>=2;architect|=q.ai_level>=1;
        if(!programmer)invalid("Assign at least one person with Programming and a computer.");
        if(brief.advanced && !architect)invalid("This advanced software needs a Software Architecture skill on its team.");
        q.usability=40+(designer?r.design_bonus:0)+(d.aesthetic=="calm"?10:0)+(d.navigation=="sidebar"?5:0);
        q.reliability=40+engineering/static_cast<int>(d.workers.size())/5+(architect?10:0);
        q.capability=40+(d.aesthetic=="dense"?10:0)+(d.navigation=="command"?10:0);
        if(d.aesthetic=="expressive")q.usability+=5;
        if(d.navigation=="top")q.usability+=5;
        if(d.focus!="balanced") {
            q.usability+=d.focus=="usability"?r.focus_bonus:-r.focus_penalty;
            q.reliability+=d.focus=="reliability"?r.focus_bonus:-r.focus_penalty;
            q.capability+=d.focus=="capability"?r.focus_bonus:-r.focus_penalty;
        }
        q.staff_days=brief.work_days*(brief.advanced?2:1);
        if(q.ai_level)q.staff_days=std::max(7,q.staff_days*r.ai_work_percent/100);
        q.fee=Cents{brief.setup_cents};
        if(d.previous) {
            const auto& previous=find(s.management->people->talent->software,d.previous);
            if(!previous.released_on || previous.design.brief!=d.brief)invalid("Refinement needs a released design of the same software type.");
            q.staff_days=std::max(7,q.staff_days*3/4);
            q.usability=std::min(100,std::max(q.usability,previous.usability)+5);
            q.reliability=std::min(100,std::max(q.reliability,previous.reliability)+5);
            q.capability=std::min(100,std::max(q.capability,previous.capability)+5);
        }
    }catch(const std::invalid_argument& e){q.blocker=e.what();}
    return q;
}
void manage_talent(SimulationState& s,const TalentAction& action,Cents protected_cash) {
    auto& t=talent(s);const auto& r=*talent_rules(s);
    std::visit([&](const auto& a) {
        using T=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<T,EnrollSkill>) {
            reconcile_employee_study(s);
            const auto quote=course_quote(s,a);if(!quote.blocker.empty())invalid(quote.blocker);
            const auto& skill=find(r.skills,a.skill);const auto key=canonical_person(s,a.person);
            const auto end=advance_days(s.date,Days{skill.course_days});
            const auto fee=Cents{skill.tuition_cents};
            if(a.payer=="personal" && key=="founder") {
                if(!s.succession || !s.succession->personal_life || s.succession->personal_cash<fee || s.succession->personal_life->unpaid.value())invalid("Settle personal bills and retain enough savings for tuition.");
                s.succession->personal_cash=checked_unit_subtract(s.succession->personal_cash,fee);
                s.succession->personal_life->spent=checked_unit_add(s.succession->personal_life->spent,fee);
            } else if(a.payer=="company") pay_company(s,fee,protected_cash,"Online university tuition","course/"+std::to_string(t.courses.size()+1));
            else if(a.payer=="personal") {
                auto& fund=t.study_funds.at(key);fund.spent=checked_unit_add(fund.spent,fee);
            } else invalid("Choose personal tuition or company sponsorship.");
            t.courses.push_back({static_cast<std::uint32_t>(t.courses.size()+1),key,a.skill,a.payer,s.date,end,false});
        } else if constexpr(std::is_same_v<T,StartSoftware>) {
            if(t.software.size()>=256)invalid("The software portfolio is full.");
            const auto q=software_quote(s,a.design);if(!q.blocker.empty())invalid(q.blocker);
            const auto id=static_cast<std::uint32_t>(t.software.size()+1);
            CapacityProject work;work.id="software/"+std::to_string(id);work.kind=ProjectKind::software;work.starts_on=s.date;
            work.staff=static_cast<std::uint32_t>(a.design.workers.size());work.required_staff_days=static_cast<std::uint32_t>(q.staff_days);work.cause="Software: "+a.design.name;
            if(q.ai_level) {
                const auto& source=find(s.capacity->sources,a.design.ai_source);
                work.source_id=source.id;work.interconnect=source.interconnect;work.requested=MilliCUPerDay{r.ai_compute_milli};
                work.total_work=MilliCU{((q.staff_days+static_cast<int>(work.staff)-1)/static_cast<int>(work.staff))*static_cast<std::int64_t>(r.ai_compute_milli)};
            }
            reserve_project(*s.capacity,s.date,work);
            pay_company(s,q.fee,protected_cash,work.cause,work.id);
            t.software.push_back({id,a.design,work.id,s.date,{},q.usability,q.reliability,q.capability,{}});
        } else if constexpr(std::is_same_v<T,ReleaseSoftware>) {
            auto& p=find(t.software,a.id);const auto& work=find(s.capacity->projects,p.project);
            if(p.released_on || work.status!=ProjectStatus::completed)invalid("Complete development before releasing this design.");
            p.released_on=s.date;
        } else if constexpr(std::is_same_v<T,ResumeSoftware>) {
            auto& p=find(t.software,a.id);const auto q=software_quote(s,p.design,p.project);if(!q.blocker.empty())invalid(q.blocker);
            resume_project(*s.capacity,s.date,p.project);p.blocker.clear();
        }
    },action);
}
void prepare_talent(SimulationState& s) {
    if(!talent_rules(s))return;
    reconcile_employee_study(s);
    auto& t=talent(s);const auto& rules=*talent_rules(s);
    for(auto& c:t.courses)if(!c.completed && c.completes_on<=s.date) {
        const auto& skill=find(rules.skills,c.skill);
        if(c.person=="founder")t.founder_skills.push_back(c.skill);
        auto* e=const_cast<Employee*>(employee(s,c.person));
        if(e) {
            auto& p=e->terms.profile;
            if(!has_skill(p,c.skill))p.skills.push_back(c.skill);
            p.engineering=std::min(100U,p.engineering+static_cast<unsigned>(skill.engineering));
            p.research=std::min(100U,p.research+static_cast<unsigned>(skill.research));
            // Evaluation credit is a signed hiring term; do not rewrite it after study.
        }
        c.completed=true;s.paused=true;
    }
    prepare_employee_study(s);
}
void reconcile_software(SimulationState& s) {
    if(!talent_rules(s))return;
    for(auto& p:talent(s).software)if(active_work(s,p.project)) {
        const auto q=software_quote(s,p.design,p.project);
        if(!q.blocker.empty()) {pause_project(*s.capacity,s.date,p.project);p.blocker=q.blocker;s.paused=true;}
    }
}
BasisPoints software_market_bonus(const SimulationState& s,ProductChannel channel,std::optional<OrdinalDay> day) {
    if(!talent_rules(s))return BasisPoints{0};
    int best=0;
    for(const auto& p:s.management->people->talent->software)if(p.released_on && *p.released_on<=day.value_or(s.date)) {
        const auto& type=p.design.brief;
        const bool fits=channel==ProductChannel::consumer?(type=="chat" || type=="desktop" || type=="browser" || type=="megaclient"):
            channel==ProductChannel::agent?(type=="coding" || type=="work" || type=="megaclient"):
            channel==ProductChannel::business?(type=="desktop" || type=="megaclient"):false;
        if(fits)best=std::max(best,(p.usability+p.reliability+p.capability)*talent_rules(s)->market_bonus_max_bp/300);
    }
    return BasisPoints{best};
}
Cents personal_tuition(const SimulationState& s,OrdinalDay before) {
    Cents total{0};if(!talent_rules(s))return total;
    for(const auto& c:s.management->people->talent->courses)if(c.payer=="personal" && c.person=="founder" && c.started_on<before)total=checked_unit_add(total,Cents{find(talent_rules(s)->skills,c.skill).tuition_cents});
    return total;
}
void validate_talent(const SimulationState& s,bool deep) {
    if(!s.management || !s.management->people)return;
    const auto* r=talent_rules(s);const auto& state=s.management->people->talent;
    if(static_cast<bool>(r)!=state.has_value())invalid("Talent state and content must agree.");if(!r)return;
    const auto& t=*state;
    const auto skills=[&](const auto& rows){if(rows.size()>r->skills.size())invalid("Too many skills.");std::set<std::string> unique;for(const auto& id:rows){(void)find(r->skills,id);if(!unique.insert(id).second)invalid("Duplicate skill.");}};
    skills(t.founder_skills);
    for(const auto& e:s.management->people->employees)skills(e.terms.profile.skills);
    for(const auto& a:s.management->people->applicants){skills(a.profile.skills);const auto role=r->roles.find(a.profile.role);if(role!=r->roles.end() && !has_skill(a.profile,role->second))invalid("Applicants need their advertised job's base skill.");}
    for(const auto& a:s.management->people->queued_applicants){skills(a.profile.skills);const auto role=r->roles.find(a.profile.role);if(role!=r->roles.end() && !has_skill(a.profile,role->second))invalid("Queued applicants need their advertised job skill.");}
    if(t.courses.size()>2048 || t.software.size()>256)invalid("Talent history exceeds its limits.");
    std::set<std::string> active;
    for(std::size_t i=0;i<t.courses.size();++i) {
        const auto& c=t.courses[i];const auto& skill=find(r->skills,c.skill);
        if(c.id!=i+1 || !skill.teachable || c.started_on>s.date || c.completes_on!=advance_days(c.started_on,Days{skill.course_days}) ||
           c.completed!=(c.completes_on<=s.date) || (c.payer!="company" && c.payer!="personal") ||
           (c.person!="founder" && !employee(s,c.person)) || (!c.completed && !active.insert(c.person).second) ||
           (c.completed && !person_has_skill(s,c.person,c.skill)))invalid("Course identity, dates, payer or acquired skill differs from its record.");
    }
    validate_employee_study(s,deep);
    if(static_cast<std::size_t>(std::ranges::count(s.capacity->projects,ProjectKind::software,&CapacityProject::kind))!=t.software.size())invalid("Software work must belong to a recorded design.");
    for(std::size_t i=0;i<t.software.size();++i) {
        const auto& p=t.software[i];const auto& work=find(s.capacity->projects,p.project);
        if(p.id!=i+1 || p.project!="software/"+std::to_string(p.id) || p.design.previous>=p.id || work.kind!=ProjectKind::software || work.starts_on!=p.started_on ||
           work.staff!=p.design.workers.size() || p.usability<0 || p.usability>100 || p.reliability<0 || p.reliability>100 || p.capability<0 || p.capability>100 ||
           (p.released_on && (work.status!=ProjectStatus::completed || *p.released_on>s.date || *p.released_on<*work.stopped_on)))invalid("Software history does not reconcile with its work.");
        (void)find(r->software,p.design.brief);label(p.design.name,80);
        std::set<std::string> unique;
        for(const auto& id:p.design.workers)if(!employee(s,id) || !unique.insert(canonical_person(s,id)).second)invalid("Software needs distinct recorded workers.");
        if(work.status==ProjectStatus::active && !software_quote(s,p.design,p.project).blocker.empty())invalid("Active software is missing skills, staff or computers.");
    }
}
}
