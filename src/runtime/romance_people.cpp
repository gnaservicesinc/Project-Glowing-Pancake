#include "romance_internal.hpp"
#include <algorithm>
namespace pgp::runtime {
using namespace core;
const RomanceRules* romance_rules(const WorldState& w) {
    const auto& s=w.simulation;return w.founding && s.management && s.management->catalog.people && s.management->catalog.people->romance?&*s.management->catalog.people->romance:nullptr;
}
std::string romance_person_id(const WorldState& w,std::string_view id) {
    if(id=="founder")return "founder";
    if(!id.starts_with("employee/"))return family_person_id(w,id);
    if(w.simulation.management && w.simulation.management->people)for(const auto& e:w.simulation.management->people->employees)if(e.terms.profile.id==id)
        return e.terms.profile.role=="Founder / researcher"?"founder":family_person_id(w,w.company_id+"/"+std::string{id});
    return family_person_id(w,id);
}
const RomanceBond* romantic_partner(const WorldState& w,std::string_view person) {
    if(!w.romance){return nullptr;}
    const auto id=romance_person_id(w,person);
    for(const auto& [key,bond]:w.romance->active){(void)key;if(bond.first==id || bond.second==id)return &bond;}return nullptr;
}
const RomanceBond* latest_romance(const WorldState& w,std::string_view first,std::string_view second) {
    if(!w.romance){return nullptr;}
    const auto a=romance_person_id(w,first),b=romance_person_id(w,second);
    const auto match=[&](const auto& bond){return (bond.first==a && bond.second==b) || (bond.first==b && bond.second==a);};
    for(const auto& [key,bond]:w.romance->active){(void)key;if(match(bond))return &bond;}
    for(auto i=w.romance->history.size();i>0;--i)if(match(w.romance->history[i-1]))return &w.romance->history[i-1];return nullptr;
}
namespace romance_detail {
People roster(const WorldState& w) {
    People people;
    if(w.romance && w.founding)people.emplace("founder",Person{"founder",w.founding->gender,w.founding->orientation,{},{},&w.romance->founder,true});
    const auto add=[&](const SimulationState& s,const std::string& owner) {
        if(!s.management || !s.management->people)return;
        std::string state,city;
        if(s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->headquarters) {
            const auto& hq=current_headquarters(s);for(const auto& market:s.infrastructure->catalog.workplace->headquarters->markets)if(market.id==hq.market){state=market.state;city=market.city;break;}
        }
        if(owner==w.company_id && people.contains("founder")){people.at("founder").state=state;people.at("founder").city=city;}
        for(const auto& e:s.management->people->employees) {
            if(e.terms.profile.role=="Founder / researcher"){continue;}
            const auto& p=e.terms.profile;if(!p.romantic)continue;
            const bool employed=!e.departed_on && e.ready_on<=s.date && s.phase!=DayPhase::closed && s.phase!=DayPhase::finished;
            const auto id=family_person_id(w,owner+"/"+p.id);people.insert_or_assign(id,Person{id,p.gender,p.orientation,employed?state:std::string{},employed?city:std::string{},&*p.romantic,employed});
        }
    };
    add(w.simulation,w.company_id);if(w.competition)for(const auto& company:w.competition->companies)add(company.simulation,company.definition_id);
    if(w.family)for(const auto& member:w.family->members)if(!people.contains(member.profile.id))people.emplace(member.profile.id,Person{member.profile.id,member.profile.gender,member.profile.orientation,{},{},&*member.profile.romantic,false});
    return people;
}
std::string social_key(const WorldState& w,std::string_view input) {auto id=std::string{input};if(w.family)for(const auto& [alias,member]:w.family->aliases)if(member==id){id=alias;break;}const auto prefix=w.company_id+"/";return id.starts_with(prefix)?std::string{id.substr(prefix.size())}:std::string{id};}
int friendship(const WorldState& w,std::string_view a,std::string_view b) {
    const auto* bond=social_link(w.simulation,social_key(w,a),social_key(w,b));return bond?bond->affinity:0;
}
std::string distance(const Person& a,const Person& b) {
    if(a.state.empty() || b.state.empty())return "remote";
    return a.state==b.state && a.city==b.city?"city":a.state==b.state?"state":"remote";
}
std::string other(const RomanceBond& b,std::string_view id){return b.first==id?b.second:b.first;}
}
bool romantic_identity_known(const WorldState& w,std::string_view person) {
    if(!w.romance){return false;}
    const auto id=romance_person_id(w,person);if(id=="founder")return true;
    if(w.romance->known_identities.contains(id))return true;
    const auto& people=*w.simulation.management->people;const PersonProfile* profile=nullptr;
    for(const auto& e:people.employees)if(w.company_id+"/"+e.terms.profile.id==id)profile=&e.terms.profile;
    if(const auto found=people.industry.find(id);found!=people.industry.end())profile=&found->second.profile;
    if(!profile)if(const auto* member=family_member(w,id))profile=&member->profile;
    if(!profile || !profile->romantic)return false;
    const auto* social=social_rules(w.simulation);
    return profile->romantic->preferences.public_orientation || (social && romance_detail::friendship(w,"founder",id)>=social->private_threshold);
}
}
