#include "romance_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::runtime {
void validate_romance(const WorldState& w,bool deep) {
    using namespace core;
    const auto invalid=[](){throw std::invalid_argument("Romance history has invalid identities, adulthood, partners, consent replies or progression dates.");};
    const auto* rules=romance_rules(w);if(static_cast<bool>(rules)!=w.romance.has_value())invalid();if(!rules)return;
    if(!w.founding)invalid();const auto& state=*w.romance;const auto day=campaign_day(w);const auto people=romance_detail::roster(w);
    validate_romantic_profile(state.founder,1997);
    for(const auto& e:w.simulation.management->people->employees)if(e.terms.profile.role=="Founder / researcher" && e.terms.profile.romantic!=std::optional{state.founder})invalid();
    if(state.founder.born_on!=to_ordinal({1997,1,1}) || state.founder.preferences.minimum_friendship!=(w.founding->orientation=="demisexual"?rules->demisexual_friendship:rules->friendship) ||
       state.next_review<w.campaign_start || state.next_review>advance_days(day,Days{rules->interval_days}) || (state.last_action && (*state.last_action<w.campaign_start || *state.last_action>day)) ||
       state.active.size()>4096 || state.history.size()>romance_history_limit || state.replies.size()>romance_history_limit || state.next_id!=state.active.size()+state.history.size()+1 || state.next_id>romance_history_limit+1)invalid();
    if(state.known_identities.size()>8192)invalid();
    for(const auto& id:state.known_identities)if(id=="founder" || !people.contains(id))invalid();
    const auto ancestors=deep?family_ancestors(w):std::map<std::string,std::set<std::string>>{};
    std::set<std::uint32_t> ids;std::set<std::string> partners;
    std::map<std::string,std::vector<std::pair<OrdinalDay,OrdinalDay>>> intervals;
    std::map<std::uint32_t,const RomanceBond*> bonds;
    const auto check=[&](const RomanceBond& b,bool active) {
        const auto a=people.find(b.first),other=people.find(b.second);
        if(b.id==0 || b.id>=state.next_id || !ids.insert(b.id).second || b.first>=b.second || a==people.end() || other==people.end() ||
           b.started_on<w.campaign_start || b.started_on>day || !adult_on(*a->second.romantic,b.started_on) || !adult_on(*other->second.romantic,b.started_on) ||
           b.last_time_together<b.started_on || b.last_time_together>day || b.affection<0 || b.affection>1000 || b.dates<0 || b.dates>10000 ||
           b.memory.empty() || b.memory.size()>256 || b.end_reason.size()>256 || b.ended_on.has_value()==active ||
           ((b.first=="founder" || b.second=="founder") && !b.known))invalid();
        if(deep) {
            const auto first=ancestors.find(b.first),second=ancestors.find(b.second);
            if((first!=ancestors.end() && first->second.contains(b.second)) || (second!=ancestors.end() && second->second.contains(b.first)))invalid();
            if(first!=ancestors.end() && second!=ancestors.end())for(const auto& parent:first->second)if(second->second.contains(parent))invalid();
        }
        const auto end=b.ended_on.value_or(day);
        if(end<b.started_on || end>day || b.last_time_together>end || (b.strained_on && (*b.strained_on<b.started_on || *b.strained_on>end)) ||
           (b.last_proposal && (*b.last_proposal<b.started_on || *b.last_proposal>end)) || (b.ended_on && b.end_reason.empty()) || (!b.ended_on && !b.end_reason.empty()))invalid();
        if(b.engaged_on && (*b.engaged_on<advance_days(b.started_on,Days{rules->dating_days}) || *b.engaged_on>end || b.dates<rules->minimum_dates))invalid();
        if(b.married_on && (!b.engaged_on || *b.married_on<advance_days(*b.engaged_on,Days{rules->engagement_days}) || *b.married_on>end))invalid();
        if(active && (!partners.insert(b.first).second || !partners.insert(b.second).second))invalid();
        bonds.emplace(b.id,&b);intervals[b.first].push_back({b.started_on,b.ended_on.value_or(advance_days(day,Days{1}))});intervals[b.second].push_back({b.started_on,b.ended_on.value_or(advance_days(day,Days{1}))});
    };
    for(const auto& [id,bond]:state.active){if(id!=bond.id)invalid();check(bond,true);}
    if(deep){for(const auto& bond:state.history)check(bond,false);}
    else if(!state.history.empty())check(state.history.back(),false);
    if(deep)for(auto& [person,rows]:intervals){(void)person;std::sort(rows.begin(),rows.end());for(std::size_t i=1;i<rows.size();++i)if(rows[i].first<rows[i-1].second)invalid();}
    std::optional<OrdinalDay> last;
    for(std::size_t index=deep?0:state.replies.size();index<state.replies.size();++index) {
        const auto& reply=state.replies[index];
        if(!people.contains(reply.person) || reply.person=="founder" || reply.day<w.campaign_start || reply.day>day || (last && reply.day<*last) || reply.response.empty() || reply.response.size()>256)invalid();
        last=reply.day;
        if(reply.action!="ask" && reply.action!="walk" && reply.action!="listen" && reply.action!="hobby" && reply.action!="engage" && reply.action!="marry" && reply.action!="separate")invalid();
        if(reply.relationship==0){if(reply.accepted || reply.action!="ask")invalid();continue;}
        const auto found=bonds.find(reply.relationship);if(found==bonds.end())invalid();const auto& b=*found->second;
        if(romance_detail::other(b,"founder")!=reply.person || (b.first!="founder" && b.second!="founder") || reply.day<b.started_on || reply.day>b.ended_on.value_or(day))invalid();
        if(reply.accepted && ((reply.action=="ask" && reply.day!=b.started_on) || (reply.action=="engage" && (!b.engaged_on || *b.engaged_on!=reply.day)) ||
            (reply.action=="marry" && (!b.married_on || *b.married_on!=reply.day)) || (reply.action=="separate" && (!b.ended_on || *b.ended_on!=reply.day))))invalid();
    }
}
}
