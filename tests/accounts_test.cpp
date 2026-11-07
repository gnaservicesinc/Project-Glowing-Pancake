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
void cash_outlook_settlement() {
    const auto first=to_ordinal({2014,1,30});
    const auto second=advance_days(first,Days{1});
    const auto february=advance_days(second,Days{1});
    auto ledger=opening_accounts(Cents{100},first);
    draw_debt(ledger,first,{"loan",Cents{50},BasisPoints{0},first,second,0,std::nullopt},"Working capital");
    add_recurring(ledger,{"lease",Cents{3100},first,february,"Monthly lease"});
    issue_invoice(ledger,first,{"bill",InvoiceKind::payable,first,Cents{180},Cents{0},"Due bill"});
    issue_invoice(ledger,first,{"sale",InvoiceKind::receivable,first,Cents{10},Cents{0},"Earned work"});
    const auto before=ledger;
    const std::array<ProjectedCashDay,2> path{{{}, {Cents{500},Cents{30}}}};
    const auto outlook=project_cash_outlook(ledger,first,Cents{20},{{first,february,Cents{20}}},path);
    check(ledger==before,"Cash outlook edited the live commitments");
    check(outlook.near_term.cash==Cents{20} && outlook.near_term.available==Cents{0} &&
          outlook.near_term.overdue_payables==Cents{40} && outlook.first_shortfall==first,
          "Outlook lost a due receipt, cash reservation or first missed payment");
    check(outlook.year.cash==Cents{160} && outlook.year.available==Cents{140} &&
          outlook.year.overdue_payables==Cents{0} && outlook.year.due_principal==Cents{0},
          "Later receipts failed to clear arrears, recurring costs, site ceiling and then principal");
}
void corporate_taxes_and_outlook() {
    const auto march=to_ordinal({2014,3,31});
    const auto april=advance_days(march,Days{1});
    const auto june=to_ordinal({2014,6,30});
    const auto due=to_ordinal({2014,7,30});
    auto ledger=opening_accounts(Cents{100000},march);
    ledger.taxes=CorporateTaxes{{BasisPoints{2000},Days{30},1,Cents{10000},BasisPoints{5000},
                                 BasisPoints{4000},Cents{500},2},march};
    record_cash(ledger,march,Cents{-15000},Account::expenses,"Opening costs","early.cost");
    close_accounts_day(ledger,march,Cents{0});
    const auto& loss=ledger.taxes->assessments.back();
    check(loss.bill==Cents{0} && loss.loss_added==Cents{10000} && tax_loss_balance(*ledger.taxes)==Cents{10000},
          "Loss carry exceeded its cap or produced a cash award");
    record_cash(ledger,april,Cents{17500},Account::revenue,"Earned sales","sales");
    record_cash(ledger,april,Cents{3500},Account::revenue,"Subsidiary dividend","group.dividend/child/1");
    record_cash(ledger,april,Cents{5000},Account::equity,"Contributed capital","investment");
    record_cash(ledger,april,Cents{-2500},Account::expenses,"Net research fee","lab/research/first/1");
    receive_customer_advance(ledger,april,"pilot/prepaid",Cents{1000});
    for (auto day=april;day<june;day=advance_days(day,Days{1})) close_accounts_day(ledger,day,Cents{0},false);
    const auto cash=accounts_report(ledger).cash;
    const auto before=ledger;
    std::array<ProjectedCashDay,31> path{};
    path.front().advance_earned=Cents{1000};
    const auto forecast=project_cash_outlook(ledger,june,Cents{0},{},path);
    check(ledger==before && forecast.near_term.corporate_tax_paid==Cents{0} &&
          forecast.year.corporate_tax_paid==Cents{1100} && forecast.year.cash==checked_unit_subtract(cash,Cents{1100}),
          "Tax forecast lost the booked quarter, earned advance, relief or actual due date");
    path.front().advance_earned=Cents{0};
    path.front().refunds_due=Cents{1000};
    const auto refund=project_cash_outlook(ledger,june,Cents{0},{},path);
    check(refund.year.corporate_tax_paid==Cents{1000} && refund.year.cash==checked_unit_subtract(cash,Cents{2000}),
          "Tax forecast deducted a refund of an unearned advance");
    issue_market_invoice(ledger,june,{"market.contract/pilot/prepaid/2014-06-30",InvoiceKind::receivable,june,
        Cents{1000},Cents{0},"Delivered contract"},Cents{1000});
    close_accounts_day(ledger,june,Cents{0});
    const auto q2=ledger.taxes->assessments.back();
    check(q2.base==TaxBase{Cents{18500},Cents{2500},Cents{2500}} && q2.loss_used==Cents{8000} &&
          q2.taxable==Cents{8000} && q2.gross_tax==Cents{1600} && q2.credit==Cents{500} && q2.bill==Cents{1100} &&
          q2.due==due && accounts_report(ledger).cash==cash && accounts_report(ledger).payables==q2.bill,
          "Quarter close counted financing as earnings or failed to create the exact tax payable");
    close_through(ledger,due);
    const auto invoice=ledger.invoices.find(tax_bill_id(june));
    check(invoice && ledger.invoices[*invoice].settled==q2.bill && accounts_report(ledger).cash==forecast.year.cash,
          "Tax did not settle once on its disclosed date");
    const auto july=advance_days(due,Days{1});
    const auto september=to_ordinal({2014,9,30});
    record_cash(ledger,july,Cents{12500},Account::revenue,"Earned sales","sales.q3");
    record_cash(ledger,july,Cents{-2500},Account::expenses,"Net research fee","lab/research/second/1");
    for (auto day=july;day<september;day=advance_days(day,Days{1})) close_accounts_day(ledger,day,Cents{0},false);
    close_accounts_day(ledger,september,Cents{0});
    const auto& q3=ledger.taxes->assessments.back();
    check(q3.loss_expired==Cents{2000} && q3.loss_used==Cents{0} && q3.credit==Cents{0} && q3.bill==Cents{2000},
          "Expired loss carry or research relief reduced a later assessment");
    auto forged=ledger;
    auto forged_return=forged.taxes->assessments.back();
    forged_return.bill=Cents{1};
    forged.taxes->assessments.replace(2,forged_return);
    refuses_unchanged(forged,[&] {validate_accounts(forged);});
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
    auto negative = malformed.postings.front();
    negative.amount = Cents{-1};
    malformed.postings = {};
    malformed.postings.push_back(std::move(negative));
    for (std::size_t i = 1; i < state.postings.size(); ++i) malformed.postings.push_back(state.postings[i]);
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
    corrupt.invoices.set_amounts(0, Cents{101}, Cents{0});
    corrupt.invoices.set_amounts(1, Cents{199}, Cents{0});
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
void settled_invoice_history() {
    auto state = opening_accounts(Cents{10});
    auto earlier = state;
    for (std::size_t i = 0; i <= accounts_open_invoice_limit; ++i) {
        const auto day = OrdinalDay{i};
        issue_market_invoice(state, day, {"market.sale/" + iso_date(day), InvoiceKind::receivable,
                             day, Cents{2}, Cents{0}, "Daily sales"});
        close_accounts_day(state, day, Cents{0}, false);
        if (i == 31) earlier = state;
    }
    check(state.invoices.size() == accounts_open_invoice_limit + 1 && state.invoices.live_rows().empty(),
          "Settled daily sales must remain auditable without filling the open invoice register");
    check(earlier.invoices.size() == 32 && accounts_report(earlier).cash == Cents{74} &&
          accounts_report_at(state, earlier.postings.size()) == accounts_report(earlier),
          "Growing the ledger must preserve the rollback checkpoint and historical balances");
    check(accounts_report(state).cash == Cents{10 + 2 * state.invoices.size()},
          "Each archived sale must collect exactly once");
    const auto next_day = OrdinalDay{accounts_open_invoice_limit + 1};
    const auto& first_invoice = earlier.invoices[0];
    refuses_unchanged(state, [&] {
        issue_invoice(state, next_day, {first_invoice.id, first_invoice.kind, next_day,
                      first_invoice.amount, Cents{0}, first_invoice.cause});
    });
    balanced(state);
}
}
int main() {
    try {
        opening_and_calendar_months();
        opening_costs_and_projection();
        cash_outlook_settlement();
        corporate_taxes_and_outlook();
        invoices_and_protected_cash();
        financing_and_rejections();
        identity_and_history_validation();
        settled_invoice_history();
        std::cout << checks << " accounts checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
