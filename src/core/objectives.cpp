#include <pgp/core/objectives.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::array<std::string_view,objective_kinds.size()> keys{
    "monopoly","domination","utopia","profit","asi","survival"};
constexpr std::array<std::string_view,objective_kinds.size()> names{
    "World Monopoly","World Domination","World Utopia","Profit","ASI Race","Simple Survival"};
std::uint64_t add(std::uint64_t a,std::uint64_t b) {
    if(b>std::numeric_limits<std::uint64_t>::max()-a) throw std::overflow_error("Objective task sum overflowed.");
    return a+b;
}
void accumulate(DeliveredWork& total,const DeliveredWork& work) {
    if(work.controlled>work.total) throw std::invalid_argument("Attributed work exceeds all delivered work.");
    total={add(total.controlled,work.controlled),add(total.total,work.total)};
}
bool share(DeliveredWork work,std::uint32_t floor) {
    return work.total && mul_div_floor(work.controlled,10000,work.total).quotient>=floor;
}
}

std::string_view objective_key(ObjectiveKind kind) noexcept {
    const auto i=static_cast<std::size_t>(kind);return i<keys.size()?keys[i]:std::string_view{};
}
std::string_view objective_name(ObjectiveKind kind) noexcept {
    const auto i=static_cast<std::size_t>(kind);return i<names.size()?names[i]:std::string_view{};
}
std::optional<ObjectiveKind> objective_kind(std::string_view key) noexcept {
    const auto it=std::find(keys.begin(),keys.end(),key);
    return it==keys.end()?std::nullopt:std::optional{objective_kinds[static_cast<std::size_t>(it-keys.begin())]};
}
void validate_objective_selection(const ObjectiveSelection& selection,bool asi_available) {
    std::array<bool,objective_kinds.size()> used{};
    const auto use=[&](ObjectiveKind kind) {
        const auto i=static_cast<std::size_t>(kind);
        if(i>=used.size() || used[i]) throw std::invalid_argument("Select one primary and distinct secondary objectives.");
        if(kind==ObjectiveKind::asi && !asi_available)
            throw std::invalid_argument("This plateau scenario has no feasible ASI route. Choose a different objective or scenario.");
        used[i]=true;
    };
    use(selection.primary);
    for(auto kind:selection.secondary) use(kind);
}
void validate_objective_charter(const ObjectiveCharter& charter) {
    validate_objective_selection(charter.opening,true);
    for(auto factor:{charter.receivables_realizable,charter.physical_assets_realizable})
        if(factor.value()<0 || factor.value()>10000) throw std::invalid_argument("Terminal asset realization must be between 0% and 100%.");
    if(charter.profit_multiple>5) throw std::invalid_argument("Terminal operating-profit multiple must be between zero and five.");
}
OrdinalDay objective_window_start(OrdinalDay deadline,ObjectiveKind kind) {
    auto date=from_ordinal(deadline);
    if(date.month!=1 || date.day!=1) throw std::invalid_argument("The objective deadline must follow a fully settled December.");
    switch(kind) {
    case ObjectiveKind::monopoly:case ObjectiveKind::domination:date.year-=5;break;
    case ObjectiveKind::utopia:date.year-=10;break;
    case ObjectiveKind::profit:case ObjectiveKind::survival:--date.year;break;
    case ObjectiveKind::asi:throw std::invalid_argument("The ASI race does not have a final accounting window.");
    }
    return to_ordinal(date);
}
bool objective_month_end(OrdinalDay day) {
    const auto date=from_ordinal(day);return date.day==days_in_month(date.year,date.month);
}
bool domination_met(const ObjectiveMonth& month) noexcept {
    return month.population_coverage>=authority_coverage_floor && month.output_coverage>=authority_coverage_floor;
}
bool utopia_met(const ObjectiveMonth& month) noexcept {
    for(std::size_t i=0;i<utopia_global_floors.size();++i)
        if(month.global_welfare[i]<utopia_global_floors[i]) return false;
    for(const auto& region:month.regional_welfare)
        if(std::any_of(region.begin(),region.end(),[](auto index){return index<utopia_region_floor;})) return false;
    return true;
}
RegionalDeliveredWork aggregate_delivery(std::span<const ObjectiveMonth> months,OrdinalDay first) {
    RegionalDeliveredWork result{};
    for(const auto& month:months) if(month.month_end>=first)
        for(std::size_t i=0;i<result.size();++i) accumulate(result[i],month.delivery[i]);
    return result;
}
DeliveredWork global_delivery(const RegionalDeliveredWork& regions) {
    DeliveredWork result;
    for(const auto& region:regions) accumulate(result,region);
    return result;
}
bool monopoly_still_possible(const RegionalDeliveredWork& regions,
    const std::array<std::uint64_t,society_regions>& future_upper_bound) {
    auto optimistic=regions;
    for(std::size_t i=0;i<regions.size();++i) {
        const auto future=future_upper_bound[i];
        accumulate(optimistic[i],{future,future});
        if(!share(optimistic[i],monopoly_region_floor)) return false;
    }
    return share(global_delivery(optimistic),monopoly_global_floor);
}
} // namespace pgp::core
