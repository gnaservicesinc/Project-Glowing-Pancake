#include "competition_internal.hpp"
#include <pgp/core/workplace_research.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(const std::string& why){throw std::invalid_argument(why);}
const ResearchCollaborationRules& rules(const SimulationState& s) {
    if(!s.management || !s.management->people || !s.management->catalog.people->collaboration || !s.lab || !s.capacity || !s.accounts || !workplace_research_enabled(s))
        invalid("This lab does not offer individual research collaboration.");
    return *s.management->catalog.people->collaboration;
}
const SimulationState& company(const WorldState& w,std::string_view id) {
    const auto* s=competition_detail::company(w,id);
    if(!s || s->phase!=DayPhase::work_ready || s->date!=campaign_day(w))invalid("Both labs must be open and ready on the same day.");
    return *s;
}
bool local_contact(const WorldState& w,const SimulationState& s,std::string_view company_id,std::string_view identity) {
    if(identity=="founder")return company_id==w.company_id;
    const auto prefix=company_id==w.company_id?std::string{}:std::string{company_id}+"/";
    if(!identity.starts_with(prefix))return false;
    identity.remove_prefix(prefix.size());
    for(const auto& e:s.management->people->employees)if(e.terms.profile.id==identity)return !e.departed_on && e.ready_on<=s.date;
    return false;
}
const CapacityProject& project(const SimulationState& s,std::string_view id) {
    const auto it=std::ranges::find(s.capacity->projects,id,&CapacityProject::id);
    if(it==s.capacity->projects.end())invalid("The collaboration lost its research project.");return *it;
}
std::int64_t free_compute(const SimulationState& s,const CapacitySource& source) {
    auto free=company_capacity(source).value();
    for(const auto& work:s.capacity->projects)if(work.source_id==source.id && work.status==ProjectStatus::active)free=checked_subtract(free,work.requested.value());
    return std::max<std::int64_t>(0,free);
}
LabAllocation allocation(const SimulationState& s,std::string_view worker,std::string_view source,MilliCUPerDay requested) {
    LabAllocation a; a.staff=1;
    for(const auto& candidate:research_workers(s))if(candidate.id==worker || (worker.empty() && candidate.unavailable.empty())) {
        if(!candidate.unavailable.empty())invalid(candidate.name+": "+candidate.unavailable);
        a.researchers={candidate.id};break;
    }
    if(a.researchers.empty())invalid("Choose an available, qualified researcher with a powered computer.");
    std::int64_t best=0;
    for(const auto& capacity:s.capacity->sources) {
        const auto available=free_compute(s,capacity);
        if((!source.empty() && capacity.id==source) || (source.empty() && available>best)) {a.source_id=capacity.id;best=available;}
    }
    if(a.source_id.empty() || best<=0 || requested.value()<=0 || (!source.empty() && requested.value()>best))invalid("Choose available compute without taking another project's reservation.");
    a.daily_compute=MilliCUPerDay{std::min(best,requested.value())};return a;
}
Cents available(const WorldState& w,const CompiledProgram& program,std::string_view id,const SimulationState& s) {
    if(id==w.company_id)return discretionary_cash(s);
    const auto& policy=*rival_definition(program,id).planning;
    Cents monthly{0};for(const auto& cost:s.accounts->recurring)if(cost.starts_on<=s.date && (!cost.ends_before || s.date<*cost.ends_before))monthly=checked_unit_add(monthly,cost.monthly_amount);
    const auto runway=checked_unit_add(policy.reserve_floor,Cents{mul_div_floor(static_cast<std::uint64_t>(monthly.value()),policy.runway_months,1).quotient});
    return std::max(Cents{0},checked_unit_subtract(uncommitted_cash(s),std::max(runway,operating_reserve_target(s))));
}
void research_ready(const SimulationState& s,std::string_view topic,const LabAllocation& a) {
    const auto definition=std::ranges::find(s.lab->catalog.research,topic,&LabResearch::id);
    if(definition==s.lab->catalog.research.end())invalid("Choose a topic both labs can research.");
    if(s.lab->research.size()>=lab_record_limit)invalid("This lab's research history is full.");
    std::uint32_t successes=0;
    for(const auto& run:s.lab->research)if(run.research_id==topic) {
        const auto status=project(s,run.project_id).status;
        if(status==ProjectStatus::active || status==ProjectStatus::paused)invalid("This topic already has unfinished work; finish or cancel it first.");
        if(run.completed)++successes;
    }
    if(successes>=definition->repeat_limit)invalid("This lab already has the allowed results for this topic.");
    const auto technology=frontier_technology_block(s,FrontierTechnology::research,topic,s.date);if(!technology.empty())invalid(technology);
    const auto why=research_workplace_blocker(s,topic,a);if(!why.empty())invalid(why);
    const auto report=capacity_report(*s.capacity);if(report.available_staff<=report.assigned_staff)invalid("This lab's staff are already committed.");
}
void bump(SimulationState& s) {
    if(s.revision==std::numeric_limits<std::uint64_t>::max())invalid("The company revision is exhausted.");++s.revision;
}
}

