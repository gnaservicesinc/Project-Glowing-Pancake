#include <pgp/core/accounts.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace pgp::core;
namespace {
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Operation> void refuses_unchanged(AccountsState& state, Operation operation) {
    const auto before = state;
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Invalid financial operation was accepted");
    check(state == before, "Rejected financial operation changed accounts");
}
void close_through(AccountsState& state, OrdinalDay target, Cents protected_cash = Cents{0}) {
    auto day = state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
    while (day <= target) {
        close_accounts_day(state, day, protected_cash);
        day = advance_days(day, Days{1});
    }
}
void balanced(const AccountsState& state) {
    validate_accounts(state);
    const auto report = accounts_report(state);
    const auto assets = checked_subtract(checked_add(checked_add(report.cash.value(), report.receivables.value()),
        report.assets.value()), report.accumulated_depreciation.value());
    const auto liabilities_and_equity = checked_add(checked_add(report.payables.value(), report.debt.value()),
        checked_add(report.equity.value(), report.profit.value()));
    check(assets == liabilities_and_equity, "Posting-derived accounting equation does not balance");
    check(std::all_of(state.postings.begin(), state.postings.end(), [&](const auto& posting) {
        return posting.amount.value() > 0 && posting.debit != posting.credit && posting.owner == state.owner &&
               !posting.cause.empty() && !posting.key.empty();
    }), "Journal posting lacks balanced accounts or causal ownership");
}
void opening_and_calendar_months() {
    auto opening = opening_accounts(Cents{120'000'000});
    const auto report = accounts_report(opening);
    check(report.cash == Cents{120'000'000} && report.equity == report.cash && report.revenue == Cents{0},
          "Opening funded cash was incorrectly recognized as income");
    const auto shares = std::accumulate(opening.holdings.begin(), opening.holdings.end(), std::int64_t{0},
        [](auto total, const auto& holding) { return total + holding.shares.value(); });
    check(shares == 10000 && opening.holdings.size() == 4,
          "Opening fully diluted ownership must conserve 10000 basis points");
    check(std::count_if(opening.holdings.begin(), opening.holdings.end(), [](const auto& holding) {
        return holding.reserved_pool && holding.shares == BasisPoints{500};
    }) == 1, "Opening option pool is one reserved 5% holding");
    balanced(opening);

    for (const CivilDate start : {CivilDate{2014, 1, 1}, CivilDate{2014, 2, 1},
                                 CivilDate{2016, 2, 1}, CivilDate{2014, 4, 1}}) {
        const auto first = to_ordinal(start);
        auto state = opening_accounts(Cents{100'000}, first);
        add_recurring(state, {"office", Cents{10'001}, first, {}, "Office contract"});
        const auto last = to_ordinal({start.year, start.month, days_in_month(start.year, start.month)});
        close_through(state, last);
        const auto monthly = accounts_report(state);
        check(monthly.cash == Cents{89'999} && monthly.expenses == Cents{10'001} && monthly.payables == Cents{0},
              "Calendar month accrual lost cents, duplicated cost or used a fixed day count");
        balanced(state);
        refuses_unchanged(state, [&] { close_accounts_day(state, last, Cents{0}); });
    }
    auto partial = opening_accounts(Cents{10000});
    add_recurring(partial, {"partial", Cents{3100}, to_ordinal({2014, 1, 16}),
                          to_ordinal({2014, 2, 1}), "Partial month service"});
    close_through(partial, to_ordinal({2014, 2, 28}));
    check(accounts_report(partial).expenses == Cents{1600},
          "Mid-month contract must charge only its 16 active calendar days");
    balanced(partial);
}
void opening_costs_and_projection() {
    auto state = opening_accounts(Cents{120'000'000});
    add_recurring(state, {"payroll", Cents{7'200'000}, OrdinalDay{0}, {}, "Payroll including benefits"});
    add_recurring(state, {"office", Cents{1'800'000}, OrdinalDay{0}, {}, "Office administration"});
    add_recurring(state, {"lease", Cents{1'800'000}, OrdinalDay{0}, {}, "Compute including power"});
    record_cash(state, OrdinalDay{0}, Cents{-1'200'000}, Account::expenses,
                "Delivered data license", "license.initial");
    const auto before = state;
    const auto projection = project_cash(state, to_ordinal({2014, 2, 28}));
    check(state == before, "Cash projection mutated authoritative balances or cursors");
    check(projection.cash == Cents{97'200'000}, "Two-month hand calculation must leave $972000");
    close_through(state, to_ordinal({2014, 2, 28}));
    const auto report = accounts_report(state);
    check(report.cash == projection.cash && report.expenses == Cents{22'800'000} &&
          report.revenue == Cents{0} && report.debt == Cents{0},
          "Opening recurring costs, data license or projection charged twice");
    refuses_unchanged(state, [&] { record_cash(state, OrdinalDay{59}, Cents{-1'200'000},
        Account::expenses, "Duplicate data license", "license.initial"); });
    balanced(state);
}
void invoices_and_protected_cash() {
    auto state = opening_accounts(Cents{1000});
    issue_invoice(state, OrdinalDay{0}, {"bill", InvoiceKind::payable, OrdinalDay{30},
                  Cents{900}, Cents{0}, "Supplier invoice"});
    check(accounts_report(state).cash == Cents{1000} && accounts_report(state).expenses == Cents{900} &&
          accounts_report(state).payables == Cents{900}, "Payable accrual incorrectly spent cash");
    refuses_unchanged(state, [&] { issue_invoice(state, OrdinalDay{0},
        {"bill", InvoiceKind::payable, OrdinalDay{30}, Cents{900}, Cents{0}, "Duplicate supplier bill"}); });
    refuses_unchanged(state, [&] { settle_invoice(state, OrdinalDay{0}, "bill", Cents{501},
                                               Cents{500}, "bill.too_much"); });
    settle_invoice(state, OrdinalDay{0}, "bill", Cents{400}, Cents{500}, "bill.part");
    check(accounts_report(state).cash == Cents{600} && accounts_report(state).payables == Cents{500},
          "Partial payable settlement changed expenses or the wrong balance");
    refuses_unchanged(state, [&] { settle_invoice(state, OrdinalDay{0}, "bill", Cents{1},
                                               Cents{0}, "bill.part"); });
    close_through(state, OrdinalDay{30}, Cents{500});
    check(accounts_report(state).cash == Cents{500} && accounts_report(state).payables == Cents{400},
          "Due payments must retain protected reservations and unpaid liabilities");
    issue_invoice(state, OrdinalDay{31}, {"sale", InvoiceKind::receivable, OrdinalDay{31},
                  Cents{800}, Cents{0}, "Delivered customer work"});
    check(accounts_report(state).revenue == Cents{800} && accounts_report(state).receivables == Cents{800} &&
          accounts_report(state).cash == Cents{500}, "Receivable recognition confused income with cash");
    close_accounts_day(state, OrdinalDay{31}, Cents{500});
    check(accounts_report(state).cash == Cents{900} && accounts_report(state).payables == Cents{0} &&
          accounts_report(state).receivables == Cents{0}, "Collected work must permit overdue liability recovery");
    balanced(state);
}
void financing_and_rejections() {
    auto state = opening_accounts(Cents{10000});
    draw_debt(state, OrdinalDay{0}, {"loan", Cents{10000}, BasisPoints{100}, OrdinalDay{0},
              OrdinalDay{59}, 0, {}}, "Approved financing");
    check(accounts_report(state).cash == Cents{20000} && accounts_report(state).debt == Cents{10000} &&
          accounts_report(state).revenue == Cents{0}, "Credit proceeds are debt, not revenue");
    close_through(state, OrdinalDay{30});
    check(accounts_report(state).expenses == Cents{100} && accounts_report(state).cash == Cents{19900},
          "Full calendar month at 1% must accrue and settle exactly 100 cents of interest");
    repay_debt(state, OrdinalDay{31}, "loan", Cents{4000}, Cents{1000}, "loan.part");
    check(accounts_report(state).cash == Cents{15900} && accounts_report(state).debt == Cents{6000} &&
          accounts_report(state).expenses == Cents{100}, "Principal repayment must not duplicate financing expense");
    refuses_unchanged(state, [&] { repay_debt(state, OrdinalDay{31}, "loan", Cents{6001}, Cents{0}, "loan.over"); });
    refuses_unchanged(state, [&] { repay_debt(state, OrdinalDay{31}, "loan", Cents{1}, Cents{0}, "loan.part"); });
    invest_equity(state, OrdinalDay{31}, "investor.new", Cents{10000}, BasisPoints{1000}, "round.one");
    check(accounts_report(state).cash == Cents{25900} && accounts_report(state).equity == Cents{20000} &&
          accounts_report(state).revenue == Cents{0}, "Equity proceeds must not increase product revenue");
    const auto shares = std::accumulate(state.holdings.begin(), state.holdings.end(), std::int64_t{0},
        [](auto total, const auto& row) { return total + row.shares.value(); });
    check(shares == 10000 && std::any_of(state.holdings.begin(), state.holdings.end(), [](const auto& row) {
        return row.holder == "investor.new" && row.shares == BasisPoints{1000} && !row.reserved_pool;
    }), "Investment dilution must conserve the fully diluted ownership pool");
    check(std::any_of(state.holdings.begin(), state.holdings.end(), [](const auto& row) {
        return row.reserved_pool && row.shares == BasisPoints{450};
    }), "Equity investment must dilute the existing pool instead of granting another pool");
    refuses_unchanged(state, [&] { invest_equity(state, OrdinalDay{31}, "investor.other", Cents{1},
                                              BasisPoints{1}, "round.one"); });
    balanced(state);

    auto malformed = state;
    malformed.holdings.front().shares = BasisPoints{0};
    bool invalid = false;
    try { validate_accounts(malformed); } catch (const std::exception&) { invalid = true; }
    check(invalid, "Corrupt ownership totals must refuse restore");
    malformed = state;
    malformed.postings.front().amount = Cents{-1};
    invalid = false;
    try { validate_accounts(malformed); } catch (const std::exception&) { invalid = true; }
    check(invalid, "Negative journal amounts must refuse restore");

    auto rounded = opening_accounts(Cents{100});
    auto reordered = rounded;
    std::reverse(reordered.holdings.begin(), reordered.holdings.end());
    invest_equity(rounded, OrdinalDay{0}, "investor.one_bp", Cents{1}, BasisPoints{1}, "tiny.round");
    invest_equity(reordered, OrdinalDay{0}, "investor.one_bp", Cents{1}, BasisPoints{1}, "tiny.round");
    const auto by_holder = [](const auto& left, const auto& right) { return left.holder < right.holder; };
    std::sort(rounded.holdings.begin(), rounded.holdings.end(), by_holder);
    std::sort(reordered.holdings.begin(), reordered.holdings.end(), by_holder);
    check(rounded.holdings == reordered.holdings, "Dilution rounding must not favor cap-table input order");
    check(std::any_of(rounded.holdings.begin(), rounded.holdings.end(), [](const auto& row) {
        return row.holder == "founder" && row.shares == BasisPoints{5499};
    }), "One-basis-point investment must distribute stable largest-remainder rounding exactly");

    auto maximum = opening_accounts(Cents{std::numeric_limits<std::int64_t>::max()});
    refuses_unchanged(maximum, [&] { invest_equity(maximum, OrdinalDay{0}, "overflow", Cents{1},
                                                 BasisPoints{1}, "overflow.round"); });
}
void identity_and_history_validation() {
    auto state = opening_accounts(Cents{0});
    for (const auto* key : {"opening", "invoice/future", "debt/future", "accrue/future", "settle/future"}) {
        refuses_unchanged(state, [&] { record_cash(state, OrdinalDay{0}, Cents{1}, Account::equity,
                                                  "Caller supplied contribution", key); });
    }
    state = opening_accounts(Cents{1000});
    issue_invoice(state, OrdinalDay{0}, {"first", InvoiceKind::payable, OrdinalDay{30}, Cents{100}, Cents{0}, "First bill"});
    issue_invoice(state, OrdinalDay{0}, {"second", InvoiceKind::payable, OrdinalDay{30}, Cents{200}, Cents{0}, "Second bill"});
    auto corrupt = state;
    corrupt.invoices[0].amount = Cents{101};
    corrupt.invoices[1].amount = Cents{199};
    bool rejected = false;
    try { validate_accounts(corrupt); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Per-document corruption must reject even when aggregate payable still reconciles");

    draw_debt(state, OrdinalDay{0}, {"history", Cents{10000}, BasisPoints{100}, OrdinalDay{0}, OrdinalDay{59}, 0, {}},
              "History loan");
    close_through(state, OrdinalDay{30});
    corrupt = state;
    ++corrupt.debts.front().interest_remainder;
    rejected = false;
    try { validate_accounts(corrupt); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Debt interest residue must match the actual closed-day principal history");
}
}
int main() {
    try {
        opening_and_calendar_months();
        opening_costs_and_projection();
        invoices_and_protected_cash();
        financing_and_rejections();
        identity_and_history_validation();
        std::cout << checks << " accounts checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
