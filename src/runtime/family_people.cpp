#include "family_internal.hpp"
#include <charconv>
#include <algorithm>
#include <array>
#include <stdexcept>
namespace pgp::runtime {
using namespace core;
const FamilyRules* family_rules(const WorldState& w) {
    return w.founding && w.simulation.management && w.simulation.management->catalog.people && w.simulation.management->catalog.people->family?&*w.simulation.management->catalog.people->family:nullptr;
}
int family_age(OrdinalDay birthday,OrdinalDay day) {
    const auto birth=from_ordinal(birthday),today=from_ordinal(day);auto years=today.year-birth.year;
    auto anniversary=birth;anniversary.year=today.year;anniversary.day=std::min(anniversary.day,days_in_month(anniversary.year,anniversary.month));if(day<to_ordinal(anniversary))--years;return years;
}
std::string family_person_id(const WorldState& w,std::string_view input) {
    auto id=std::string{input};if(id.starts_with("employee/"))id=w.company_id+"/"+id;
    if(w.family)if(const auto found=w.family->aliases.find(id);found!=w.family->aliases.end())return found->second;
    return id;
}
const FamilyMember* family_member(const WorldState& w,std::string_view id) {
    if(!w.family)return nullptr;const auto canonical=family_person_id(w,id);const std::string_view key{canonical};constexpr std::string_view prefix="family/person/";
    if(!key.starts_with(prefix))return nullptr;std::uint32_t index=0;const auto number=key.substr(prefix.size());const auto parsed=std::from_chars(number.data(),number.data()+number.size(),index);
    if(parsed.ec!=std::errc{} || parsed.ptr!=number.data()+number.size() || index==0 || index>w.family->members.size() || canonical!=std::string{prefix}+std::to_string(index))return nullptr;
    return &w.family->members[index-1];
}
std::map<std::string,std::set<std::string>> family_ancestors(const WorldState& w) {
    std::map<std::string,std::vector<std::string>> parents;
    if(w.family)for(const auto& edge:w.family->edges)parents[edge.child].push_back(edge.parent);
    std::map<std::string,std::set<std::string>> result;
    for(const auto& [child,rows]:parents) {
        (void)rows;auto& ancestors=result[child];std::vector<std::string> stack{child};
        while(!stack.empty()){auto person=std::move(stack.back());stack.pop_back();if(!ancestors.insert(person).second)continue;if(const auto found=parents.find(person);found!=parents.end())for(const auto& parent:found->second)stack.push_back(parent);}
    }
    return result;
}
bool family_related(const WorldState& w,std::string_view first,std::string_view second) {
    if(!w.family)return false;const auto a=family_person_id(w,first),b=family_person_id(w,second);if(a==b)return true;
    const auto ancestry=family_ancestors(w);const auto aa=ancestry.find(a),bb=ancestry.find(b);
    const auto includes=[&](const auto& found,const std::string& person){return found!=ancestry.end() && found->second.contains(person);};
    if(includes(aa,b) || includes(bb,a))return true;
    if(aa!=ancestry.end() && bb!=ancestry.end())for(const auto& person:aa->second)if(bb->second.contains(person))return true;
    return false;
}
namespace family_detail {
CivilDate anniversary(OrdinalDay birthday,int years) {auto date=from_ordinal(birthday);date.year+=years;date.day=std::min(date.day,days_in_month(date.year,date.month));return date;}
FamilyMember make_member(const WorldState& w,std::uint32_t id,OrdinalDay birth,std::string_view name,bool known,bool seeks_work,std::string_view family_name) {
    const auto& rules=*family_rules(w);const auto& people=*w.simulation.management->catalog.people;
    const auto seed=w.root_seed+UINT64_C(0x46414d494c59)*id;SplitMix64 identity{seed},aptitude{seed^UINT64_C(0x4348494c44574f52)};
    FamilyMember member;auto& p=member.profile;p.id="family/person/"+std::to_string(id);p.gender=std::array{"woman","man","nonbinary"}[identity.bounded(3)];
    p.pronouns=p.gender=="woman"?"she/her":p.gender=="man"?"he/him":"they/them";p.orientation=std::array{"straight","gay_or_lesbian","bisexual","pansexual","asexual","demisexual"}[identity.bounded(6)];
    const auto first=static_cast<std::size_t>(identity.bounded(people.first_names.size()));const auto last=people.last_names[identity.bounded(people.last_names.size())];
    const auto chosen_surname=family_name.empty()?last:std::string{family_name};p.name=std::string{name};
    if(p.name.empty())for(std::size_t attempt=0;attempt<people.first_names.size();++attempt){p.name=people.first_names[(first+attempt)%people.first_names.size()]+" "+chosen_surname;if(!w.family || std::ranges::none_of(w.family->members,[&](const auto& existing){return existing.profile.name==p.name;}))break;}
    p.birth_year=from_ordinal(birth).year;p.applied_on=campaign_day(w);p.portrait=make_portrait(seed,p.id);p.preferred_mission="research";
    p.research=static_cast<std::uint32_t>(40+aptitude.bounded(31));p.engineering=static_cast<std::uint32_t>(40+aptitude.bounded(31));p.evaluation=static_cast<std::uint32_t>(40+aptitude.bounded(31));p.mission_fit=50;
    p.romantic=RomanticProfile{birth,generate_romantic_preferences(p.gender,p.orientation,seed,*romance_rules(w))};
    for(const auto& group:people.trait_groups)p.traits.push_back(group[identity.bounded(group.size())]);
    const auto& track=rules.tracks[aptitude.bounded(rules.tracks.size())];member.track=track.skill;
    std::vector<std::string> extras;for(const auto& skill:people.talent->skills)if(skill.teachable && skill.prerequisite.empty() && skill.id!=track.skill)extras.push_back(skill.id);
    if(!extras.empty())member.extra_skill=extras[aptitude.bounded(extras.size())];
    member.joined_on=campaign_day(w);member.learning_since=to_ordinal(anniversary(birth,6));member.next_application=to_ordinal(anniversary(birth,18));member.known=known;member.seeks_work=seeks_work;
    if(adult_on(*p.romantic,campaign_day(w))){p.role=track.role;p.education="Independent practical study";p.skills={track.skill};if(!member.extra_skill.empty())p.skills.push_back(member.extra_skill);member.qualified_on=to_ordinal(anniversary(birth,18));}
    else {p.role="Growing up";p.education="Childhood learning";}
    return member;
}
bool has_parent(const WorldState& w,std::string_view parent,std::string_view child) {
    return w.family && std::ranges::any_of(w.family->edges,[&](const auto& edge){return edge.parent==parent && edge.child==child && !edge.ended_on && edge.kind!="step";});
}
int dependents(const WorldState& w,std::string_view person) {
    std::set<std::string> children;if(!w.family)return 0;
    for(const auto& edge:w.family->edges)if(edge.parent==person && !edge.ended_on)if(const auto* child=family_member(w,edge.child);child && !adult_on(*child->profile.romantic,campaign_day(w)))children.insert(edge.child);
    for(const auto& [id,plan]:w.family->pending){(void)id;if((plan.first==person || plan.second==person) && plan.kind!="adopt_placement")children.insert("pending/"+std::to_string(plan.id));}
    return static_cast<int>(children.size());
}
void remember_due(FamilyState& state,OrdinalDay day){if(!state.next_due || day<*state.next_due)state.next_due=day;}
void add_edge(WorldState& w,std::string parent,std::string child,std::string kind,OrdinalDay day,bool known,std::uint32_t marriage) {
    if(w.family->edges.size()>=maximum_family_edges)throw std::invalid_argument("The family relationship register is full.");
    w.family->edges.push_back({std::move(parent),std::move(child),std::move(kind),day,{},known,marriage});
}
bool apply_member(WorldState& w,FamilyMember& member,std::string_view owner,SplitMix64& rng) {
    SimulationState* company=owner==w.company_id?&w.simulation:nullptr;
    if(!company && w.competition)for(auto& rival:w.competition->companies)if(rival.definition_id==owner)company=&rival.simulation;
    if(!company || company->phase==DayPhase::closed || company->phase==DayPhase::finished || !company->management || !company->management->people)return false;
    auto& p=*company->management->people;const auto& r=*company->management->catalog.people;
    if((p.applicants.size()>=r.candidates && p.queued_applicants.size()>=12) || p.next_id>=1000000)return false;
    Applicant applicant;applicant.profile=member.profile;applicant.profile.id="employee/"+std::to_string(p.next_id++);applicant.profile.applied_on=company->date;
    applicant.profile.role=std::ranges::find(family_rules(w)->tracks,member.track,&FamilyLearningTrack::skill)->role;
    applicant.profile.bio.clear();
    applicant.salary=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(r.monthly_salary.value()),static_cast<std::uint64_t>(p.offered_pay.value()),10000)};applicant.signing=r.signing_cost;
    applicant.evaluation_credit=BasisPoints{r.maximum_evaluation_credit.value()*applicant.profile.evaluation/100};if(r.recruitment)initialize_recruitment(applicant,*r.recruitment,rng);
    std::string referrer="founder",referrer_name=w.identity->founder_name;
    for(const auto& edge:w.family->edges)if(edge.child==member.profile.id && edge.kind!="step") {
        if(edge.parent=="founder" && owner==w.company_id)break;
        if(const auto* parent=family_member(w,edge.parent);parent && parent->application && parent->application->company==owner){referrer=std::string{owner}+"/"+parent->application->person;referrer_name=parent->profile.name;break;}
        if(edge.parent.starts_with(std::string{owner}+"/employee/")) {
            referrer=edge.parent;const auto local=edge.parent.substr(owner.size()+1);
            for(const auto& employee:p.employees)if(employee.terms.profile.id==local)referrer_name=employee.terms.profile.name;
            break;
        }
    }
    applicant.recommendation=ApplicantRecommendation{referrer,referrer_name,"family",company->date};
    member.application=FamilyApplication{std::string{owner},applicant.profile.id,campaign_day(w)};w.family->aliases.emplace(std::string{owner}+"/"+applicant.profile.id,member.profile.id);
    if(p.applicants.size()<r.candidates)p.applicants.push_back(std::move(applicant));else p.queued_applicants.push_back(std::move(applicant));return true;
}
void qualify_and_apply(WorldState& w,SplitMix64& rng) {
    auto& state=*w.family;const auto day=campaign_day(w);const auto& rules=*family_rules(w);
    for(std::size_t i=0;i<state.members.size();++i) {
        auto member=state.members[i];bool changed=false;const auto& born=*member.profile.romantic;
        const auto qualifies=std::max(to_ordinal(anniversary(born.born_on,18)),advance_days(member.learning_since,Days{rules.study_days}));
        if(!member.qualified_on && day>=qualifies) {
            const auto& track=*std::ranges::find(rules.tracks,member.track,&FamilyLearningTrack::skill);member.profile.skills={track.skill};if(!member.extra_skill.empty() && member.extra_skill!=track.skill)member.profile.skills.push_back(member.extra_skill);
            member.profile.role=track.role;member.profile.education="Independent practical study";member.qualified_on=day;member.next_application=day;changed=true;
        }
        if(member.seeks_work && member.qualified_on && !member.application && day>=member.next_application) {
            std::vector<std::string> employers;
            for(const auto& edge:state.edges)if(edge.child==member.profile.id && edge.kind!="step") {
                if(edge.parent=="founder")employers.push_back(w.company_id);
                else {const auto slash=edge.parent.find("/employee/");if(slash!=edge.parent.npos)employers.push_back(edge.parent.substr(0,slash));else if(const auto* parent=family_member(w,edge.parent);parent && parent->application)employers.push_back(parent->application->company);}
            }
            std::sort(employers.begin(),employers.end());employers.erase(std::unique(employers.begin(),employers.end()),employers.end());
            if(!employers.empty() && rng.bounded(10000)<static_cast<std::uint64_t>(rules.application_bp)) {
                for(const auto& owner:employers) {
                    if(apply_member(w,member,owner,rng))break;
                }
            }
            member.next_application=advance_days(day,Days{rules.application_interval_days});changed=true;
        }
        if(!member.qualified_on)remember_due(state,qualifies);
        else if(member.seeks_work && !member.application)remember_due(state,member.next_application);
        if(changed)state.members.replace(i,std::move(member));
    }
}
void reconcile_steps(WorldState& w,bool force) {
    auto& state=*w.family;std::set<std::uint32_t> marriages;
    if(w.romance)for(const auto& [id,bond]:w.romance->active)if(bond.married_on)marriages.insert(id);
    if(!force && marriages==state.marriages)return;
    const auto day=campaign_day(w);
    for(std::size_t i=0;i<state.edges.size();++i){auto edge=state.edges[i];if(edge.kind=="step" && !edge.ended_on && (!marriages.contains(edge.marriage) || !has_parent(w,w.romance->active.at(edge.marriage).first==edge.parent?w.romance->active.at(edge.marriage).second:w.romance->active.at(edge.marriage).first,edge.child))){edge.ended_on=day;state.edges.replace(i,std::move(edge));}}
    std::map<std::string,std::vector<const FamilyEdge*>> children;
    std::set<std::pair<std::string,std::string>> existing;
    for(const auto& edge:state.edges)if(!edge.ended_on){existing.emplace(edge.parent,edge.child);if(edge.kind!="step")children[edge.parent].push_back(&edge);}
    std::vector<FamilyEdge> additions;
    if(w.romance)for(const auto& [id,bond]:w.romance->active)if(bond.married_on)for(const auto& parent:{bond.first,bond.second}) {
        const auto other=parent==bond.first?bond.second:bond.first;const auto found=children.find(parent);if(found==children.end())continue;
        for(const auto* edge:found->second)if(other!=edge->child && existing.emplace(other,edge->child).second)additions.push_back({other,edge->child,"step",day,{},edge->known && bond.known,id});
    }
    for(const auto& edge:additions)add_edge(w,edge.parent,edge.child,edge.kind,edge.started_on,edge.known,edge.marriage);
    state.marriages=std::move(marriages);
}
}
}
