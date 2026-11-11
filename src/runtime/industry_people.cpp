#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/runtime.hpp>
#include <algorithm>
#include <unordered_map>
namespace pgp::runtime {
namespace {
template<class Visit>void visit_people(const WorldState& world,const CompiledProgram& program,Visit visit) {
    if(!world.competition)return;
    for(const auto& rival:world.competition->companies) {
        const auto& s=rival.simulation;if(!s.management || !s.management->people || !s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->headquarters)continue;
        const auto& name=rival_definition(program,rival.definition_id).name;
        for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher") {
            const auto* location=&core::current_headquarters(s);
            if(e.departed_on)for(const auto& premises:s.infrastructure->workplace->headquarters->premises)
                if(premises.entered_on<=*e.departed_on && (!premises.left_on || *e.departed_on<*premises.left_on))location=&premises;
            for(const auto& market:s.infrastructure->catalog.workplace->headquarters->markets)if(market.id==location->market) {
                const auto id=rival.definition_id+"/"+e.terms.profile.id;
                const bool employed=!e.departed_on && s.phase!=core::DayPhase::closed && s.phase!=core::DayPhase::finished;
                visit(id,core::IndustryPerson{e.terms.profile,rival.definition_id,employed?name:"Former "+name,market.state,market.city,e.ready_on<=s.date});
            }
        }
    }
}
}
void synchronize_industry_people(WorldState& world,const CompiledProgram& program) {
    auto& s=world.simulation;if(!core::social_rules(s) || s.phase==core::DayPhase::closed || s.phase==core::DayPhase::finished || observing_world(world))return;
    auto& peers=s.management->people->industry;
    visit_people(world,program,[&](const auto& id,auto person){
        const auto found=peers.find(id);if(found==peers.end()){if(peers.size()>=8192)throw std::invalid_argument("The industry person register is full.");peers.emplace(id,std::move(person));}
        else if(found->second!=person)found->second=std::move(person);
    });
    auto& network=*s.management->people->social;
    if(s.date<network.next_industry_meeting)return;
    std::unordered_map<std::string,std::size_t> edges;edges.reserve(network.links.size());
    for(std::size_t i=0;i<network.links.size();++i)edges.emplace(network.links[i].first+"\n"+network.links[i].second,i);
    if(world.competition)for(const auto& rival:world.competition->companies)if(rival.simulation.management && rival.simulation.management->people && rival.simulation.management->people->social)
        for(const auto& source:rival.simulation.management->people->social->links) {
            const auto a=rival.definition_id+"/"+source.first,b=rival.definition_id+"/"+source.second;
            if(!peers.contains(a) || !peers.contains(b))continue;
            auto bond=source;bond.first=a;bond.second=b;bond.known=false;
            const auto key=a+"\n"+b;const auto found=edges.find(key);
            if(found!=edges.end()){auto& existing=network.links[found->second];bond.known=existing.known;if(existing!=bond)existing=std::move(bond);}
            else if(network.links.size()<8192){edges.emplace(key,network.links.size());network.links.push_back(std::move(bond));}
        }
    core::prepare_industry_social(s);
}
void validate_industry_people(const WorldState& world,const CompiledProgram& program) {
    const auto& s=world.simulation;if(!core::social_rules(s) || s.phase==core::DayPhase::closed || s.phase==core::DayPhase::finished || observing_world(world))return;
    const auto& peers=s.management->people->industry;std::size_t expected=0;
    visit_people(world,program,[&](const auto& id,const auto& person){
        ++expected;const auto found=peers.find(id);
        if(found==peers.end() || found->second!=person)throw std::invalid_argument("An industry contact differs from their actual employer, identity, skills or HQ.");
    });
    if(peers.size()!=expected)throw std::invalid_argument("An industry contact has no actual source person.");
}
}
