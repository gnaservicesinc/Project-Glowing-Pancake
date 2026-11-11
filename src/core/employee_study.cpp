#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
const Employee* worker(const SimulationState& s,std::string_view id) {
    if(!s.management || !s.management->people)return nullptr;
    const auto& rows=s.management->people->employees;
    const auto it=std::ranges::find_if(rows,[&](const auto& e){return id=="founder"?e.terms.profile.role=="Founder / researcher":e.terms.profile.id==id;});
    return it==rows.end()?nullptr:&*it;
}
std::string key(const SimulationState& s,std::string_view id) {
    const auto* e=worker(s,id);return e && e->terms.profile.role=="Founder / researcher"?"founder":std::string{id};
}
std::string compensation_person(const Posting& p) {
    if(p.credit!=Account::cash)return {};
    // Only cash actually paid counts. Accrued/unpaid payroll cannot fund a class.
    if(p.debit==Account::payables && p.document_id.starts_with("recurring/") && p.document_id.size()>21)
        return p.document_id.substr(10,p.document_id.size()-21); // prefix + /YYYY-MM-DD
    if(p.debit==Account::expenses && p.document_id.empty())
        return p.key.starts_with("dismissal/")?p.key.substr(10):p.key;
    return {};
}
Cents budget(const TalentRules& r,const TalentState::StudyFund& fund) {
    return checked_unit_subtract(Cents{mul_div_floor(static_cast<std::uint64_t>(fund.compensation.value()),static_cast<std::uint64_t>(r.employee_study_share_bp),10000).quotient},fund.spent);
}
bool useful(const SkillDefinition& skill) {return skill.engineering>0 || skill.research>0 || skill.id=="ui_design";}
}

CourseQuote course_quote(const SimulationState& s,const EnrollSkill& a) {
    CourseQuote q;
    try {
        const auto* rules=talent_rules(s);if(!rules)throw std::invalid_argument("Online university is unavailable.");
        const auto& t=*s.management->people->talent;
        const auto found=std::ranges::find(rules->skills,a.skill,&SkillDefinition::id);
        if(found==rules->skills.end())throw std::invalid_argument("Choose an available course.");
        const auto& skill=*found;const auto who=key(s,a.person);q.fee=Cents{skill.tuition_cents};
        if(t.courses.size()>=2048 || !skill.teachable)throw std::invalid_argument("This qualification is not available through an online class.");
        if(who!="founder") {const auto* e=worker(s,a.person);if(!e || e->departed_on || e->ready_on>s.date)throw std::invalid_argument("Choose an onboarded employee.");}
        if(person_has_skill(s,who,a.skill))throw std::invalid_argument("This person already has that skill.");
        if(!skill.prerequisite.empty() && !person_has_skill(s,who,skill.prerequisite))throw std::invalid_argument("Complete the prerequisite skill first.");
        if(std::ranges::any_of(t.courses,[&](const auto& c){return c.person==who && !c.completed;}))throw std::invalid_argument("Finish the current part-time course before enrolling again.");
        if(advance_days(s.date,Days{skill.course_days})>=s.end_date)throw std::invalid_argument("The course would finish after the campaign.");
        if(a.payer=="company") {
            q.budget=uncommitted_cash(s);if(q.budget<q.fee)throw std::invalid_argument("Uncommitted company cash cannot cover this payment.");
        } else if(a.payer=="personal" && who=="founder") {
            if(!s.succession || !s.succession->personal_life || s.succession->personal_life->unpaid.value())throw std::invalid_argument("Settle personal bills and retain enough savings for tuition.");
            q.budget=s.succession->personal_cash;if(q.budget<q.fee)throw std::invalid_argument("Settle personal bills and retain enough savings for tuition.");
        } else if(a.payer=="personal") {
            const auto fund=t.study_funds.find(who);
            if(!rules->employee_study_share_bp || fund==t.study_funds.end())throw std::invalid_argument("This employee has no self-funded learning budget; offer sponsorship.");
            q.budget=budget(*rules,fund->second);
            if(q.budget<q.fee)throw std::invalid_argument("They are still saving from paid compensation. Offer sponsorship or check back after payday.");
        } else throw std::invalid_argument("Choose personal tuition or company sponsorship.");
    }catch(const std::invalid_argument& e){q.blocker=e.what();}
    return q;
}

