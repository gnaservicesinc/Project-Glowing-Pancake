#include <pgp/core/agent_service.hpp>
#include <pgp/core/random.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
std::string_view agent_tools_key(AgentTools tools) {
    switch (tools) {
    case AgentTools::draft:return "draft";
    case AgentTools::internal:return "internal";
    case AgentTools::external:return "external";
    }
    throw std::invalid_argument("Unknown agent tool permission");
}
std::string_view agent_oversight_key(AgentOversight oversight) {
    switch (oversight) {
    case AgentOversight::batch:return "batch";
    case AgentOversight::each_action:return "each_action";
    }
    throw std::invalid_argument("Unknown agent oversight mode");
}
AgentTools parse_agent_tools(std::string_view key) {
    for (const auto value:{AgentTools::draft,AgentTools::internal,AgentTools::external})
        if (agent_tools_key(value)==key) return value;
    throw std::invalid_argument("Unknown agent tool permission");
}
AgentOversight parse_agent_oversight(std::string_view key) {
    for (const auto value:{AgentOversight::batch,AgentOversight::each_action})
        if (agent_oversight_key(value)==key) return value;
    throw std::invalid_argument("Unknown agent oversight mode");
}
std::uint64_t agent_review_capacity(std::uint64_t reviewed_tasks,AgentControls controls) {
    (void)agent_tools_key(controls.tools);
    (void)agent_oversight_key(controls.oversight);
    // Drafts need a staff execution pass. Per-action approval adds a check
    // before a recommendation or write can reach the customer.
    const auto passes=1U+(controls.tools==AgentTools::draft?1U:0U)+(controls.oversight==AgentOversight::each_action?1U:0U);
    return std::max<std::uint64_t>(1,reviewed_tasks/passes);
}
Cents agent_remedy_rate(AgentControls controls,Cents authored_task_price) {
    if (controls.tools==AgentTools::draft || controls.oversight==AgentOversight::each_action) return Cents{0};
    const auto base=std::max<std::int64_t>(1,authored_task_price.value());
    return Cents{controls.tools==AgentTools::external?checked_add(base,base):base};
}
AgentFailures agent_failures(const AgentPlan* plan,OrdinalDay day,std::string_view service,std::string_view group,std::uint64_t attempts) {
    if (!plan || attempts==0) return {};
    // Round sub-task expectations with a keyed daily draw. It consumes no
    // campaign stream and stays stable across save/load and retries.
    std::uint64_t key=static_cast<std::uint64_t>(day.value());
    for (const auto id:{service,group}) {
        for (const char ch:id) key=(key^static_cast<unsigned char>(ch))*UINT64_C(1099511628211);
        key=(key^UINT64_C(255))*UINT64_C(1099511628211);
    }
    SplitMix64 draw{key};
    const auto expected=mul_div_floor(attempts,static_cast<std::uint64_t>(plan->failure_rate.value()),10000);
    const auto failed=expected.quotient+(draw.bounded(10000)<expected.remainder?1U:0U);
    const auto executed=plan->controls.tools!=AgentTools::draft && plan->controls.oversight==AgentOversight::batch?failed:0;
    return {failed,executed,Cents{mul_div_floor(executed,static_cast<std::uint64_t>(plan->remedy_per_action.value()),1).quotient}};
}
} // namespace pgp::core