CollaborationQuote collaboration_quote(const WorldState& w,const CompiledProgram& program,std::string_view initiator,const CollaborationRequest& request) {
    CollaborationQuote q;
    try {
        if(!w.competition || !w.competition->setup || w.competition->collaborations.size()>=collaboration_limit || !w.simulation.management || !w.simulation.management->people || !w.simulation.management->people->social)
            invalid("Research jams need an active industry social network and space for another agreement.");
        const auto& first=company(w,initiator);const auto& policy=rules(first);
        const auto peer=w.simulation.management->people->industry.find(request.contact);
        if(peer==w.simulation.management->people->industry.end() || !peer->second.active || peer->second.company==initiator)invalid("Choose an onboarded contact at another operating major HQ.");
        q.partner=peer->second.company;q.contact=peer->second.profile.id;
        const auto& second=company(w,q.partner);const auto& other_policy=rules(second);
        const auto threshold=std::max(policy.minimum_friendship,other_policy.minimum_friendship);
        for(const auto& link:w.simulation.management->people->social->links) {
            const auto candidate=link.first==request.contact?link.second:link.second==request.contact?link.first:std::string{};
            if(!candidate.empty() && link.affinity>=threshold && link.affinity>q.friendship && local_contact(w,first,initiator,candidate) &&
               (initiator!=w.company_id || link.known || candidate=="founder")) {q.friendship=link.affinity;q.connection=candidate;}
        }
        if(q.connection.empty())invalid("Build a friendship with this researcher, or discover a coworker's friendship with them, before proposing a joint project.");
        for(const auto& old:w.competition->collaborations) {
            const bool involved=old.first_company==initiator || old.second_company==initiator || old.first_company==q.partner || old.second_company==q.partner;
            const auto status=involved?collaboration_status(w,old):std::string{};
            if(involved && (status=="working" || status=="paused"))invalid("Finish the existing research jam before committing either lab to another.");
            if(((old.first_company==initiator && old.second_company==q.partner) || (old.second_company==initiator && old.first_company==q.partner)) &&
               first.date<advance_days(old.started_on,Days{std::max(policy.cooldown_days,other_policy.cooldown_days)}))invalid("Give this partnership time before starting another jam together.");
        }
        q.first=allocation(first,request.worker,request.source,request.rate);
        q.second=allocation(second,q.contact,{},request.rate);
        research_ready(first,request.topic,q.first);research_ready(second,request.topic,q.second);
        q.coordination_fee=std::max(policy.coordination_fee,other_policy.coordination_fee);
        q.total_fee=checked_unit_add(research_cash_fee(first,request.topic),q.coordination_fee);
        if(available(w,program,initiator,first)<q.total_fee)invalid("Your lab cannot cover research and coordination without using protected funds.");
        if(available(w,program,q.partner,second)<research_cash_fee(second,request.topic))invalid("The partner cannot fund its share while preserving its runway.");
        q.work_credit=std::min(policy.work_credit,other_policy.work_credit);
        const auto days=[&](const SimulationState& s,const LabAllocation& a){const auto& topic=*std::ranges::find(s.lab->catalog.research,request.topic,&LabResearch::id);return coordinated_staff_days(s.lab->catalog,topic,research_team(s,a.researchers).compatibility_bp,q.work_credit);};
        q.first_days=days(first,q.first);q.second_days=days(second,q.second);
    }catch(const std::invalid_argument& e){q.blocker=e.what();}
    return q;
}

std::string collaboration_status(const WorldState& w,const ResearchCollaboration& c) {
    const auto* a=competition_detail::company(w,c.first_company);const auto* b=competition_detail::company(w,c.second_company);
    if(!a || !b || !a->lab || !b->lab)return "interrupted";
    const auto& first=project(*a,c.first_project);const auto& second=project(*b,c.second_project);
    if(first.status==ProjectStatus::cancelled || second.status==ProjectStatus::cancelled)return "cancelled";
    for(const auto& run:a->lab->research)if(run.project_id==c.first_project && run.failed)return "failed";
    for(const auto& run:b->lab->research)if(run.project_id==c.second_project && run.failed)return "failed";
    if(first.status==ProjectStatus::completed && second.status==ProjectStatus::completed)return "completed";
    if(a->phase==DayPhase::closed || b->phase==DayPhase::closed || a->phase==DayPhase::finished || b->phase==DayPhase::finished)return "interrupted";
    return first.status==ProjectStatus::paused || second.status==ProjectStatus::paused?"paused":"working";
}

