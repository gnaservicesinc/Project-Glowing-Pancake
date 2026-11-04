#include <pgp/core/market.hpp>
#include <pgp/core/allocation.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void text_valid(std::string_view text, std::size_t maximum = 100) {
    if (text.empty() || text.size() > maximum || std::any_of(text.begin(), text.end(),
        [](unsigned char c) { return c < 32 || c == 127; })) invalid("Market text must be nonempty, bounded and contain no control characters");
}
OrdinalDay current_day(const MarketState& state) {
    return state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
}
void require_day(const MarketState& state, OrdinalDay day) {
    if (current_day(state) != day) invalid("Market operation must name the current unclosed day");
}
template<class Row> const Row& find_id(const std::vector<Row>& rows, std::string_view id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.id == id; });
    if (found == rows.end()) invalid("Market reference does not exist");
    return *found;
}
const CapacityProject& project(const CapacityState& state, const MarketProduct& product) {
    const auto& row = find_id(state.projects, product.capacity_project_id);
    if (row.kind != ProjectKind::serving) invalid("Market product requires a serving capacity project");
    return row;
}
bool available(const MarketProduct& product, const CapacityState& capacity) {
    return product.available && project(capacity, product).status == ProjectStatus::active;
}
const MarketPreference* preference(const MarketSegment& segment, std::string_view product_id) {
    const auto found = std::find_if(segment.preferences.begin(), segment.preferences.end(),
        [&](const auto& row) { return row.product_id == product_id; });
    return found == segment.preferences.end() ? nullptr : &*found;
}
OrdinalDay month_start(OrdinalDay day) {
    const auto civil = from_ordinal(day);
    return to_ordinal({civil.year, civil.month, 1});
}
bool month_end(OrdinalDay day) {
    const auto civil = from_ordinal(day);
    return civil.day == days_in_month(civil.year, civil.month);
}
Cents multiplied(Cents amount, std::uint64_t count) {
    return Cents{mul_div_floor(static_cast<std::uint64_t>(amount.value()), count, 1).quotient};
}
Cents daily_fee(const MarketContract& contract, OrdinalDay day) {
    if (!contract.started_on || day < *contract.started_on || day >= contract.ends_before) return Cents{0};
    const auto begin = std::max(month_start(day), *contract.started_on);
    const auto civil = from_ordinal(day);
    const auto denominator = days_in_month(civil.year, civil.month);
    const auto elapsed = static_cast<std::uint64_t>(day.value() - begin.value());
    const auto before = mul_div_floor(static_cast<std::uint64_t>(contract.monthly_fee.value()), elapsed, denominator).quotient;
    const auto after = mul_div_floor(static_cast<std::uint64_t>(contract.monthly_fee.value()), elapsed + 1, denominator).quotient;
    return Cents{after - before};
}
Cents credit(const MarketContract& contract, Cents fee, std::uint64_t missing) {
    const std::uint64_t factors[]{static_cast<std::uint64_t>(fee.value()), missing,
                                 static_cast<std::uint64_t>(contract.service_credit_basis_points.value())};
    return Cents{mul_div_floor(factors, contract.tasks_per_day * 10000).quotient};
}
std::string invoice_id(std::string_view kind, std::string_view id, OrdinalDay day) {
    return "market." + std::string(kind) + "/" + std::string(id) + "/" + iso_date(day);
}
void invoice(AccountsState& accounts, OrdinalDay day, std::string id, InvoiceKind kind,
             Cents amount, std::string cause) {
    if (amount.value() == 0) return;
    issue_invoice(accounts, day, {std::move(id), kind, day, amount, Cents{0}, std::move(cause)});
}
Cents net_charge(const ContractDayResult& result) {
    return checked_unit_add(checked_unit_subtract(result.gross_fee, result.service_credit), result.usage_charge);
}
std::uint64_t weight(std::uint64_t utility, std::uint64_t threshold) {
    const auto positive = utility > threshold ? utility - threshold : 0;
    return mul_div_floor(positive, positive, 1).quotient;
}
std::uint64_t installed(const MarketState& state, std::string_view segment_id, std::string_view product_id) {
    for (auto adoption = state.adoptions.rbegin(); adoption != state.adoptions.rend(); ++adoption) {
        if (adoption->segment_id != segment_id) continue;
        const auto found = std::find_if(adoption->shares.begin(), adoption->shares.end(),
            [&](const auto& row) { return row.product_id == product_id; });
        return found == adoption->shares.end() ? 0 : static_cast<std::uint64_t>(found->share.value());
    }
    return product_id == "outside" ? 10000 : 0;
}
void review_adoption(MarketState& state, OrdinalDay day) {
    const auto begin = month_start(day);
    for (const auto& segment : state.segments) {
        std::map<std::string, std::uint64_t> observed;
        observed["outside"] = 0;
        std::uint64_t total = 0;
        for (const auto& record : state.days) {
            if (record.day < begin || record.day > day) continue;
            const auto found = std::find_if(record.segments.begin(), record.segments.end(),
                [&](const auto& row) { return row.segment_id == segment.id; });
            if (found == record.segments.end()) continue;
            total += found->demand;
            observed["outside"] += found->outside_tasks;
            for (const auto& result : record.contracts) {
                const auto& contract = find_id(state.contracts, result.contract_id);
                if (contract.segment_id == segment.id) observed[contract.product_id] += result.delivered_tasks;
            }
            for (const auto& delivery : record.discretionary)
                if (delivery.segment_id == segment.id) observed[delivery.product_id] += delivery.tasks;
        }
        if (total == 0) continue;
        std::vector<WeightedQuota> quotas;
        for (const auto& [id, tasks] : observed) quotas.push_back({id, tasks});
        MarketAdoption adoption{day, segment.id, total, {}};
        for (const auto& share : apportion_units(10000, quotas))
            adoption.shares.push_back({share.id, BasisPoints{share.units}});
        state.adoptions.push_back(std::move(adoption));
    }
}
template<class Operation> void mutate(MarketState& state, Operation operation) {
    validate_market(state);
    auto next = state;
    operation(next);
    validate_market(next);
    state = std::move(next);
}
std::uint64_t contract_supply(const CapacityDay& plan, std::string_view project_id) {
    const auto found = std::find_if(plan.projects.begin(), plan.projects.end(),
        [&](const auto& row) { return row.project_id == project_id; });
    return found == plan.projects.end() ? 0 : found->protected_tasks;
}
std::uint64_t total_supply(const CapacityDay& plan, std::string_view project_id) {
    const auto found = std::find_if(plan.projects.begin(), plan.projects.end(),
        [&](const auto& row) { return row.project_id == project_id; });
    return found == plan.projects.end() ? 0 : found->useful_tasks;
}
} // namespace

