#pragma once
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <stdexcept>
namespace pgp::core::governance_detail {
[[noreturn]] inline void invalid(const char* why) {throw std::invalid_argument(why);}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& r){
        if constexpr(requires {r.id;}) return r.id==id;
        else return r.candidate==id;
    });
    if (it==rows.end()) invalid("The corporate record is unavailable");
    return *it;
}
inline const GovernanceCatalog& catalog(const SimulationState& s) {
    if (!s.management || !s.management->catalog.governance || !s.management->governance || !s.accounts || !s.capacity)
        invalid("This scenario has no corporate governance rules");
    return *s.management->catalog.governance;
}
inline GovernanceState& governance(SimulationState& s) {(void)catalog(s);return *s.management->governance;}
inline std::string grant_holder(std::string_view id) {return "executive.grant/"+std::string(id);}
inline BasisPoints shares_of(const std::vector<EquityHolding>& h,std::string_view id) {
    for (const auto& row:h) if (row.holder==id) return row.shares;
    return BasisPoints{0};
}
inline std::string pool_id(const std::vector<EquityHolding>& h) {
    for (const auto& row:h) if (row.reserved_pool) return row.holder;
    invalid("The reserved option pool is missing");
}
BasisPoints vesting_due(const std::vector<EquityHolding>&,const OptionGrant&,const ExecutiveAppointment&,const ExecutiveCandidate&,OrdinalDay);
void apply_capital(std::vector<EquityHolding>&,std::vector<OptionGrant>&,const std::vector<ExecutiveAppointment>&,const GovernanceCatalog&,const CapitalEvent&);
void record_capital(SimulationState&,CapitalEvent);
BoardMotion& new_motion(SimulationState&,BoardMotionKind,std::string,Cents,Cents,BasisPoints);
std::vector<BoardBallot> ballots_for(const SimulationState&,const BoardMotion&,bool,Cents);
void execute_finance(SimulationState&,BoardMotion&);
}
