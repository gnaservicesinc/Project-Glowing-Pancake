#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace pgp::core {
std::vector<std::string> distinct_person_traits(const PersonProfile& person) {
    std::set<std::string> traits(person.traits.begin(),person.traits.end());
    traits.insert("Gender: "+person.gender);
    traits.insert("Education: "+person.education);
    traits.insert("Motivation: "+person.preferred_mission);
    return {traits.begin(),traits.end()};
}
std::uint32_t shared_trait_compatibility(std::uint64_t matches,std::uint64_t pairs) {
    // Five matches per pair saturate the gain. Unshared traits never enter
    // the denominator, so gaining a new perspective cannot erase affinity.
    constexpr std::uint64_t matches_for_full_gain=5;
    if(!pairs) return 0;
    const auto target=pairs*matches_for_full_gain;
    return static_cast<std::uint32_t>(std::min(matches,target)*10000/target);
}
TeamDynamics team_dynamics(std::span<const PersonProfile* const> people) {
    if(people.size()>256) throw std::invalid_argument("Team exceeds the individual roster limit.");
    TeamDynamics result;
    std::map<std::string,std::uint64_t> frequency;
    std::set<std::string> identities;
    for(const auto* p:people) {
        if(!p || !identities.insert(p->id).second) throw std::invalid_argument("A team must contain distinct people.");
        // Each trait counts once per person; identity/education/mission contribute
        // perspectives without changing a person's underlying ability scores.
        for(const auto& trait:distinct_person_traits(*p)) ++frequency[trait];
    }
    std::uint64_t shared=0;
    for(const auto& [trait,count]:frequency) {result.traits.push_back(trait);shared+=count*(count-1);}
    result.creativity=static_cast<std::uint32_t>(frequency.size());
    if(people.size()>1) result.compatibility_bp=shared_trait_compatibility(shared,people.size()*(people.size()-1));
    return result;
}
TeamDynamics research_team(const SimulationState& state,std::span<const std::string> ids,
                           const std::map<std::string,std::vector<std::string>,std::less<>>& original_traits) {
    if(!state.management || !state.management->people) return {};
    std::vector<const PersonProfile*> people;
    std::vector<PersonProfile> recorded;
    if(!original_traits.empty()) recorded.reserve(ids.size());
    for(const auto& id:ids) {
        const auto& employees=state.management->people->employees;
        const auto found=std::ranges::find_if(employees,[&](const auto& employee){return employee.terms.profile.id==id;});
        if(found==employees.end()) throw std::invalid_argument("Research team member is missing.");
        if(const auto prior=original_traits.find(id);prior!=original_traits.end()) {
            recorded.push_back(found->terms.profile);
            recorded.back().traits=prior->second;
            people.push_back(&recorded.back());
        } else people.push_back(&found->terms.profile);
    }
    return team_dynamics(people);
}
}