MarketState opening_market(OrdinalDay starts_on, std::string owner) {
    MarketState state;
    state.starts_on = starts_on; state.owner = std::move(owner);
    validate_market(state);
    return state;
}
MarketReport market_report(const MarketState& state) {
    MarketReport report;
    for (const auto& day : state.days) {
        for (const auto& row : day.segments) {
            report.delivered_tasks += row.contract_tasks + row.discretionary_tasks;
            report.outside_tasks += row.outside_tasks;
            report.customer_charges = checked_unit_add(report.customer_charges, row.charged);
        }
        for (const auto& row : day.contracts) {
            report.service_credits = checked_unit_add(report.service_credits, row.service_credit);
            report.penalties = checked_unit_add(report.penalties, row.penalty);
        }
    }
    for (const auto& row : state.contracts) {
        if (row.status == ContractStatus::active) ++report.active_contracts;
        if (row.status == ContractStatus::cancelled)
            report.penalties = checked_unit_add(report.penalties, row.cancellation_penalty);
    }
    return report;
}
void validate_market(const MarketState& state) {
    text_valid(state.owner, 128); (void)from_ordinal(state.starts_on);
    if (state.products.size() > market_product_limit || state.segments.size() > market_segment_limit ||
        state.contracts.size() > market_contract_limit || state.days.size() > market_day_limit ||
        state.adoptions.size() > market_record_limit) invalid("Market state exceeds deterministic catalog/history bounds");
    if (state.closed_through) {
        (void)from_ordinal(*state.closed_through);
        if (*state.closed_through < state.starts_on || state.days.size() !=
            static_cast<std::uint64_t>(state.closed_through->value() - state.starts_on.value() + 1))
            invalid("Market daily close cursor differs from complete history");
    } else if (!state.days.empty()) invalid("Market daily history has no close cursor");
    const auto latest = current_day(state);
    std::set<std::string> ids, projects;
    for (const auto& product : state.products) {
        text_valid(product.id); text_valid(product.capacity_project_id);
        if (product.id == "outside" || !ids.insert(product.id).second || !projects.insert(product.capacity_project_id).second ||
            product.price_per_task.value() < 0 || product.price_per_task.value() > market_price_limit)
            invalid("Product has duplicate identity/source or an invalid task price");
    }
    ids.clear();
    for (const auto& segment : state.segments) {
        text_valid(segment.id);
        if (!ids.insert(segment.id).second || segment.daily_tasks > market_task_limit || segment.daily_budget.value() < 0 ||
            segment.daily_budget.value() > 100'000'000'000'000'000LL || segment.outside_utility > market_utility_limit ||
            segment.reservation_threshold > market_utility_limit || segment.preferences.size() > market_product_limit)
            invalid("Segment demand, budget, utility or catalog bounds are invalid");
        std::set<std::string> products_seen;
        for (const auto& pref : segment.preferences) {
            (void)find_id(state.products, pref.product_id);
            if (!products_seen.insert(pref.product_id).second || pref.utility > market_utility_limit ||
                pref.switching_cost > market_utility_limit || pref.willingness_to_pay.value() < 0 ||
                pref.willingness_to_pay.value() > market_price_limit)
                invalid("Segment preference has duplicate product, invalid utility or willingness to pay");
        }
    }
    ids.clear();
    for (const auto& contract : state.contracts) {
        text_valid(contract.id); text_valid(contract.cause, 256);
        (void)find_id(state.segments, contract.segment_id); (void)find_id(state.products, contract.product_id);
        (void)from_ordinal(contract.accepted_on); (void)from_ordinal(contract.start_by); (void)from_ordinal(contract.ends_before);
        if (!ids.insert(contract.id).second || contract.tasks_per_day == 0 || contract.tasks_per_day > market_task_limit ||
            contract.monthly_fee.value() < 0 || contract.monthly_fee.value() > 100'000'000'000'000'000LL ||
            contract.usage_price.value() < 0 || contract.usage_price.value() > market_price_limit ||
            contract.missing_task_penalty.value() < 0 || contract.missing_task_penalty.value() > market_price_limit ||
            contract.missed_start_penalty.value() < 0 || contract.cancellation_penalty.value() < 0 ||
            contract.accepted_on < state.starts_on || contract.accepted_on > latest || contract.start_by < contract.accepted_on ||
            contract.ends_before <= contract.start_by)
            invalid("Contract identity, price, demand or deadline is invalid");
        if (contract.started_on) {
            (void)from_ordinal(*contract.started_on);
            if (*contract.started_on < contract.accepted_on || *contract.started_on > contract.start_by || *contract.started_on > latest)
                invalid("Contract activation lies outside its acceptance/deadline");
        }
        if (contract.stopped_on) {
            (void)from_ordinal(*contract.stopped_on);
            if (*contract.stopped_on < contract.accepted_on || *contract.stopped_on > latest ||
                (contract.started_on && *contract.stopped_on < *contract.started_on)) invalid("Contract stop cursor is invalid");
        }
        switch (contract.status) {
        case ContractStatus::pending:
            if (contract.started_on || contract.stopped_on || (state.closed_through && *state.closed_through >= contract.start_by))
                invalid("Pending contract has prior work or an expired start deadline");
            break;
        case ContractStatus::active:
            if (!contract.started_on || contract.stopped_on || latest >= contract.ends_before)
                invalid("Active contract has no activation or has passed its exclusive end");
            break;
        case ContractStatus::completed:
            if (!contract.started_on || contract.stopped_on != std::optional{contract.ends_before})
                invalid("Completed contract does not preserve its exclusive end");
            break;
        case ContractStatus::missed:
            if (contract.started_on || contract.stopped_on != std::optional{contract.start_by})
                invalid("Missed contract has an invalid deadline cursor");
            break;
        case ContractStatus::cancelled:
            if (!contract.stopped_on) invalid("Cancelled contract has no stop cursor");
            break;
        default: invalid("Unknown contract status");
        }
    }
    std::size_t records = 0;
    std::map<std::string, std::size_t> contract_days;
    std::set<std::string> seen_segments;
    for (std::size_t index = 0; index < state.days.size(); ++index) {
        const auto& day = state.days[index];
        if (day.day.value() != checked_add(state.starts_on.value(), static_cast<std::int64_t>(index)) ||
            day.contracts.size() > market_contract_limit || day.discretionary.size() > market_product_limit * market_segment_limit ||
            day.segments.size() > market_segment_limit || day.allocation_rounds > market_product_limit + 2 * market_product_limit * market_segment_limit + 1)
            invalid("Market day is out of order or exceeds finite allocation bounds");
        records += day.contracts.size() + day.discretionary.size() + day.segments.size();
        if (records > market_record_limit) invalid("Market required records exceed their deterministic bound");
        std::map<std::string, std::uint64_t> delivered_contract, delivered_discretionary, promised;
        std::map<std::string, Cents> charges;
        std::set<std::string> daily_contracts;
        for (const auto& result : day.contracts) {
            const auto& contract = find_id(state.contracts, result.contract_id);
            if (!daily_contracts.insert(result.contract_id).second) invalid("Duplicate daily contract result");
            const bool missed = contract.status == ContractStatus::missed && day.day == contract.start_by;
            if (missed) {
                if (result.promised_tasks != 0 || result.delivered_tasks != 0 || result.gross_fee != Cents{0} ||
                    result.service_credit != Cents{0} || result.usage_charge != Cents{0} || result.penalty != contract.missed_start_penalty)
                    invalid("Missed-start consequence differs from accepted terms");
            } else {
                if (!contract.started_on || day.day < *contract.started_on || day.day >= contract.ends_before ||
                    (contract.status == ContractStatus::cancelled && day.day >= *contract.stopped_on) ||
                    result.promised_tasks != contract.tasks_per_day || result.delivered_tasks > result.promised_tasks ||
                    result.gross_fee != daily_fee(contract, day.day) ||
                    result.service_credit != credit(contract, result.gross_fee, result.promised_tasks - result.delivered_tasks) ||
                    result.usage_charge != multiplied(contract.usage_price, result.delivered_tasks) ||
                    result.penalty != multiplied(contract.missing_task_penalty, result.promised_tasks - result.delivered_tasks))
                    invalid("Contract delivery, service credit or charge differs from its accepted terms");
                promised[contract.segment_id] += result.promised_tasks;
                delivered_contract[contract.segment_id] += result.delivered_tasks;
                auto [found, fresh] = charges.try_emplace(contract.segment_id, Cents{0}); (void)fresh;
                found->second = checked_unit_add(found->second, net_charge(result));
            }
            ++contract_days[contract.id];
        }
        std::set<std::pair<std::string, std::string>> delivery_ids;
        for (const auto& delivery : day.discretionary) {
            const auto& product_row = find_id(state.products, delivery.product_id);
            const auto& segment = find_id(state.segments, delivery.segment_id);
            const auto* pref = preference(segment, product_row.id);
            if (!delivery_ids.emplace(delivery.segment_id, delivery.product_id).second || delivery.tasks == 0 ||
                delivery.tasks > market_task_limit || delivery.charged != multiplied(product_row.price_per_task, delivery.tasks) ||
                !pref || !pref->eligible || product_row.price_per_task > pref->willingness_to_pay)
                invalid("Discretionary delivery has invalid eligibility, identity, tasks or price");
            delivered_discretionary[segment.id] += delivery.tasks;
            auto [found, fresh] = charges.try_emplace(segment.id, Cents{0}); (void)fresh;
            found->second = checked_unit_add(found->second, delivery.charged);
        }
        std::set<std::string> daily_segments;
        for (const auto& row : day.segments) {
            const auto& segment = find_id(state.segments, row.segment_id);
            if (!daily_segments.insert(segment.id).second || row.demand != segment.daily_tasks || row.budget != segment.daily_budget ||
                row.contract_tasks != delivered_contract[segment.id] || row.discretionary_tasks != delivered_discretionary[segment.id] ||
                row.contract_tasks > row.demand || row.discretionary_tasks > row.demand || row.outside_tasks > row.demand ||
                row.contract_tasks + row.discretionary_tasks + row.outside_tasks != row.demand || promised[segment.id] > row.demand ||
                row.charged != (charges.contains(segment.id) ? charges.at(segment.id) : Cents{0}) || row.charged > row.budget ||
                row.charged.value() < 0 || row.unspent != checked_unit_subtract(row.budget, row.charged))
                invalid("Segment report loses demand, double spends budget or differs from actual work");
            text_valid(row.outside_reason, 256);
        }
        for (const auto& id : seen_segments)
            if (!daily_segments.contains(id)) invalid("An introduced segment disappeared from daily market history");
        seen_segments.insert(daily_segments.begin(), daily_segments.end());
        for (const auto& [id, count] : delivered_contract) if (count && !daily_segments.contains(id)) invalid("Contract work has no segment report");
        for (const auto& [id, count] : delivered_discretionary) if (count && !daily_segments.contains(id)) invalid("Customer work has no segment report");
    }
    for (const auto& contract : state.contracts) {
        std::uint64_t expected = contract.status == ContractStatus::missed ? 1 : 0;
        if (contract.started_on && state.closed_through) {
            auto end = std::min(advance_days(*state.closed_through, Days{1}), contract.ends_before);
            if (contract.status == ContractStatus::cancelled) end = std::min(end, *contract.stopped_on);
            if (end > *contract.started_on) expected = static_cast<std::uint64_t>(end.value() - contract.started_on->value());
        }
        if (contract_days[contract.id] != expected) invalid("Contract committed day coverage is incomplete");
    }
    std::set<std::pair<OrdinalDay, std::string>> adoption_ids;
    for (const auto& adoption : state.adoptions) {
        (void)find_id(state.segments, adoption.segment_id);
        if (!state.closed_through || adoption.reviewed_on > *state.closed_through || adoption.reviewed_on < state.starts_on ||
            !month_end(adoption.reviewed_on) || !adoption_ids.emplace(adoption.reviewed_on, adoption.segment_id).second ||
            adoption.observed_tasks == 0 || adoption.shares.empty() || adoption.shares.size() > market_product_limit + 1)
            invalid("Installed share review has an invalid identity, month or count");
        std::int64_t total = 0;
        std::set<std::string> share_ids;
        for (const auto& row : adoption.shares) {
            if (row.product_id != "outside") (void)find_id(state.products, row.product_id);
            if (!share_ids.insert(row.product_id).second) invalid("Duplicate installed product share");
            total = checked_add(total, row.share.value());
        }
        if (total != 10000 || !share_ids.contains("outside")) invalid("Installed shares must conserve all demand including outside");
    }
    // Recompute monthly adoption from actual useful work, without replaying
    // eligibility or reading current product availability as historical truth.
    MarketState reviewed = state; reviewed.adoptions.clear();
    for (const auto& day : state.days) if (month_end(day.day)) review_adoption(reviewed, day.day);
    if (reviewed.adoptions != state.adoptions) invalid("Installed monthly shares differ from committed useful-work history");
    (void)market_report(state);
}

