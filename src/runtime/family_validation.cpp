#include "family_internal.hpp"
#include <algorithm>
#include <stdexcept>
namespace pgp::runtime {
void validate_family(const WorldState& w,bool deep) {
    using namespace core;
    const auto invalid=[](){throw std::invalid_argument("Family history has invalid identities, care dates, kinship, learning or applications.");};
    const auto* rules=family_rules(w);if(static_cast<bool>(rules)!=w.family.has_value())invalid();if(!rules)return;
    if(!w.romance || !w.founding || !w.identity)invalid();const auto& state=*w.family;const auto day=campaign_day(w);
    if(state.members.size()<2 || state.members.size()>maximum_family_people || state.edges.size()>maximum_family_edges || state.pending.size()>128 || state.history.size()>maximum_family_plans || state.replies.size()>maximum_family_plans ||
       state.next_plan!=state.pending.size()+state.history.size()+1 || state.next_plan>maximum_family_plans+1 || state.next_review<w.campaign_start || state.next_review>advance_days(day,Days{rules->interval_days}) ||
       (state.last_action && (*state.last_action<w.campaign_start || *state.last_action>day)) || (state.next_due && *state.next_due<day) || state.aliases.size()>state.members.size())invalid();
    std::set<std::uint32_t> marriages;for(const auto& [id,b]:w.romance->active)if(b.married_on)marriages.insert(id);if(state.marriages!=marriages)invalid();
    if(!deep)return;
    const auto people=romance_detail::roster(w);const auto& talents=*talent_rules(w.simulation);std::set<std::string> skills;
    for(const auto& skill:talents.skills)skills.insert(skill.id);
    std::map<std::string,std::string> aliases;
    const auto member_check=[&](const FamilyMember& m,std::size_t i) {
        const auto& p=m.profile;validate_portrait(p.portrait);
        if(!p.romantic || p.id!="family/person/"+std::to_string(i+1) || p.name.empty() || p.name.size()>128 || p.name.find('\0')!=p.name.npos || (p.gender!="woman" && p.gender!="man" && p.gender!="nonbinary") || p.pronouns.empty() || p.pronouns.size()>64 || p.research>100 || p.engineering>100 || p.evaluation>100 || p.experience>100 || p.traits.size()>16 || p.skills.size()>96 ||
           m.joined_on<w.campaign_start || m.joined_on>day || p.romantic->born_on>m.joined_on || p.applied_on!=m.joined_on)invalid();
        validate_romantic_profile(*p.romantic,p.birth_year);
        const auto track=std::ranges::find(rules->tracks,m.track,&FamilyLearningTrack::skill);if(track==rules->tracks.end())invalid();
        std::set<std::string> distinct;for(const auto& skill:p.skills)if(!skills.contains(skill) || !distinct.insert(skill).second)invalid();
        if(!m.extra_skill.empty() && !skills.contains(m.extra_skill))invalid();
        const auto birthday=to_ordinal(family_detail::anniversary(p.romantic->born_on,18));
        if(m.learning_since<p.romantic->born_on || (m.learning_since>day && m.learning_since!=to_ordinal(family_detail::anniversary(p.romantic->born_on,6))) || m.next_application<birthday)invalid();
        if(m.qualified_on) {
            if(*m.qualified_on<std::max(birthday,advance_days(m.learning_since,Days{rules->study_days})) || *m.qualified_on>day || !has_skill(p,track->skill) || (i>=2 && p.role!=track->role))invalid();
        } else if(!p.skills.empty() || m.application)invalid();
        if(i<2 && (!m.known || m.seeks_work || m.joined_on!=w.campaign_start))invalid();
        if(m.application) {
            const auto& a=*m.application;if(!m.qualified_on || a.day<*m.qualified_on || a.day>day || !adult_on(*p.romantic,a.day) || !a.person.starts_with("employee/"))invalid();
            const SimulationState* company=a.company==w.company_id?&w.simulation:nullptr;if(!company && w.competition)for(const auto& c:w.competition->companies)if(c.definition_id==a.company)company=&c.simulation;
            if(!company || !company->management || !company->management->people)invalid();const auto& employed=*company->management->people;
            const PersonProfile* actual=nullptr;for(const auto& row:employed.applicants)if(row.profile.id==a.person)actual=&row.profile;for(const auto& row:employed.queued_applicants)if(row.profile.id==a.person)actual=&row.profile;for(const auto& row:employed.employees)if(row.terms.profile.id==a.person)actual=&row.terms.profile;
            if(actual && (actual->name!=p.name || actual->gender!=p.gender || actual->pronouns!=p.pronouns || actual->orientation!=p.orientation || actual->portrait!=p.portrait || !actual->romantic || actual->romantic->born_on!=p.romantic->born_on || actual->applied_on!=a.day))invalid();
            aliases.emplace(a.company+"/"+a.person,p.id);
        }
    };
    for(std::size_t i=0;i<state.members.size();++i)member_check(state.members[i],i);
    if(aliases!=state.aliases)invalid();
    const auto ancestors=family_ancestors(w);std::set<std::string> active;
    for(const auto& e:state.edges) {
        const auto parent=people.find(e.parent),child=people.find(e.child);
        if(parent==people.end() || child==people.end() || e.parent==e.child || (e.kind!="biological" && e.kind!="adoptive" && e.kind!="foster" && e.kind!="step") ||
           e.started_on<child->second.romantic->born_on || e.started_on>day || (e.ended_on && (*e.ended_on<e.started_on || *e.ended_on>day)) ||
           (e.started_on<w.campaign_start && !(e.child=="founder" && e.kind=="biological" && e.started_on==w.romance->founder.born_on)) || !adult_on(*parent->second.romantic,e.started_on))invalid();
        if(const auto found=ancestors.find(e.parent);found!=ancestors.end() && found->second.contains(e.child))invalid();
        if(!e.ended_on && !active.insert(e.parent+"\n"+e.child+"\n"+e.kind).second)invalid();
        if(e.kind=="step") {
            if(!e.marriage || (!e.ended_on && !marriages.contains(e.marriage)))invalid();
        } else if(e.marriage || child->second.romantic->born_on<to_ordinal(family_detail::anniversary(parent->second.romantic->born_on,rules->minimum_parent_gap)))invalid();
    }
    std::set<std::uint32_t> plans;
    const auto plan_check=[&](const FamilyPlan& p,bool pending) {
        if(!p.id || p.id>=state.next_plan || !plans.insert(p.id).second || !people.contains(p.first) || (!p.second.empty() && (!people.contains(p.second) || p.first==p.second)) ||
           (p.kind!="birth" && p.kind!="adopt" && p.kind!="foster" && p.kind!="adopt_placement") || (p.band!="baby" && p.band!="child" && p.band!="teen") || p.name.size()>128 || p.requested_on<w.campaign_start || p.requested_on>day ||
           p.due_on!=advance_days(p.requested_on,Days{p.kind=="birth"?rules->birth_days:p.kind=="foster"?rules->foster_days:rules->adoption_days}) || p.resolved_on.has_value()==pending ||
           (pending && (p.status!="pending" || p.due_on<day)) || (!pending && (p.status!="completed" && p.status!="cancelled")) || (p.resolved_on && (*p.resolved_on<p.due_on || *p.resolved_on>day)) ||
           (p.kind=="birth" && p.second.empty()) || !adult_on(*people.at(p.first).romantic,p.requested_on) || (!p.second.empty() && !adult_on(*people.at(p.second).romantic,p.requested_on)))invalid();
        if(p.status=="completed" && !family_member(w,p.child))invalid();if(p.kind=="adopt_placement" && !family_member(w,p.child))invalid();
    };
    for(const auto& [id,p]:state.pending){if(id!=p.id)invalid();plan_check(p,true);}for(const auto& p:state.history)plan_check(p,false);
    std::optional<OrdinalDay> previous;
    for(const auto& r:state.replies) {
        if(r.day<w.campaign_start || r.day>day || (previous && r.day<*previous) || r.message.empty() || r.message.size()>512 ||
           (r.action!="birth" && r.action!="adopt" && r.action!="foster" && r.action!="adopt_placement" && r.action!="track" && r.action!="apply") || (r.plan && (!r.accepted || !plans.contains(r.plan))) || (!r.subject.empty() && !family_member(w,r.subject)))invalid();
        previous=r.day;
    }
}
}
