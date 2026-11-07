#pragma once

#include <pgp/core/accounts.hpp>

namespace pgp::core {
struct SimulationState;

inline constexpr std::size_t finance_sample_days=30;

struct CashMovement {
    Cents received{0}, paid{0};
    [[nodiscard]] Cents net() const { return checked_unit_subtract(received,paid); }
};
struct CompanyPeriod {
    OrdinalDay first{0}, last{0};
    Cents opening_cash{0}, closing_cash{0};
    CashMovement operations, investing, financing;
    Cents revenue{0}, expenses{0}, product_revenue{0}, service_cost{0};
    Cents corporate_tax_expense{0};
    std::uint64_t delivered_tasks=0;
};
// Reports use only settled days. Same-morning transactions are reflected in
// the separate current balance sheet.
[[nodiscard]] std::optional<CompanyPeriod> recent_company_period(const SimulationState&);

struct DatedCustomerCash {
    OrdinalDay starts_on{0}, ends_before{0};
    Cents per_day{0};
};
struct CompanyCashCase {
    unsigned demand_percent=100;
    std::uint64_t tasks_per_day=0;
    Cents discretionary_sales_per_day{0};
    CashOutlook cash;
};
struct CompanyCashForecast {
    std::size_t sample_days=0;
    Cents reserved_cash{0};
    std::array<CompanyCashCase,3> cases;
};
[[nodiscard]] CompanyCashForecast forecast_company_finances(const SimulationState&,
    std::span<const DatedCustomerCash> wholesale = {});
} // namespace pgp::core
