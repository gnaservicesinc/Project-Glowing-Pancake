#include "family_internal.hpp"
#include <algorithm>
#include <stdexcept>
namespace pgp::runtime {
using namespace core;
namespace {
bool chance(SplitMix64& rng,int bp){return rng.bounded(10000)<static_cast<std::uint64_t>(bp);}
bool caretaker(const WorldState& w,std::string_view child) {return w.family && std::ranges::any_of(w.family->edges,[&](const auto& e){return e.parent=="founder" && e.child==child && !e.ended_on && e.kind!="step";});}
void reply(WorldState& w,const FamilyRequest& request,std::string message,bool yes,std::uint32_t plan=0) {
    auto& state=*w.family;if(state.replies.size()>=maximum_family_plans)throw std::invalid_argument("The family conversation history is full.");
    const auto day=campaign_day(w);state.last_action=day;state.replies.push_back({day,request.action,request.child,std::move(message),yes,plan});
    w.romance->last_action=day;w.simulation.management->people->social->last_outing=w.simulation.date;
}
}
void initialize_family(WorldState& w) {
    const auto* rules=family_rules(w);if(!rules)return;if(!w.romance || !w.identity)throw std::invalid_argument("Families need a founder and shared personal history.");
    FamilyState state;state.stream=w.root_seed^UINT64_C(0x46414d494c59564d);state.next_review=advance_days(campaign_day(w),Days{rules->interval_days});w.family=std::move(state);
    SplitMix64 identities{w.family->stream};
    for(std::uint32_t id=1;id<=2;++id) {
        const auto born=to_ordinal({1997-static_cast<int>(24+identities.bounded(12)),static_cast<unsigned>(1+identities.bounded(12)),1});
        const auto surname=w.identity->founder_name.substr(w.identity->founder_name.find_last_of(' ')==std::string::npos?0:w.identity->founder_name.find_last_of(' ')+1);
        auto member=family_detail::make_member(w,id,born,{},true,false,surname);member.profile.role="Family member";
        const auto person=member.profile.id;w.family->members.push_back(std::move(member));family_detail::add_edge(w,person,"founder","biological",w.romance->founder.born_on,true);
    }
    w.family->stream=identities.state();
}
namespace family_detail {
std::string blocker(const WorldState& w,const FamilyRequest& request,const romance_detail::People& people) {
    const auto* rules=family_rules(w);if(!rules || !w.family || !w.romance)return "Family life is unavailable in this campaign.";
    const auto day=campaign_day(w);const auto& state=*w.family;
    if((state.last_action && *state.last_action==day) || (w.romance->last_action && *w.romance->last_action==day) || (w.simulation.management->people->social->last_outing && *w.simulation.management->people->social->last_outing==w.simulation.date))return "You already made personal plans today. Give this conversation its own day.";
    const auto me=people.find("founder");if(me==people.end() || !adult_on(*me->second.romantic,day))return "Family plans need an adult caregiver.";
    if(state.replies.size()>=maximum_family_plans)return "The family conversation history is full.";
    if(request.name.size()>128 || request.name.find('\0')!=request.name.npos)return "Choose a name with at most 128 characters.";
    if(request.action=="track" || request.action=="apply" || request.action=="adopt_placement") {
        const auto* child=family_member(w,request.child);if(!child || !child->known)return "Choose someone in your family album.";
        if(request.action=="apply") {
            if(!child->qualified_on || !adult_on(*child->profile.romantic,day))return "They need to become an adult and complete practical study before applying.";
            if(child->application)return "A previous application is recorded.";
            if(w.simulation.management->people->queued_applicants.size()>=12)return "The recruitment waiting list is full. Review existing applications first.";
        } else {
            if(!caretaker(w,child->profile.id))return "Choose a child currently in your care.";
            if(adult_on(*child->profile.romantic,day))return "They are an adult now; their learning and care choices belong to them.";
            if(request.action=="track") {
                if(family_age(child->profile.romantic->born_on,day)<6)return "Let them reach school age before exploring a practical learning path.";
                if(std::ranges::find(rules->tracks,request.track,&FamilyLearningTrack::skill)==rules->tracks.end())return "Choose a supported learning path.";
            } else {
                const auto placement=std::ranges::find_if(state.edges,[&](const auto& e){return e.parent=="founder" && e.child==child->profile.id && e.kind=="foster" && !e.ended_on;});
                if(placement==state.edges.end())return "This child is not in an active foster placement with you.";
                if(day<advance_days(placement->started_on,Days{rules->foster_adoption_days}))return "Give the placement time before requesting an adoption review.";
                if(std::ranges::any_of(state.pending,[&](const auto& row){return row.second.child==child->profile.id;}))return "An adoption review is already underway for this child.";
            }
        }
        const auto cooldown=request.action=="track"?rules->study_days:rules->application_interval_days;
        for(auto i=state.replies.size();i>0;--i){const auto& r=state.replies[i-1];if(r.subject==child->profile.id && r.action==request.action && day<advance_days(r.day,Days{cooldown}))return "Give them time before repeating this conversation.";}
        if(request.action!="adopt_placement")return {};
    } else if(request.action!="birth" && request.action!="adopt" && request.action!="foster")return "Choose a family plan, learning conversation or application invitation.";
    if(state.pending.size()>=128 || state.next_plan>maximum_family_plans || state.members.size()+state.pending.size()>=maximum_family_people || state.edges.size()+4>=maximum_family_edges)return "The family register is full.";
    if(request.action!="birth" && !request.name.empty())return "Children joining through adoption or foster care keep their own names.";
    const auto* partner=romantic_partner(w,"founder");
    if(request.action=="birth" || request.with_partner) {
        if(!partner || !partner->married_on)return "A shared household plan needs a spouse. Adoption and fostering also support an individual caregiver.";
        if(day<advance_days(*partner->married_on,Days{rules->married_days}) || partner->affection<romance_rules(w)->marriage_affection)return "Make time for your marriage before starting a shared family plan.";
    }
    if(request.action=="adopt_placement")return {};
    for(auto i=state.replies.size();i>0;--i){const auto& previous=state.replies[i-1];if((previous.action=="birth" || previous.action=="adopt" || previous.action=="foster") && day<advance_days(previous.day,Days{rules->application_interval_days}))return "Give everyone time before opening another family-planning conversation.";}
    if(dependents(w,"founder")>=rules->maximum_children)return "Your household is at its current dependent-care limit.";
    const auto recent=[&](const FamilyPlan& p){return (p.first=="founder" || p.second=="founder") && p.kind!="adopt_placement" && day<advance_days(p.requested_on,Days{rules->household_cooldown});};
    for(const auto& [id,p]:state.pending){(void)id;if(recent(p))return "Let your household settle before beginning another family plan.";}
    for(auto i=state.history.size();i>0;--i)if(recent(state.history[i-1]))return "Let your household settle before beginning another family plan.";
    const auto min_age=request.band=="baby"?0:request.band=="child"?4:request.band=="teen"?13:-1;
    if(min_age<0)return "Choose a baby, child or teenager age group.";
    if(request.action!="birth" && family_age(me->second.romantic->born_on,day)<min_age+rules->minimum_parent_gap)return "This age group needs an older caregiver. You can choose a younger age group.";
    if((request.action=="birth" || request.with_partner) && partner) {
        const auto other=romance_detail::other(*partner,"founder");const auto found=people.find(other);
        if(found==people.end() || !adult_on(*found->second.romantic,day) || dependents(w,other)>=rules->maximum_children)return "Your partner is not available for another dependent-care commitment.";
        if(request.action!="birth" && family_age(found->second.romantic->born_on,day)<min_age+rules->minimum_parent_gap)return "This age group needs an older caregiver in both parenting roles.";
    }
    return {};
}
}
FamilyQuote family_quote(const WorldState& w,const FamilyRequest& request) {
    FamilyQuote quote;quote.blocker=family_detail::blocker(w,request,romance_detail::roster(w));const auto* rules=family_rules(w);if(!rules)return quote;
    const auto delay=request.action=="birth"?rules->birth_days:request.action=="adopt" || request.action=="adopt_placement"?rules->adoption_days:request.action=="foster"?rules->foster_days:0;
    if(delay)quote.due_on=advance_days(campaign_day(w),Days{delay});
    if(request.action=="birth")quote.explanation="Plan to welcome a baby together.";
    else if(request.action=="foster")quote.explanation="Offer a foster placement for "+std::to_string(rules->foster_term_days)+" days. An adoption review opens after "+std::to_string(rules->foster_adoption_days)+" days of care.";
    else if(request.action=="adopt" || request.action=="adopt_placement")quote.explanation="Start an adoption review.";
    else if(request.action=="track") {
        quote.explanation="Explore this interest together.";
        if(const auto* child=family_member(w,request.child))quote.explanation+=" Study continues until at least "+iso_date(std::max(to_ordinal(family_detail::anniversary(child->profile.romantic->born_on,18)),advance_days(campaign_day(w),Days{rules->study_days})))+".";
    } else quote.explanation="Mention the opening to them.";
    return quote;
}
std::string manage_family(WorldState& w,const FamilyRequest& request) {
    const auto quote=family_quote(w,request);if(!quote.blocker.empty())throw std::invalid_argument(quote.blocker);
    auto& state=*w.family;const auto& rules=*family_rules(w);const auto day=campaign_day(w);SplitMix64 rng{state.stream};bool yes=true;std::string message;std::uint32_t plan=0;
    if(request.action=="track") {
        const auto* original=family_member(w,request.child);auto child=*original;yes=family_age(child.profile.romantic->born_on,day)<12 || chance(rng,rules.older_child_consent_bp);
        if(yes){child.track=request.track;child.learning_since=day;state.members.replace(static_cast<std::size_t>(std::stoul(child.profile.id.substr(14))-1),child);message="You made time to explore their interests.";family_detail::remember_due(state,std::max(to_ordinal(family_detail::anniversary(child.profile.romantic->born_on,18)),advance_days(day,Days{rules.study_days})));}
        else message="They would rather follow their current interests. You listened.";
    } else if(request.action=="apply") {
        auto child=*family_member(w,request.child);yes=chance(rng,rules.consent_bp);
        if(yes){yes=family_detail::apply_member(w,child,w.company_id,rng);if(yes){const auto index=static_cast<std::size_t>(std::stoul(child.profile.id.substr(14))-1);state.members.replace(index,child);}}
        message=yes?"They sent an application.":"They are keeping their options open.";
    } else {
        yes=chance(rng,rules.consent_bp);
        if(request.action=="adopt_placement")if(const auto* child=family_member(w,request.child);family_age(child->profile.romantic->born_on,day)>=12)yes=chance(rng,rules.older_child_consent_bp) && yes;
        if(yes) {
            std::string second;if(request.action=="birth" || request.with_partner)second=romance_detail::other(*romantic_partner(w,"founder"),"founder");
            plan=family_detail::plan(w,"founder",second,request,true).id;message="Your plan is on the calendar.";
        } else message="This isn't the right time for this plan.";
    }
    state.stream=rng.state();reply(w,request,message,yes,plan);return message;
}
void prepare_family(WorldState& w) {
    if(!w.family)return;auto& state=*w.family;family_detail::reconcile_steps(w);
    const auto day=campaign_day(w);const auto& rules=*family_rules(w);const bool review=day>=state.next_review;
    if(!review && (!state.next_due || day<*state.next_due))return;
    state.next_due.reset();SplitMix64 rng{state.stream};family_detail::resolve_plans(w,rng);family_detail::qualify_and_apply(w,rng);
    if(review && rules.npc_family_bp>0 && w.romance && state.pending.size()<128 && state.next_plan<=maximum_family_plans && state.members.size()+state.pending.size()<maximum_family_people) {
        const auto people=romance_detail::roster(w);std::set<std::string> recent;
        for(const auto& p:state.history)if(day<advance_days(p.requested_on,Days{rules.household_cooldown})){recent.insert(p.first);if(!p.second.empty())recent.insert(p.second);}
        for(const auto& [id,p]:state.pending){(void)id;recent.insert(p.first);if(!p.second.empty())recent.insert(p.second);}
        for(const auto& [id,bond]:w.romance->active) {
            (void)id;if(bond.first=="founder" || bond.second=="founder" || !bond.married_on || day<advance_days(*bond.married_on,Days{rules.married_days}) || bond.affection<romance_rules(w)->marriage_affection || recent.contains(bond.first) || recent.contains(bond.second) || family_detail::dependents(w,bond.first)>=rules.maximum_children || family_detail::dependents(w,bond.second)>=rules.maximum_children)continue;
            const auto a=people.find(bond.first),b=people.find(bond.second);if(a==people.end() || b==people.end())continue;
            if(!chance(rng,rules.npc_family_bp))continue;
            const auto kind=rng.bounded(100);FamilyRequest request;request.action=kind<static_cast<std::uint64_t>(rules.birth_weight)?"birth":kind<static_cast<std::uint64_t>(rules.birth_weight+rules.adoption_weight)?"adopt":"foster";
            const auto parent_age=std::min(family_age(a->second.romantic->born_on,day),family_age(b->second.romantic->born_on,day));request.band=parent_age>=rules.minimum_parent_gap+17 && rng.bounded(3)==0?"teen":parent_age>=rules.minimum_parent_gap+8 && rng.bounded(2)==0?"child":"baby";
            (void)family_detail::plan(w,bond.first,bond.second,request,bond.known);recent.insert(bond.first);recent.insert(bond.second);
            if(state.pending.size()>=128 || state.next_plan>maximum_family_plans || state.members.size()+state.pending.size()>=maximum_family_people)break;
        }
    }
    family_detail::reconcile_steps(w,true);
    for(const auto& [id,p]:state.pending){(void)id;family_detail::remember_due(state,p.due_on);}
    state.stream=rng.state();if(review)state.next_review=advance_days(day,Days{rules.interval_days});
}
}
