#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime {
#if !PGP_DISTRIBUTION
void refill_playtest_resources(WorldState&);
void validate_playtest_morning(const Checkpoint&);
void edit_playtest_person(WorldState&,std::string_view,const std::vector<std::string>&,const std::array<int,5>&);
void deliver_playtest_orders(WorldState&);
void complete_playtest_research(WorldState&,std::string_view);
#endif
}
