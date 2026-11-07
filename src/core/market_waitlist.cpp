#include "market_waitlist.hpp"

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::ranges::find(rows,id,&Rows::value_type::id);
    if (row==rows.end()) invalid("Waitlist refers to a missing service, workload or customer group");
    return *row;
}
using PolicyKey=std::pair<std::string_view,OrdinalDay>;
PolicyKey policy_key(const MarketWaitlistChange& row) {return {row.product_id,row.starts_on};}
using CustomerKey=std::pair<std::string_view,std::string_view>;
CustomerKey customer_key(const MarketWaitlistDay& row) {return {row.product_id,row.segment_id};}
const MarketWaitlistDay* waiting_group(const MarketDay* day,CustomerKey customer) {
    if (!day) return nullptr;
    const auto row=std::ranges::lower_bound(day->waitlists,customer,{},customer_key);
    return row!=day->waitlists.end() && customer_key(*row)==customer?&*row:nullptr;
}
struct RecentCustomers {
    const MarketDay* day=nullptr;
    std::uint64_t total=0;
};
RecentCustomers established(const MarketState& state,std::string_view product,OrdinalDay day) {
    RecentCustomers peak;
    const auto available=std::min<std::uint64_t>(state.days.size(),static_cast<std::uint64_t>(std::max<std::int64_t>(0,day.value()-state.starts_on.value())));
    const auto first=available-std::min<std::uint64_t>(available,waitlist_lookback_days);
    for (auto i=available;i>first;--i) {
        const auto& record=state.days[static_cast<std::size_t>(i-1)];
        std::uint64_t total=0;
        for (const auto& sale:record.discretionary) if (sale.product_id==product && sale.local_copy_project.empty()) total+=sale.tasks;
        if (total>peak.total) peak={&record,total};
    }
    return peak;
}
std::uint64_t sold(const MarketDay* day,CustomerKey customer) {
    if (!day) return 0;
    const auto row=std::ranges::find_if(day->discretionary,[&](const auto& sale) {
        return sale.product_id==customer.first && sale.segment_id==customer.second && sale.local_copy_project.empty();
    });
    return row==day->discretionary.end()?0:row->tasks;
}
std::uint64_t excess(std::uint64_t amount,std::uint64_t baseline) {
    return amount>baseline?amount-baseline:0;
}
struct Intake {
    const std::string* segment;
    AllocationEdge* edge;
    std::uint64_t headroom;
    const MarketWaitlistDay* waiting;
};
void grant_intake(std::vector<Intake>& groups,std::uint64_t quota) {
    std::sort(groups.begin(),groups.end(),[](const auto& a,const auto& b) {
        using Order=std::pair<std::optional<OrdinalDay>,std::string_view>;
        return Order{a.waiting?a.waiting->waiting_since:std::nullopt,*a.segment}<
               Order{b.waiting?b.waiting->waiting_since:std::nullopt,*b.segment};
    });
    for (auto& group:groups) {
        if (!group.waiting || !group.waiting->waiting_since) continue;
        const auto allotted=std::min({quota,group.headroom,group.waiting->waiting_tasks});
        group.edge->task_limit=*group.edge->task_limit+allotted;
        group.headroom-=allotted;
        quota-=allotted;
    }
    if (quota==0) return;
    std::vector<WeightedQuota> headroom;
    std::uint64_t remaining=0;
    for (const auto& group:groups) if (group.headroom>0) {
        headroom.push_back({*group.segment,group.headroom});
        remaining+=group.headroom;
    }
    if (remaining==0) return;
    const auto extra=apportion_units(std::min(quota,remaining),headroom);
    for (auto& group:groups) {
        const auto allotment=std::ranges::lower_bound(extra,*group.segment,{},&ApportionedQuota::id);
        if (allotment!=extra.end() && allotment->id==*group.segment)
            group.edge->task_limit=*group.edge->task_limit+allotment->units;
    }
}
}

