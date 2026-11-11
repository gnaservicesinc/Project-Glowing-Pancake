#include "family_internal.hpp"
#include <algorithm>
#include <stdexcept>
namespace pgp::runtime::family_detail {
FamilyPlan& plan(WorldState& w,std::string first,std::string second,const FamilyRequest& request,bool known) {
    auto& state=*w.family;const auto& rules=*family_rules(w);if(state.next_plan>maximum_family_plans || state.pending.size()>=128)throw std::invalid_argument("The family planning register is full.");
    FamilyPlan p;p.id=state.next_plan++;p.first=std::move(first);p.second=std::move(second);p.kind=request.action;p.band=request.band;p.name=request.name;p.child=request.child;p.requested_on=campaign_day(w);p.known=known;
    p.due_on=advance_days(p.requested_on,Days{p.kind=="birth"?rules.birth_days:p.kind=="foster"?rules.foster_days:rules.adoption_days});remember_due(state,p.due_on);return state.pending.emplace(p.id,std::move(p)).first->second;
}
void resolve_plans(WorldState& w,SplitMix64& rng) {
    auto& state=*w.family;const auto& rules=*family_rules(w);const auto day=campaign_day(w);const auto people=romance_detail::roster(w);
    std::vector<std::uint32_t> resolved;
    for(auto& [id,p]:state.pending)if(day>=p.due_on) {
        bool okay=people.contains(p.first) && (p.second.empty() || people.contains(p.second));
        if(okay && !p.second.empty() && p.kind!="birth") {const auto* bond=romantic_partner(w,p.first);okay=bond && bond->married_on && romance_detail::other(*bond,p.first)==p.second;}
        if(okay && p.kind=="adopt_placement") {
            const auto* member=family_member(w,p.child);okay=member && std::ranges::any_of(state.edges,[&](const auto& edge){return edge.parent==p.first && edge.child==p.child && edge.kind=="foster" && !edge.ended_on;});
            if(okay) {
                for(std::size_t i=0;i<state.edges.size();++i){auto edge=state.edges[i];if(edge.child==p.child && edge.kind=="foster" && !edge.ended_on){edge.ended_on=day;state.edges.replace(i,std::move(edge));}}
                add_edge(w,p.first,p.child,"adoptive",day,p.known);if(!p.second.empty())add_edge(w,p.second,p.child,"adoptive",day,p.known);
            }
        } else if(okay) {
            if(state.members.size()>=maximum_family_people){okay=false;}
            else {
                OrdinalDay born=p.due_on;
                if(p.kind!="birth") {
                    const auto minimum=p.band=="teen"?13:p.band=="child"?4:0;const auto ceiling=p.band=="teen"?17:p.band=="child"?8:1;
                    auto earliest=to_ordinal(anniversary(people.at(p.first).romantic->born_on,rules.minimum_parent_gap));
                    if(!p.second.empty())earliest=std::max(earliest,to_ordinal(anniversary(people.at(p.second).romantic->born_on,rules.minimum_parent_gap)));
                    const auto maximum=std::min(ceiling,family_age(earliest,p.due_on));
                    if(maximum<minimum)okay=false;
                    else {const int age=minimum+static_cast<int>(rng.bounded(static_cast<std::uint64_t>(maximum-minimum+1)));born=to_ordinal(anniversary(p.due_on,-age));born=OrdinalDay{born.value()-static_cast<std::int64_t>(rng.bounded(301))};born=std::max(born,earliest);}
                    if(okay && family_age(born,p.due_on)>=12)okay=rng.bounded(10000)<static_cast<std::uint64_t>(rules.older_child_consent_bp);
                }
                if(okay) {
                    std::string surname;
                    if(p.kind=="birth") {
                        std::string parent_name;if(p.first=="founder")parent_name=w.identity->founder_name;
                        else if(const auto* parent=family_member(w,p.first))parent_name=parent->profile.name;
                        else if(const auto found=w.simulation.management->people->industry.find(p.first);found!=w.simulation.management->people->industry.end())parent_name=found->second.profile.name;
                        else for(const auto& worker:w.simulation.management->people->employees)if(romance_person_id(w,worker.terms.profile.id)==p.first)parent_name=worker.terms.profile.name;
                        surname=parent_name.substr(parent_name.find_last_of(' ')==std::string::npos?0:parent_name.find_last_of(' ')+1);
                    }
                    auto member=make_member(w,static_cast<std::uint32_t>(state.members.size()+1),born,p.name,p.known,true,surname);p.child=member.profile.id;state.members.push_back(std::move(member));
                    const auto kind=p.kind=="birth"?"biological":p.kind=="adopt"?"adoptive":"foster";add_edge(w,p.first,p.child,kind,day,p.known);if(!p.second.empty())add_edge(w,p.second,p.child,kind,day,p.known);
                }
            }
        }
        p.status=okay?"completed":"cancelled";p.resolved_on=day;state.history.push_back(p);resolved.push_back(id);
    }
    for(const auto id:resolved)state.pending.erase(id);
    // Foster care has an agreed review date. A pending adoption review keeps
    // care continuous until its outcome, while preserving the foster history.
    for(std::size_t i=0;i<state.edges.size();++i) {
        auto edge=state.edges[i];if(edge.kind!="foster" || edge.ended_on)continue;
        const auto* member=family_member(w,edge.child);const auto end=std::min(advance_days(edge.started_on,Days{rules.foster_term_days}),to_ordinal(anniversary(member->profile.romantic->born_on,18)));
        const bool adoption=std::ranges::any_of(state.pending,[&](const auto& row){return row.second.kind=="adopt_placement" && row.second.child==edge.child;});
        if(day>=end && !adoption){edge.ended_on=day;state.edges.replace(i,std::move(edge));}
        else if(!adoption)remember_due(state,end);
    }
    // Family members can tell trusted contacts about their household. This is
    // knowledge, not a requirement to have a professional friendship with a child.
    std::set<std::string> shared{"founder"};for(const auto& member:state.members)if(member.known)shared.insert(member.profile.id);
    if(w.simulation.management->people->social)for(const auto& edge:w.simulation.management->people->social->links)if(edge.affinity>=romance_rules(w)->friendship && (edge.first=="founder" || edge.second=="founder"))shared.insert(romance_person_id(w,edge.first=="founder"?edge.second:edge.first));
    for(std::size_t i=0;i<state.edges.size();++i) {
        auto edge=state.edges[i];if(edge.known || !shared.contains(edge.parent))continue;edge.known=true;state.edges.replace(i,edge);
        if(const auto* child=family_member(w,edge.child);child && !child->known){auto disclosed=*child;disclosed.known=true;state.members.replace(static_cast<std::size_t>(std::stoul(disclosed.profile.id.substr(14))-1),std::move(disclosed));}
    }
}
}
