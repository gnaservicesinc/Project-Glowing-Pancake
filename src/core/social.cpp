#include <pgp/core/social.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* why){throw std::invalid_argument(why);}
const Employee* employee(const SimulationState& s,std::string_view id) {
    if(!s.management || !s.management->people)return nullptr;
    const auto& rows=s.management->people->employees;
    const auto it=std::ranges::find_if(rows,[&](const auto& e){return id=="founder"?e.terms.profile.role=="Founder / researcher":e.terms.profile.id==id;});
    return it==rows.end()?nullptr:&*it;
}
bool present(const SimulationState& s,std::string_view id) {
    if(id=="founder"){const auto where=executive_location(s);return !where.traveling && !where.appointment;}
    const auto* e=employee(s,id);return e && !e->departed_on && e->ready_on<=s.date;
}
bool exists(const SimulationState& s,std::string_view id){return id=="founder" || employee(s,id);}
const std::vector<std::string>& traits(const SimulationState& s,std::string_view id) {
    if(id=="founder")return s.management->people->social->founder_traits;
    return employee(s,id)->terms.profile.traits;
}
bool public_trait(std::string_view t) {
    return t.starts_with("Specialty: ") || t.starts_with("Practice: ") || t.starts_with("Industry: ") ||
        t.starts_with("Collaboration: ") || t.starts_with("Perspective: ");
}
std::pair<std::string,std::string> ordered(const SimulationState& s,std::string_view a,std::string_view b) {
    auto first=social_id(s,a),second=social_id(s,b);if(second<first)std::swap(first,second);return {first,second};
}
SocialLink& link(SimulationState& s,std::string_view a,std::string_view b) {
    const auto [first,second]=ordered(s,a,b);auto& rows=s.management->people->social->links;
    const auto it=std::ranges::find_if(rows,[&](const auto& row){return row.first==first && row.second==second;});
    if(it!=rows.end())return *it;
    if(rows.size()>=8192)invalid("The social circle has reached its supported limit.");
    rows.push_back({first,second,0,0,s.date,s.date,{},"Met at headquarters",first=="founder" || second=="founder"});return rows.back();
}
int fit(const SimulationState& s,std::string_view a,std::string_view b) {
    const auto& r=*social_rules(s);const auto& left=traits(s,a);const auto& right=traits(s,b);
    int shared=0,friction=0;
    for(const auto& t:left) {
        if(std::ranges::find(right,t)!=right.end())++shared;
        else if(t.starts_with("Social: ") || t.starts_with("Temperament: "))++friction;
    }
    return shared*r.shared_gain-friction*r.friction;
}
void reveal(SimulationState& s,const SocialLink& bond) {
    if(bond.first!="founder" && bond.second!="founder")return;
    const auto id=bond.first=="founder"?bond.second:bond.first;
    auto& known=s.management->people->social->known_traits[id];const auto& all=traits(s,id);const auto& r=*social_rules(s);
    const bool outgoing=std::ranges::find(all,"Social: outgoing")!=all.end();
    for(const auto& t:all) {
        const bool private_info=t.starts_with("Upbringing: ");
        const auto threshold=private_info?r.private_threshold:outgoing?r.outgoing_threshold:r.thresholds[1];
        if((public_trait(t) || bond.affinity>=threshold) && std::ranges::find(known,t)==known.end())known.push_back(t);
    }
}
void encounter(SimulationState& s,std::string_view a,std::string_view b,int base,std::string memory) {
    auto& bond=link(s,a,b);bond.affinity=std::min(1000,bond.affinity+std::max(1,base+fit(s,a,b)));
    bond.encounters=std::min(100000,bond.encounters+1);bond.last_contact=s.date;bond.memory=std::move(memory);reveal(s,bond);
}
}
const SocialRules* social_rules(const SimulationState& s) {
    return s.management && s.management->people && s.management->catalog.people && s.management->catalog.people->social?&*s.management->catalog.people->social:nullptr;
}
void validate_social_rules(const SocialRules& r) {
    if(r.interval_days<7 || r.interval_days>30 || r.weekly_gain<1 || r.weekly_gain>30 || r.shared_gain<0 || r.shared_gain>5 ||
       r.friction<0 || r.friction>10 || r.interaction_gain<1 || r.interaction_gain>50 || r.outgoing_threshold<1 ||
       r.outgoing_threshold>200 || r.private_threshold<200 || r.private_threshold>850 || r.remote_decay<0 || r.remote_decay>10 || r.mood_cap<0 || r.mood_cap>10)
        invalid("Social rules exceed their bounded contact and benefit limits.");
    int previous=0;for(int threshold:r.thresholds){if(threshold<=previous || threshold>1000)invalid("Friendship thresholds must increase.");previous=threshold;}
}
std::string social_id(const SimulationState& s,std::string_view id) {
    const auto* e=employee(s,id);return e && e->terms.profile.role=="Founder / researcher"?"founder":std::string(id);
}
const SocialLink* social_link(const SimulationState& s,std::string_view a,std::string_view b) {
    if(!social_rules(s) || !s.management->people->social)return nullptr;
    const auto [first,second]=ordered(s,a,b);const auto& rows=s.management->people->social->links;
    const auto it=std::ranges::find_if(rows,[&](const auto& row){return row.first==first && row.second==second;});return it==rows.end()?nullptr:&*it;
}
void initialize_social(SimulationState& s,std::uint64_t seed) {
    if(!social_rules(s))return;
    SocialState state;SplitMix64 rng{seed^UINT64_C(0x534f4349414c5631)};state.stream=rng.next();state.next_meeting=advance_days(s.date,Days{social_rules(s)->interval_days});
    s.management->people->social=std::move(state);
}
void prepare_social(SimulationState& s) {
    if(!social_rules(s))return;auto& state=*s.management->people->social;const auto& r=*social_rules(s);
    if(s.date<state.next_meeting)return;
    std::vector<std::string> roster;if(present(s,"founder"))roster.push_back("founder");
    for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher" && present(s,e.terms.profile.id))roster.push_back(e.terms.profile.id);
    SplitMix64 rng{state.stream};
    // A shuffled matching is linear in roster size: each person has at most one
    // spontaneous encounter per week. Established ties get their own bounded pass.
    for(std::size_t n=roster.size();n>1;--n) {const auto j=static_cast<std::size_t>(rng.bounded(n));std::swap(roster[n-1],roster[j]);}
    for(std::size_t i=1;i<roster.size();i+=2) {
        const auto* old=social_link(s,roster[i-1],roster[i]);
        if(!old && state.links.size()>=8192)continue;
        encounter(s,roster[i-1],roster[i],r.weekly_gain,"A conversation in the office");
    }
    for(auto& bond:state.links) {
        if(bond.last_contact==s.date)continue;
        if(present(s,bond.first) && present(s,bond.second) && bond.affinity>=r.thresholds[1]) {
            bond.affinity=std::min(1000,bond.affinity+std::max(1,r.weekly_gain+fit(s,bond.first,bond.second)));
            bond.last_contact=s.date;bond.encounters=std::min(100000,bond.encounters+1);bond.memory="Caught up with an office friend";reveal(s,bond);
        } else if(s.date.value()-bond.last_contact.value()>=28)bond.affinity=std::max(0,bond.affinity-r.remote_decay);
    }
    std::set<std::string> founder_friends;
    for(const auto& bond:state.links)if(bond.affinity>=r.thresholds[2]) {
        if(bond.first=="founder")founder_friends.insert(bond.second);
        if(bond.second=="founder")founder_friends.insert(bond.first);
    }
    for(auto& bond:state.links)if(!bond.known && bond.affinity>=r.thresholds[3] &&
        founder_friends.contains(bond.first) && founder_friends.contains(bond.second))bond.known=true;
    state.stream=rng.state();state.next_meeting=advance_days(s.date,Days{r.interval_days});
}
std::string social_blocker(const SimulationState& s,std::string_view person) {
    if(!social_rules(s))return "The social circle is unavailable.";
    const auto id=social_id(s,person);const auto& state=*s.management->people->social;
    if(id=="founder" || !present(s,id))return "Choose an onboarded HQ coworker.";
    const auto location=executive_location(s);
    if(location.traveling || location.appointment)return "Finish your trip or appointment before making plans at HQ.";
    if(state.last_outing && *state.last_outing==s.date)return "You have already made time for someone today.";
    const auto* bond=social_link(s,"founder",id);
    if(bond && bond->last_invited && s.date<advance_days(*bond->last_invited,Days{social_rules(s)->interval_days}))return "Give this person a little space; make plans again next week.";
    return {};
}
void socialize(SimulationState& s,const Socialize& action) {
    const auto reason=social_blocker(s,action.person);if(!reason.empty())throw std::invalid_argument(reason);
    if(action.activity!="chat" && action.activity!="listen" && action.activity!="hobby")invalid("Choose a conversation, listening or shared interests.");
    const auto id=social_id(s,action.person);const auto& r=*social_rules(s);
    int gain=r.interaction_gain;std::string memory="Made time for a relaxed conversation";
    if(action.activity=="listen") {if(person_has_skill(s,"founder","empathy"))gain+=r.shared_gain;memory="Listened and made room for a different point of view";}
    if(action.activity=="hobby") {
        bool shared=false;for(const auto& t:traits(s,"founder"))if(t.starts_with("Interest: ") && std::ranges::find(traits(s,id),t)!=traits(s,id).end())shared=true;
        if(shared)gain+=r.weekly_gain;memory=shared?"Spent time on a shared interest":"Tried something new together";
    }
    encounter(s,"founder",id,gain,memory);link(s,"founder",id).last_invited=s.date;s.management->people->social->last_outing=s.date;
}
void note_social_contact(SimulationState& s,std::string_view person) {
    if(!social_rules(s) || !present(s,person))return;
    auto& bond=link(s,"founder",person);
    if(bond.encounters && s.date.value()-bond.last_contact.value()<social_rules(s)->interval_days)return;
    encounter(s,"founder",person,social_rules(s)->weekly_gain,"Checked in and caught up");
}
std::string friendship_name(const SimulationState& s,int affinity,bool coworker) {
    if(!social_rules(s))return "Acquaintance";const auto& t=social_rules(s)->thresholds;
    if(affinity>=t[4])return "Best friends";if(affinity>=t[3])return "Close friend";if(affinity>=t[2])return "Friend";
    if(affinity>=t[1])return coworker?"Work friend":"Casual friend";if(affinity>=t[0])return "Casual friend";return "Acquaintance";
}
std::vector<std::string> visible_traits(const SimulationState& s,const PersonProfile& p) {
    if(!social_rules(s))return p.traits;const auto id=social_id(s,p.id);if(id=="founder")return p.traits;
    const auto& known=s.management->people->social->known_traits;const auto it=known.find(id);std::vector<std::string> result;
    for(const auto& t:p.traits)if(public_trait(t) || (it!=known.end() && std::ranges::find(it->second,t)!=it->second.end()))result.push_back(t);return result;
}
int friendship_mood(const SimulationState& s,std::string_view person) {
    if(!social_rules(s))return 0;int points=0;const auto id=social_id(s,person);
    for(const auto& bond:s.management->people->social->links)if(bond.affinity>=social_rules(s)->thresholds[2] && (bond.first==id || bond.second==id)) {
        const auto& other=bond.first==id?bond.second:bond.first;if(present(s,other))points+=bond.affinity>=social_rules(s)->thresholds[3]?2:1;
    }
    return std::min(points,social_rules(s)->mood_cap);
}
void validate_social(const SimulationState& s) {
    if(!s.management || !s.management->people)return;const auto* r=social_rules(s);const auto& state=s.management->people->social;
    if(static_cast<bool>(r)!=state.has_value())invalid("Social state and rules disagree.");if(!r)return;const auto& p=*state;
    if(p.next_meeting<s.management->people->starts_on || p.next_meeting>advance_days(s.date,Days{r->interval_days}) ||
       (p.last_outing && *p.last_outing>s.date) || p.links.size()>8192 || p.known_traits.size()>256 || p.founder_traits.size()>16)invalid("Social dates or size exceed their bounds.");
    std::set<std::string> founder_traits;for(const auto& t:p.founder_traits)if(t.empty() || t.size()>80 || !founder_traits.insert(t).second)invalid("Invalid founder trait.");
    std::set<std::pair<std::string,std::string>> pairs;
    for(const auto& bond:p.links) {
        if(bond.first>=bond.second || !exists(s,bond.first) || !exists(s,bond.second) || social_id(s,bond.first)!=bond.first || social_id(s,bond.second)!=bond.second ||
           !pairs.emplace(bond.first,bond.second).second || bond.affinity<0 || bond.affinity>1000 || bond.encounters<0 || bond.encounters>100000 ||
           bond.met_on<s.management->people->starts_on || bond.met_on>bond.last_contact || bond.last_contact>s.date ||
           (bond.last_invited && (*bond.last_invited<bond.met_on || *bond.last_invited>bond.last_contact)) || bond.memory.empty() || bond.memory.size()>160)
            invalid("A relationship has invalid participants, progression or dates.");
    }
    for(const auto& [id,known]:p.known_traits) {
        if(id=="founder" || !exists(s,id) || known.size()>16)invalid("Unknown trait disclosure recipient.");
        const auto& all=traits(s,id);std::set<std::string> unique;for(const auto& t:known)if(std::ranges::find(all,t)==all.end() || !unique.insert(t).second)invalid("Disclosed traits must belong to this person.");
    }
}
}