std::optional<std::uint64_t> market_waitlist_limit(const MarketState& state,const MarketProduct& product,OrdinalDay day) {
    if (state.waitlist_changes.empty() || (product.retirement && product.retirement->announced_on<=day)) return {};
    const auto after=std::ranges::upper_bound(state.waitlist_changes,PolicyKey{product.id,day},{},policy_key);
    if (after==state.waitlist_changes.begin() || (after-1)->product_id!=product.id) return {};
    return (after-1)->new_tasks_per_day;
}
std::uint64_t waitlist_established_tasks(const MarketState& state,std::string_view product,OrdinalDay day) {
    return established(state,product,day).total;
}
void validate_waitlist_change(const MarketState& state,const CapacityState& capacity,OrdinalDay day,
                              std::string_view product_id,std::optional<std::uint64_t> limit) {
    const auto today=state.closed_through?advance_days(*state.closed_through,Days{1}):state.starts_on;
    if (day!=today) invalid("Waitlist changes apply before today's customer work");
    const auto& product=find(state.products,product_id);
    const auto& work=find(capacity.projects,product.capacity_project_id);
    if (!product.available || work.status!=ProjectStatus::active || product.retirement)
        invalid("Choose an operating service without a retirement notice");
    if (limit && (*limit>market_task_limit || *limit>work.tasks_per_day))
        invalid("Choose a daily intake limit no higher than the service's task capacity");
    if (market_waitlist_limit(state,product,day)==limit) invalid("Those enrollment terms are already in effect");
    const auto key=PolicyKey{product_id,day};
    const auto position=std::ranges::lower_bound(state.waitlist_changes,key,{},policy_key);
    if (state.waitlist_changes.size()>=market_waitlist_change_limit &&
        (position==state.waitlist_changes.end() || policy_key(*position)!=key))
        invalid("The service enrollment history is full");
}
void set_market_waitlist(MarketState& state,const CapacityState& capacity,OrdinalDay day,
                         std::string_view product_id,std::optional<std::uint64_t> limit) {
    validate_waitlist_change(state,capacity,day,product_id,limit);
    const auto key=PolicyKey{product_id,day};
    const auto position=std::ranges::lower_bound(state.waitlist_changes,key,{},policy_key);
    if (position!=state.waitlist_changes.end() && policy_key(*position)==key) position->new_tasks_per_day=limit;
    else state.waitlist_changes.insert(position,{std::string(product_id),day,limit});
}

void market_detail::apply_waitlist_limits(const MarketState& state,OrdinalDay day,const std::vector<AllocationProduct>& products,
                                          std::vector<AllocationSegment>& segments) {
    if (state.waitlist_changes.empty()) return;
    const auto* previous=state.days.empty()?nullptr:&state.days.back();
    for (const auto& product:state.products) {
        const auto limit=market_waitlist_limit(state,product,day);
        if (!limit) continue;
        const auto& supply=find(products,product.id);
        const auto recent=established(state,product.id,day);
        std::vector<Intake> groups;
        std::uint64_t incumbents=0;
        for (auto& segment:segments) {
            const auto edge=std::ranges::find(segment.edges,product.id,&AllocationEdge::product_id);
            if (edge==segment.edges.end() || edge->weight==0) continue;
            auto ceiling=std::min(edge->task_limit.value_or(market_task_limit),segment.tasks);
            if (supply.price.value()>0) ceiling=std::min(ceiling,static_cast<std::uint64_t>(segment.budget.value()/supply.price.value()));
            if (supply.customer_compute_per_task.value()>0)
                ceiling=std::min(ceiling,maximum_tasks(MilliCU{segment.customer_compute.value()},supply.customer_compute_per_task));
            const auto retained=std::min(ceiling,sold(recent.day,{product.id,segment.id}));
            edge->waitlist_interest_limit=ceiling;
            edge->retained_tasks=retained;
            edge->task_limit=retained;
            incumbents+=retained;
            if (ceiling>retained) groups.push_back({&segment.id,&*edge,ceiling-retained,waiting_group(previous,{product.id,segment.id})});
        }
        grant_intake(groups,std::min(*limit,excess(supply.capacity,incumbents)));
    }
}

void market_detail::settle_waitlists(const MarketState& state,MarketDay& day,const std::vector<AllocationSegment>& segments,
                                    const AllocationResult& allocation) {
    if (state.waitlist_changes.empty()) return;
    const auto* previous=state.days.empty()?nullptr:&state.days.back();
    for (const auto& segment:segments) {
        if (std::ranges::none_of(segment.edges,[](const auto& edge){return edge.waitlist_interest_limit.has_value();})) continue;
        const auto& result=find(allocation.segments,segment.id);
        for (const auto& edge:segment.edges) {
            if (!edge.waitlist_interest_limit) continue;
            const auto sale=std::ranges::find(result.deliveries,edge.product_id,&AllocationDelivery::product_id);
            const auto interest=std::ranges::find(result.waitlist_interest,edge.product_id,&AllocationInterest::product_id);
            const auto delivered=sale==result.deliveries.end()?0:sale->tasks;
            const auto admitted=excess(delivered,edge.retained_tasks);
            const auto waiting=interest==result.waitlist_interest.end()?0:excess(interest->tasks,std::max(edge.retained_tasks,delivered));
            if (admitted==0 && waiting==0) continue;
            const auto* prior=waiting_group(previous,{edge.product_id,segment.id});
            day.waitlists.push_back({edge.product_id,segment.id,admitted,waiting,
                waiting>0?std::optional{prior?prior->waiting_since.value_or(day.day):day.day}:std::nullopt});
        }
    }
    day.waitlists.sort([](const auto& a,const auto& b){return customer_key(a)<customer_key(b);});
}

