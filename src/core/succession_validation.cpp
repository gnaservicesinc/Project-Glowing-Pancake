#include "succession_internal.hpp"
#include <set>

namespace pgp::core {
namespace {
using namespace succession_detail;
void label(std::string_view value,std::size_t limit=128) {
    if(value.empty() || value.size()>limit || value.find('\0')!=value.npos) invalid("Leadership text exceeds its supported bounds.");
}
bool pending(SuccessionStatus status) {return status==SuccessionStatus::proposed || status==SuccessionStatus::approved;}
}
void validate_succession_catalog(const SuccessionCatalog& c) {
    if(c.personal_life) validate_personal_life_catalog(*c.personal_life);
    label(c.founder_name);label(c.founder_board,64);
    if(c.founder_birth_year<1800 || c.founder_birth_year>9000 || c.executive_retirement_age<60 || c.executive_retirement_age>80 ||
        c.chair_retirement_age<c.executive_retirement_age || c.chair_retirement_age>90 || c.minimum_candidate_age<25 ||
        c.maximum_candidate_age<c.minimum_candidate_age || c.maximum_candidate_age>=c.executive_retirement_age ||
        c.maximum_term_years<5 || c.maximum_term_years>30 || c.caretaker_term_years<3 || c.caretaker_term_years>10 ||
        c.caretaker_term_years>c.maximum_term_years || c.chair_seats==0 || c.chair_seats>3 || c.trust_seats>c.chair_seats ||
        c.transition_days.value()<30 || c.transition_days.value()>365 || c.planning_notice<c.transition_days || c.planning_notice.value()>1096 ||
        c.vote_window.value()<7 || c.vote_window.value()>60 || c.vote_window>=c.transition_days || c.transition_work.value()<=0 || c.transition_work.value()>100000000 ||
        c.founder_salary.value()<0 || c.founder_salary.value()>10000000 || c.chair_salary.value()<0 || c.chair_salary>c.founder_salary ||
        c.cash_floor.value()<0 || c.cash_floor.value()>1000000000 || c.personal_cash.value()<0 || c.personal_cash.value()>1000000000000 ||
        c.personal_liabilities.value()<0 || c.personal_liabilities.value()>1000000000000 || c.tracks.size()<2 || c.tracks.size()>4)
        invalid("The leadership charter needs finite careers, preparation, board rights and compensation.");
    if(c.departure) {
        const auto& rules=*c.departure;
        if(rules.messages.empty() || rules.messages.size()>8) invalid("Departure needs one to eight authored messages.");
        if(rules.press) {
            if(rules.press->reasons.empty() || rules.press->reasons.size()>8) invalid("Choose one to eight resignation reasons.");
            std::set<std::string> reasons;
            for(const auto& reason:rules.press->reasons) {
                label(reason.id,48);label(reason.title);label(reason.copy,2048);
                if(!reasons.insert(reason.id).second) invalid("Resignation reasons need distinct IDs.");
            }
            for(const auto effect:rules.press->challenged_effect)
                if(effect < -1000 || effect > 0) invalid("Challenged claims cannot grant public confidence.");
        }
        std::set<std::string> messages;
        for(const auto& m:rules.messages) {
            label(m.id,48);label(m.title);label(m.copy,2048);
            if(!messages.insert(m.id).second || m.fee.value()<0 || m.fee.value()>10000000) invalid("Departure messages need distinct IDs and bounded fees.");
            if(m.claim && (!rules.press || (*m.claim!="profit" && *m.claim!="research" &&
                *m.claim!="delivery" && *m.claim!="publication_commitment")))
                invalid("Departure claims need a supported fact check and press rules.");
            for(auto v:m.completed_effect) if(v < -1000 || v > 1000) invalid("Departure audience effects must be bounded.");
        }
        for(auto v:rules.broken_effect) if(v < -1000 || v > 0) invalid("A broken departure commitment cannot improve public standing.");
    }
    if(c.asi) {
        const auto& ai=*c.asi;label(ai.priorities,512);
        if(ai.signing_cost.value()<0 || ai.signing_cost.value()>100000000 || ai.monthly_operations.value()<=0 || ai.monthly_operations.value()>10000000 ||
            !ai.maximum_term_years || ai.maximum_term_years>c.maximum_term_years || ai.control_scrutiny_below>10000)
            invalid("ASI appointments need finite terms and disclosed hosted-operation costs.");
        for(const auto effect:ai.appointment_effect) if(effect < -1000 || effect > 1000) invalid("AI leadership reactions must be bounded.");
        for(const auto effect:ai.weak_control_effect) if(effect < -1000 || effect > 0) invalid("Weak control evidence cannot grant public confidence.");
    }
    std::set<std::string> ids;
    for(const auto& track:c.tracks) {
        label(track.id,48);label(track.priorities,512);
        if(track.id=="asi" || track.id.starts_with("asi.") || track.id=="outside" || track.id.starts_with("outside.") ||
            !ids.insert(track.id).second || track.first_birth_year<1800 || track.first_birth_year>9000 || track.names.empty() || track.names.size()>8 ||
            track.cohort_years<10 || track.cohort_years>c.maximum_candidate_age-c.minimum_candidate_age+1 ||
            track.signing_cost.value()<0 || track.signing_cost.value()>100000000 || track.monthly_salary.value()<=0 || track.monthly_salary.value()>10000000)
            invalid("Leadership tracks need distinct identities, recurring cohorts and explicit paid terms.");
        for(const auto& name:track.names) label(name);
    }
    if(c.outside_candidates) {
        if(c.outside_candidates->empty() || c.outside_candidates->size()>4)
            invalid("Choose one to four named outside leadership candidates.");
        for(const auto& candidate:*c.outside_candidates) {
            label(candidate.id,48);label(candidate.name);label(candidate.background,512);label(candidate.priorities,512);
            if(!candidate.id.starts_with("outside.") || candidate.id.size()==8 || !ids.insert(candidate.id).second ||
                candidate.birth_year<1800 || candidate.birth_year>9000 || candidate.signing_cost.value()<0 ||
                candidate.signing_cost.value()>100000000 || candidate.monthly_salary.value()<=0 || candidate.monthly_salary.value()>10000000)
                invalid("Outside leadership candidates need distinct identities, birth years and explicit paid terms.");
            for(const auto effect:candidate.appointment_effect)
                if(effect < -1000 || effect > 1000) invalid("Outside appointment reactions must be bounded.");
        }
    }
}
void validate_succession_opening(const SuccessionCatalog& c,OrdinalDay start,OrdinalDay end) {
    if(from_ordinal(start).year-c.founder_birth_year<18) succession_detail::invalid("The founding executive must be an adult at the scenario opening.");
    const auto retirement=succession_detail::year_day(c.founder_birth_year+static_cast<int>(c.executive_retirement_age));
    const auto needs=start>=retirement?start:succession_detail::term_due(c,c.founder_birth_year,start,false);
    if(needs<end && leadership_candidates(c,needs).empty())
        succession_detail::invalid("The first executive retirement needs an eligible successor cohort within this campaign.");
}
void validate_succession(const SimulationState& s,bool deep) {
    using namespace succession_detail;
    const bool configured=s.management && s.management->catalog.succession.has_value();
    if(configured!=s.succession.has_value()) invalid("The leadership charter and its saved history must be present together.");
    if(!s.succession) return;
    const auto& actor=*s.succession;const auto& c=succession_catalog(s);
    if(c.asi && (!s.frontier || !s.frontier->catalog.asi_route)) invalid("ASI leadership requires this company's independent qualification route.");
    if (actor.founder_name) label(*actor.founder_name);
    if(!s.accounts || !s.capacity || actor.terms.empty() || actor.controls.empty() || actor.terms.size()>succession_term_limit ||
        actor.controls.size()>succession_notice_limit || actor.plans.size()>succession_plan_limit || actor.notices.size()>succession_notice_limit ||
        actor.founder_holder!=s.management->catalog.scoring.founder || actor.posting_cursor>s.accounts->postings.size())
        invalid("The founder trust needs its actual company, financial cursor and bounded leadership history.");
    if(c.departure.has_value()!=actor.releases.has_value()) invalid("Departure rules and saved releases must be present together.");
    validate_personal_life(s,deep);
    if(!deep) return;
    validate_founder_departure(s);
    validate_succession_catalog(c);
    const auto formation_day=(s.infrastructure && s.infrastructure->workplace)
        ? s.infrastructure->workplace->starts_on
        : s.accounts->starts_on;
    validate_succession_opening(c,formation_day,s.end_date);
    Cents paid=c.personal_cash;std::vector<Cents> cash_prefix{Cents{0}};cash_prefix.reserve(s.accounts->postings.size()+1);
    for(const auto& posting:s.accounts->postings) {
        if(founder_payment(posting)) paid=checked_unit_add(paid,posting.amount);
        auto cash=cash_prefix.back();
        if(posting.debit==Account::cash) cash=checked_unit_add(cash,posting.amount);
        if(posting.credit==Account::cash) cash=checked_unit_subtract(cash,posting.amount);
        cash_prefix.push_back(cash);
    }
    if(actor.personal_life) paid=checked_unit_subtract(paid,actor.personal_life->spent);
    if(paid!=actor.personal_cash) invalid("Founder personal cash differs from paid compensation and private spending.");
    const auto in_time=[&](OrdinalDay day){return day>=s.accounts->starts_on && day<=s.date;};
    std::set<std::string> salaries,projects;
    for(std::size_t i=0;i<actor.terms.size();++i) {
        const auto& term=actor.terms[i];
        if(term.portrait) validate_portrait(*term.portrait);
        if(term.id!=i+1 || !in_time(term.starts_on) || term.due_on!=term_due(c,term.birth_year,term.starts_on,term.caretaker,term.asi.has_value()) || term.due_on<=term.starts_on ||
            (i==0?term.starts_on!=formation_day:term.starts_on<=actor.terms[i-1].starts_on || actor.terms[i-1].ended_on!=term.starts_on) ||
            (i+1==actor.terms.size()?term.ended_on.has_value():!term.ended_on || *term.ended_on>term.due_on))
            invalid("An executive term bypassed its actual handover or finite retirement boundary.");
        Cents salary{0};
        if(term.person=="founder") {
            if(i!=0 || term.plan || term.caretaker || term.asi || term.name!=actor.founder_name.value_or(c.founder_name) || term.birth_year!=c.founder_birth_year)
                invalid("A successor inherited the original founder's identity.");
            salary=c.founder_salary;
        } else {
            if(term.plan>actor.plans.size()) invalid("A handover has no actual approved plan.");
            const auto person=leadership_candidate(s,term.person,term.plan?actor.plans[term.plan-1].proposed_on:term.starts_on);
            const auto eligible=term.caretaker?leadership_candidates(c,term.starts_on):leadership_candidates(s,term.starts_on);
            if(term.name!=person.name || term.birth_year!=person.birth_year || term.asi!=person.asi || (term.asi && term.portrait) ||
                std::ranges::none_of(eligible,[&](const auto& p){return p.id==term.person;}))
                invalid("The executive's person or age differs from the actual eligible cohort.");
            salary=person.monthly_salary;
            if(term.plan) {
                if(term.plan>actor.plans.size()) invalid("A handover has no actual approved plan.");
                const auto& p=actor.plans[term.plan-1];
                if(p.status!=SuccessionStatus::activated || p.candidate!=term.person || p.incumbent+1!=term.id || p.finished_on!=term.starts_on || term.caretaker || p.asi!=term.asi)
                    invalid("The executive term differs from its approved handover.");
            } else {
                if(!term.caretaker || (i && term.starts_on!=actor.terms[i-1].due_on)) invalid("An unplanned appointment bypassed the charter's caretaker boundary.");
                auto chosen=eligible.begin();
                if(i) chosen=std::ranges::find_if(eligible,[&](const auto& p){return p.id!=actor.terms[i-1].person;});
                if(chosen==eligible.end()) chosen=eligible.begin();
                if(chosen==eligible.end() || chosen->id!=term.person) invalid("Caretaker selection differs from the charter's stable order.");
            }
        }
        if(salary.value()>0) {
            const auto id=salary_id(term);const auto& obligation=named(s.accounts->recurring,id);salaries.insert(id);
            if(obligation.monthly_amount!=salary || obligation.starts_on!=term.starts_on || obligation.ends_before!=term.ended_on)
                invalid("Executive compensation differs from its actual dated engagement.");
        }
    }
    if(s.phase!=DayPhase::closed && s.date<s.end_date && s.date>=actor.terms.back().due_on)
        invalid("The current executive remained after the retirement deadline.");
    bool chair=false;std::optional<OrdinalDay> chair_start,chair_end;
    for(std::size_t i=0;i<actor.controls.size();++i) {
        const auto& control=actor.controls[i];
        if(!in_time(control.day) || control.term==0 || control.term>actor.terms.size() || static_cast<unsigned>(control.role)>2 ||
            control.day<actor.terms[control.term-1].starts_on ||
            (i && (control.day<actor.controls[i-1].day || control.term<actor.controls[i-1].term || control.term>actor.controls[i-1].term+1 || control.role<actor.controls[i-1].role)))
            invalid("Leadership control changed outside its recorded executive relationship.");
        if(i==0 && (control.term!=1 || control.day!=actor.terms.front().starts_on ||
            control.role!=(actor.terms.front().person=="founder"?FounderRole::executive:FounderRole::trust)))
            invalid("Opening leadership control differs from the founding role.");
        if(i && control.term==actor.controls[i-1].term && !(actor.controls[i-1].role==FounderRole::chair && control.role==FounderRole::trust))
            invalid("Only the founder's remaining chair role can retire within one executive term.");
        if(control.role==FounderRole::executive && control.term!=1) invalid("The founding executive returned after succession.");
        if(control.role==FounderRole::chair && !chair) {chair_start=control.day;chair=true;}
        if(control.role!=FounderRole::chair && chair && !chair_end) chair_end=control.day;
        if(control.role==FounderRole::chair && control.day>=year_day(c.founder_birth_year+static_cast<int>(c.chair_retirement_age)))
            invalid("The personal founder chair exceeded its retirement date.");
    }
    for(const auto& term:actor.terms) if(std::ranges::none_of(actor.controls,[&](const auto& r){return r.term==term.id && r.day==term.starts_on;}))
        invalid("An executive appointment has no board-control record.");
    if(founder_role(actor)==FounderRole::chair && s.date<s.end_date && s.phase!=DayPhase::closed &&
        s.date>=year_day(c.founder_birth_year+static_cast<int>(c.chair_retirement_age))) invalid("The founder's personal chair tenure expired.");
    if(chair_start && c.chair_salary.value()>0) {
        const auto& stipend=named(s.accounts->recurring,"leadership/founder.chair");salaries.insert(stipend.id);
        if(stipend.starts_on!=chair_start || stipend.ends_before!=chair_end || stipend.monthly_amount!=c.chair_salary)
            invalid("The founder chair stipend differs from its actual personal role.");
    }
    std::size_t outstanding=0;
    for(std::size_t i=0;i<actor.plans.size();++i) {
        const auto& p=actor.plans[i];
        if(p.id!=i+1 || !in_time(p.proposed_on) || p.incumbent==0 || p.incumbent>actor.terms.size() || p.vote_before!=advance_days(p.proposed_on,c.vote_window) ||
            static_cast<unsigned>(p.status)>6 || (p.finished_on && (!in_time(*p.finished_on) || *p.finished_on<p.proposed_on)))
            invalid("The succession proposal lost its identity or response window.");
        check_proposal(s,{p.candidate,p.handover_on,p.retain_chair,p.allocation},p.proposed_on,p.incumbent);
        if(pending(p.status)) {
            ++outstanding;
            if(p.incumbent!=actor.terms.back().id || p.finished_on || (p.status==SuccessionStatus::proposed && s.date>=p.vote_before))
                invalid("A pending leadership plan outlived its executive or decision window.");
        } else if(!p.finished_on) invalid("A closed leadership proposal needs an actual finish date.");
        const auto person=leadership_candidate(s,p.candidate,p.proposed_on);
        if(p.asi!=person.asi) invalid("The ASI nomination differs from its dated independent qualification and control evidence.");
        if(p.decided_on) {
            if(p.charter_cursor.has_value()!=s.management->governance.has_value())
                invalid("Leadership approval needs its actual corporate charter history.");
            if(p.charter_cursor) {
                const auto& charters=s.management->governance->charters;
                const auto cursor=*p.charter_cursor;
                if(cursor>charters.size() || (cursor && charters[cursor-1].day>*p.decided_on) ||
                    (cursor<charters.size() && charters[cursor].day<*p.decided_on) ||
                    (cursor==0 && !charters.empty() && charters.front().motion.empty()))
                    invalid("Leadership approval used charter terms from a different date.");
            }
            if(!p.player_vote || !in_time(*p.decided_on) || *p.decided_on<p.proposed_on || *p.decided_on>=p.vote_before || p.posting_cursor>s.accounts->postings.size() ||
                p.control_cursor==0 || p.control_cursor>actor.controls.size() || actor.controls[p.control_cursor-1].day>*p.decided_on ||
                (p.control_cursor<actor.controls.size() && actor.controls[p.control_cursor].day<*p.decided_on) ||
                cash_prefix[p.posting_cursor]!=p.reviewed_cash || p.ballots!=succession_ballots(s,p,*p.player_vote,person.signing_cost))
                invalid("Leadership approval differs from actual cash, dated board seats or recorded votes.");
            std::uint32_t yes=0,total=0;for(const auto& vote:p.ballots) {total+=vote.seats;if(vote.support) yes+=vote.seats;}
            const bool approved=yes>total/2;
            if((p.status==SuccessionStatus::rejected)==approved || (approved && p.project.empty())) invalid("The succession outcome bypassed its board majority.");
        } else if(p.player_vote || !p.ballots.empty() || p.posting_cursor || p.control_cursor || p.charter_cursor || p.reviewed_cash.value()!=0 || !p.project.empty() ||
            p.status==SuccessionStatus::approved || p.status==SuccessionStatus::activated || p.status==SuccessionStatus::rejected)
            invalid("An unvoted successor contains fabricated approval or work.");
        if(p.status==SuccessionStatus::expired && p.finished_on!=p.vote_before) invalid("The unvoted successor expired on the wrong day.");
        if(!p.project.empty()) {
            const auto& project=named(s.capacity->projects,p.project);
            if(!projects.insert(p.project).second || p.project!="leadership.transition/"+std::to_string(p.id) || project.starts_on!=p.decided_on ||
                project.kind!=ProjectKind::research || project.source_id!=p.allocation.source_id || project.requested!=p.allocation.daily_compute ||
                project.staff!=p.allocation.staff || project.total_work!=transition_work(c,p.allocation))
                invalid("Leadership preparation differs from its paid finite work.");
            if(person.signing_cost.value()>0 && std::ranges::none_of(s.accounts->postings,[&](const auto& r){return r.key==p.project && r.day==p.decided_on &&
                r.debit==Account::expenses && r.credit==Account::cash && r.amount==person.signing_cost;})) invalid("Leadership preparation has no actual fee.");
            if(p.status==SuccessionStatus::activated && (!p.finished_on || *p.finished_on<p.handover_on || project.status!=ProjectStatus::completed ||
                !project.stopped_on || *project.stopped_on>=*p.finished_on || p.incumbent>=actor.terms.size() || actor.terms[p.incumbent].plan!=p.id))
                invalid("A handover began before its approved exercises completed.");
            if(!pending(p.status) && p.status!=SuccessionStatus::activated && project.status==ProjectStatus::active)
                invalid("A cancelled leadership plan retained active work.");
        }
    }
    if(outstanding>1) invalid("Multiple successor plans would claim the same executive position.");
    for(const auto& obligation:s.accounts->recurring) if(obligation.id.starts_with("leadership/") && !salaries.contains(obligation.id))
        invalid("A leadership salary has no actual executive or founder role.");
    for(const auto& project:s.capacity->projects) if(project.id.starts_with("leadership.transition/") && !projects.contains(project.id))
        invalid("A transition project has no approved successor.");
    for(std::size_t i=0;i<actor.notices.size();++i) {
        const auto& n=actor.notices[i];
        if(n.id!=i+1 || n.term==0 || n.term>actor.terms.size() || n.plan>actor.plans.size() || !in_time(n.day) || n.deadline<n.day ||
            (n.kind!="planning" && n.kind!="handover" && n.kind!="caretaker" && n.kind!="chair_retired" && n.kind!="transition_stopped" && n.kind!="transition_delayed"))
            invalid("A leadership notice has an invalid source, date or deadline.");
    }
}
}
