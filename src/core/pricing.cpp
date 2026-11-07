#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
template<class Rows> const auto* lookup(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    return it==rows.end()?nullptr:&*it;
}
std::string_view binding(const MarketState& market,const MarketProduct& product,OrdinalDay day) {
    std::string_view result=product.capacity_project_id;
    for (const auto& row:market.capacity_bindings)
        if (row.product_id==product.id && row.starts_on<=day) result=row.project_id;
    return result;
}
}
PricingReview pricing_review(const SimulationState& s,const DelegationPolicy& policy,OrdinalDay day,Cents before) {
    if (!s.market || !s.capacity || !policy.pricing) throw std::invalid_argument("Pricing review needs a market, capacity and explicit bounds");
    const auto& market=*s.market;const auto& capacity=*s.capacity;const auto& bounds=*policy.pricing;
    const auto* product=lookup(market.products,policy.target_id);
    if (!product) throw std::invalid_argument("Pricing product no longer exists");
    PricingReview result{before,"Hold: no settled operating days are available."};
    const auto first=std::max({market.starts_on.value(),capacity.starts_on.value(),checked_subtract(day.value(),7)});
    std::uint64_t delivered=0,slots=0,observations=0;bool constrained=false;
    const auto profile=binding(market,*product,day);
    for (auto date=first;date<day.value();++date) {
        const auto mi=static_cast<std::size_t>(date-market.starts_on.value());
        const auto ci=static_cast<std::size_t>(date-capacity.starts_on.value());
        if (mi>=market.days.size() || ci>=capacity.days.size()) continue;
        const auto& settled=market.days[mi];const auto& usage=capacity.days[ci];
        if (market_waitlist_limit(market,*product,settled.day))
            return {before,"Hold: recent settled work was subject to a customer waitlist."};
        if (binding(market,*product,OrdinalDay{date})!=profile) return {before,"Hold: the serving allocation changed within the seven-day observation window."};
        const auto* project=lookup(capacity.projects,profile);
        const auto work=std::find_if(usage.projects.begin(),usage.projects.end(),[&](const auto& row){return row.project_id==profile;});
        if (!project || work==usage.projects.end()) continue;
        ++observations;
        if (!project->customer_inference) {
            const auto* source=lookup(capacity.sources,project->source_id);
            const auto used=std::find_if(usage.sources.begin(),usage.sources.end(),[&](const auto& row){return row.source_id==project->source_id;});
            if (!source || used==usage.sources.end() || used->effective<source->nominal)
                return {before,"Hold: the recent serving source had a known capacity reduction."};
            const auto available=checked_unit_subtract(checked_unit_subtract(used->effective,used->wholesale_available),used->replica_available);
            constrained=constrained || used->used.value()>=available.value() || work->assigned_staff<project->staff;
        }
        const auto provisioned=project->customer_inference?project->tasks_per_day:
            std::min(project->tasks_per_day,maximum_tasks(MilliCU{project->requested.value()},project->inference_milli_CU_per_task));
        // Seven days of already bounded serving slots and deliveries fit uint64.
        slots+=provisioned>work->protected_tasks?provisioned-work->protected_tasks:0;
        for (const auto& delivery:settled.discretionary)
            if (delivery.product_id==product->id) delivered+=delivery.tasks;
    }
    if (!observations) return result;
    if (!slots) return {before,"Hold: protected contracts left no discretionary serving slots in the recent window."};
    const auto utilization=mul_div_floor(std::min(delivered,slots),10000,slots).quotient;
    const auto percent=std::to_string(utilization/100)+"."+(utilization%100<10?"0":"")+std::to_string(utilization%100)+"%";
    result.explanation="Observed "+std::to_string(delivered)+" / "+std::to_string(slots)+" discretionary slots across "+
        std::to_string(observations)+" settled days ("+percent+" utilization). ";
    if (utilization<=static_cast<std::uint64_t>(bounds.low_utilization.value()) && constrained)
        result.explanation+="Hold: serving capacity was fully occupied; low sales do not establish weak demand.";
    else if (utilization<=static_cast<std::uint64_t>(bounds.low_utilization.value())) {
        result.quote=Cents{std::max(bounds.floor.value(),checked_subtract(before.value(),bounds.step.value()))};
        result.explanation+=result.quote==before?"Hold at the authorized floor.":"Lower by one authorized step, clamped to the floor.";
    } else if (utilization>=static_cast<std::uint64_t>(bounds.high_utilization.value())) {
        result.quote=Cents{std::min(bounds.ceiling.value(),checked_add(before.value(),bounds.step.value()))};
        result.explanation+=result.quote==before?"Hold at the authorized ceiling.":"Raise by one authorized step, clamped to the ceiling.";
    } else result.explanation+="Hold: utilization is between the authorized thresholds.";
    return result;
}
}
