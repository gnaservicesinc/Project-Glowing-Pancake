#pragma once
#include <pgp/core/simulation.hpp>
namespace pgp::runtime::competition_detail {
struct PeoplePlan {
    std::string action,subject;
    core::CommandAction command;
};
std::optional<PeoplePlan> plan_individual_hire(const core::SimulationState&,core::Cents usable,core::Cents runway);
}
