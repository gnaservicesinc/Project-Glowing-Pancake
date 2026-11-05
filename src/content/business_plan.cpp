#include <pgp/content/business_plan.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace pgp::content {
std::string canonical_business_plan_bytes(const BusinessPlan& plan, core::OrdinalDay starts_on) {
    const auto& row = plan.training;
    if (row.id.empty() || row.source_id != "lease.initial" || row.kind != core::ProjectKind::training ||
        row.status != core::ProjectStatus::active || row.starts_on != starts_on || row.stopped_on ||
        row.completed_work.value() != 0 || row.tasks_per_day != 0 || row.inference_milli_CU_per_task.value() != 0 ||
        row.useful_tasks != 0 || row.monthly_support_cost.value() != 0 || row.minimum_memory_mib != 0 ||
        row.interconnect != "general" || row.cause != "Scenario training: " + row.id || plan.data_license.value() <= 0 ||
        plan.expansion_on <= starts_on || plan.rescue_proceeds.value() <= 0 || plan.rescue_shares.value() == 0)
        throw std::invalid_argument("Business plan contains incompatible initial project or financing state");
    using Json = nlohmann::json;
    return Json{{"training",{{"id",row.id},{"requested_milli_CU_per_day",row.requested.value()},
        {"staff",row.staff},{"total_work_milli_CU",row.total_work.value()},{"data_license_cents",plan.data_license.value()}}},
        {"expansion_on",core::iso_date(plan.expansion_on)},
        {"expansion",Json::parse(canonical_market_setup_bytes(plan.expansion,plan.expansion_on))},
        {"rescue_proceeds_cents",plan.rescue_proceeds.value()},{"rescue_shares_basis_points",plan.rescue_shares.value()}}.dump();
}
}