void validate_market_links(const MarketState& state, const AccountsState& accounts, const CapacityState& capacity) {
    validate_market(state); validate_accounts(accounts); validate_capacity(capacity);
    const auto account_day = accounts.closed_through ? advance_days(*accounts.closed_through, Days{1}) : accounts.starts_on;
    const auto capacity_day = capacity.closed_through ? advance_days(*capacity.closed_through, Days{1}) : capacity.starts_on;
    if (state.owner != accounts.owner || state.owner != capacity.owner || state.starts_on < accounts.starts_on ||
        state.starts_on < capacity.starts_on || current_day(state) != account_day || current_day(state) != capacity_day)
        invalid("Market, accounts and capacity must share owner and current unclosed day");
    for (const auto& product_row : state.products) (void)project(capacity, product_row);
    struct ExpectedInvoice { InvoiceKind kind; Cents amount; OrdinalDay due; };
    std::map<std::string, ExpectedInvoice> expected;
    const auto expect = [&](std::string id, InvoiceKind kind, Cents amount, OrdinalDay day) {
        if (amount.value() > 0 && !expected.emplace(std::move(id), ExpectedInvoice{kind, amount, day}).second)
            invalid("Market consequences repeat an invoice identity");
    };
    for (const auto& day : state.days) {
        std::map<std::string, std::uint64_t> delivered, protected_delivered;
        for (const auto& result : day.contracts) {
            const auto& contract = find_id(state.contracts, result.contract_id);
            delivered[contract.product_id] += result.delivered_tasks;
            protected_delivered[contract.product_id] += result.delivered_tasks;
            expect(invoice_id("contract", contract.id, day.day), InvoiceKind::receivable, net_charge(result), day.day);
            expect(invoice_id("penalty", contract.id, day.day), InvoiceKind::payable, result.penalty, day.day);
        }
        for (const auto& result : day.discretionary) {
            delivered[result.product_id] += result.tasks;
            expect(invoice_id("sale", result.segment_id + "/" + result.product_id, day.day),
                   InvoiceKind::receivable, result.charged, day.day);
        }
        const auto record = std::find_if(capacity.days.begin(), capacity.days.end(),
            [&](const auto& row) { return row.day == day.day; });
        if (record == capacity.days.end()) invalid("Market delivery has no matching capacity day");
        for (const auto& product_row : state.products) {
            const auto work = std::find_if(record->projects.begin(), record->projects.end(),
                [&](const auto& row) { return row.project_id == product_row.capacity_project_id; });
            const auto actual_tasks = work == record->projects.end() ? 0 : work->useful_tasks;
            const auto actual_protected = work == record->projects.end() ? 0 : work->protected_tasks;
            if (actual_tasks != delivered[product_row.id] || actual_protected != protected_delivered[product_row.id])
                invalid("Market useful work differs from actual serving or contract priority");
        }
    }
    for (const auto& contract : state.contracts)
        if (contract.status == ContractStatus::cancelled)
            expect(invoice_id("cancel", contract.id, *contract.stopped_on), InvoiceKind::payable,
                   contract.cancellation_penalty, *contract.stopped_on);
    for (const auto& row : accounts.invoices) {
        if (!row.id.starts_with("market.")) continue;
        const auto found = expected.find(row.id);
        if (found == expected.end() || row.kind != found->second.kind || row.amount != found->second.amount || row.due != found->second.due)
            invalid("Market invoice differs from actual billed work, refund or penalty");
        expected.erase(found);
    }
    if (!expected.empty()) invalid("Market work is missing a recognized invoice");
}
void add_market_product(MarketState& state, const CapacityState& capacity, MarketProduct row) {
    mutate(state, [&](auto& next) {
        (void)project(capacity, row);
        next.products.push_back(std::move(row));
        std::sort(next.products.begin(), next.products.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void add_market_segment(MarketState& state, MarketSegment row) {
    mutate(state, [&](auto& next) {
        std::sort(row.preferences.begin(), row.preferences.end(), [](const auto& a, const auto& b) { return a.product_id < b.product_id; });
        next.segments.push_back(std::move(row));
        std::sort(next.segments.begin(), next.segments.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void set_product_available(MarketState& state, std::string_view product_id, bool value) {
    mutate(state, [&](auto& next) {
        const auto found = std::find_if(next.products.begin(), next.products.end(), [&](const auto& row) { return row.id == product_id; });
        if (found == next.products.end()) invalid("Product availability refers to an unknown product");
        found->available = value;
    });
}
void accept_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, MarketContract contract) {
    mutate(state, [&](auto& next) {
        require_day(next, day);
        if (contract.status != ContractStatus::pending || contract.accepted_on != day || contract.started_on || contract.stopped_on)
            invalid("A newly accepted contract must record today's acceptance without prior delivery state");
        const auto& segment = find_id(next.segments, contract.segment_id);
        const auto& product_row = find_id(next.products, contract.product_id);
        const auto* pref = preference(segment, product_row.id);
        if (!pref || !pref->eligible || contract.usage_price > pref->willingness_to_pay)
            invalid("Customer contract fails eligibility or agreed usage-price willingness");
        next.contracts.push_back(std::move(contract));
        std::map<std::string, std::uint64_t> segment_tasks, product_tasks;
        std::map<std::string, Cents> segment_cost;
        for (const auto& row : next.contracts) {
            if (row.status != ContractStatus::pending && row.status != ContractStatus::active) continue;
            segment_tasks[row.segment_id] += row.tasks_per_day;
            product_tasks[row.product_id] += row.tasks_per_day;
            const auto worst_fee = Cents{row.monthly_fee.value() / 28 + (row.monthly_fee.value() % 28 != 0 ? 1 : 0)};
            auto [cost, fresh] = segment_cost.try_emplace(row.segment_id, Cents{0}); (void)fresh;
            cost->second = checked_unit_add(cost->second, checked_unit_add(worst_fee, multiplied(row.usage_price, row.tasks_per_day)));
        }
        for (const auto& [id, tasks] : segment_tasks) {
            const auto& customer = find_id(next.segments, id);
            if (tasks > customer.daily_tasks || segment_cost.at(id) > customer.daily_budget)
                invalid("Accepted contracts exceed the same customer's task demand or daily budget");
        }
        for (const auto& [id, tasks] : product_tasks)
            if (tasks > project(capacity, find_id(next.products, id)).tasks_per_day)
                invalid("Accepted contract tasks exceed the product's committed serving profile");
        std::sort(next.contracts.begin(), next.contracts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void start_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, std::string_view contract_id) {
    mutate(state, [&](auto& next) {
        require_day(next, day);
        const auto found = std::find_if(next.contracts.begin(), next.contracts.end(), [&](const auto& row) { return row.id == contract_id; });
        if (found == next.contracts.end() || found->status != ContractStatus::pending || day > found->start_by ||
            !available(find_id(next.products, found->product_id), capacity))
            invalid("Contract cannot start without an eligible available product before its deadline");
        found->started_on = day; found->status = ContractStatus::active;
    });
}
void cancel_contract(MarketState& state, AccountsState& accounts, OrdinalDay day, std::string_view contract_id) {
    validate_market(state); validate_accounts(accounts); require_day(state, day);
    auto next = state; auto ledger = accounts;
    const auto found = std::find_if(next.contracts.begin(), next.contracts.end(), [&](const auto& row) { return row.id == contract_id; });
    if (found == next.contracts.end() || (found->status != ContractStatus::pending && found->status != ContractStatus::active))
        invalid("Only a pending or active customer contract can be cancelled");
    found->status = ContractStatus::cancelled; found->stopped_on = day;
    invoice(ledger, day, invoice_id("cancel", found->id, day), InvoiceKind::payable,
            found->cancellation_penalty, found->cause);
    validate_market(next); validate_accounts(ledger);
    state = std::move(next); accounts = std::move(ledger);
}
void close_market_day(MarketState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day) {
    validate_market_links(state, accounts, capacity); require_day(state, day);
    auto next = state; auto ledger = accounts; auto pool = capacity;
    std::set<std::string> linked;
    for (const auto& product_row : next.products) linked.insert(product_row.capacity_project_id);
    for (const auto& row : pool.projects)
        if (row.kind == ProjectKind::serving && row.status == ProjectStatus::active && !linked.contains(row.id))
            invalid("Every active serving workload must be linked before market work advances");
    MarketDay record; record.day = day;
    std::map<std::string, std::uint64_t> requested_contracts;
    std::map<std::string, std::vector<WeightedQuota>> product_contracts;
    for (auto& row : next.contracts) {
        if (row.status == ContractStatus::pending && day >= row.start_by) {
            row.status = ContractStatus::missed; row.stopped_on = row.start_by;
            record.contracts.push_back({row.id, 0, 0, Cents{0}, Cents{0}, Cents{0}, row.missed_start_penalty});
            invoice(ledger, day, invoice_id("penalty", row.id, day), InvoiceKind::payable, row.missed_start_penalty, row.cause);
        } else if (row.status == ContractStatus::active) {
            requested_contracts[row.product_id] += row.tasks_per_day;
            product_contracts[row.product_id].push_back({row.id, row.tasks_per_day});
        }
    }
    std::vector<ServingRequest> full_requests;
    for (const auto& row : next.products) {
        const auto& profile = project(pool, row);
        if (profile.status != ProjectStatus::active) continue;
        const auto contracts = requested_contracts[row.id];
        if (contracts > profile.tasks_per_day) invalid("Contract tasks exceed the bound serving profile");
        const bool enabled = available(row, pool);
        full_requests.push_back({profile.id, enabled ? contracts : 0, enabled ? profile.tasks_per_day - contracts : 0});
    }
    const auto plan = preview_capacity_day(pool, day, full_requests);
    std::map<std::string, std::uint64_t> delivered_contracts;
    std::map<std::string, std::uint64_t> product_contract_delivered;
    for (const auto& [id, claims] : product_contracts) {
        const auto& product_row = find_id(next.products, id);
        const auto available_tasks = contract_supply(plan, product_row.capacity_project_id);
        for (const auto& allocation : apportion_units(std::min(available_tasks, requested_contracts[id]), claims)) {
            delivered_contracts[allocation.id] = allocation.units;
            product_contract_delivered[id] += allocation.units;
        }
    }
    std::map<std::string, std::uint64_t> segment_reserved, segment_contract_delivered;
    std::map<std::string, Cents> segment_charged;
    for (auto& row : next.contracts) {
        if (row.status != ContractStatus::active) continue;
        const auto delivered = delivered_contracts[row.id];
        const auto gross = daily_fee(row, day);
        ContractDayResult result{row.id, row.tasks_per_day, delivered, gross,
            credit(row, gross, row.tasks_per_day - delivered), multiplied(row.usage_price, delivered),
            multiplied(row.missing_task_penalty, row.tasks_per_day - delivered)};
        record.contracts.push_back(result);
        segment_reserved[row.segment_id] += row.tasks_per_day;
        segment_contract_delivered[row.segment_id] += delivered;
        auto [charged, fresh] = segment_charged.try_emplace(row.segment_id, Cents{0}); (void)fresh;
        charged->second = checked_unit_add(charged->second, net_charge(result));
        invoice(ledger, day, invoice_id("contract", row.id, day), InvoiceKind::receivable, net_charge(result), row.cause);
        invoice(ledger, day, invoice_id("penalty", row.id, day), InvoiceKind::payable, result.penalty, row.cause);
        if (advance_days(day, Days{1}) >= row.ends_before) {
            row.status = ContractStatus::completed; row.stopped_on = row.ends_before;
        }
    }
    std::vector<AllocationProduct> products;
    for (const auto& row : next.products) {
        const auto total = total_supply(plan, row.capacity_project_id);
        products.push_back({row.id, row.price_per_task, total - product_contract_delivered[row.id]});
    }
    std::vector<AllocationSegment> segments;
    for (const auto& row : next.segments) {
        const auto reserved = segment_reserved[row.id];
        const auto charged = segment_charged.contains(row.id) ? segment_charged.at(row.id) : Cents{0};
        if (reserved > row.daily_tasks || charged > row.daily_budget) invalid("Contract work exceeds the customer's single task/budget snapshot");
        AllocationSegment segment{row.id, row.daily_tasks - reserved, checked_unit_subtract(row.daily_budget, charged),
                                  weight(row.outside_utility, row.reservation_threshold), {}};
        const auto outside_share = installed(next, row.id, "outside");
        for (const auto& pref : row.preferences) {
            const auto& product_row = find_id(next.products, pref.product_id);
            if (!pref.eligible || !available(product_row, pool) || product_row.price_per_task > pref.willingness_to_pay) continue;
            const auto old_share = installed(next, row.id, product_row.id);
            const auto other_installed = 10000 - outside_share - old_share;
            const auto switching = mul_div_round_half_up(pref.switching_cost, other_installed, 10000);
            const auto utility = pref.utility > switching ? pref.utility - switching : 0;
            const auto adoption_cap = std::min<std::uint64_t>(10000, old_share + static_cast<std::uint64_t>(row.maximum_monthly_migration.value()));
            const auto limit = mul_div_floor(row.daily_tasks, adoption_cap, 10000).quotient;
            segment.edges.push_back({product_row.id, weight(utility, row.reservation_threshold), limit});
        }
        segments.push_back(std::move(segment));
    }
    const auto allocation = allocate_demand(products, segments);
    record.allocation_rounds = allocation.rounds;
    std::map<std::string, std::uint64_t> discretionary_totals;
    for (const auto& result : allocation.segments) {
        const auto& segment = find_id(next.segments, result.id);
        std::uint64_t delivered = 0;
        Cents charged = segment_charged.contains(result.id) ? segment_charged.at(result.id) : Cents{0};
        for (const auto& work : result.deliveries) {
            record.discretionary.push_back({result.id, work.product_id, work.tasks, work.cost});
            delivered += work.tasks; discretionary_totals[work.product_id] += work.tasks;
            charged = checked_unit_add(charged, work.cost);
            invoice(ledger, day, invoice_id("sale", result.id + "/" + work.product_id, day), InvoiceKind::receivable,
                    work.cost, "Customer work: " + work.product_id + " / " + result.id);
        }
        const auto missing_contract = segment_reserved[result.id] - segment_contract_delivered[result.id];
        std::string reason;
        const auto append_reason = [&](std::string_view name, std::uint64_t count) {
            if (count == 0) return;
            if (!reason.empty()) reason += ",";
            reason += name; reason += ":"; reason += std::to_string(count);
        };
        append_reason("contract_shortfall", missing_contract);
        for (const auto& outside : result.outside) {
            switch (outside.reason) {
            case AllocationOutsideReason::preference: append_reason("preference", outside.tasks); break;
            case AllocationOutsideReason::no_eligible_product: append_reason("no_eligible_product", outside.tasks); break;
            case AllocationOutsideReason::zero_utility: append_reason("zero_utility", outside.tasks); break;
            case AllocationOutsideReason::budget: append_reason("budget", outside.tasks); break;
            case AllocationOutsideReason::capacity: append_reason("capacity", outside.tasks); break;
            case AllocationOutsideReason::edge_limit: append_reason("migration_limit", outside.tasks); break;
            }
        }
        if (reason.empty()) reason = "none";
        record.segments.push_back({result.id, segment.daily_tasks, segment.daily_budget, segment_contract_delivered[result.id],
            delivered, result.outside_tasks + missing_contract, charged, result.remaining_budget, std::move(reason)});
    }
    std::vector<ServingRequest> actual_requests;
    for (const auto& row : next.products) {
        const auto& profile = project(pool, row);
        if (profile.status == ProjectStatus::active)
            actual_requests.push_back({profile.id, product_contract_delivered[row.id], discretionary_totals[row.id]});
    }
    close_capacity_day(pool, day, actual_requests);
    std::sort(record.contracts.begin(), record.contracts.end(), [](const auto& a, const auto& b) { return a.contract_id < b.contract_id; });
    std::sort(record.discretionary.begin(), record.discretionary.end(), [](const auto& a, const auto& b) {
        return std::tie(a.segment_id, a.product_id) < std::tie(b.segment_id, b.product_id);
    });
    next.days.push_back(std::move(record)); next.closed_through = day;
    if (month_end(day)) review_adoption(next, day);
    validate_market(next); validate_accounts(ledger); validate_capacity(pool);
    // Accounts intentionally remains on this day's close boundary; the parent
    // transaction settles it after all market consequences are recognized.
    state = std::move(next); accounts = std::move(ledger); capacity = std::move(pool);
}
} // namespace pgp::core