void reconcile_employee_study(SimulationState& s) {
    const auto* r=talent_rules(s);if(!r || !r->employee_study_share_bp)return;
    auto& t=*s.management->people->talent;
    for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher" && !t.study_funds.contains(e.terms.profile.id))
        t.study_funds.emplace(e.terms.profile.id,TalentState::StudyFund{Cents{0},Cents{0},advance_days(e.ready_on,Days{r->employee_study_interval_days})});
    if(t.compensation_cursor>s.accounts->postings.size())throw std::invalid_argument("Employee compensation cursor is ahead of company payments.");
    // Incremental scan: ordinary days never rescan the company's full history.
    for(;t.compensation_cursor<s.accounts->postings.size();++t.compensation_cursor) {
        const auto& p=s.accounts->postings[static_cast<std::size_t>(t.compensation_cursor)];
        const auto fund=t.study_funds.find(compensation_person(p));
        if(fund!=t.study_funds.end())fund->second.compensation=checked_unit_add(fund->second.compensation,p.amount);
    }
}

void prepare_employee_study(SimulationState& s) {
    const auto* r=talent_rules(s);if(!r || !r->employee_study_share_bp)return;
    auto& t=*s.management->people->talent;
    for(const auto& e:s.management->people->employees) {
        const auto fund=t.study_funds.find(e.terms.profile.id);
        if(fund==t.study_funds.end() || e.departed_on || e.ready_on>s.date || fund->second.next_review>s.date)continue;
        fund->second.next_review=advance_days(s.date,Days{r->employee_study_interval_days});
        for(const auto& skill:r->skills)if(useful(skill)) {
            EnrollSkill course{e.terms.profile.id,skill.id,"personal"};
            if(course_quote(s,course).blocker.empty()) {manage_talent(s,course,Cents{0});break;}
        }
    }
}

void validate_employee_study(const SimulationState& s,bool deep) {
    const auto* r=talent_rules(s);if(!r)return;
    const auto& t=*s.management->people->talent;
    if(!r->employee_study_share_bp) {
        if(!t.study_funds.empty() || t.compensation_cursor || std::ranges::any_of(t.courses,[](const auto& c){return c.payer=="personal" && c.person!="founder";}))throw std::invalid_argument("Self-funded study requires authored employee learning budgets.");
        return;
    }
    if(t.study_funds.size()!=static_cast<std::size_t>(std::ranges::count_if(s.management->people->employees,[](const auto& e){return e.terms.profile.role!="Founder / researcher";})) || t.compensation_cursor>s.accounts->postings.size())
        throw std::invalid_argument("Employee learning funds must belong to the actual roster.");
    std::map<std::string,Cents> fees,received;
    for(const auto& c:t.courses)if(c.payer=="personal" && c.person!="founder") {
        const auto skill=std::ranges::find(r->skills,c.skill,&SkillDefinition::id);
        if(skill==r->skills.end() || !skill->teachable)throw std::invalid_argument("Employee self-study needs an available course.");
        auto [it,added]=fees.try_emplace(c.person,Cents{0});it->second=checked_unit_add(it->second,Cents{skill->tuition_cents});
    }
    if(deep)for(std::size_t i=0;i<t.compensation_cursor;++i) {
        const auto& p=s.accounts->postings[i];const auto who=compensation_person(p);
        if(!t.study_funds.contains(who))continue;
        auto [it,added]=received.try_emplace(who,Cents{0});it->second=checked_unit_add(it->second,p.amount);
    }
    for(const auto& [who,fund]:t.study_funds) {
        const auto* e=worker(s,who);const auto fee=fees.find(who),paid=received.find(who);
        if(!e || e->terms.profile.role=="Founder / researcher" || fund.compensation<Cents{0} || fund.spent<Cents{0} || budget(*r,fund)<Cents{0} ||
           fund.spent!=(fee==fees.end()?Cents{0}:fee->second) || fund.next_review<e->ready_on ||
           fund.next_review>advance_days(std::max(s.date,e->ready_on),Days{r->employee_study_interval_days}) ||
           (deep && fund.compensation!=(paid==received.end()?Cents{0}:paid->second)))throw std::invalid_argument("Employee study savings differ from paid compensation, tuition or review dates.");
    }
}
}
