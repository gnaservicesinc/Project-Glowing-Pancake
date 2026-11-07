#pragma once

#include <pgp/core/calendar.hpp>
#include <pgp/core/paged_history.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::core {
struct AccountsState;

struct CorporateTaxPolicy {
    BasisPoints rate{0};
    Days payment_delay{0};
    std::uint32_t loss_carry_quarters=0;
    Cents loss_carry_cap{0};
    BasisPoints loss_offset{0};
    BasisPoints research_credit{0};
    Cents research_credit_cap{0};
    std::uint32_t research_credit_quarters=0;
    auto operator<=>(const CorporateTaxPolicy&) const = default;
};
struct TaxBase {
    Cents income{0}, deductions{0}, research_fees{0};
    auto operator<=>(const TaxBase&) const = default;
};
struct TaxLoss {
    OrdinalDay originated_on{0};
    Cents remaining{0};
    auto operator<=>(const TaxLoss&) const = default;
};
struct TaxAssessment {
    OrdinalDay first{0}, through{0}, due{0};
    TaxBase base;
    Cents loss_used{0}, loss_added{0}, loss_expired{0};
    Cents taxable{0}, gross_tax{0}, credit{0}, bill{0};
    auto operator<=>(const TaxAssessment&) const = default;
};
inline constexpr std::size_t tax_return_limit=retained_campaign_days/90+2;
inline constexpr std::size_t tax_loss_limit=33;
inline constexpr std::string_view tax_bill_prefix="tax.corporate/";
struct CorporateTaxes {
    CorporateTaxPolicy policy;
    OrdinalDay starts_on{0};
    std::vector<TaxLoss> losses{};
    PagedHistory<TaxAssessment,16> assessments{};
    auto operator<=>(const CorporateTaxes&) const = default;
};

void validate_corporate_tax_policy(const CorporateTaxPolicy&);
void validate_corporate_taxes(const AccountsState&);
[[nodiscard]] TaxBase open_tax_base(const AccountsState&);
[[nodiscard]] Cents tax_loss_balance(const CorporateTaxes&);
[[nodiscard]] std::optional<OrdinalDay> last_research_credit_day(const CorporateTaxes&);
[[nodiscard]] std::string tax_bill_id(OrdinalDay through);
// Called after regular accruals and before cash settlement in a ledger candidate.
// A forecast can supply the booked portion of its first quarter.
[[nodiscard]] const TaxAssessment* close_corporate_tax(AccountsState&,OrdinalDay,const TaxBase& carried_base={});
} // namespace pgp::core