void start_collaboration(WorldState& w,const CompiledProgram& program,std::string_view initiator,const CollaborationRequest& request) {
    const auto q=collaboration_quote(w,program,initiator,request);if(!q.blocker.empty())invalid(q.blocker);
    auto& a=*competition_detail::company(w,initiator);auto& b=*competition_detail::company(w,q.partner);
    ResearchCollaboration agreement;agreement.id="research-jam/"+std::to_string(w.competition->collaborations.size()+1);
    agreement.first_company=initiator;agreement.second_company=q.partner;agreement.connection=q.connection;agreement.contact=request.contact;agreement.topic=request.topic;
    agreement.started_on=a.date;agreement.coordination_fee=q.coordination_fee;agreement.work_credit=q.work_credit;agreement.friendship=q.friendship;
    const auto reserved=[](const SimulationState& s){return checked_unit_subtract(s.cash,uncommitted_cash(s));};
    if(q.coordination_fee.value()) {
        record_cash(*a.accounts,a.date,Cents{-q.coordination_fee.value()},Account::expenses,"Research jam coordination",agreement.id+"/pay");
        record_cash(*b.accounts,b.date,q.coordination_fee,Account::revenue,"Research jam coordination",agreement.id+"/receive");
    }
    a.cash=accounts_report(*a.accounts).cash;b.cash=accounts_report(*b.accounts).cash;
    const ResearchCooperation first{agreement.id,agreement.second_company,q.work_credit},second{agreement.id,agreement.first_company,q.work_credit};
    (void)start_funded_research(a,request.topic,q.first,reserved(a),&first);
    (void)start_funded_research(b,request.topic,q.second,reserved(b),&second);
    agreement.first_project=a.lab->research.back().project_id;agreement.second_project=b.lab->research.back().project_id;
    if(initiator!=w.company_id)bump(a);
    bump(b);
    w.competition->collaborations.push_back(std::move(agreement));
}

void publish_collaboration(WorldState& w,std::string_view id) {
    if(!w.competition)invalid("There is no joint research agreement.");
    const auto it=std::ranges::find(w.competition->collaborations,id,&ResearchCollaboration::id);
    if(it==w.competition->collaborations.end() || it->published_on || collaboration_status(w,*it)!="completed")invalid("Both labs must complete their work before publishing the joint result once.");
    for(const auto& owner:{it->first_company,it->second_company}) {
        auto& s=*competition_detail::company(w,owner);
        if(std::ranges::none_of(s.lab->publications,[&](const auto& paper){return paper.research_id==it->topic;}))publish_research(*s.lab,*s.capacity,s.date,it->topic);
        if(owner!=w.company_id)bump(s);
    }
    it->published_on=campaign_day(w);
}

void publish_industry_collaborations(WorldState& w,const CompiledProgram& program) {
    if(!w.competition)return;
    for(const auto& c:w.competition->collaborations)if(c.first_company!=w.company_id && c.second_company!=w.company_id && !c.published_on && collaboration_status(w,c)=="completed" &&
        rival_definition(program,c.first_company).planning->publish_results && rival_definition(program,c.second_company).planning->publish_results)publish_collaboration(w,c.id);
}

std::string plan_research_collaboration(WorldState& w,const CompiledProgram& program,std::string_view initiator) {
    if(!w.competition || !w.simulation.management || !w.simulation.management->people || !w.simulation.management->people->social)return {};
    const auto* s=competition_detail::company(w,initiator);if(!s || !s->management || !s->management->catalog.people || !s->management->catalog.people->collaboration)return {};
    const auto prefix=std::string{initiator}+"/";int examined=0;
    for(const auto& link:w.simulation.management->people->social->links) {
        const auto mine=link.first.starts_with(prefix)?link.first:link.second.starts_with(prefix)?link.second:std::string{};
        const auto peer=link.first==mine?link.second:link.first;
        if(link.affinity<s->management->catalog.people->collaboration->minimum_friendship || mine.empty() || peer=="founder" || peer.starts_with(prefix) || !w.simulation.management->people->industry.contains(peer))continue;
        for(const auto& topic:rival_definition(program,initiator).planning->research) {
            if(std::ranges::any_of(s->lab->research,[&](const auto& run){return run.research_id==topic && (run.completed || project(*s,run.project_id).status==ProjectStatus::active || project(*s,run.project_id).status==ProjectStatus::paused);}))continue;
            if(++examined>16)return {};
            CollaborationRequest request{peer,topic,mine.substr(prefix.size()),{},MilliCUPerDay{4000}};
            if(collaboration_quote(w,program,initiator,request).blocker.empty()) {start_collaboration(w,program,initiator,request);return w.competition->collaborations.back().id;}
        }
    }
    return {};
}
}
