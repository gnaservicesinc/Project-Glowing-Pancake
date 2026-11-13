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
const IndustryPerson* peer(const SimulationState& s,std::string_view id) {
    const auto& peers=s.management->people->industry;const auto it=peers.find(std::string(id));return it==peers.end()?nullptr:&it->second;
}
bool external(const SimulationState& s,std::string_view id){return peer(s,id)!=nullptr;}
bool exists(const SimulationState& s,std::string_view id){return id=="founder" || employee(s,id) || external(s,id);}
std::pair<std::string,std::string> location(const SimulationState& s,std::string_view id) {
    if(const auto* other=peer(s,id))return {other->state,other->city};
    if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->headquarters)return {"local","HQ"};
    const auto& premises=current_headquarters(s);
    for(const auto& market:s.infrastructure->catalog.workplace->headquarters->markets)if(market.id==premises.market)return {market.state,market.city};
    return {};
}
std::string employer(const SimulationState& s,std::string_view id) {if(const auto* other=peer(s,id))return other->company;return s.accounts?s.accounts->owner:"company.player";}
bool former(const SimulationState& s,std::string_view id){const auto* e=employee(s,id);return e && e->departed_on.has_value();}
bool available(const SimulationState& s,std::string_view id){if(const auto* other=peer(s,id))return other->active;if(const auto* e=employee(s,id))return e->ready_on<=s.date;return present(s,id);}
const std::vector<std::string>& traits(const SimulationState& s,std::string_view id) {
    if(id=="founder")return s.management->people->social->founder_traits;
    if(const auto* other=peer(s,id))return other->profile.traits;
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
void advance_bond(SimulationState& s,SocialLink& bond,int base,std::string memory,int ceiling=1000,bool affinity_fit=true) {
    if(is_personal_rival(s,bond.first,bond.second))return;
    bond.affinity=std::min(std::max(bond.affinity,ceiling),bond.affinity+std::max(1,base+(affinity_fit?fit(s,bond.first,bond.second):0)));
    bond.encounters=std::min(100000,bond.encounters+1);bond.last_contact=s.date;bond.memory=std::move(memory);reveal(s,bond);
}
void encounter(SimulationState& s,std::string_view a,std::string_view b,int base,std::string memory,int ceiling=1000,bool affinity_fit=true) {
    if(is_personal_rival(s,a,b))return;
    advance_bond(s,link(s,a,b),base,std::move(memory),ceiling,affinity_fit);
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
    if(r.city_chance_bp<1 || r.city_chance_bp>10000 || r.state_chance_bp<1 || r.state_chance_bp>r.city_chance_bp ||
       r.remote_chance_bp<0 || r.remote_chance_bp>r.state_chance_bp || r.city_gain<1 || r.city_gain>40 || r.state_gain<1 || r.state_gain>r.city_gain ||
       r.remote_gain<1 || r.remote_gain>r.state_gain || r.remote_ceiling<1 || r.remote_ceiling>r.thresholds[2] || r.remote_breakthrough_bp<0 || r.remote_breakthrough_bp>100 ||
       r.referral_days<30 || r.referral_days>365)invalid("Geographic social rates, online limits or referral intervals are invalid.");
    if(r.mission_conflict)validate_mission_conflict_rules(*r.mission_conflict);
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
    SocialState state;SplitMix64 rng{seed^UINT64_C(0x534f4349414c5631)};state.stream=rng.next();state.industry_stream=rng.next();state.next_industry_meeting=advance_days(s.date,Days{social_rules(s)->interval_days});state.next_meeting=advance_days(s.date,Days{social_rules(s)->interval_days});
    if(social_rules(s)->mission_conflict)state.mission_conflict=MissionConflictState{s.management->people->mission,s.date,{}};
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
        if(is_personal_rival(s,bond.first,bond.second) || external(s,bond.first) || external(s,bond.second) || bond.last_contact==s.date)continue;
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
std::string social_distance(const SimulationState& s,std::string_view a,std::string_view b) {
    if(former(s,a) || former(s,b))return "remote";
    const auto left=location(s,a),right=location(s,b);
    return left==right?"city":left.first==right.first?"state":"remote";
}
void prepare_industry_social(SimulationState& s) {
    if(!social_rules(s))return;auto& state=*s.management->people->social;const auto& r=*social_rules(s);
    if(s.date<state.next_industry_meeting)return;
    SplitMix64 rng{state.industry_stream};std::vector<std::string> roster;
    if(present(s,"founder"))roster.push_back("founder");
    for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher" && present(s,e.terms.profile.id))roster.push_back(e.terms.profile.id);
    for(const auto& [id,other]:s.management->people->industry)if(other.active)roster.push_back(id);
    for(std::size_t n=roster.size();n>1;--n)std::swap(roster[n-1],roster[static_cast<std::size_t>(rng.bounded(n))]);
    const auto contact=[&](const std::string& a,const std::string& b,bool established,SocialLink* existing=nullptr) {
        const auto distance=social_distance(s,a,b);
        const auto chance=distance=="city"?r.city_chance_bp:distance=="state"?r.state_chance_bp:r.remote_chance_bp;
        if(rng.bounded(10000)>=static_cast<std::uint64_t>(chance))return;
        const auto* old=existing?existing:social_link(s,a,b);if(!old && state.links.size()>=8192)return;
        const bool remote=distance=="remote";int ceiling=1000;
        if(remote)ceiling=(old && old->affinity>=r.thresholds[2]) || rng.bounded(10000)<static_cast<std::uint64_t>(r.remote_breakthrough_bp)?1000:r.remote_ceiling;
        const auto gain=distance=="city"?r.city_gain:distance=="state"?r.state_gain:r.remote_gain;
        const auto memory=remote?"Caught up online across state lines":distance=="state"?"Connected through the regional lab community":established?"Caught up with a friend in town":"Met through the local lab community";
        if(existing)advance_bond(s,*existing,gain,memory,ceiling,!remote);
        else encounter(s,a,b,gain,memory,ceiling,!remote);
    };
    for(std::size_t i=1;i<roster.size();i+=2)if(employer(s,roster[i-1])!=employer(s,roster[i]) && (external(s,roster[i-1]) || external(s,roster[i])))contact(roster[i-1],roster[i],false);
    // Existing edges get one opportunity each; no all-pairs daily scan.
    const auto count=state.links.size();
    for(std::size_t i=0;i<count;++i) {
        auto& bond=state.links[i];if((!external(s,bond.first) && !external(s,bond.second)) || employer(s,bond.first)==employer(s,bond.second) || bond.last_contact==s.date)continue;
        if(available(s,bond.first) && available(s,bond.second) && bond.affinity>=r.thresholds[1])contact(bond.first,bond.second,true,&bond);
        if(s.date.value()-bond.last_contact.value()>=28)bond.affinity=std::max(0,bond.affinity-r.remote_decay);
    }
    std::set<std::string> friends;
    for(const auto& bond:state.links)if(bond.affinity>=r.thresholds[2]) {
        if(bond.first=="founder")friends.insert(bond.second);if(bond.second=="founder")friends.insert(bond.first);
    }
    for(auto& bond:state.links)if(!bond.known && bond.affinity>=r.thresholds[3] && friends.contains(bond.first) && friends.contains(bond.second))bond.known=true;
    state.industry_stream=rng.state();state.next_industry_meeting=advance_days(s.date,Days{r.interval_days});
}
std::string social_introduction(const SimulationState& s,std::string_view friend_id) {
    if(!social_rules(s))return {};
    const auto* friendship=social_link(s,"founder",friend_id);if(!friendship || friendship->affinity<social_rules(s)->thresholds[3])return {};
    for(const auto& bond:s.management->people->social->links)if(bond.affinity>=social_rules(s)->thresholds[3] && (bond.first==friend_id || bond.second==friend_id)) {
        const auto& other=bond.first==friend_id?bond.second:bond.first;
        if(other!="founder" && available(s,other) && !social_link(s,"founder",other))return other;
    }
    return {};
}
std::string social_referral_blocker(const SimulationState& s,std::string_view id) {
    if(!social_rules(s))return "The social circle is unavailable.";
    const auto* bond=social_link(s,"founder",id);
    if(!bond || bond->affinity<social_rules(s)->thresholds[2] || !available(s,id))return "Build a friendship before asking for a recruiting referral.";
    const auto& referrals=s.management->people->social->referrals;
    if(const auto last=referrals.find(std::string(id));last!=referrals.end() && s.date<advance_days(last->second,Days{social_rules(s)->referral_days}))return "Give this friend time to find another referral.";
    const auto& applicants=s.management->people->applicants;
    const auto current=std::ranges::count_if(applicants,[&](const auto& a){const auto status=recruitment_status(a,s.date);return status=="available" || status=="fixed" || status=="waiting" || status=="counter" || status=="agreed";});
    if(static_cast<std::size_t>(current)>=s.management->catalog.people->candidates && s.management->people->queued_applicants.size()>=12)return "The recommended-candidate waiting list is full. Review current applications before asking for another referral.";
    return {};
}
std::string social_blocker(const SimulationState& s,std::string_view person) {
    if(!social_rules(s))return "The social circle is unavailable.";
    const auto id=social_id(s,person);
    if(is_personal_rival(s,"founder",id))return "They are keeping their distance.";
    const auto& state=*s.management->people->social;
    if(id=="founder" || !available(s,id))return "Choose an available coworker or industry contact.";
    if(external(s,id) && social_distance(s,"founder",id)=="remote" && !social_link(s,"founder",id))return "An introduction helps bridge the distance; meet through a close friend first.";
    const auto location=executive_location(s);
    if(location.traveling || location.appointment)return "Finish your trip or appointment before making plans at HQ.";
    if(state.last_outing && *state.last_outing==s.date)return "You have already made time for someone today.";
    const auto* bond=social_link(s,"founder",id);
    if(bond && bond->last_invited && s.date<advance_days(*bond->last_invited,Days{social_rules(s)->interval_days}))return "Give this person a little space; make plans again next week.";
    return {};
}
void socialize(SimulationState& s,const Socialize& action) {
    const auto reason=social_blocker(s,action.person);if(!reason.empty())throw std::invalid_argument(reason);
    if(action.activity!="chat" && action.activity!="listen" && action.activity!="hobby" && action.activity!="introduce" && action.activity!="referral")invalid("Choose a conversation, listening or shared interests.");
    const auto id=social_id(s,action.person);const auto& r=*social_rules(s);
    if(action.activity=="introduce") {
        const auto other=social_introduction(s,id);if(other.empty())invalid("This close friend has no new introduction to make right now.");
        encounter(s,"founder",other,r.weekly_gain,"Introduced by a trusted close friend");
        link(s,"founder",id).last_invited=s.date;s.management->people->social->last_outing=s.date;return;
    }
    if(action.activity=="referral") {
        const auto blocker=social_referral_blocker(s,id);if(!blocker.empty())throw std::invalid_argument(blocker);
        add_referred_applicant(s,id);s.management->people->social->referrals.insert_or_assign(id,s.date);
        link(s,"founder",id).last_invited=s.date;s.management->people->social->last_outing=s.date;return;
    }
    int gain=r.interaction_gain;std::string memory="Made time for a relaxed conversation";
    if(action.activity=="listen") {if(person_has_skill(s,"founder","empathy"))gain+=r.shared_gain;memory="Listened and made room for a different point of view";}
    if(action.activity=="hobby") {
        bool shared=false;for(const auto& t:traits(s,"founder"))if(t.starts_with("Interest: ") && std::ranges::find(traits(s,id),t)!=traits(s,id).end())shared=true;
        if(shared)gain+=r.weekly_gain;memory=shared?"Spent time on a shared interest":"Tried something new together";
    }
    int ceiling=1000;bool fit_enabled=true;
    if(external(s,id) || former(s,id)) {
        const auto distance=social_distance(s,"founder",id);
        if(distance=="state")gain=std::max(1,gain*r.state_gain/r.city_gain);
        if(distance=="remote") {
            gain=r.remote_gain;
            fit_enabled=false;
            const auto* old=social_link(s,"founder",id);
            const auto* concern=mission_concern(s,id);
            const bool rebuilding=concern && concern->rivalry_on && concern->settled_on;
            if(rebuilding)gain=std::max(gain,r.interaction_gain/2);
            ceiling=rebuilding || (old && old->affinity>=r.thresholds[2])?1000:r.remote_ceiling;
            memory="Made time to catch up online";
        }
    }
    encounter(s,"founder",id,gain,memory,ceiling,fit_enabled);link(s,"founder",id).last_invited=s.date;s.management->people->social->last_outing=s.date;
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
std::vector<std::string> visible_traits(const SimulationState& s,const PersonProfile& p,std::string_view identity) {
    if(!social_rules(s))return p.traits;const auto id=social_id(s,identity.empty()?std::string_view{p.id}:identity);if(id=="founder")return p.traits;
    if(is_personal_rival(s,"founder",id))return {};
    const auto& known=s.management->people->social->known_traits;const auto it=known.find(id);std::vector<std::string> result;
    for(const auto& t:p.traits)if(public_trait(t) || (it!=known.end() && std::ranges::find(it->second,t)!=it->second.end()))result.push_back(t);return result;
}
int friendship_mood(const SimulationState& s,std::string_view person) {
    if(!social_rules(s))return 0;int points=0;const auto id=social_id(s,person);
    for(const auto& bond:s.management->people->social->links)if(!is_personal_rival(s,bond.first,bond.second) && bond.affinity>=social_rules(s)->thresholds[2] && (bond.first==id || bond.second==id)) {
        const auto& other=bond.first==id?bond.second:bond.first;if(present(s,other))points+=bond.affinity>=social_rules(s)->thresholds[3]?2:1;
    }
    return std::min(points,social_rules(s)->mood_cap);
}
void validate_social(const SimulationState& s) {
    if(!s.management || !s.management->people)return;const auto* r=social_rules(s);const auto& state=s.management->people->social;
    if(static_cast<bool>(r)!=state.has_value())invalid("Social state and rules disagree.");if(!r)return;const auto& p=*state;
    if(p.next_meeting<s.management->people->starts_on || p.next_meeting>advance_days(s.date,Days{r->interval_days}) ||
       (p.last_outing && *p.last_outing>s.date) || p.links.size()>8192 || p.known_traits.size()>8192 || p.founder_traits.size()>person_trait_limit)invalid("Social dates or size exceed their bounds.");
    if(p.next_industry_meeting<s.management->people->starts_on || p.next_industry_meeting>advance_days(s.date,Days{r->interval_days}) || p.referrals.size()>8192)invalid("Industry contact dates or referrals are invalid.");
    for(const auto& [id,day]:p.referrals)if(!exists(s,id) || day>s.date || day<s.management->people->starts_on)invalid("Invalid saved recruiting referral.");
    std::set<std::string> founder_traits;for(const auto& t:p.founder_traits)if(t.empty() || t.size()>80 || !founder_traits.insert(t).second)invalid("Invalid founder trait.");
    std::set<std::string> hostile;if(p.hostile_peers.size()>64)invalid("Too many hostile peer entries.");
    for(const auto& id:p.hostile_peers)if(id.empty() || id=="founder" || id.size()>160 || !hostile.insert(id).second)invalid("Invalid hostile peer.");
    std::set<std::pair<std::string,std::string>> pairs;
    for(const auto& bond:p.links) {
        if(bond.first>=bond.second || !exists(s,bond.first) || !exists(s,bond.second) || social_id(s,bond.first)!=bond.first || social_id(s,bond.second)!=bond.second ||
           !pairs.emplace(bond.first,bond.second).second || bond.affinity<0 || bond.affinity>1000 || bond.encounters<0 || bond.encounters>100000 ||
           bond.met_on<s.management->people->starts_on || bond.met_on>bond.last_contact || bond.last_contact>s.date ||
           (bond.last_invited && (*bond.last_invited<bond.met_on || *bond.last_invited>bond.last_contact)) || bond.memory.empty() || bond.memory.size()>160)
            invalid("A relationship has invalid participants, progression or dates.");
    }
    validate_mission_conflicts(s);
    for(const auto& [id,known]:p.known_traits) {
        if(id=="founder" || !exists(s,id) || known.size()>person_trait_limit)invalid("Unknown trait disclosure recipient.");
        const auto& all=traits(s,id);std::set<std::string> unique;for(const auto& t:known)if(std::ranges::find(all,t)==all.end() || !unique.insert(t).second)invalid("Disclosed traits must belong to this person.");
    }
}
bool founder_has_trait(const SimulationState& state, std::string_view target) noexcept {
    if(!state.management || !state.management->people) return false;
    const auto& people=*state.management->people;
    if(people.social && std::ranges::any_of(people.social->founder_traits,[target](const auto& t){return trait_matches(t,target);}))
        return true;
    for(const auto& employee:people.employees)
        if(employee.terms.profile.role=="Founder / researcher" && person_has_trait(employee.terms.profile,target))
            return true;
    return false;
}
BasisPoints founder_risk_scale_bp(const SimulationState& state) noexcept {
    return founder_has_trait(state,"calculated risk taker")?BasisPoints{9000}:BasisPoints{10000};
}
}
