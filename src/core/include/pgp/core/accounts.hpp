#pragma once

#include <pgp/core/calendar.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::core {

enum class Account { cash, receivables, assets, accumulated_depreciation,
                     payables, debt, equity, revenue, expenses };
[[nodiscard]] std::string_view to_string(Account account) noexcept;
struct Posting {
    std::uint64_t id = 0;
    OrdinalDay day{0};
    Account debit = Account::cash;
    Account credit = Account::equity;
    Cents amount{0};
    std::string cause;
    std::string owner;
    std::string key;
    std::string document_id;
    auto operator<=>(const Posting&) const = default;
};
enum class InvoiceKind { receivable, payable };
struct Invoice {
    std::string id;
    InvoiceKind kind = InvoiceKind::payable;
    OrdinalDay due{0};
    Cents amount{0};
    Cents settled{0};
    std::string cause;
    auto operator<=>(const Invoice&) const = default;
};
struct RecurringObligation {
    std::string id;
    Cents monthly_amount{0};
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> ends_before;
    std::string cause;
    auto operator<=>(const RecurringObligation&) const = default;
};
struct Debt {
    std::string id;
    Cents principal{0};
    BasisPoints monthly_interest{0};
    OrdinalDay drawn_on{0};
    OrdinalDay due{0};
    std::uint64_t interest_remainder = 0;
    std::optional<OrdinalDay> interest_month;
    auto operator<=>(const Debt&) const = default;
};
struct EquityHolding {
    std::string holder;
    BasisPoints shares{0};
    bool reserved_pool = false;
    auto operator<=>(const EquityHolding&) const = default;
};
inline constexpr std::size_t accounts_posting_limit = 262144;
inline constexpr std::size_t accounts_document_limit = 8192;
inline constexpr std::size_t accounts_terms_limit = 256;
struct AccountsState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::uint64_t next_posting_id = 1;
    std::vector<Posting> postings;
    std::vector<Invoice> invoices;
    std::vector<RecurringObligation> recurring;
    std::vector<Debt> debts;
    std::vector<EquityHolding> holdings;
    auto operator<=>(const AccountsState&) const = default;
};
// All balances use their normal accounting sign; expenses are debit-positive,
// liabilities/equity/revenue are credit-positive. Reports never own cash.
struct AccountsReport {
    Cents cash{0};
    Cents receivables{0};
    Cents assets{0};
    Cents accumulated_depreciation{0};
    Cents payables{0};
    Cents debt{0};
    Cents equity{0};
    Cents revenue{0};
    Cents expenses{0};
    Cents profit{0};
    auto operator<=>(const AccountsReport&) const = default;
};
[[nodiscard]] AccountsState opening_accounts(Cents cash, OrdinalDay starts_on = OrdinalDay{0},
                                            std::string owner = "company.player");
[[nodiscard]] AccountsReport accounts_report(const AccountsState& state);
void validate_accounts(const AccountsState& state);
// Each mutation validates a copied candidate and commits atomically. Reused
// document/transaction keys are refused, including previously settled records.
void record_cash(AccountsState& state, OrdinalDay day, Cents delta, Account offset,
                 std::string cause, std::string key);
void add_recurring(AccountsState& state, RecurringObligation obligation);
// Stop future service from the current morning, retaining prior accruals. An
// exclusive end equal to the start represents cancellation before first work.
void end_recurring(AccountsState& state, std::string_view id, OrdinalDay ends_before);
void issue_invoice(AccountsState& state, OrdinalDay day, Invoice invoice);
void settle_invoice(AccountsState& state, OrdinalDay day, std::string_view id,
                    Cents amount, Cents protected_cash, std::string key);
void draw_debt(AccountsState& state, OrdinalDay day, Debt debt, std::string cause);
void repay_debt(AccountsState& state, OrdinalDay day, std::string_view id,
                Cents amount, Cents protected_cash, std::string key);
void invest_equity(AccountsState& state, OrdinalDay day, std::string investor,
                   Cents proceeds, BasisPoints acquired, std::string key);
// Conserving Hamilton dilution, also shared by approved pool expansions and
// noncash acquisition consideration. Neither helper creates cash.
void dilute_holdings(std::vector<EquityHolding>& holdings,std::string investor,BasisPoints acquired);
void issue_asset_equity(AccountsState&,OrdinalDay,std::string investor,Cents value,BasisPoints acquired,std::string key);
// Accrue exactly the next day, then collect due receivables and pay due
// obligations in (due,id) order from unreserved cash. Unpaid amounts persist.
// Principal repayment requires repay_debt; maturity is visible in projections.
void close_accounts_day(AccountsState& state, OrdinalDay day, Cents protected_cash, bool validate_io = true);
struct CashProjection {
    OrdinalDay through{0};
    Cents cash{0};
    Cents available{0};
    Cents overdue_payables{0};
    Cents due_principal{0};
    auto operator<=>(const CashProjection&) const = default;
};
[[nodiscard]] CashProjection project_cash(const AccountsState& state, OrdinalDay through,
                                         Cents protected_cash = Cents{0});
} // namespace pgp::core