void market_detail::validate_waitlist_history(const MarketState& state) {
    const auto today=state.closed_through?advance_days(*state.closed_through,Days{1}):state.starts_on;
    if (state.waitlist_changes.size()>market_waitlist_change_limit) invalid("Service enrollment history exceeds its bound");
    std::optional<PolicyKey> previous_key;
    for (const auto& change:state.waitlist_changes) {
        const auto& product=find(state.products,change.product_id);
        const auto key=policy_key(change);
        if ((previous_key && *previous_key>=key) || change.starts_on<state.starts_on || change.starts_on>today ||
            (change.new_tasks_per_day && *change.new_tasks_per_day>market_task_limit) ||
            (product.retirement && change.starts_on>product.retirement->announced_on))
            invalid("Enrollment terms have an invalid date, limit or duplicate change");
        previous_key=key;
    }
    if (state.waitlist_changes.empty()) {
        if (state.days.totals().waitlist_rows!=0) invalid("Waitlist activity has no dated policy");
        return;
    }
    const MarketDay* previous=nullptr;
    struct CustomerBudget {std::uint64_t signed_tasks=0, waiting=0; Cents charges{0};};
    struct ServiceAudit {
        bool prepared=false;
        std::optional<std::uint64_t> limit;
        RecentCustomers established;
        std::uint64_t admitted=0;
    };
    std::vector<ServiceAudit> services(state.products.size());
    std::vector<CustomerBudget> budgets(state.segments.size());
    for (const auto& day:state.days) {
        std::fill(services.begin(),services.end(),ServiceAudit{});
        const auto prepare=[&](const MarketProduct& product) -> ServiceAudit& {
            auto& row=services[static_cast<std::size_t>(&product-state.products.data())];
            if (!row.prepared) {
                row.prepared=true;
                row.limit=market_waitlist_limit(state,product,day.day);
                if (row.limit) row.established=established(state,product.id,day.day);
            }
            return row;
        };
        for (const auto& sale:day.discretionary) {
            if (!sale.local_copy_project.empty()) continue;
            const auto& product=find(state.products,sale.product_id);
            const auto& service=prepare(product);
            if (!service.limit) continue;
            const CustomerKey key{sale.product_id,sale.segment_id};
            const auto admitted=excess(sale.tasks,sold(service.established.day,key));
            const auto* row=waiting_group(&day,key);
            if (admitted!=(row?row->admitted_tasks:0)) invalid("Waitlist admissions differ from settled discretionary work");
        }
        if (day.waitlists.empty()) {
            previous=&day;
            continue;
        }
        std::fill(budgets.begin(),budgets.end(),CustomerBudget{});
        const auto customer_budget=[&](const MarketSegment& segment) -> CustomerBudget& {
            return budgets[static_cast<std::size_t>(&segment-state.segments.data())];
        };
        for (const auto& work:day.contracts) {
            const auto& contract=find(state.contracts,work.contract_id);
            auto& budget=customer_budget(find(state.segments,contract.segment_id));
            budget.signed_tasks+=work.promised_tasks;
            budget.charges=checked_unit_add(budget.charges,work.customer_charge());
        }
        std::optional<CustomerKey> previous_group;
        for (const auto& row:day.waitlists) {
            const auto key=customer_key(row);
            const auto& product=find(state.products,row.product_id);
            const auto& segment=find(state.segments,row.segment_id);
            auto& service=prepare(product);
            const auto pref=std::ranges::find(segment.preferences,product.id,&MarketPreference::product_id);
            const auto price=market_price_on(state,product.id,day.day);
            const auto* queued=waiting_group(previous,key);
            const auto age=row.waiting_tasks>0?std::optional{queued?queued->waiting_since.value_or(day.day):day.day}:std::nullopt;
            const auto sales=sold(&day,key);
            const auto incumbent=sold(service.established.day,key);
            if (!service.limit || (previous_group && *previous_group>=key) || (row.admitted_tasks==0 && row.waiting_tasks==0) ||
                row.waiting_since!=age || pref==segment.preferences.end() || !pref->eligible || !market_access(product,segment) ||
                price>pref->willingness_to_pay || row.admitted_tasks!=excess(sales,incumbent) ||
                row.waiting_tasks>market_task_limit)
                invalid("Waitlist activity has an invalid policy, buyer, date or admission count");
            previous_group=key;
            service.admitted+=row.admitted_tasks;
            if (service.admitted>*service.limit) invalid("Service exceeded its published daily intake limit");
            if (row.waiting_tasks==0) continue;
            auto& budget=customer_budget(segment);
            budget.waiting+=row.waiting_tasks;
            budget.charges=checked_unit_add(budget.charges,Cents{mul_div_floor(row.waiting_tasks,static_cast<std::uint64_t>(price.value()),1).quotient});
            const auto remaining=excess(effective_market_demand(state,segment,day.day),budget.signed_tasks);
            if (budget.waiting>remaining || budget.charges>segment.daily_budget ||
                row.waiting_tasks>excess(remaining,std::max(sales,incumbent)))
                invalid("Waitlisted interest exceeds the available customer demand or budget");
        }
        previous=&day;
    }
}
} // namespace pgp::core
