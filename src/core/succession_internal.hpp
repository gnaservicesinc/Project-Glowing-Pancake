#pragma once
#include <pgp/core/simulation.hpp>
#include "governance_internal.hpp"
#include <algorithm>

namespace pgp::core::succession_detail {
[[noreturn]] inline void invalid(std::string message) {throw std::invalid_argument(std::move(message));}
template<class Rows> const auto& named(const Rows& rows,std::string_view id) {
    using Row=typename Rows::value_type;
    const auto found=std::ranges::find(rows,id,&Row::id);
    if(found==rows.end()) invalid("A leadership reference is unavailable.");
    return *found;
}
inline OrdinalDay year_day(int year) {return to_ordinal({year,1,1});}
inline OrdinalDay anniversary(OrdinalDay day,unsigned years) {
    auto date=from_ordinal(day);date.year+=static_cast<int>(years);
    date.day=std::min(date.day,days_in_month(date.year,date.month));return to_ordinal(date);
}
[[nodiscard]] OrdinalDay term_due(const SuccessionCatalog&,int birth,OrdinalDay start,bool caretaker,bool asi=false);
[[nodiscard]] std::string salary_id(const LeadershipTerm&);
[[nodiscard]] bool founder_payment(const Posting&);
[[nodiscard]] std::vector<BoardBallot> succession_ballots(const SimulationState&,const SuccessionPlan&,bool,Cents);
[[nodiscard]] MilliCU transition_work(const SuccessionCatalog&,const LabAllocation&);
void check_proposal(const SimulationState&,const ProposeSuccession&,OrdinalDay proposed,std::uint64_t incumbent);
}
