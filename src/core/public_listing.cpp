#include "governance_internal.hpp"

namespace pgp::core {
namespace {
using namespace governance_detail;

auto postings_from(const AccountsState& accounts,OrdinalDay day) {
    return std::ranges::lower_bound(accounts.postings,day,{},&Posting::day);
}

PostingTotals period_totals(const AccountsState& accounts,OrdinalDay begin,OrdinalDay end) {
    PostingTotals totals;
    const auto stop=postings_from(accounts,end);
    for (auto posting=postings_from(accounts,begin);posting!=stop;++posting) totals.add(*posting);
    return totals;
}

Cents balance(const PostingTotals& totals,Account account) {
    return totals.balances[static_cast<std::size_t>(account)];
}

OrdinalDay next_quarter(OrdinalDay begin) {
    const auto date=from_ordinal(begin);
    const auto next_month=1U+3U*((date.month-1U)/3U+1U);
    return to_ordinal(next_month==13U?CivilDate{date.year+1,1,1}:CivilDate{date.year,next_month,1});
}
}

ListingQualification listing_qualification(const SimulationState& state,OrdinalDay day) {
    const auto& rules=catalog(state).public_listing;
    if (!rules) invalid("This campaign has no public listing terms");
    const auto& accounts=*state.accounts;
    ListingQualification result;
    result.eligible_on=advance_days(accounts.starts_on,rules->minimum_age);
    result.old_enough=day>=result.eligible_on;
    if (!result.old_enough) return result;
    const auto totals=period_totals(accounts,OrdinalDay{checked_subtract(day.value(),rules->performance_days.value())},day);
    result.revenue=balance(totals,Account::revenue);
    result.profit=checked_unit_subtract(result.revenue,balance(totals,Account::expenses));
    result.qualified=result.revenue>=rules->minimum_revenue && result.profit>=rules->minimum_profit;
    return result;
}

std::optional<OrdinalDay> public_listing_date(const SimulationState& state) {
    if (!state.management || !state.management->governance || !state.management->catalog.governance ||
        !state.management->catalog.governance->public_listing) return std::nullopt;
    const auto& rules=*state.management->catalog.governance->public_listing;
    for (const auto& motion:state.management->governance->motions) {
        if (motion.kind==BoardMotionKind::financing && motion.executed && motion.subject==rules.offer_id)
            return motion.resolved_on;
    }
    return std::nullopt;
}

const PublicFiling* latest_public_filing(const SimulationState& state,OrdinalDay day) {
    if (!state.management || !state.management->governance) return nullptr;
    const auto& filings=state.management->governance->public_filings;
    const auto upper=std::ranges::upper_bound(filings,day,{},&PublicFiling::ends_before);
    return upper==filings.begin()?nullptr:&*std::prev(upper);
}

void close_public_reporting_day(SimulationState& state) {
    if (!state.management || !state.management->governance || !state.management->catalog.governance ||
        !state.management->catalog.governance->public_listing) return;
    const auto end=advance_days(state.date,Days{1});
    const auto boundary=from_ordinal(end);
    if (boundary.day!=1 || (boundary.month-1)%3!=0) return;
    const auto listed=public_listing_date(state);
    if (!listed) return;
    auto& filings=state.management->governance->public_filings;
    const auto begin=filings.empty()?*listed:filings.back().ends_before;
    if (next_quarter(begin)!=end || filings.size()>=public_filing_limit) invalid("Public filing calendar exceeds its ordered reporting window");
    const auto totals=period_totals(*state.accounts,begin,end);
    filings.push_back({begin,end,balance(totals,Account::revenue),balance(totals,Account::expenses),state.cash});
}

namespace governance_detail {
void validate_public_filings(const SimulationState& state) {
    const auto& filings=state.management->governance->public_filings;
    const auto listed=public_listing_date(state);
    if (!listed) {
        if (!filings.empty()) invalid("Public filings require an approved public listing");
        return;
    }
    const auto& accounts=*state.accounts;
    const auto closed=accounts.closed_through?advance_days(*accounts.closed_through,Days{1}):accounts.starts_on;
    // Each posting in a reported period is visited once. The pre-listing
    // balance is reconciled once and then carried between consecutive filings.
    if (filings.empty() && next_quarter(*listed)>closed) return;
    auto begin=*listed;
    auto cash=balance(period_totals(accounts,accounts.starts_on,begin),Account::cash);
    for (const auto& filing:filings) {
        const auto end=next_quarter(begin);
        if (end>closed || filing.starts_on!=begin || filing.ends_before!=end)
            invalid("Public filing is missing a closed calendar quarter or lists future results");
        const auto totals=period_totals(accounts,begin,end);
        cash=checked_unit_add(cash,balance(totals,Account::cash));
        if (filing.ending_cash!=cash || filing.revenue!=balance(totals,Account::revenue) ||
            filing.expenses!=balance(totals,Account::expenses))
            invalid("Public filing differs from its settled ledger");
        begin=end;
    }
    if (next_quarter(begin)<=closed) invalid("A settled public quarter has no filing");
}
}
}
