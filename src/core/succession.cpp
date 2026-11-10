#include "succession_internal.hpp"
#include <charconv>

namespace pgp::core {
namespace succession_detail {
OrdinalDay term_due(const SuccessionCatalog& c,int birth,OrdinalDay start,bool caretaker) {
    return std::min(year_day(birth+static_cast<int>(c.executive_retirement_age)),
        anniversary(start,caretaker?c.caretaker_term_years:c.maximum_term_years));
}
std::string salary_id(const LeadershipTerm& term) {
    return term.person=="founder"?"leadership/founder.ceo":"leadership/ceo."+std::to_string(term.id);
}
bool founder_payment(const Posting& p) {
    return p.debit==Account::payables && p.credit==Account::cash && p.document_id.starts_with("recurring/leadership/founder.");
}
MilliCU transition_work(const SuccessionCatalog& c,const LabAllocation& a) {
    if(a.daily_compute.value()<=0 || a.daily_compute.value()>1'000'000 || a.staff==0 || a.staff>32)
        invalid("A leadership transition needs positive compute and one to 32 staff.");
    return std::max(c.transition_work,integrate(a.daily_compute,c.transition_days));
}
std::vector<BoardBallot> succession_ballots(const SimulationState& s,const SuccessionPlan& p,bool support,Cents fee) {
    if(s.management->governance) {
        BoardMotion motion;motion.kind=BoardMotionKind::acquisition;motion.cash=fee;
        motion.posting_cursor=p.posting_cursor;motion.leadership_cursor=p.control_cursor;
        return governance_detail::ballots_for(s,motion,support,p.reviewed_cash);
    }
    return {{"charter.trustees",1,checked_unit_subtract(p.reviewed_cash,fee)>=succession_catalog(s).cash_floor}};
}
void check_proposal(const SimulationState& s,const ProposeSuccession& p,OrdinalDay proposed,std::uint64_t incumbent) {
    const auto& c=succession_catalog(s);const auto& future=*s.succession;
    if(incumbent==0 || incumbent>future.terms.size()) invalid("Choose the current executive term.");
    const auto& prior=future.terms[incumbent-1];
    if(p.handover_on<advance_days(proposed,c.transition_days) || p.handover_on>prior.due_on || p.handover_on>=s.end_date ||
        advance_days(proposed,c.vote_window)>=p.handover_on)
        invalid("Allow the disclosed transition work and board window before the incumbent's retirement deadline.");
    const auto candidate=leadership_candidate(c,p.candidate);
    const auto age=from_ordinal(p.handover_on).year-candidate.birth_year;
    if(age<static_cast<int>(c.minimum_candidate_age) || age>static_cast<int>(c.maximum_candidate_age) || candidate.id==prior.person)
        invalid("Choose a different eligible successor for the actual handover date.");
    if(p.retain_chair && (prior.person!="founder" ||
        p.handover_on>=year_day(c.founder_birth_year+static_cast<int>(c.chair_retirement_age))))
        invalid("The founder's personal chair role has already ended or would exceed its retirement date.");
    (void)named(s.capacity->sources,p.allocation.source_id);
    const auto work=transition_work(c,p.allocation);
    const auto days=work.value()/p.allocation.daily_compute.value()+(work.value()%p.allocation.daily_compute.value()!=0?1:0);
    if(advance_days(proposed,Days{days})>p.handover_on)
        invalid("The funded transition work cannot finish by that handover date.");
}
}
namespace {
using namespace succession_detail;
void notice(SimulationState& s,std::string kind,std::uint64_t term,std::uint64_t plan,OrdinalDay deadline) {
    auto& records=s.succession->notices;
    if(std::ranges::any_of(records,[&](const auto& n){return n.kind==kind && n.term==term && n.plan==plan;})) return;
    if(records.size()>=succession_notice_limit) invalid("The bounded leadership notice history is full.");
    records.push_back({records.size()+1,term,plan,std::move(kind),s.date,deadline,false});s.paused=true;
}
void end_salary(SimulationState& s,std::string_view id) {
    if(std::ranges::any_of(s.accounts->recurring,[&](const auto& r){return r.id==id && !r.ends_before;}))
        end_recurring(*s.accounts,id,s.date);
}
void set_role(SimulationState& s,FounderRole role) {
    auto& actor=*s.succession;const auto& c=succession_catalog(s);
    if(!actor.controls.empty() && founder_role(actor)==FounderRole::chair && role!=FounderRole::chair)
        end_salary(s,"leadership/founder.chair");
    if(role==FounderRole::chair && (actor.controls.empty() || founder_role(actor)!=FounderRole::chair) && c.chair_salary.value()>0)
        add_recurring(*s.accounts,{"leadership/founder.chair",c.chair_salary,s.date,{},"Founder chair stipend"});
    if(actor.controls.size()>=succession_notice_limit) invalid("The bounded leadership control history is full.");
    actor.controls.push_back({s.date,actor.terms.back().id,role});
}
void appoint(SimulationState& s,const LeadershipCandidate& candidate,bool caretaker,std::uint64_t plan,FounderRole role) {
    auto& actor=*s.succession;
    if(actor.terms.size()>=succession_term_limit) invalid("The bounded executive term history is full.");
    if(!actor.terms.empty()) {
        auto& previous=actor.terms.back();previous.ended_on=s.date;end_salary(s,salary_id(previous));
    }
    LeadershipTerm term{actor.terms.size()+1,plan,candidate.id,candidate.name,candidate.birth_year,s.date,
        term_due(succession_catalog(s),candidate.birth_year,s.date,caretaker),{},caretaker};
    if(candidate.id!="founder") term.portrait=make_portrait(0,"leadership/"+candidate.id);
    actor.terms.push_back(term);
    if(candidate.monthly_salary.value()>0) add_recurring(*s.accounts,{salary_id(term),candidate.monthly_salary,s.date,{},"Agreed chief executive salary: "+candidate.name});
    set_role(s,role);
}
SuccessionPlan& plan_at(SimulationState& s,std::uint64_t id) {
    if(!s.succession || id==0 || id>s.succession->plans.size()) invalid("Choose a current leadership proposal.");
    return s.succession->plans[id-1];
}
void stop_plan(SimulationState& s,SuccessionPlan& p,SuccessionStatus status) {
    if(!p.project.empty() && (named(s.capacity->projects,p.project).status==ProjectStatus::active || named(s.capacity->projects,p.project).status==ProjectStatus::paused))
        cancel_project(*s.capacity,s.date,p.project);
    p.status=status;p.finished_on=s.date;
}
}
const SuccessionCatalog& succession_catalog(const SimulationState& s) {
    if(!s.management || !s.management->catalog.succession) succession_detail::invalid("This scenario has no leadership succession charter.");
    return *s.management->catalog.succession;
}
LeadershipCandidate leadership_candidate(const SuccessionCatalog& c,std::string_view id) {
    using namespace succession_detail;
    const auto dot=id.rfind('.');
    if(dot==id.npos) invalid("Choose an available leadership cohort.");
    const auto& track=named(c.tracks,id.substr(0,dot));
    int year=0;const auto token=id.substr(dot+1);const auto result=std::from_chars(token.data(),token.data()+token.size(),year);
    if(result.ec!=std::errc{} || result.ptr!=token.data()+token.size() || year<track.first_birth_year || year>9900 ||
        (year-track.first_birth_year)%static_cast<int>(track.cohort_years)!=0 || std::to_string(year)!=token)
        invalid("The successor identity differs from its authored cohort.");
    const auto generation=static_cast<std::size_t>((year-track.first_birth_year)/static_cast<int>(track.cohort_years));
    return {std::string(id),track.names.at(generation%track.names.size()),track.priorities,year,track.signing_cost,track.monthly_salary};
}
std::vector<LeadershipCandidate> leadership_candidates(const SuccessionCatalog& c,OrdinalDay day) {
    const auto year=from_ordinal(day).year;std::vector<LeadershipCandidate> result;
    for(const auto& track:c.tracks) {
        const auto first=std::max(0,(year-static_cast<int>(c.maximum_candidate_age)-track.first_birth_year+static_cast<int>(track.cohort_years)-1)/static_cast<int>(track.cohort_years));
        for(int born=track.first_birth_year+first*static_cast<int>(track.cohort_years);born<=year-static_cast<int>(c.minimum_candidate_age);born+=static_cast<int>(track.cohort_years))
            result.push_back(leadership_candidate(c,track.id+"."+std::to_string(born)));
    }
    return result;
}
FounderRole founder_role(const SuccessionState& s) {return s.controls.back().role;}
const SuccessionNotice* required_succession_notice(const SimulationState& s) {
    if(s.succession) for(const auto& n:s.succession->notices) if(!n.acknowledged) return &n;
    return nullptr;
}
std::optional<OrdinalDay> next_succession_boundary(const SimulationState& s) {
    if(!s.succession) return {};
    const auto& actor=*s.succession;const auto& c=succession_catalog(s);std::optional<OrdinalDay> next;
    const auto consider=[&](OrdinalDay day){if(day>s.date && day<s.end_date && (!next || day<*next)) next=day;};
    consider(actor.terms.back().due_on);
    consider(OrdinalDay{actor.terms.back().due_on.value()-c.planning_notice.value()});
    if(founder_role(actor)==FounderRole::chair) consider(succession_detail::year_day(c.founder_birth_year+static_cast<int>(c.chair_retirement_age)));
    for(const auto& p:actor.plans) {
        if(p.status==SuccessionStatus::proposed) consider(p.vote_before);
        if(p.status==SuccessionStatus::approved) consider(p.handover_on);
    }
    return next;
}
std::vector<BoardMember> succession_board(const SimulationState& s,std::vector<BoardMember> board,std::uint64_t cursor) {
    if(!s.succession) {if(cursor) succession_detail::invalid("Board control refers to absent leadership history.");return board;}
    const auto& actor=*s.succession;const auto& c=succession_catalog(s);
    if(cursor==0 || cursor>actor.controls.size()) succession_detail::invalid("Board control cursor exceeds the actual leadership history.");
    const auto& control=actor.controls[cursor-1];
    if(control.role==FounderRole::executive) return board;
    for(auto& member:board) if(member.id==c.founder_board) {
        member.seats=control.role==FounderRole::chair?c.chair_seats:c.trust_seats;
        member.name=control.role==FounderRole::chair?actor.founder_name.value_or(c.founder_name)+", chair":"Founder trust representative";
    }
    std::erase_if(board,[](const auto& member){return member.seats==0;});
    const auto& term=actor.terms.at(control.term-1);
    board.push_back({"leadership.ceo",term.name+", chief executive",1,false,c.cash_floor,BasisPoints{1000}});
    return board;
}
void initialize_succession(SimulationState& s, std::optional<std::string> founder_name) {
    if(!s.management || !s.management->catalog.succession) return;
    if(s.succession || !s.accounts || !s.capacity) succession_detail::invalid("Initialize leadership once on an operating company.");
    const auto& c=succession_catalog(s);validate_succession_catalog(c);
    validate_succession_opening(c,s.date,s.end_date);
    SuccessionState actor;actor.founder_holder=s.management->catalog.scoring.founder;
    actor.founder_name=std::move(founder_name);
    if(c.departure) actor.releases.emplace();
    if(c.personal_life) actor.personal_life.emplace();
    actor.personal_cash=c.personal_cash;actor.posting_cursor=s.accounts->postings.size();s.succession=std::move(actor);
    if(s.date<succession_detail::year_day(c.founder_birth_year+static_cast<int>(c.executive_retirement_age)))
        appoint(s,{"founder",s.succession->founder_name.value_or(c.founder_name),{},c.founder_birth_year,Cents{0},c.founder_salary},false,0,FounderRole::executive);
    else {
        const auto candidates=leadership_candidates(c,s.date);
        if(candidates.empty()) succession_detail::invalid("The opening needs an eligible leadership cohort.");
        appoint(s,candidates.front(),true,0,FounderRole::trust);
    }
    prepare_succession(s);
}
void propose_succession(SimulationState& s,const ProposeSuccession& request) {
    if(!s.succession || s.succession->plans.size()>=succession_plan_limit) succession_detail::invalid("The leadership planning register is unavailable or full.");
    auto& actor=*s.succession;
    if(std::ranges::any_of(actor.plans,[](const auto& p){return p.status==SuccessionStatus::proposed || p.status==SuccessionStatus::approved;}))
        succession_detail::invalid("Resolve or cancel the current leadership proposal first.");
    succession_detail::check_proposal(s,request,s.date,actor.terms.back().id);
    SuccessionPlan p;p.id=actor.plans.size()+1;p.incumbent=actor.terms.back().id;p.candidate=request.candidate;
    p.proposed_on=s.date;p.vote_before=advance_days(s.date,succession_catalog(s).vote_window);p.handover_on=request.handover_on;
    p.retain_chair=request.retain_chair;p.allocation=request.allocation;actor.plans.push_back(std::move(p));s.paused=true;
}
void vote_succession(SimulationState& s,const VoteSuccession& request,Cents protected_cash) {
    using namespace succession_detail;
    auto& p=plan_at(s,request.id);const auto& c=succession_catalog(s);
    if(p.status!=SuccessionStatus::proposed || s.date>=p.vote_before || p.incumbent!=s.succession->terms.back().id)
        invalid("The leadership vote is resolved, expired or bound to a previous executive.");
    check_proposal(s,{p.candidate,p.handover_on,p.retain_chair,p.allocation},s.date,p.incumbent);
    const auto person=leadership_candidate(c,p.candidate);
    if(checked_unit_add(person.signing_cost,c.cash_floor)>checked_unit_subtract(s.cash,protected_cash))
        invalid("The transition fee would breach available cash or its disclosed floor.");
    p.reviewed_cash=s.cash;p.posting_cursor=s.accounts->postings.size();p.control_cursor=s.succession->controls.size();
    p.player_vote=request.support;p.decided_on=s.date;p.ballots=succession_ballots(s,p,request.support,person.signing_cost);
    std::uint32_t yes=0,total=0;for(const auto& vote:p.ballots) {total+=vote.seats;if(vote.support) yes+=vote.seats;}
    if(yes<=total/2) {p.status=SuccessionStatus::rejected;p.finished_on=s.date;return;}
    const auto report=capacity_report(*s.capacity);
    if(static_cast<std::uint64_t>(report.assigned_staff)+p.allocation.staff>report.available_staff) invalid("Preserve staff already assigned to company obligations.");
    const auto& source=named(s.capacity->sources,p.allocation.source_id);
    auto spare=company_capacity(source).value();
    for(const auto& work:s.capacity->projects) if(work.status==ProjectStatus::active && work.source_id==source.id)
        spare=checked_subtract(spare,work.requested.value());
    if(p.allocation.daily_compute.value()>spare) invalid("Use spare compute after existing company obligations.");
    CapacityProject work;work.id="leadership.transition/"+std::to_string(p.id);work.source_id=source.id;work.interconnect=source.interconnect;
    work.kind=ProjectKind::research;work.starts_on=s.date;work.requested=p.allocation.daily_compute;work.staff=p.allocation.staff;
    work.total_work=transition_work(c,p.allocation);work.cause="Leadership transition exercises: "+person.name;
    reserve_project(*s.capacity,s.date,work);
    if(person.signing_cost.value()>0) record_cash(*s.accounts,s.date,Cents{-person.signing_cost.value()},Account::expenses,"Approved leadership preparation: "+person.name,work.id);
    s.cash=accounts_report(*s.accounts).cash;p.project=work.id;p.status=SuccessionStatus::approved;
}
void cancel_succession(SimulationState& s,std::uint64_t id) {
    auto& p=plan_at(s,id);
    if(p.status!=SuccessionStatus::proposed && p.status!=SuccessionStatus::approved) succession_detail::invalid("Choose a pending or funded leadership plan.");
    stop_plan(s,p,SuccessionStatus::cancelled);
}
void retire_founder_chair(SimulationState& s) {
    if(!s.succession || founder_role(*s.succession)!=FounderRole::chair) succession_detail::invalid("The founder has no current personal chair role.");
    set_role(s,FounderRole::trust);notice(s,"chair_retired",s.succession->terms.back().id,0,s.date);
}
void acknowledge_succession(SimulationState& s,std::uint64_t id) {
    if(!s.succession || id==0 || id>s.succession->notices.size() || s.succession->notices[id-1].acknowledged)
        succession_detail::invalid("Choose an unread leadership notice.");
    s.succession->notices[id-1].acknowledged=true;
}
void prepare_succession(SimulationState& s) {
    if(!s.succession) return;
    using namespace succession_detail;
    auto& actor=*s.succession;const auto& c=succession_catalog(s);
    for(std::size_t i=actor.posting_cursor;i<s.accounts->postings.size();++i)
        if(founder_payment(s.accounts->postings[i])) actor.personal_cash=checked_unit_add(actor.personal_cash,s.accounts->postings[i].amount);
    actor.posting_cursor=s.accounts->postings.size();
    prepare_personal_life(s);
    if(s.date>=s.end_date || s.phase==DayPhase::closed || s.phase==DayPhase::finished) return;
    if(founder_role(actor)==FounderRole::chair && s.date>=year_day(c.founder_birth_year+static_cast<int>(c.chair_retirement_age)))
        retire_founder_chair(s);
    for(auto& p:actor.plans) {
        if(p.status==SuccessionStatus::proposed && s.date>=p.vote_before) {p.status=SuccessionStatus::expired;p.finished_on=p.vote_before;}
        if(p.status!=SuccessionStatus::approved) continue;
        if(p.incumbent!=actor.terms.back().id || named(s.capacity->projects,p.project).status==ProjectStatus::cancelled) {
            stop_plan(s,p,SuccessionStatus::obsolete);notice(s,"transition_stopped",actor.terms.back().id,p.id,actor.terms.back().due_on);continue;
        }
        if(s.date<p.handover_on) continue;
        if(named(s.capacity->projects,p.project).status==ProjectStatus::completed && s.date<=actor.terms.back().due_on) {
            const auto person=leadership_candidate(c,p.candidate);
            auto role=founder_role(actor);
            if(role==FounderRole::executive) role=p.retain_chair?FounderRole::chair:FounderRole::trust;
            appoint(s,person,false,p.id,role);p.status=SuccessionStatus::activated;p.finished_on=s.date;
            notice(s,"handover",actor.terms.back().id,p.id,actor.terms.back().due_on);
        } else notice(s,"transition_delayed",actor.terms.back().id,p.id,actor.terms.back().due_on);
    }
    if(s.date>=actor.terms.back().due_on) {
        for(auto& p:actor.plans) if(p.status==SuccessionStatus::proposed || p.status==SuccessionStatus::approved) stop_plan(s,p,SuccessionStatus::obsolete);
        const auto available=leadership_candidates(c,s.date);
        if(available.empty()) invalid("The charter has no eligible caretaker cohort.");
        const auto found=std::ranges::find_if(available,[&](const auto& row){return row.id!=actor.terms.back().person;});
        const auto& person=found==available.end()?available.front():*found;
        const auto role=founder_role(actor)==FounderRole::executive?FounderRole::trust:founder_role(actor);
        appoint(s,person,true,0,role);notice(s,"caretaker",actor.terms.back().id,0,actor.terms.back().due_on);
    }
    if(advance_days(s.date,c.planning_notice)>=actor.terms.back().due_on)
        notice(s,"planning",actor.terms.back().id,0,actor.terms.back().due_on);
}
}
