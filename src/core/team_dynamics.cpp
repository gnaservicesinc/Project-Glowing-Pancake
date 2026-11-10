#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace pgp::core {
TeamDynamics team_dynamics(std::span<const PersonProfile* const> people) {
    if(people.size()>256) throw std::invalid_argument("Team exceeds the individual roster limit.");
    TeamDynamics result;
    std::map<std::string,std::uint64_t> frequency;
    std::set<std::string> identities;
    std::uint64_t slots=0;
    for(const auto* p:people) {
        if(!p || !identities.insert(p->id).second) throw std::invalid_argument("A team must contain distinct people.");
        // Each trait counts once per person; identity/education/mission contribute
        // perspectives without changing a person's underlying ability scores.
        std::set<std::string> traits(p->traits.begin(),p->traits.end());
        traits.insert("Gender: "+p->gender);traits.insert("Education: "+p->education);traits.insert("Motivation: "+p->preferred_mission);
        for(const auto& trait:traits) ++frequency[trait];
        slots+=traits.size();
    }
    std::uint64_t shared=0;
    for(const auto& [trait,count]:frequency) {result.traits.push_back(trait);shared+=count*(count-1);}
    result.creativity=static_cast<std::uint32_t>(frequency.size());
    // Weighted pairwise overlap: identical profiles reach 100%; differences add
    // no shared-trait gain. Frequency aggregation avoids a pairwise daily loop.
    if(people.size()>1 && slots) result.compatibility_bp=static_cast<std::uint32_t>(shared*10000/((people.size()-1)*slots));
    return result;
}
TeamDynamics research_team(const SimulationState& state,std::span<const std::string> ids) {
    if(!state.management || !state.management->people) return {};
    std::vector<const PersonProfile*> people;
    for(const auto& id:ids) {
        const auto& employees=state.management->people->employees;
        const auto found=std::ranges::find_if(employees,[&](const auto& employee){return employee.terms.profile.id==id;});
        if(found==employees.end()) throw std::invalid_argument("Research team member is missing.");
        people.push_back(&found->terms.profile);
    }
    return team_dynamics(people);
}
}
