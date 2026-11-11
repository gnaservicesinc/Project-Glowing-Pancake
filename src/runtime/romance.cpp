#include "romance_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::runtime {
using namespace core;
namespace {
[[noreturn]] void invalid(const std::string& why){throw std::invalid_argument(why);}
void finish(RomanceState& state,std::uint32_t id,OrdinalDay day,std::string reason) {
    auto found=state.active.find(id);auto bond=found->second;bond.ended_on=day;bond.end_reason=std::move(reason);bond.memory=bond.end_reason;state.history.push_back(std::move(bond));state.active.erase(found);
}
RomanceBond& begin(RomanceState& state,std::string a,std::string b,OrdinalDay day,const RomanceRules& rules,bool known) {
    if(state.next_id>romance_history_limit)invalid("The relationship history is full.");
    if(b<a){std::swap(a,b);}
    RomanceBond bond;bond.id=state.next_id++;bond.first=std::move(a);bond.second=std::move(b);bond.started_on=day;bond.last_time_together=day;
    bond.affection=rules.initial_affection;bond.known=known;bond.memory="Both chose to start dating";return state.active.emplace(bond.id,std::move(bond)).first->second;
}
bool chance(SplitMix64& stream,int bp){return stream.bounded(10000)<static_cast<std::uint64_t>(bp);}
void claim_day(WorldState& w) {
    const auto day=campaign_day(w);w.romance->last_action=day;
    if(w.simulation.management && w.simulation.management->people && w.simulation.management->people->social)w.simulation.management->people->social->last_outing=w.simulation.date;
}
}
void initialize_romance(WorldState& w) {
    const auto* r=romance_rules(w);if(!r)return;if(!w.founding)invalid("The personal romance world needs a founder identity.");
    RomanceState state;state.founder={to_ordinal({1997,1,1}),generate_romantic_preferences(w.founding->gender,w.founding->orientation,w.root_seed,*r)};
    state.founder.preferences.interested=true;state.founder.preferences.public_orientation=true;
    SplitMix64 seed{w.root_seed^UINT64_C(0x524f4d414e434556)};state.stream=seed.next();state.next_review=advance_days(campaign_day(w),Days{r->interval_days});
    for(auto& e:w.simulation.management->people->employees)if(e.terms.profile.role=="Founder / researcher")e.terms.profile.romantic=state.founder;
    w.romance=std::move(state);
}
namespace romance_detail {
std::string blocked(const WorldState& w,const People& people,const RomanceRequest& request) {
    const auto* r=romance_rules(w);if(!r || !w.romance)return "Romance is unavailable in this campaign.";
    const auto day=campaign_day(w);const auto id=romance_person_id(w,request.person);const auto me=people.find("founder"),them=people.find(id);
    if(me==people.end() || them==people.end() || id=="founder")return "Choose someone in your social circle.";
    if(family_related(w,"founder",id))return "Family relationships stay separate from romance.";
    if(!adult_on(*me->second.romantic,day) || !adult_on(*them->second.romantic,day))return "Romantic relationships are for adults only.";
    const auto* current=romantic_partner(w,"founder");const bool together=current && other(*current,"founder")==id;
    if(request.action=="separate")return together?std::string{}:"Choose your current partner.";
    if((w.romance->last_action && *w.romance->last_action==day) || (w.simulation.management->people->social->last_outing && *w.simulation.management->people->social->last_outing==w.simulation.date))return "You have already made personal plans today. Leave a little room in the calendar.";
    if(request.action=="ask") {
        if(current)return "You are already in a relationship. Resolve it before asking someone else out.";
        if(const auto* previous=latest_romance(w,"founder",id);previous && previous->ended_on && day<advance_days(*previous->ended_on,Days{r->proposal_cooldown}))return "Give each other space after the breakup before asking to start again.";
        if(!w.romance->founder.preferences.interested)return "You are not seeking romance. You can change your own preferences.";
        if(friendship(w,"founder",id)<r->friendship)return "Build a friendship before asking about romance.";
        if(const auto* partner=romantic_partner(w,id);partner && partner->known)return "They have shared that they are already in a relationship.";
        for(auto i=w.romance->replies.size();i>0;--i){const auto& reply=w.romance->replies[i-1];if(reply.person==id && reply.action=="ask" && day<advance_days(reply.day,Days{r->proposal_cooldown}))return "Respect their answer and give them space before asking again.";}
        return {};
    }
    if(!together)return "Choose your current romantic partner.";
    if(request.action=="walk" || request.action=="listen" || request.action=="hobby") {
        if(day<advance_days(current->last_time_together,Days{r->date_cooldown}))return "Make your next date after this week's plans have had room to breathe.";
        if(request.action=="walk") {
            if(distance(me->second,them->second)!="city")return "A walk together needs both of you in the same city. Try an online plan instead.";
            const auto where=executive_location(w.simulation);if(where.traveling || where.appointment)return "Finish your trip or appointment before planning a walk at home.";
        }
        return {};
    }
    if(request.action!="engage" && request.action!="marry")return "Choose an invitation, date, proposal or separation.";
    if(current->last_proposal && day<advance_days(*current->last_proposal,Days{r->proposal_cooldown}))return "Give your partner time before proposing another big step.";
    if(request.action=="engage") {
        if(current->engaged_on || current->married_on)return "You have already taken this step together.";
        if(day<advance_days(current->started_on,Days{r->dating_days}) || current->affection<r->engagement_affection || current->dates<r->minimum_dates)return "Let the relationship grow through time and dates before discussing engagement.";
    } else {
        if(!current->engaged_on || current->married_on)return "Become engaged before planning a marriage.";
        if(day<advance_days(*current->engaged_on,Days{r->engagement_days}) || current->affection<r->marriage_affection)return "Give the engagement time and keep making time for each other.";
    }
    return {};
}
}
RomanceQuote romance_quote(const WorldState& w,const RomanceRequest& request) {
    const auto people=romance_detail::roster(w);RomanceQuote q;q.blocker=romance_detail::blocked(w,people,request);
    q.explanation=request.action=="ask"?"You can ask, and they choose their answer. A polite no does not damage your friendship.":request.action=="engage" || request.action=="marry"?"This is a shared decision; your partner may want more time.":request.action=="separate"?"The romantic relationship ends. Your friendship has its own history and does not automatically end with it.":"A little time together, at no charge. Friendship and romantic closeness are separate.";return q;
}
std::string pursue_romance(WorldState& w,const RomanceRequest& request) {
    auto people=romance_detail::roster(w);const auto why=romance_detail::blocked(w,people,request);if(!why.empty())invalid(why);
    auto& state=*w.romance;const auto& rules=*romance_rules(w);const auto day=campaign_day(w);const auto id=romance_person_id(w,request.person);SplitMix64 rng{state.stream};
    if(state.replies.size()>=romance_history_limit)invalid("The personal invitation history is full.");
    RomanticReply reply{id,request.action,{},day,0,false};
    if(request.action=="ask") {
        const auto& a=people.at("founder");const auto& b=people.at(id);
        const bool mutual=!romantic_partner(w,id) && mutual_romantic_interest(a.gender,a.romantic->preferences,b.gender,b.romantic->preferences,romance_detail::friendship(w,"founder",id));
        const bool yes=chance(rng,rules.ask_acceptance_bp) && mutual;
        reply.accepted=yes;reply.response=yes?"They said yes. You're dating!":"They thanked you for asking and would like to keep things platonic.";
        if(yes)reply.relationship=begin(state,"founder",id,day,rules,true).id;
    } else {
        const auto* current=romantic_partner(w,"founder");const auto relationship=current->id;auto& bond=state.active.at(relationship);reply.relationship=relationship;
        if(request.action=="separate") {
            reply.accepted=true;reply.response=bond.married_on?"You separated. The marriage is now part of your shared history.":"You ended the romantic relationship and gave each other room.";
            finish(state,relationship,day,reply.response);
        } else if(request.action=="engage" || request.action=="marry") {
            bond.last_proposal=day;reply.accepted=chance(rng,rules.proposal_acceptance_bp);
            if(reply.accepted){if(request.action=="engage")bond.engaged_on=day;else bond.married_on=day;reply.response=request.action=="engage"?"You both said yes. You're engaged!":"You shared your vows. You're married!";}
            else reply.response="Your partner would like more time. You are still together.";
            bond.memory=reply.response;
        } else {
            const auto distance=romance_detail::distance(people.at("founder"),people.at(id));
            const auto gain=distance=="city"?rules.date_gain:distance=="state"?std::max(1,rules.date_gain*rules.state_gain/rules.city_gain):std::max(1,rules.date_gain/2);
            bond.affection=std::min(1000,bond.affection+gain);bond.last_time_together=day;bond.dates=std::min(10000,bond.dates+1);bond.strained_on.reset();
            bond.memory=request.action=="walk"?"A walk with time for a real conversation":request.action=="listen"?"An unhurried call and space to listen":"A shared interest, a few laughs, and time together";
            reply.accepted=true;reply.response=bond.memory;
        }
    }
    claim_day(w);state.stream=rng.state();state.replies.push_back(reply);return reply.response;
}
void set_romantic_preferences(WorldState& w,bool interested,const std::vector<std::string>& genders) {
    if(!w.romance){invalid("Romantic preferences are unavailable.");}
    auto candidate=w.romance->founder;candidate.preferences.interested=interested;candidate.preferences.genders=genders;validate_romantic_profile(candidate,1997);w.romance->founder=std::move(candidate);
    for(auto& e:w.simulation.management->people->employees)if(e.terms.profile.role=="Founder / researcher")e.terms.profile.romantic=w.romance->founder;
}
void prepare_romance(WorldState& w) {
    if(!w.romance){return;}
    auto& state=*w.romance;const auto& rules=*romance_rules(w);const auto day=campaign_day(w);
    const auto& p=*w.simulation.management->people;const auto* social=social_rules(w.simulation);
    // Index ties once instead of scanning every link for every identity.
    std::map<std::string,int> founder_friends;
    std::map<std::string,std::string> local_ids;
    for(const auto& e:p.employees)local_ids.emplace(e.terms.profile.id,e.terms.profile.role=="Founder / researcher"?"founder":family_person_id(w,w.company_id+"/"+e.terms.profile.id));
    const auto canonical=[&](const std::string& id){const auto found=local_ids.find(id);return found==local_ids.end()?id:found->second;};
    if(p.social)for(const auto& edge:p.social->links)if(edge.first=="founder" || edge.second=="founder") {
        auto id=edge.first=="founder"?edge.second:edge.first;
        if(id.starts_with("employee/"))id=w.company_id+"/"+id;
        id=family_person_id(w,id);
        founder_friends.emplace(std::move(id),edge.affinity);
    }
    const auto affinity=[&](const std::string& id){const auto found=founder_friends.find(id);return found==founder_friends.end()?0:found->second;};
    const auto remember=[&](const PersonProfile& person,const std::string& id) {
        if(person.romantic && (person.romantic->preferences.public_orientation || (social && affinity(id)>=social->private_threshold)))state.known_identities.insert(id);
    };
    for(const auto& e:p.employees)if(e.terms.profile.role!="Founder / researcher")remember(e.terms.profile,family_person_id(w,w.company_id+"/"+e.terms.profile.id));
    for(const auto& [id,person]:p.industry)remember(person.profile,family_person_id(w,id));
    if(day<state.next_review)return;
    const auto people=romance_detail::roster(w);const auto ancestors=family_ancestors(w);
    const auto related=[&](const std::string& first,const std::string& second){const auto a=ancestors.find(first),b=ancestors.find(second);if((a!=ancestors.end() && a->second.contains(second)) || (b!=ancestors.end() && b->second.contains(first)))return true;if(a!=ancestors.end() && b!=ancestors.end())for(const auto& person:a->second)if(b->second.contains(person))return true;return false;};
    SplitMix64 rng{state.stream};std::map<std::string,std::uint32_t> partners;
    std::vector<std::uint32_t> ended;
    for(auto& [id,bond]:state.active) {
        partners.emplace(bond.first,id);partners.emplace(bond.second,id);const auto a=people.find(bond.first),b=people.find(bond.second);
        const bool founder=bond.first=="founder" || bond.second=="founder";
        const auto where=a!=people.end() && b!=people.end()?romance_detail::distance(a->second,b->second):"remote";
        if(!founder && chance(rng,rules.npc_progress_bp)) {
            const auto gain=where=="city"?rules.city_gain:where=="state"?rules.state_gain:0;
            if(gain){bond.affection=std::min(1000,bond.affection+gain);bond.last_time_together=day;bond.dates=std::min(10000,bond.dates+1);bond.memory="Made time for each other";}
        }
        if(day.value()-bond.last_time_together.value()>=rules.neglect_days)bond.affection=std::max(0,bond.affection-rules.remote_decay);
        if(bond.affection<=rules.strain_affection) {if(!bond.strained_on){bond.strained_on=day;bond.memory="Time apart has put this relationship under strain. Make space for a conversation.";}else if(day>=advance_days(*bond.strained_on,Days{rules.strain_grace_days}))ended.push_back(id);}
        else bond.strained_on.reset();
        if(!founder && !bond.strained_on && bond.dates>=rules.minimum_dates) {
            if(!bond.engaged_on && day>=advance_days(bond.started_on,Days{rules.dating_days}) && bond.affection>=rules.engagement_affection && chance(rng,rules.npc_progress_bp)){bond.engaged_on=day;bond.memory="Both chose to become engaged";}
            else if(bond.engaged_on && !bond.married_on && day>=advance_days(*bond.engaged_on,Days{rules.engagement_days}) && bond.affection>=rules.marriage_affection && chance(rng,rules.npc_progress_bp)){bond.married_on=day;bond.memory="Shared their vows and married";}
        }
        if(!bond.known && affinity(bond.first)>=rules.friendship && affinity(bond.second)>=rules.friendship)bond.known=true;
    }
    for(auto id:ended){const auto bond=state.active.at(id);partners.erase(bond.first);partners.erase(bond.second);finish(state,id,day,"After sustained strain and time to reconnect, the relationship ended");}
    if(w.simulation.management && w.simulation.management->people && w.simulation.management->people->social && state.next_id<=romance_history_limit) {
        std::map<std::string,OrdinalDay> latest_end;
        for(const auto& b:state.history)if(b.ended_on){latest_end.insert_or_assign(b.first,*b.ended_on);latest_end.insert_or_assign(b.second,*b.ended_on);}
        for(const auto& edge:w.simulation.management->people->social->links) {
            if(edge.affinity<rules.friendship || edge.first=="founder" || edge.second=="founder")continue;
            const auto first=canonical(edge.first),second=canonical(edge.second);if(partners.contains(first) || partners.contains(second) || related(first,second))continue;
            const auto a=people.find(first),b=people.find(second);if(a==people.end() || b==people.end() || !a->second.employed || !b->second.employed ||
                !adult_on(*a->second.romantic,day) || !adult_on(*b->second.romantic,day) || romance_detail::distance(a->second,b->second)=="remote" ||
                !mutual_romantic_interest(a->second.gender,a->second.romantic->preferences,b->second.gender,b->second.romantic->preferences,edge.affinity))continue;
            const auto recent=[&](const std::string& who){const auto found=latest_end.find(who);return found!=latest_end.end() && day<advance_days(found->second,Days{rules.proposal_cooldown});};
            if(recent(first) || recent(second))continue;
            if(chance(rng,rules.npc_start_bp)){const auto id=begin(state,first,second,day,rules,edge.known).id;partners.emplace(first,id);partners.emplace(second,id);if(state.next_id>romance_history_limit)break;}
        }
    }
    state.stream=rng.state();state.next_review=advance_days(day,Days{rules.interval_days});
}
}
