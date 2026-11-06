#include "objectives_internal.hpp"

#include <algorithm>
#include <stdexcept>

namespace pgp::runtime::objective_detail {
using namespace core;
namespace {
template<class T> const T& named(const std::vector<T>& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if(it==rows.end()) throw std::invalid_argument("Objective delivery lost its source customer or contract.");
    return *it;
}
template<class T> const T& monthly_source(const std::vector<T>& rows,OrdinalDay day) {
    const auto it=std::lower_bound(rows.begin(),rows.end(),day,[](const auto& row,OrdinalDay date){return row.month_end<date;});
    if(it==rows.end() || it->month_end!=day) throw std::invalid_argument("Objective check has no matching settled world month.");
    return *it;
}
}
const ObjectiveCharter& charter(const WorldState& world,const CompiledProgram& program) {
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if(!scenario || !scenario->objectives) throw std::invalid_argument("This scenario does not authorize campaign objectives.");
    return *scenario->objectives;
}
std::vector<const SimulationState*> institutions(const WorldState& world) {
    std::vector<const SimulationState*> result{&world.simulation};
    if(world.competition) for(const auto& rival:world.competition->companies)
        if(rival.simulation.accounts) result.push_back(&rival.simulation);
    return result;
}
bool controlled_on(const WorldState& world,std::string_view institution,OrdinalDay day) {
    if(institution==world.company_id) return true;
    if(world.competition) for(const auto& deal:world.competition->acquisitions)
        if(deal.target==institution && deal.completed_on && *deal.completed_on<=day) return true;
    return false;
}
ObjectiveMonth month(const WorldState& world,OrdinalDay day) {
    const auto& objectives=*world.objectives;
    const auto& society=*world.society;
    const auto& outcomes=monthly_source(society.months,day);
    ObjectiveMonth result;result.month_end=day;
    std::uint64_t population=0;
    std::array<std::uint64_t,utopia_global_floors.size()> weighted{};
    for(std::size_t i=0;i<society_regions;++i) {
        const auto people=society.catalog.regions[i].population_weight;
        population+=people;
        auto& region=result.regional_welfare[i];
        region=welfare_indices(outcomes.components[i]);
        for(std::size_t j=0;j<region.size();++j) weighted[j]+=static_cast<std::uint64_t>(region[j])*people;
    }
    for(std::size_t j=0;j<weighted.size();++j)
        result.global_welfare[j]=static_cast<std::uint32_t>(weighted[j]/population);
    if(day<objective_window_start(objectives.deadline,ObjectiveKind::monopoly)) return result;

    const auto& authority=monthly_source(world.authority->months,day);
    result.population_coverage=authority.population_coverage;
    result.output_coverage=authority.output_coverage;
    const auto civil=from_ordinal(day);
    const auto first=to_ordinal({civil.year,civil.month,1});
    for(std::size_t i=0;i<society_regions;++i) result.delivery[i].total=outcomes.outcomes[i].delivered;
    // Settled market receipts belong to the product's publisher, including
    // customer-hosted licensed/open work. Wholesale hosting adds no second
    // task. A subsidiary's receipts transfer from its actual closing date.
    for(const auto* institution:institutions(world)) {
        if(!institution->market || !controlled_on(world,institution->accounts->owner,day)) continue;
        const auto& market=*institution->market;
        const auto add=[&](std::string_view segment_id,std::uint64_t tasks) {
            if(!tasks) return;
            const auto& segment=named(market.segments,segment_id);
            if(segment.region_id.empty()) return;
            auto& work=result.delivery[society_region_index(society.catalog,segment.region_id)];
            if(tasks>work.total-work.controlled) throw std::invalid_argument("Attributed monopoly tasks exceed settled world delivery.");
            work.controlled+=tasks;
        };
        auto it=std::lower_bound(market.days.begin(),market.days.end(),first,[](const auto& row,OrdinalDay date){return row.day<date;});
        for(;it!=market.days.end() && it->day<=day;++it) if(controlled_on(world,market.owner,it->day)) {
            for(const auto& row:it->discretionary) add(row.segment_id,row.tasks);
            for(const auto& row:it->contracts) if(row.delivered_tasks)
                add(named(market.contracts,row.contract_id).segment_id,row.delivered_tasks);
        }
    }
    return result;
}
std::array<std::uint64_t,society_regions> remaining_delivery(const WorldState& world,OrdinalDay settled) {
    const auto days=std::max<std::int64_t>(0,world.objectives->deadline.value()-settled.value()-1);
    std::array<std::uint64_t,society_regions> result{};
    for(const auto& segment:world.simulation.market->segments) if(!segment.region_id.empty()) {
        auto& region=result[society_region_index(world.society->catalog,segment.region_id)];
        // Use the actual demand system's ceiling, including its largest
        // allowed modifier. Signed promises are bounded by base demand.
        const auto maximum=mul_div_floor(static_cast<std::uint64_t>(days),maximum_daily_demand(segment),1).quotient;
        region=static_cast<std::uint64_t>(checked_add(static_cast<std::int64_t>(region),static_cast<std::int64_t>(maximum)));
    }
    return result;
}
std::optional<AsiRaceResult> race_through(const WorldState& world,OrdinalDay through) {
    std::optional<AsiRaceResult> result;
    for(const auto* institution:institutions(world)) if(institution->frontier) {
        const auto qualified=earliest_frontier_qualification(*institution->frontier,true);
        if(!qualified || *qualified>through) continue;
        if(!result || *qualified<result->day) result=AsiRaceResult{*qualified,{}};
        if(*qualified==result->day) result->winners.push_back(institution->accounts->owner);
    }
    if(result) std::sort(result->winners.begin(),result->winners.end());
    return result;
}
bool retains_control(const WorldState& world) {
    const auto& state=world.simulation;
    const auto& founder=state.succession->founder_holder;
    if(founder_role(*state.succession)==FounderRole::executive ||
        std::any_of(state.accounts->holdings.begin(),state.accounts->holdings.end(),
        [&](const auto& row){return row.holder==founder && row.shares.value()>5000;})) return true;
    const auto board=current_board(state);
    return std::any_of(board.begin(),board.end(),[](const auto& member){return member.player && member.seats>0;});
}
} // namespace pgp::runtime::objective_detail
