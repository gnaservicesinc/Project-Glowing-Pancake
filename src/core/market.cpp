#include <pgp/core/market.hpp>
#include <pgp/core/allocation.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pgp::core {
namespace {
constexpr std::int64_t maximum_demand_factor=15000;
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
    if (amount.value() != 0)
        issue_market_invoice(accounts, day, {std::move(id), kind, day, amount, Cents{0}, std::move(cause)});
}
Cents net_charge(const ContractDayResult& result) {
    return checked_unit_add(checked_unit_subtract(result.gross_fee, result.service_credit), result.usage_charge);
}
std::uint64_t weight(std::uint64_t utility, std::uint64_t threshold) {
    const auto positive = utility > threshold ? utility - threshold : 0;
    return mul_div_floor(positive, positive, 1).quotient;
}
std::uint64_t installed(const MarketState& state, std::string_view segment_id, std::string_view product_id) {
    const auto recent_review = regional_market(state)
        ? std::optional{OrdinalDay{checked_subtract(month_start(current_day(state)).value(), 1)}} : std::nullopt;
    for (auto adoption = state.adoptions.rbegin(); adoption != state.adoptions.rend(); ++adoption) {
        if (recent_review && adoption->reviewed_on < *recent_review) break;
        if (adoption->segment_id != segment_id) continue;
        const auto found = std::find_if(adoption->shares.begin(), adoption->shares.end(),
            [&](const auto& row) { return row.product_id == product_id; });
        return found == adoption->shares.end() ? 0 : static_cast<std::uint64_t>(found->share.value());
    }
    return product_id == "outside" ? 10000 : 0;
}
void append_monthly_adoption(const MarketState& state, MarketAdoptionHistory& adoptions, OrdinalDay day) {
    const auto begin = month_start(day);
    const bool regional = regional_market(state);
    const auto first = std::lower_bound(state.days.begin(),state.days.end(),begin,
        [](const auto& record, auto date) { return record.day < date; });
    for (const auto& segment : state.segments) {
        std::map<std::string, std::uint64_t> observed;
        observed["outside"] = 0;
        std::uint64_t total = 0;
        for (auto entry = first; entry != state.days.end() && entry->day <= day; ++entry) {
            const auto& record = *entry;
            const auto found = std::find_if(record.segments.begin(), record.segments.end(),
                [&](const auto& row) { return row.segment_id == segment.id; });
            if (found == record.segments.end()) {
                if (regional) {
                    const auto demand = effective_market_demand(state, segment, record.day);
                    total += demand;
                    observed["outside"] += demand;
                }
                continue;
            }
            total += found->demand;
            observed["outside"] += found->outside_tasks;
            for (const auto& result : record.contracts) {
                const auto& contract = find_id(state.contracts, result.contract_id);
                if (contract.segment_id == segment.id && (!regional || result.delivered_tasks != 0))
                    observed[contract.product_id] += result.delivered_tasks;
            }
            for (const auto& delivery : record.discretionary)
                if (delivery.segment_id == segment.id) observed[delivery.product_id] += delivery.tasks;
        }
        if (total == 0 || (regional && observed.size() == 1)) continue;
        std::vector<WeightedQuota> quotas;
        for (const auto& [id, tasks] : observed) quotas.push_back({id, tasks});
        MarketAdoption adoption{day, segment.id, total, {}};
        for (const auto& share : apportion_units(10000, quotas))
            adoption.shares.push_back({share.id, BasisPoints{share.units}});
        adoptions.push_back(std::move(adoption));
    }
}
void review_adoption(MarketState& state, OrdinalDay day) {
    append_monthly_adoption(state, state.adoptions, day);
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

std::string_view channel_key(ProductChannel channel) {
    switch (channel) {
    case ProductChannel::legacy: return "legacy";
    case ProductChannel::business: return "business";
    case ProductChannel::api: return "api";
    case ProductChannel::consumer: return "consumer";
    case ProductChannel::licensed: return "licensed";
    case ProductChannel::open_weights: return "open_weights";
    case ProductChannel::agent: return "agent";
    }
    invalid("Unknown distribution channel");
}
ProductChannel parse_channel(std::string_view key) {
    for (const auto channel : {ProductChannel::legacy, ProductChannel::business, ProductChannel::api,
            ProductChannel::consumer, ProductChannel::licensed, ProductChannel::open_weights, ProductChannel::agent})
        if (channel_key(channel) == key) return channel;
    invalid("Unknown distribution channel");
}
bool customer_hosted(ProductChannel channel) {
    return channel == ProductChannel::licensed || channel == ProductChannel::open_weights;
}
bool regional_market(const MarketState& state) {
    return !state.segments.empty() && !state.segments.front().region_id.empty();
}
bool market_access(const MarketProduct& product, const MarketSegment& segment) {
    return (product.region_id.empty() || product.region_id == segment.region_id) &&
        (!segment.requires_local || customer_hosted(product.channel));
}
BasisPoints market_migration(const MarketProduct& product, const MarketSegment& segment) {
    return std::min(product.monthly_migration, segment.maximum_monthly_migration);
}

MarketState opening_market(OrdinalDay starts_on, std::string owner) {
    MarketState state;
    state.starts_on = starts_on; state.owner = std::move(owner);
    validate_market(state);
    return state;
}
Cents market_price_on(const MarketState& state, std::string_view id, OrdinalDay day) {
    const auto key=std::pair{id,day};
    const auto upper=std::upper_bound(state.prices.begin(),state.prices.end(),key,[](const auto& value,const auto& row) {
        return value<std::pair{std::string_view{row.product_id},row.starts_on};
    });
    if (upper!=state.prices.begin()) {
        const auto& row=*std::prev(upper);
        if (row.product_id==id) return row.price;
    }
    return find_id(state.products,id).price_per_task;
}
std::uint64_t public_market_demand(const MarketState& state, const MarketSegment& segment, OrdinalDay day) {
    std::int64_t delta=0;
    for (const auto& change:state.demand_changes)
        if (change.segment_id==segment.id && change.starts_on<=day && day<change.expires_before)
            delta=checked_add(delta,change.delta.value());
    const auto factor=std::clamp(checked_add(10000,delta),INT64_C(5000),maximum_demand_factor);
    return std::min(market_task_limit,mul_div_round_half_up(segment.daily_tasks,static_cast<std::uint64_t>(factor),10000));
}
std::uint64_t maximum_daily_demand(const MarketSegment& segment) {
    return std::min(market_task_limit,mul_div_round_half_up(segment.daily_tasks,static_cast<std::uint64_t>(maximum_demand_factor),10000));
}
std::uint64_t effective_market_demand(const MarketState& state,const MarketSegment& segment,OrdinalDay day) {
    const auto demand=public_market_demand(state,segment,day);
    std::uint64_t promised=0;
    for (const auto& contract:state.contracts)
        if (contract.segment_id==segment.id && contract.started_on && *contract.started_on<=day && day<contract.ends_before &&
            (!contract.stopped_on || day<*contract.stopped_on)) promised+=contract.tasks_per_day;
    return std::max(demand,promised); // Existing signed work is not cancelled by a demand shock.
}
void add_market_demand_change(MarketState& state, OrdinalDay day, MarketDemandChange change) {
    mutate(state,[&](auto& next) {
        require_day(next,day);
        if (change.starts_on!=day) invalid("Demand changes cannot rewrite settled customer work");
        for (const auto& prior:next.demand_changes)
            if (prior.segment_id==change.segment_id && prior.expires_before>day)
                invalid("A segment cannot stack overlapping external demand incidents");
        next.demand_changes.push_back(std::move(change));
    });
}
void MarketHistoryCount::add(const MarketDay& day) {
    const auto added = day.contracts.size() + day.discretionary.size() + day.segments.size();
    if (added > market_settlement_limit - rows) invalid("Market settlement history reached its retained row bound");
    rows += added;
}
MarketReport market_report(const MarketState& state) {
    MarketReport report;
    for (const auto& day : state.days) {
        std::uint64_t served_today = 0;
        for (const auto& row : day.segments) {
            served_today += row.contract_tasks + row.discretionary_tasks;
            if (!day.total_demand) report.outside_tasks += row.outside_tasks;
            report.customer_charges = checked_unit_add(report.customer_charges, row.charged);
        }
        report.delivered_tasks += served_today;
        if (day.total_demand) {
            if (served_today > *day.total_demand) invalid("Regional deliveries exceed total demand");
            report.outside_tasks += *day.total_demand - served_today;
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
    if (state.demand_changes.size()>64) invalid("External demand incident history exceeds its bound");
    std::set<std::string_view> change_ids;
    for (const auto& change:state.demand_changes) {
        text_valid(change.id); (void)find_id(state.segments,change.segment_id);
        if (!change_ids.insert(change.id).second || change.starts_on<state.starts_on || change.starts_on>latest ||
            change.expires_before<=change.starts_on || change.expires_before.value()-change.starts_on.value()>90 ||
            change.delta.value()<-2500 || change.delta.value()>2500 || change.delta.value()==0)
            invalid("External demand incident has invalid identity, duration or magnitude");
    }
    std::vector<const MarketDemandChange*> ordered_changes;
    for (const auto& change:state.demand_changes) ordered_changes.push_back(&change);
    std::sort(ordered_changes.begin(),ordered_changes.end(),[](const auto* a,const auto* b) {
        return std::tie(a->segment_id,a->starts_on,a->id)<std::tie(b->segment_id,b->starts_on,b->id);
    });
    for (std::size_t i=1;i<ordered_changes.size();++i)
        if (ordered_changes[i-1]->segment_id==ordered_changes[i]->segment_id && ordered_changes[i-1]->expires_before>ordered_changes[i]->starts_on)
            invalid("Saved external demand incidents overlap for the same segment");
    std::unordered_set<std::string_view> ids, projects;
    ids.reserve(state.products.size());
    projects.reserve(state.products.size());
    for (const auto& product : state.products) {
        text_valid(product.id); text_valid(product.capacity_project_id);
        (void)channel_key(product.channel);
        if (!product.region_id.empty()) text_valid(product.region_id, 64);
        if (product.id == "outside" || !ids.insert(product.id).second || !projects.insert(product.capacity_project_id).second ||
            product.price_per_task.value() < 0 || product.price_per_task.value() > market_price_limit ||
            (product.channel == ProductChannel::open_weights && product.price_per_task.value()!=0))
            invalid("Product has duplicate identity/source or an invalid task price");
    }
    if (state.prices.size()>8192) invalid("Market price history exceeds its bound");
    std::optional<std::pair<std::string,OrdinalDay>> previous_price;
    std::map<std::string,Cents> last_price;
    for (const auto& row:state.prices) {
        const auto key=std::pair{row.product_id,row.starts_on};
        (void)find_id(state.products,row.product_id);
        if ((previous_price && *previous_price>=key) || row.starts_on<state.starts_on || row.starts_on>latest ||
            row.price.value()<0 || row.price.value()>market_price_limit ||
            (find_id(state.products, row.product_id).channel == ProductChannel::open_weights && row.price.value()!=0) ||
            (!last_price.contains(row.product_id) && row.starts_on!=state.starts_on)) invalid("Market price history is not canonical or has no opening price");
        previous_price=key;last_price.insert_or_assign(row.product_id,row.price);
    }
    for (const auto& [id,price]:last_price) if (find_id(state.products,id).price_per_task!=price) invalid("Current price differs from its actual price history");
    ids.clear();
    ids.reserve(state.segments.size());
    std::unordered_set<std::string_view> products_seen;
    products_seen.reserve(state.products.size());
    const bool regional = regional_market(state);
    for (const auto& segment : state.segments) {
        text_valid(segment.id);
        if (regional) text_valid(segment.region_id, 64);
        if (!segment.customer_interconnect.empty()) text_valid(segment.customer_interconnect, 64);
        if (segment.region_id.empty() == regional ||
            (segment.customer_compute.value() > 0 && (segment.customer_memory_mib == 0 || segment.customer_interconnect.empty())) ||
            (!regional && (segment.customer_compute.value() != 0 || segment.customer_memory_mib != 0 ||
                !segment.customer_interconnect.empty() || segment.requires_local)))
            invalid("Regional customer hardware and location terms are inconsistent");
        if (!ids.insert(segment.id).second || segment.daily_tasks > market_task_limit || segment.daily_budget.value() < 0 ||
            segment.daily_budget.value() > 100'000'000'000'000'000LL || segment.outside_utility > market_utility_limit ||
            segment.reservation_threshold > market_utility_limit || segment.preferences.size() > market_product_limit)
            invalid("Segment demand, budget, utility or catalog bounds are invalid");
        products_seen.clear();
        for (const auto& pref : segment.preferences) {
            (void)find_id(state.products, pref.product_id);
            if (!products_seen.insert(pref.product_id).second || pref.utility > market_utility_limit ||
                pref.switching_cost > market_utility_limit || pref.willingness_to_pay.value() < 0 ||
                pref.willingness_to_pay.value() > market_price_limit)
                invalid("Segment preference has duplicate product, invalid utility or willingness to pay");
        }
    }
    for (const auto& product:state.products) if (!product.region_id.empty() &&
        std::none_of(state.segments.begin(),state.segments.end(),[&](const auto& row) {return row.region_id==product.region_id;}))
        invalid("Product launch territory is outside the customer catalog");
    ids.clear();
    ids.reserve(state.contracts.size());
    for (const auto& contract : state.contracts) {
        text_valid(contract.id); text_valid(contract.cause, 256);
        const auto& segment=find_id(state.segments,contract.segment_id);
        const auto& product_row=find_id(state.products,contract.product_id);
        const auto* pref=preference(segment,product_row.id);
        if ((product_row.channel!=ProductChannel::legacy && product_row.channel!=ProductChannel::business) ||
            !market_access(product_row,segment) || (regional && (!pref || !pref->eligible || contract.usage_price>pref->willingness_to_pay)))
            invalid("Saved customer contract exceeds its channel or territory");
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
    std::unordered_map<std::string_view, std::size_t> contract_days;
    contract_days.reserve(state.contracts.size());
    std::unordered_set<std::string_view> seen_segments;
    seen_segments.reserve(state.segments.size());
    std::unordered_map<std::string_view, std::uint64_t> delivered_contract, delivered_discretionary, promised;
    std::unordered_map<std::string_view, Cents> charges;
    std::unordered_set<std::string_view> daily_contracts, daily_segments;
    delivered_contract.reserve(state.segments.size());
    delivered_discretionary.reserve(state.segments.size());
    promised.reserve(state.segments.size());
    charges.reserve(state.segments.size());
    daily_contracts.reserve(state.contracts.size());
    daily_segments.reserve(state.segments.size());
    std::vector<std::pair<std::string_view, std::string_view>> delivery_ids;
    std::unordered_map<std::string_view, MilliCU> customer_work;
    customer_work.reserve(state.segments.size());
    std::uint64_t fixed_demand=0;
    std::vector<const MarketSegment*> dated_demand;
    if (regional) {
        std::unordered_set<std::string_view> affected;
        for (const auto& row:state.demand_changes) affected.insert(row.segment_id);
        for (const auto& row:state.contracts) affected.insert(row.segment_id);
        for (const auto& row:state.segments) {
            if (affected.contains(row.id)) dated_demand.push_back(&row);
            else fixed_demand+=row.daily_tasks;
        }
    }
    for (std::size_t index = 0; index < state.days.size(); ++index) {
        const auto& day = state.days[index];
        if (day.day.value() != checked_add(state.starts_on.value(), static_cast<std::int64_t>(index)) ||
            day.contracts.size() > market_contract_limit || day.discretionary.size() > market_product_limit * market_segment_limit ||
            day.segments.size() > market_segment_limit || day.allocation_rounds > allocation_product_limit + allocation_edge_limit + 1 ||
            day.total_demand.has_value() != regional)
            invalid("Market day is out of order or exceeds finite allocation bounds");
        if (regional) {
            auto total_demand=fixed_demand;
            for (const auto* segment:dated_demand) total_demand+=effective_market_demand(state,*segment,day.day);
            if (*day.total_demand != total_demand) invalid("Regional customer demand differs from the authored and dated terms");
        }
        delivered_contract.clear();
        delivered_discretionary.clear();
        promised.clear();
        charges.clear();
        daily_contracts.clear();
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
        delivery_ids.clear();
        customer_work.clear();
        for (const auto& delivery : day.discretionary) {
            const auto& product_row = find_id(state.products, delivery.product_id);
            const auto& segment = find_id(state.segments, delivery.segment_id);
            const auto* pref = preference(segment, product_row.id);
            const std::pair<std::string_view, std::string_view> key{delivery.segment_id, delivery.product_id};
            const bool duplicate = std::find(delivery_ids.begin(), delivery_ids.end(), key) != delivery_ids.end();
            delivery_ids.push_back(key);
            if (duplicate || delivery.tasks == 0 ||
                delivery.tasks > market_task_limit || delivery.charged != multiplied(market_price_on(state,product_row.id,day.day), delivery.tasks) ||
                !pref || !pref->eligible || !market_access(product_row, segment) ||
                customer_hosted(product_row.channel) != (delivery.customer_compute.value() > 0) ||
                market_price_on(state,product_row.id,day.day) > pref->willingness_to_pay)
                invalid("Discretionary delivery has invalid eligibility, identity, tasks or price");
            if (delivery.customer_compute.value() > 0) {
                auto& used = customer_work.try_emplace(segment.id, MilliCU{0}).first->second;
                used = checked_unit_add(used, delivery.customer_compute);
                if (used.value() > segment.customer_compute.value()) invalid("Local deliveries exceed the buyer's hardware budget");
            }
            delivered_discretionary[segment.id] += delivery.tasks;
            auto [found, fresh] = charges.try_emplace(segment.id, Cents{0}); (void)fresh;
            found->second = checked_unit_add(found->second, delivery.charged);
        }
        daily_segments.clear();
        for (const auto& row : day.segments) {
            const auto& segment = find_id(state.segments, row.segment_id);
            const auto dc_it = delivered_contract.find(segment.id);
            const auto dd_it = delivered_discretionary.find(segment.id);
            const auto pr_it = promised.find(segment.id);
            const auto ch_it = charges.find(segment.id);
            const auto dc = dc_it == delivered_contract.end() ? 0 : dc_it->second;
            const auto dd = dd_it == delivered_discretionary.end() ? 0 : dd_it->second;
            const auto pr = pr_it == promised.end() ? 0 : pr_it->second;
            const auto ch = ch_it == charges.end() ? Cents{0} : ch_it->second;
            if (!daily_segments.insert(segment.id).second || row.demand != effective_market_demand(state,segment,day.day) || row.budget != segment.daily_budget ||
                row.contract_tasks != dc || row.discretionary_tasks != dd ||
                row.contract_tasks > row.demand || row.discretionary_tasks > row.demand || row.outside_tasks > row.demand ||
                row.contract_tasks + row.discretionary_tasks + row.outside_tasks != row.demand || pr > row.demand ||
                row.charged != ch || row.charged > row.budget ||
                row.charged.value() < 0 || row.unspent != checked_unit_subtract(row.budget, row.charged))
                invalid("Segment report loses demand, double spends budget or differs from actual work");
            text_valid(row.outside_reason, 256);
        }
        if (!regional) {
            for (const auto& id : seen_segments)
                if (!daily_segments.contains(id)) invalid("An introduced segment disappeared from daily market history");
            seen_segments.insert(daily_segments.begin(), daily_segments.end());
        }
        for (const auto& [id, count] : promised) if (count && !daily_segments.contains(id)) invalid("A promise has no segment report");
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
        const auto cd_it = contract_days.find(contract.id);
        const auto actual_days = cd_it == contract_days.end() ? 0 : cd_it->second;
        if (actual_days != expected) invalid("Contract committed day coverage is incomplete");
    }
    std::set<std::pair<OrdinalDay, std::string_view>> adoption_ids;
    std::unordered_set<std::string_view> share_ids;
    for (const auto& adoption : state.adoptions) {
        (void)find_id(state.segments, adoption.segment_id);
        if (!state.closed_through || adoption.reviewed_on > *state.closed_through || adoption.reviewed_on < state.starts_on ||
            !month_end(adoption.reviewed_on) || !adoption_ids.emplace(adoption.reviewed_on, adoption.segment_id).second ||
            adoption.observed_tasks == 0 || adoption.shares.empty() || adoption.shares.size() > market_product_limit + 1)
            invalid("Installed share review has an invalid identity, month or count");
        std::int64_t total = 0;
        share_ids.clear();
        for (const auto& row : adoption.shares) {
            if (row.product_id != "outside") (void)find_id(state.products, row.product_id);
            if (!share_ids.insert(row.product_id).second) invalid("Duplicate installed product share");
            total = checked_add(total, row.share.value());
        }
        if (total != 10000 || !share_ids.contains("outside")) invalid("Installed shares must conserve all demand including outside");
    }
    // Recompute monthly adoption from actual useful work, without replaying
    // eligibility or reading current product availability as historical truth.
    MarketAdoptionHistory reviewed_adoptions;
    for (const auto& day : state.days) if (month_end(day.day)) append_monthly_adoption(state, reviewed_adoptions, day.day);
    if (reviewed_adoptions != state.adoptions) invalid("Installed monthly shares differ from committed useful-work history");
    (void)market_report(state);
}

void validate_market_links(const MarketState& state, const AccountsState& accounts, const CapacityState& capacity) {
    validate_market(state); validate_accounts(accounts); validate_capacity(capacity);
    const auto account_day = accounts.closed_through ? advance_days(*accounts.closed_through, Days{1}) : accounts.starts_on;
    const auto capacity_day = capacity.closed_through ? advance_days(*capacity.closed_through, Days{1}) : capacity.starts_on;
    if (state.owner != accounts.owner || state.owner != capacity.owner || state.starts_on < accounts.starts_on ||
        state.starts_on < capacity.starts_on || current_day(state) != account_day || current_day(state) != capacity_day)
        invalid("Market, accounts and capacity must share owner and current unclosed day");
    for (const auto& product_row : state.products)
        if (project(capacity, product_row).customer_inference != customer_hosted(product_row.channel))
            invalid("Product inference must use its authored company or customer hardware");
    if (state.capacity_bindings.size()>4096) invalid("Product capacity history exceeds its bound");
    std::map<std::string,std::pair<OrdinalDay,std::string>> last_binding;
    std::set<std::string> bound_projects;
    for (const auto& binding:state.capacity_bindings) {
        (void)find_id(state.products,binding.product_id);
        const auto& work=find_id(capacity.projects,binding.project_id);
        const auto previous=last_binding.find(binding.product_id);
        if (work.kind!=ProjectKind::serving || work.starts_on!=binding.starts_on || binding.starts_on>current_day(state) ||
            !bound_projects.insert(binding.project_id).second ||
            (previous!=last_binding.end() && binding.starts_on<previous->second.first))
            invalid("Product capacity history has an invalid serving boundary");
        if (previous!=last_binding.end()) {
            const auto& prior=find_id(capacity.projects,previous->second.second);
            if (prior.status!=ProjectStatus::cancelled || (!prior.stopped_on || *prior.stopped_on>binding.starts_on))
                invalid("A replaced serving profile must stop at its replacement boundary");
        }
        last_binding.insert_or_assign(binding.product_id,std::pair{binding.starts_on,binding.project_id});
    }
    for (const auto& [id,binding]:last_binding)
        if (find_id(state.products,id).capacity_project_id!=binding.second) invalid("Current product differs from its last serving profile");
    struct ExpectedInvoice { InvoiceKind kind; Cents amount; OrdinalDay due; };
    std::unordered_map<std::string, ExpectedInvoice> expected;
    expected.reserve(state.days.size() * 4 + state.contracts.size());
    const auto expect = [&](std::string id, InvoiceKind kind, Cents amount, OrdinalDay day) {
        if (amount.value() > 0 && !expected.emplace(std::move(id), ExpectedInvoice{kind, amount, day}).second)
            invalid("Market consequences repeat an invoice identity");
    };
    std::unordered_map<std::string_view, std::uint64_t> delivered, protected_delivered;
    delivered.reserve(state.products.size());
    protected_delivered.reserve(state.products.size());
    for (const auto& day : state.days) {
        delivered.clear();
        protected_delivered.clear();
        const auto date_text = iso_date(day.day);
        for (const auto& result : day.contracts) {
            const auto& contract = find_id(state.contracts, result.contract_id);
            delivered[contract.product_id] += result.delivered_tasks;
            protected_delivered[contract.product_id] += result.delivered_tasks;
            const auto charge = net_charge(result);
            if (charge.value() > 0)
                expect("market.contract/" + contract.id + "/" + date_text, InvoiceKind::receivable, charge, day.day);
            if (result.penalty.value() > 0)
                expect("market.penalty/" + contract.id + "/" + date_text, InvoiceKind::payable, result.penalty, day.day);
        }
        for (const auto& result : day.discretionary) {
            delivered[result.product_id] += result.tasks;
            if (!day.total_demand && result.charged.value() > 0)
                expect("market.sale/" + result.segment_id + "/" + result.product_id + "/" + date_text,
                       InvoiceKind::receivable, result.charged, day.day);
        }
        if (day.total_demand) {
            Cents sales{0};
            for (const auto& result : day.discretionary) sales = checked_unit_add(sales, result.charged);
            expect("market.sale/combined/" + date_text, InvoiceKind::receivable, sales, day.day);
        }
        // validate_capacity proved complete, contiguous dates. Indexing the
        // matching day preserves the date check without rescanning all earlier
        // days for every historical receipt during each new daily transaction.
        const auto offset = day.day.value() - capacity.starts_on.value();
        if (offset < 0 || static_cast<std::uint64_t>(offset) >= capacity.days.size())
            invalid("Market delivery has no matching capacity day");
        const auto* record = &capacity.days[static_cast<std::size_t>(offset)];
        if (record->day != day.day) invalid("Market delivery has no matching capacity date");
        for (const auto& product_row : state.products) {
            std::string_view binding = product_row.capacity_project_id;
            for (const auto& row : state.capacity_bindings)
                if (row.product_id == product_row.id && row.starts_on <= day.day) binding = row.project_id;
            const auto work = std::find_if(record->projects.begin(), record->projects.end(),
                [&](const auto& row) { return row.project_id == binding; });
            const auto actual_tasks = work == record->projects.end() ? 0 : work->useful_tasks;
            const auto actual_protected = work == record->projects.end() ? 0 : work->protected_tasks;
            const auto del_it = delivered.find(product_row.id);
            const auto pdel_it = protected_delivered.find(product_row.id);
            const auto expected_tasks = del_it == delivered.end() ? 0 : del_it->second;
            const auto expected_protected = pdel_it == protected_delivered.end() ? 0 : pdel_it->second;
            if (actual_tasks != expected_tasks || actual_protected != expected_protected)
                invalid("Market useful work differs from actual serving or contract priority");
            if (expected_tasks != 0) {
                const auto& profile = find_id(capacity.projects, binding);
                if (profile.customer_inference != customer_hosted(product_row.channel))
                    invalid("Product changed who performs its inference");
                if (profile.customer_inference) for (const auto& sale : day.discretionary) {
                    if (sale.product_id != product_row.id) continue;
                    const auto& customer = find_id(state.segments, sale.segment_id);
                    if (sale.customer_compute != required_inference(sale.tasks, profile.inference_milli_CU_per_task) ||
                        profile.minimum_memory_mib > customer.customer_memory_mib ||
                        profile.interconnect != customer.customer_interconnect)
                        invalid("Local inference bypassed actual buyer compute or compatibility");
                }
            }
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
        next.capacity_bindings.push_back({row.id,row.capacity_project_id,project(capacity,row).starts_on});
        next.products.push_back(std::move(row));
        std::sort(next.products.begin(), next.products.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void replace_product_capacity(MarketState& state, const CapacityState& capacity, OrdinalDay day,
                              std::string_view product_id, std::string project_id) {
    require_day(state, day);
    const auto found=std::find_if(state.products.begin(),state.products.end(),[&](const auto& row){return row.id==product_id;});
    if (found==state.products.end()) invalid("Replacement service product does not exist");
    auto& product_row=*found;
    if (state.capacity_bindings.size() >= 4096) invalid("Product capacity history is full");
    if (std::none_of(state.capacity_bindings.begin(),state.capacity_bindings.end(),[&](const auto& row){return row.product_id==product_id;}))
        state.capacity_bindings.push_back({product_row.id,product_row.capacity_project_id,project(capacity,product_row).starts_on});
    product_row.capacity_project_id=std::move(project_id);
    (void)project(capacity,product_row);
    state.capacity_bindings.push_back({product_row.id,product_row.capacity_project_id,day});
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
void set_market_price(MarketState& state,OrdinalDay day,std::string_view id,Cents price) {
    mutate(state,[&](auto& next) {
        require_day(next,day);
        auto found=std::find_if(next.products.begin(),next.products.end(),[&](const auto& row){return row.id==id;});
        if (found==next.products.end() || price.value()<0 || price.value()>market_price_limit) invalid("Choose an existing product and a bounded nonnegative price");
        if (found->channel == ProductChannel::open_weights && price.value() != 0)
            invalid("Published open weights carry no per-task license charge");
        if (found->price_per_task==price) return;
        if (next.prices.size()+2>8192) invalid("Market price history is full");
        if (std::none_of(next.prices.begin(),next.prices.end(),[&](const auto& row){return row.product_id==id;}))
            next.prices.push_back({std::string(id),next.starts_on,found->price_per_task});
        const auto today=std::find_if(next.prices.begin(),next.prices.end(),[&](const auto& row){return row.product_id==id && row.starts_on==day;});
        if (today==next.prices.end()) next.prices.push_back({std::string(id),day,price});else today->price=price;
        found->price_per_task=price;
        std::sort(next.prices.begin(),next.prices.end(),[](const auto& a,const auto& b){return std::tie(a.product_id,a.starts_on)<std::tie(b.product_id,b.starts_on);});
    });
}
void add_market_preference(MarketState& state,std::string_view segment_id,MarketPreference preference_row) {
    mutate(state,[&](auto& next) {
        const auto found=std::find_if(next.segments.begin(),next.segments.end(),[&](const auto& row){return row.id==segment_id;});
        if (found==next.segments.end() || preference(*found,preference_row.product_id)) invalid("A new customer preference needs an existing segment and a new product");
        found->preferences.push_back(std::move(preference_row));
        std::sort(found->preferences.begin(),found->preferences.end(),[](const auto& a,const auto& b){return a.product_id<b.product_id;});
    });
}
void validate_contract_acceptance(const MarketState& state,const CapacityState& capacity,OrdinalDay day,const MarketContract& contract) {
        require_day(state, day);
        if (contract.status != ContractStatus::pending || contract.accepted_on != day || contract.started_on || contract.stopped_on ||
            state.contracts.size()>=market_contract_limit || std::any_of(state.contracts.begin(),state.contracts.end(),[&](const auto& row){return row.id==contract.id;}))
            invalid("A newly accepted contract must record today's acceptance without prior delivery state");
        const auto& segment = find_id(state.segments, contract.segment_id);
        const auto& product_row = find_id(state.products, contract.product_id);
        const auto* pref = preference(segment, product_row.id);
        if (!pref || !pref->eligible || !market_access(product_row, segment) ||
            (product_row.channel != ProductChannel::legacy && product_row.channel != ProductChannel::business) ||
            contract.usage_price > pref->willingness_to_pay)
            invalid("Customer contract fails eligibility or agreed usage-price willingness");
        std::map<std::string, std::uint64_t> segment_tasks, product_tasks;
        std::map<std::string, Cents> segment_cost;
        const auto include=[&](const auto& row) {
            if (row.status != ContractStatus::pending && row.status != ContractStatus::active) return;
            segment_tasks[row.segment_id] += row.tasks_per_day;
            product_tasks[row.product_id] += row.tasks_per_day;
            const auto worst_fee = Cents{row.monthly_fee.value() / 28 + (row.monthly_fee.value() % 28 != 0 ? 1 : 0)};
            auto [cost, fresh] = segment_cost.try_emplace(row.segment_id, Cents{0}); (void)fresh;
            cost->second = checked_unit_add(cost->second, checked_unit_add(worst_fee, multiplied(row.usage_price, row.tasks_per_day)));
        };
        for (const auto& row:state.contracts) include(row);
        include(contract);
        for (const auto& [id, tasks] : segment_tasks) {
            const auto& customer = find_id(state.segments, id);
            if (tasks > customer.daily_tasks || segment_cost.at(id) > customer.daily_budget)
                invalid("Accepted contracts exceed the same customer's task demand or daily budget");
        }
        for (const auto& [id, tasks] : product_tasks)
            if (tasks > project(capacity, find_id(state.products, id)).tasks_per_day)
                invalid("Accepted contract tasks exceed the product's committed serving profile");
}
void accept_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, MarketContract contract) {
    mutate(state, [&](auto& next) {
        validate_contract_acceptance(next,capacity,day,contract);
        next.contracts.push_back(std::move(contract));
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
namespace {
struct MarketClosePlan {
    MarketDay record;
    std::map<std::string,std::uint64_t> product_contract_delivered,segment_reserved,segment_contract_delivered;
    std::map<std::string,Cents> segment_charged;
    std::vector<AllocationProduct> products;
    std::vector<AllocationSegment> segments;
};
MarketClosePlan prepare_market(const MarketState& next,const CapacityState& pool,OrdinalDay day,bool validate_io,
    const std::vector<MarketAdoption>* industry_adoptions = nullptr, const MarketInfluences& influences = {}) {
    require_day(next,day);
    if (influences.size() > market_product_limit * market_segment_limit) invalid("Regional market influence exceeds its size bound");
    using ContextKey=std::pair<std::string_view,std::string_view>;
    std::map<ContextKey, const MarketInfluence*> contextual;
    for (const auto& influence : influences) {
        (void)find_id(next.products, influence.product_id);
        (void)find_id(next.segments, influence.segment_id);
        if (influence.awareness.value() > 3000 ||
            !contextual.emplace(ContextKey{influence.product_id,influence.segment_id}, &influence).second)
            invalid("Regional awareness and eligibility need unique bounded market links");
    }
    const auto context = [&](const std::string& product, const std::string& segment) -> const MarketInfluence* {
        if(contextual.empty()) return nullptr;
        const auto found = contextual.find(ContextKey{product,segment});
        return found == contextual.end() ? nullptr : found->second;
    };
    if (industry_adoptions) {
        if (industry_adoptions->size()>market_segment_limit) invalid("Industry opportunity needs one bounded adoption snapshot per segment");
        std::set<std::string> segments;
        for (const auto& row:*industry_adoptions) {
            (void)find_id(next.segments,row.segment_id);
            if (!segments.insert(row.segment_id).second || row.reviewed_on>=day || !month_end(row.reviewed_on) ||
                row.observed_tasks==0 || row.shares.empty() || row.shares.size()>allocation_product_limit+1)
                invalid("Industry adoption snapshot has invalid dates, identities or bounds");
            std::uint64_t total=0;std::set<std::string> products;
            for (const auto& share:row.shares) {
                text_valid(share.product_id,256);if (!products.insert(share.product_id).second) invalid("Industry adoption snapshot repeats a product");
                total+=static_cast<std::uint64_t>(share.share.value());
            }
            if (total!=10000 || !products.contains("outside")) invalid("Industry adoption shares do not conserve customer use");
        }
    }
    MarketDay record; record.day = day;
    std::map<std::string, std::uint64_t> requested_contracts;
    std::map<std::string, std::vector<WeightedQuota>> product_contracts;
    for (const auto& row : next.contracts) {
        if (row.status == ContractStatus::pending && day >= row.start_by) {
            record.contracts.push_back({row.id, 0, 0, Cents{0}, Cents{0}, Cents{0}, row.missed_start_penalty});
        } else if (row.status == ContractStatus::active) {
            const auto* rule = context(row.product_id,row.segment_id);
            if (!rule || rule->eligible) {
                requested_contracts[row.product_id] += row.tasks_per_day;
                product_contracts[row.product_id].push_back({row.id, row.tasks_per_day});
            }
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
    const auto plan = preview_capacity_day(pool, day, full_requests,validate_io);
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
    for (const auto& row : next.contracts) {
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
    }
    std::vector<AllocationProduct> products;
    for (const auto& row : next.products) {
        const auto total = total_supply(plan, row.capacity_project_id);
        const auto& profile = project(pool, row);
        products.push_back({row.id, row.price_per_task, total - product_contract_delivered[row.id],
                            profile.customer_inference ? profile.inference_milli_CU_per_task : MilliCU{0}});
    }
    std::vector<AllocationSegment> segments;
    for (const auto& row : next.segments) {
        const auto reserved = segment_reserved[row.id];
        const auto charged = segment_charged.contains(row.id) ? segment_charged.at(row.id) : Cents{0};
        const auto demand=effective_market_demand(next,row,day);
        if (reserved > demand || charged > row.daily_budget) invalid("Contract work exceeds the customer's single task/budget snapshot");
        AllocationSegment segment{row.id, demand - reserved, checked_unit_subtract(row.daily_budget, charged),
                                  weight(row.outside_utility, row.reservation_threshold), {}, row.customer_compute};
        const auto share=[&](std::string_view id) {
            if (!industry_adoptions) return installed(next,row.id,id);
            const auto key=id=="outside"?std::string{"outside"}:next.owner+":"+std::string(id);
            for (auto adoption=industry_adoptions->rbegin();adoption!=industry_adoptions->rend();++adoption) if (adoption->segment_id==row.id) {
                for (const auto& entry:adoption->shares) if (entry.product_id==key) return static_cast<std::uint64_t>(entry.share.value());
                return std::uint64_t{0};
            }
            return std::uint64_t{id=="outside"?10000U:0U};
        };
        const auto outside_share = share("outside");
        for (const auto& pref : row.preferences) {
            const auto& product_row = find_id(next.products, pref.product_id);
            const auto* rule = context(product_row.id,row.id);
            if (rule && !rule->eligible) continue;
            if (!pref.eligible || !market_access(product_row, row) || !available(product_row, pool) ||
                product_row.price_per_task > pref.willingness_to_pay) continue;
            const auto& profile = project(pool, product_row);
            if (profile.customer_inference && (profile.minimum_memory_mib > row.customer_memory_mib ||
                profile.interconnect != row.customer_interconnect)) continue;
            const auto old_share = share(product_row.id);
            const auto other_installed = 10000 - outside_share - old_share;
            const auto switching = mul_div_round_half_up(pref.switching_cost, other_installed, 10000);
            const auto awareness = rule ? static_cast<std::uint64_t>(rule->awareness.value()) : 0;
            const auto known = std::min(market_utility_limit, mul_div_round_half_up(pref.utility, 10000 + awareness, 10000));
            const auto utility = known > switching ? known - switching : 0;
            const auto migration = static_cast<std::uint64_t>(market_migration(product_row, row).value());
            const auto adoption_cap = std::min<std::uint64_t>(10000, old_share + migration);
            const auto limit = mul_div_floor(demand, adoption_cap, 10000).quotient;
            const auto retained = product_row.channel != ProductChannel::legacy && old_share > migration
                ? mul_div_floor(demand, old_share - migration, 10000).quotient : 0;
            segment.edges.push_back({product_row.id, weight(utility, row.reservation_threshold), limit, retained});
        }
        segments.push_back(std::move(segment));
    }
    return {std::move(record),std::move(product_contract_delivered),std::move(segment_reserved),std::move(segment_contract_delivered),
        std::move(segment_charged),std::move(products),std::move(segments)};
}
void validate_assigned(const MarketClosePlan& plan,const AllocationResult& assigned) {
    if (assigned.segments.size()!=plan.segments.size() || assigned.rounds>allocation_edge_limit+allocation_product_limit+allocation_segment_limit)
        invalid("Industry allocation has an invalid segment/round bound");
    std::set<std::string> segments;
    std::map<std::string,std::uint64_t> totals;
    for (const auto& result:assigned.segments) {
        const auto& input=find_id(plan.segments,result.id);
        if (!segments.insert(result.id).second || result.deliveries.size()>plan.products.size()) invalid("Industry allocation repeats a segment or exceeds its product bound");
        std::uint64_t delivered=0;Cents charged{0};std::set<std::string> products;
        MilliCU local_used{0};
        for (const auto& row:result.deliveries) {
            const auto& product_row=find_id(plan.products,row.product_id);
            const auto edge=std::find_if(input.edges.begin(),input.edges.end(),[&](const auto& value){return value.product_id==row.product_id;});
            if (!products.insert(row.product_id).second || row.tasks==0 || row.tasks>market_task_limit || edge==input.edges.end() ||
                edge->weight==0 || (edge->task_limit && row.tasks>*edge->task_limit) || row.cost!=multiplied(product_row.price,row.tasks))
                invalid("Industry allocation bypassed eligibility, migration or actual price");
            delivered+=row.tasks;totals[row.product_id]+=row.tasks;charged=checked_unit_add(charged,row.cost);
            if (product_row.customer_compute_per_task.value() > 0)
                local_used = checked_unit_add(local_used, required_inference(row.tasks, product_row.customer_compute_per_task));
        }
        if (delivered>input.tasks || result.outside_tasks!=input.tasks-delivered || charged>input.budget ||
            result.remaining_budget!=checked_unit_subtract(input.budget,charged) || local_used.value()>input.customer_compute.value())
            invalid("Industry allocation exceeds this company's task, money or customer hardware projection");
    }
    for (const auto& row:plan.products) if (totals[row.id]>row.capacity) invalid("Industry allocation exceeds actual serving capacity");
}
std::vector<ServingRequest> actual_serving_requests(const MarketState& market, const CapacityState& capacity,
    const MarketClosePlan& plan, const AllocationResult& allocation) {
    std::map<std::string, std::uint64_t> discretionary_totals;
    for (const auto& segment : allocation.segments)
        for (const auto& work : segment.deliveries) discretionary_totals[work.product_id] += work.tasks;
    std::vector<ServingRequest> requests;
    for (const auto& product : market.products) {
        const auto& profile = project(capacity, product);
        if (profile.status != ProjectStatus::active) continue;
        const auto contracted = plan.product_contract_delivered.find(product.id);
        requests.push_back({profile.id, contracted == plan.product_contract_delivered.end() ? 0 : contracted->second,
                            discretionary_totals[product.id]});
    }
    return requests;
}
}
MarketOpportunity market_opportunity(const MarketState& state,const CapacityState& capacity,OrdinalDay day,
    const std::vector<MarketAdoption>* adoptions, const MarketInfluences& influences) {
    auto plan=prepare_market(state,capacity,day,false,adoptions,influences);std::vector<MarketContractClaim> claims;
    for (const auto& row:state.segments) claims.push_back({row.id,plan.segment_reserved[row.id],plan.segment_charged.contains(row.id)?plan.segment_charged.at(row.id):Cents{0}});
    return {std::move(plan.products),std::move(plan.segments),std::move(claims)};
}
CapacityDay preview_market_work(const MarketState& state, const CapacityState& capacity, OrdinalDay day,
    const AllocationResult& assigned, const std::vector<MarketAdoption>& industry_adoptions, const MarketInfluences& influences) {
    const auto plan = prepare_market(state, capacity, day, false, &industry_adoptions,influences);
    validate_assigned(plan, assigned);
    return preview_capacity_day(capacity, day, actual_serving_requests(state, capacity, plan, assigned), false);
}
void close_market_day(MarketState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day, bool validate_io, const AllocationResult* assigned,
    const std::vector<MarketAdoption>* industry_adoptions, std::span<const WholesaleUse> wholesale, const MarketInfluences& influences) {
    if (validate_io) validate_market_links(state, accounts, capacity);
    require_day(state, day);
    if (state.days.size() >= market_day_limit) invalid("Market history reached its supported day bound");
    MarketState market_candidate; AccountsState ledger_candidate; CapacityState capacity_candidate;
    if (validate_io) { market_candidate=state; ledger_candidate=accounts; capacity_candidate=capacity; }
    auto& next=validate_io?market_candidate:state;
    auto& ledger=validate_io?ledger_candidate:accounts;
    auto& pool=validate_io?capacity_candidate:capacity;
    std::set<std::string> linked;
    for (const auto& product_row : next.products) linked.insert(product_row.capacity_project_id);
    for (const auto& row : pool.projects)
        if (row.kind == ProjectKind::serving && row.status == ProjectStatus::active && !linked.contains(row.id))
            invalid("Every active serving workload must be linked before market work advances");
    if (industry_adoptions && !assigned) invalid("Industry adoption context requires its shared allocation");
    auto prepared=prepare_market(next,pool,day,validate_io,industry_adoptions,influences);
    if (assigned) validate_assigned(prepared,*assigned);
    auto& record=prepared.record;
    auto& segment_reserved=prepared.segment_reserved;
    auto& segment_contract_delivered=prepared.segment_contract_delivered;
    auto& segment_charged=prepared.segment_charged;
    for (const auto& result:record.contracts) {
        auto found=std::find_if(next.contracts.begin(),next.contracts.end(),[&](const auto& row){return row.id==result.contract_id;});
        auto& row=*found;
        if (row.status==ContractStatus::pending) {row.status=ContractStatus::missed;row.stopped_on=row.start_by;}
        else if (advance_days(day,Days{1})>=row.ends_before) {row.status=ContractStatus::completed;row.stopped_on=row.ends_before;}
        invoice(ledger,day,invoice_id("contract",row.id,day),InvoiceKind::receivable,net_charge(result),row.cause);
        invoice(ledger,day,invoice_id("penalty",row.id,day),InvoiceKind::payable,result.penalty,row.cause);
    }
    AllocationResult stand_alone;
    if (!assigned) stand_alone = allocate_demand(prepared.products, prepared.segments);
    const auto& allocation = assigned ? *assigned : stand_alone;
    record.allocation_rounds = allocation.rounds;
    const bool regional = regional_market(next);
    if (regional) record.total_demand = 0;
    Cents combined_sales{0};
    for (const auto& result : allocation.segments) {
        const auto& segment = find_id(next.segments, result.id);
        const auto demand = effective_market_demand(next, segment, day);
        if (regional) *record.total_demand += demand;
        std::uint64_t delivered = 0;
        Cents charged = segment_charged.contains(result.id) ? segment_charged.at(result.id) : Cents{0};
        for (const auto& work : result.deliveries) {
            const auto per_task = find_id(prepared.products, work.product_id).customer_compute_per_task;
            const auto local_work = per_task.value() > 0 ? required_inference(work.tasks, per_task) : MilliCU{0};
            record.discretionary.push_back({result.id, work.product_id, work.tasks, work.cost, local_work});
            delivered += work.tasks;
            charged = checked_unit_add(charged, work.cost);
            if (regional) combined_sales = checked_unit_add(combined_sales, work.cost);
            else invoice(ledger, day, invoice_id("sale", result.id + "/" + work.product_id, day), InvoiceKind::receivable,
                         work.cost, "Customer work: " + work.product_id + " / " + result.id);
        }
        if (regional && delivered == 0 && segment_reserved[result.id] == 0) continue;
        const auto missing_contract = segment_reserved[result.id] - segment_contract_delivered[result.id];
        std::string reason;
        const auto append_reason = [&](std::string_view name, std::uint64_t count) {
            if (count == 0) return;
            if (!reason.empty()) reason += ",";
            reason += name; reason += ":"; reason += std::to_string(count);
        };
        append_reason("contract_shortfall", missing_contract);
        if (assigned) append_reason("other_providers_or_outside",result.outside_tasks);
        for (const auto& outside : assigned?std::vector<AllocationOutside>{}:result.outside) {
            switch (outside.reason) {
            case AllocationOutsideReason::preference: append_reason("preference", outside.tasks); break;
            case AllocationOutsideReason::no_eligible_product: append_reason("no_eligible_product", outside.tasks); break;
            case AllocationOutsideReason::zero_utility: append_reason("zero_utility", outside.tasks); break;
            case AllocationOutsideReason::budget: append_reason("budget", outside.tasks); break;
            case AllocationOutsideReason::capacity: append_reason("capacity", outside.tasks); break;
            case AllocationOutsideReason::edge_limit: append_reason("migration_limit", outside.tasks); break;
            case AllocationOutsideReason::customer_compute: append_reason("customer_hardware", outside.tasks); break;
            }
        }
        if (reason.empty()) reason = "none";
        record.segments.push_back({result.id, demand, segment.daily_budget, segment_contract_delivered[result.id],
            delivered, result.outside_tasks + missing_contract, charged, result.remaining_budget, std::move(reason)});
    }
    if (regional) invoice(ledger, day, invoice_id("sale", "combined", day), InvoiceKind::receivable,
                          combined_sales, "Delivered regional customer work");
    close_capacity_day(pool, day, actual_serving_requests(next, pool, prepared, allocation), false, wholesale);
    record.contracts.sort([](const auto& a, const auto& b) { return a.contract_id < b.contract_id; });
    record.discretionary.sort([](const auto& a, const auto& b) {
        return std::tie(a.segment_id, a.product_id) < std::tie(b.segment_id, b.product_id);
    });
    if (!next.days.empty()) record.share_unchanged(next.days.back());
    next.days.push_back(std::move(record)); next.closed_through = day;
    if (month_end(day)) review_adoption(next, day);
    if (validate_io) {
        validate_market(next); validate_accounts(ledger); validate_capacity(pool);
    }
    // Accounts intentionally remains on this day's close boundary; the parent
    // transaction settles it after all market consequences are recognized.
    if (validate_io) { state=std::move(next); accounts=std::move(ledger); capacity=std::move(pool); }
}
} // namespace pgp::core
