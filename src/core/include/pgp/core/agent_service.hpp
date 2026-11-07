#pragma once

#include <pgp/core/calendar.hpp>
#include <pgp/core/units.hpp>
#include <optional>
#include <string_view>

namespace pgp::core {
enum class AgentTools { draft, internal, external };
enum class AgentOversight { batch, each_action };
struct AgentControls {
    AgentTools tools=AgentTools::internal;
    AgentOversight oversight=AgentOversight::batch;
    auto operator<=>(const AgentControls&) const = default;
};
struct AgentPlan {
    AgentControls controls;
    BasisPoints failure_rate{0};
    Cents remedy_per_action{0};
    std::optional<BasisPoints> shifted_failure_rate{};
    auto operator<=>(const AgentPlan&) const = default;
};
struct AgentFailures {
    std::uint64_t failed=0, executed=0;
    Cents remedy{0};
    auto operator<=>(const AgentFailures&) const = default;
};
[[nodiscard]] std::string_view agent_tools_key(AgentTools);
[[nodiscard]] std::string_view agent_oversight_key(AgentOversight);
[[nodiscard]] AgentTools parse_agent_tools(std::string_view);
[[nodiscard]] AgentOversight parse_agent_oversight(std::string_view);
[[nodiscard]] std::uint64_t agent_review_capacity(std::uint64_t reviewed_tasks,AgentControls);
[[nodiscard]] Cents agent_remedy_rate(AgentControls,Cents authored_task_price);
[[nodiscard]] AgentFailures agent_failures(const AgentPlan*,OrdinalDay,std::string_view service,std::string_view group,std::uint64_t attempts,
    BasisPoints shifted_share=BasisPoints{0});
} // namespace pgp::core
