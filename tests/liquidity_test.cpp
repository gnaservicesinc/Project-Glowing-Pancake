#include <pgp/core/liquidity.hpp>

#include <iostream>
#include <stdexcept>

using namespace pgp::core;
namespace {
std::size_t checks=0;
void check(bool condition,const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Operation> void refuses_unchanged(LiquidityState& state,Operation operation) {
    const auto before=state;bool refused=false;
    try { operation(); } catch (const std::exception&) { refused=true; }
    check(refused,"Invalid liquidity operation was accepted");
    check(state==before,"Rejected liquidity operation changed state");
}
void close_day(AccountsState& accounts,LiquidityState& state,OrdinalDay day,Cents protected_cash=Cents{0}) {
    close_accounts_day(accounts,day,protected_cash);
    observe_liquidity(state,accounts,advance_days(day,Days{1}));
}
void warning_and_recovery() {
    auto accounts=opening_accounts(Cents{100});auto state=opening_liquidity();
    issue_invoice(accounts,OrdinalDay{0},{"bill",InvoiceKind::payable,OrdinalDay{0},Cents{200},Cents{0},"Delivered supplier service"});
    refresh_liquidity(state,accounts);
    check(state.status==LiquidityStatus::operating && state.workouts.empty(),"Today's unsettled invoices must not start an early workout");
    close_day(accounts,state,OrdinalDay{0});
    const auto report=liquidity_report(state);
    check(state.status==LiquidityStatus::workout && report.pending_warning==1 && report.deadline==OrdinalDay{31},
          "First unpaid settlement must warn next morning with exactly 30 days to respond");
    check(report.unpaid_payables==Cents{100} && report.matured_principal==Cents{0} && report.oldest && report.oldest->reference=="bill",
          "Warning must explain the outstanding obligation rather than accounting loss");
    check(state.workouts.front().initial_shortfall==Cents{100} && state.workouts.front().warned_on==OrdinalDay{1},
          "Workouts must preserve original shortfall and warned date");
    refuses_unchanged(state,[&]{acknowledge_liquidity_warning(state,2);});
    acknowledge_liquidity_warning(state,1);
    check(!liquidity_report(state).pending_warning,"Acknowledgment did not clear only the required warning");
    const auto first=state.workouts.front();
    invest_equity(accounts,OrdinalDay{1},"rescue",Cents{50},BasisPoints{100},"rescue.first");
    settle_invoice(accounts,OrdinalDay{1},"bill",Cents{50},Cents{0},"pay.partial");
    refresh_liquidity(state,accounts);
    check(state.status==LiquidityStatus::workout && state.exposure.unpaid_payables==Cents{50} && state.workouts.front().deadline==first.deadline,
          "Partial rescue must preserve the original workout deadline");
    draw_debt(accounts,OrdinalDay{1},{"bridge",Cents{50},BasisPoints{0},OrdinalDay{1},OrdinalDay{100},0,{}},"Bridge financing");
    settle_invoice(accounts,OrdinalDay{1},"bill",Cents{50},Cents{0},"pay.remaining");refresh_liquidity(state,accounts);
    check(state.status==LiquidityStatus::operating && state.workouts.front().resolved_on==OrdinalDay{1} &&
          state.exposure.unpaid_payables==Cents{0},"Financing plus actual payment failed to resolve the workout");
    check(accounts_report(accounts).revenue==Cents{0} && accounts_report(accounts).debt==Cents{50},
          "Liquidity rescue financing must remain distinct from product revenue");
    issue_invoice(accounts,OrdinalDay{1},{"later",InvoiceKind::payable,OrdinalDay{1},Cents{1},Cents{0},"Later independent bill"});
    close_day(accounts,state,OrdinalDay{1});
    check(state.workouts.size()==2 && state.workouts.back().warned_on==OrdinalDay{2} && state.workouts.back().deadline==OrdinalDay{32},
          "A later genuinely unpaid obligation must start a new independently identified workout");
    validate_liquidity_links(state,accounts);
}
void exact_deadline_and_protected_cash() {
    auto accounts=opening_accounts(Cents{100});auto state=opening_liquidity();
    issue_invoice(accounts,OrdinalDay{0},{"bill",InvoiceKind::payable,OrdinalDay{0},Cents{100},Cents{0},"Reserved cash cannot pay twice"});
    close_day(accounts,state,OrdinalDay{0},Cents{100});
    check(state.exposure.unpaid_payables==Cents{100} && accounts_report(accounts).cash==Cents{100},
          "Protected cash cannot silently fund due settlement");
    acknowledge_liquidity_warning(state,1);
    for (std::int64_t day=1;day<31;++day) close_day(accounts,state,OrdinalDay{day},Cents{100});
    check(state.status==LiquidityStatus::workout && state.observed_on==OrdinalDay{31},
          "The workout must allow a rescue command on the deadline morning");
    auto rescued_accounts=accounts;auto rescued_state=state;
    settle_invoice(rescued_accounts,OrdinalDay{31},"bill",Cents{100},Cents{0},"deadline.payment");
    refresh_liquidity(rescued_state,rescued_accounts);
    check(rescued_state.status==LiquidityStatus::operating && rescued_state.workouts.front().resolved_on==OrdinalDay{31},
          "Last-day legitimate payment must prevent closure");
    const auto accounts_before_closure=accounts;
    close_liquidity_workout(state,accounts);
    check(state.status==LiquidityStatus::closed && state.observed_on==OrdinalDay{31} && state.workouts.front().closed_on==OrdinalDay{31},
          "Unresolved workout must close at its exact deadline before another service interval");
    check(accounts==accounts_before_closure && state.checked_through==OrdinalDay{30},
          "Closure must preserve exactly 30 completed response intervals without new accounting work");
    const auto final=state;
    refuses_unchanged(state,[&]{acknowledge_liquidity_warning(state,1);});
    check(state==final && accounts_report(accounts).payables==Cents{100},"Closure must retain debt and causal financial history");
}
void debts_and_solvent_losses() {
    auto accounts=opening_accounts(Cents{1000});auto state=opening_liquidity();
    record_cash(accounts,OrdinalDay{0},Cents{-100},Account::expenses,"Loss without missed obligations","loss");
    close_day(accounts,state,OrdinalDay{0});
    check(state.status==LiquidityStatus::operating && accounts_report(accounts).profit==Cents{-100},
          "Negative profit alone must not start an insolvency workout");
    draw_debt(accounts,OrdinalDay{1},{"maturing",Cents{200},BasisPoints{0},OrdinalDay{1},OrdinalDay{2},0,{}},"Scheduled principal");
    close_day(accounts,state,OrdinalDay{1});
    check(state.status==LiquidityStatus::operating,"Future principal was treated as already unpaid");
    close_day(accounts,state,OrdinalDay{2});
    check(state.status==LiquidityStatus::workout && state.exposure.matured_principal==Cents{200} &&
          state.exposure.oldest->kind==LiquidityCauseKind::matured_debt,"Matured unserviced principal must produce its own causal warning");
    repay_debt(accounts,OrdinalDay{3},"maturing",Cents{200},Cents{0},"principal.pay");refresh_liquidity(state,accounts);
    check(state.status==LiquidityStatus::operating && accounts_report(accounts).debt==Cents{0},"Actual principal repayment failed to cure the workout");
    auto bad=state;bad.workouts.front().deadline=OrdinalDay{34};
    bool rejected=false;try {validate_liquidity(bad);} catch (const std::exception&) {rejected=true;}
    check(rejected,"A corrupted workout deadline was accepted");
}
}
int main() {
    try {warning_and_recovery();exact_deadline_and_protected_cash();debts_and_solvent_losses();}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<checks<<" liquidity checks passed\n";
}
