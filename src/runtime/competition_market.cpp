#include "competition_internal.hpp"
#include <pgp/runtime/society.hpp>
#include <pgp/runtime/authority.hpp>
#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
bool competition_detail::operating(const core::SimulationState& state) {
    using namespace core;
    if (state.phase==DayPhase::closed || state.phase==DayPhase::finished) return false;
    if (state.liquidity && state.liquidity->status==LiquidityStatus::workout && state.date>=*liquidity_report(*state.liquidity).deadline) return false;
    return true;
}
namespace {
using namespace core;
using competition_detail::operating;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (it==rows.end()) invalid("Industry allocation refers to an unknown product or segment");return *it;
}
const MarketDay* settled(const SimulationState& state,OrdinalDay day) {
    if (!state.market || day<state.market->starts_on) return nullptr;
    const auto offset=static_cast<std::uint64_t>(day.value()-state.market->starts_on.value());
    if (offset>=state.market->days.size()) return nullptr;
    return &state.market->days[static_cast<std::size_t>(offset)];
}
std::vector<const SimulationState*> sellers(const WorldState& world) {
    std::vector<const SimulationState*> result{&world.simulation};
    for (const auto& rival:world.competition->companies) if (rival.planning && rival.planning->entered_on) result.push_back(&rival.simulation);
    return result;
}
std::vector<MarketAdoption> adoption(const WorldState& world,OrdinalDay day) {
    const auto civil=from_ordinal(day);const auto begin=to_ordinal({civil.year,civil.month,1});
    const auto& player_market = *world.simulation.market;
    const bool regional = regional_market(player_market);
    const auto participants = sellers(world);
    struct SegmentReview {
        std::map<std::string,std::uint64_t> work;
        std::uint64_t demand = 0;
        std::uint64_t delivered = 0;
    };
    std::map<std::string_view, SegmentReview> reviews;
    for (auto date = begin; date <= day; date = advance_days(date, Days{1})) {
        const auto* player = settled(world.simulation,date);
        if (!player) continue;
        if (regional) {
            for (const auto& segment : player_market.segments)
                reviews[segment.id].demand += effective_market_demand(player_market, segment, date);
        } else {
            for (const auto& segment : player->segments) reviews[segment.segment_id].demand += segment.demand;
        }
        for (const auto* seller : participants) {
            const auto* record = settled(*seller,date);
            if (!record) continue;
            const auto prefix = seller->market->owner + ":";
            for (const auto& row : record->discretionary) {
                auto& review = reviews[row.segment_id];
                review.work[prefix + row.product_id] += row.tasks;
                review.delivered += row.tasks;
            }
            for (const auto& row : record->contracts) {
                if (regional && row.delivered_tasks == 0) continue;
                const auto& contract = find(seller->market->contracts,row.contract_id);
                auto& review = reviews[contract.segment_id];
                review.work[prefix + contract.product_id] += row.delivered_tasks;
                review.delivered += row.delivered_tasks;
            }
        }
    }
    std::vector<MarketAdoption> result;
    for (const auto& segment : player_market.segments) {
        auto& review = reviews[segment.id];
        if (review.delivered > review.demand) invalid("Industry useful work duplicates customer demand");
        if (review.demand == 0 || (regional && review.delivered == 0)) continue;
        review.work["outside"] = review.demand - review.delivered;
        std::vector<WeightedQuota> quotas;
        for (const auto& [id,tasks]:review.work) quotas.push_back({id,tasks});
        MarketAdoption row{day,segment.id,review.demand,{}};
        for (const auto& share:apportion_units(10000,quotas)) row.shares.push_back({share.id,BasisPoints{share.units}});
        result.push_back(std::move(row));
    }
    return result;
}
}
IndustryAllocations prepare_competition_day(WorldState& world,const CompiledProgram& program) {
    if (!world.competition || !world.competition->setup || !operating(world.simulation)) return {};
    const auto day=world.simulation.date;
    for (auto& rival:world.competition->companies) competition_detail::prepare_rival_morning(world,program,rival);
    // Copy each newly public demand incident into the same owning market rules.
    // A new entrant joins an active incident from its actual opening date.
    for (auto& rival:world.competition->companies) if (rival.planning && rival.planning->entered_on && operating(rival.simulation)) {
        auto& state=rival.simulation;
        for (auto change:world.simulation.market->demand_changes) {
            if (change.starts_on>day || change.expires_before<=state.market->starts_on ||
                std::any_of(state.market->demand_changes.begin(),state.market->demand_changes.end(),[&](const auto& row){return row.id==change.id;})) continue;
            change.starts_on=std::max(change.starts_on,state.market->starts_on);
            if (change.starts_on!=day) invalid("A rival missed a public demand incident");
            Simulation candidate{state,TrustedSimulationState{}};
            const auto result=candidate.submit_in_transaction({state.revision,AddMarketDemandChange{change}});
            if (!result.accepted()) throw std::invalid_argument(result.explanation);
            state=std::move(candidate).release_checkpoint();
        }
    }
    const auto installed=industry_adoption_snapshot(world);
    std::map<std::string,MarketOpportunity,std::less<>> opportunities;
    for (const auto* seller:sellers(world)) if (operating(*seller) && seller->market)
        opportunities.emplace(seller->market->owner,market_opportunity(*seller->market,*seller->capacity,day,&installed,
            regional_market_conditions(world,*seller,day)));
    std::vector<AllocationProduct> products;std::vector<AllocationSegment> segments;
    for (std::size_t index=0; index<world.simulation.market->segments.size(); ++index) {
        const auto& row = world.simulation.market->segments[index];
        const auto& own = opportunities.at(world.company_id).segments.at(index);
        AllocationSegment segment{row.id,effective_market_demand(*world.simulation.market,row,day),row.daily_budget,own.outside_weight,{},row.customer_compute};
        std::uint64_t promised=0;Cents charged{0};
        for (const auto& [owner,opportunity]:opportunities) {
            const auto& claim = opportunity.contracts.at(index);
            const auto& source = opportunity.segments.at(index);
            if (claim.segment_id != row.id || source.id != row.id || source.customer_compute != row.customer_compute)
                invalid("Sellers must share the same ordered customer and hardware snapshot");
            promised += claim.promised;
            charged = checked_unit_add(charged,claim.charged);
            for (auto edge:source.edges) {edge.product_id=owner+":"+edge.product_id;segment.edges.push_back(std::move(edge));}
        }
        if (promised>segment.tasks || charged>segment.budget) invalid("Signed industry work exceeds the single customer task/budget snapshot");
        segment.tasks-=promised;segment.budget=checked_unit_subtract(segment.budget,charged);segments.push_back(std::move(segment));
    }
    for (const auto& [owner,opportunity]:opportunities) for (auto product:opportunity.products) {product.id=owner+":"+product.id;products.push_back(std::move(product));}
    const auto shared=allocate_demand(products,segments);IndustryAllocations result;
    for (const auto& [owner,opportunity]:opportunities) {
        AllocationResult assigned;assigned.rounds=shared.rounds;assigned.round_bound=shared.round_bound;
        for (const auto& input:opportunity.segments) {
            AllocationSegmentResult segment{input.id,{},input.tasks,input.budget,{}};
            const auto& total=find(shared.segments,input.id);const auto prefix=owner+":";
            for (auto delivery:total.deliveries) if (delivery.product_id.starts_with(prefix)) {
                delivery.product_id.erase(0,prefix.size());segment.outside_tasks-=delivery.tasks;
                segment.remaining_budget=checked_unit_subtract(segment.remaining_budget,delivery.cost);segment.deliveries.push_back(std::move(delivery));
            }
            if (segment.outside_tasks>0) segment.outside.push_back({AllocationOutsideReason::preference,segment.outside_tasks});
            assigned.segments.push_back(std::move(segment));
        }
        result.emplace(owner,std::move(assigned));
    }
    return result;
}
std::vector<MarketAdoption> industry_adoption_snapshot(const WorldState& world, std::optional<OrdinalDay> work_day) {
    std::vector<MarketAdoption> result;std::set<std::string> segments;
    if (world.competition && world.competition->setup) {
        const auto civil = from_ordinal(work_day.value_or(world.simulation.date));
        const auto recent = OrdinalDay{checked_subtract(to_ordinal({civil.year,civil.month,1}).value(),1)};
        for (auto row=world.competition->adoptions.rbegin();row!=world.competition->adoptions.rend();++row) {
            if (regional_market(*world.simulation.market) && row->reviewed_on < recent) break;
            if (segments.insert(row->segment_id).second) result.push_back(*row);
            if (segments.size()==world.simulation.market->segments.size()) break;
        }
    }
    return result;
}
namespace competition_detail {
const SimulationState* company(const WorldState& world,std::string_view id) {
    if (id==world.company_id) return &world.simulation;
    if (world.competition) for (const auto& row:world.competition->companies)
        if (row.definition_id==id && (!row.planning || row.planning->entered_on)) return &row.simulation;
    return nullptr;
}
SimulationState* company(WorldState& world,std::string_view id) {
    return const_cast<SimulationState*>(company(static_cast<const WorldState&>(world),id));
}
void append_industry_adoption(WorldState& world,OrdinalDay day) {
    const auto civil=from_ordinal(day);if (civil.day!=days_in_month(civil.year,civil.month)) return;
    auto rows=adoption(world,day);auto& target=world.competition->adoptions;
    if (target.size()+rows.size()>market_record_limit) invalid("Industry adoption history exceeds its bound");
    target.insert(target.end(),std::make_move_iterator(rows.begin()),std::make_move_iterator(rows.end()));
}
void validate_industry(const WorldState& world,bool deep) {
    if (!world.simulation.market || world.competition->adoptions.size()>market_record_limit) invalid("Planned competition needs a bounded shared market");
    const auto participants=sellers(world);
    // Protect signed customer promises before a new command can overbook them.
    for (const auto& segment:world.simulation.market->segments) {
        std::uint64_t promised=0;Cents budget{0};
        for (const auto* seller:participants) if (seller->market && operating(*seller)) for (const auto& contract:seller->market->contracts)
            if (contract.segment_id==segment.id && (contract.status==ContractStatus::pending || contract.status==ContractStatus::active)) {
                promised+=contract.tasks_per_day;
                budget=checked_unit_add(budget,Cents{checked_add(contract.monthly_fee.value()/28+(contract.monthly_fee.value()%28!=0?1:0),
                    static_cast<std::int64_t>(mul_div_floor(static_cast<std::uint64_t>(contract.usage_price.value()),contract.tasks_per_day,1).quotient))});
            }
        if (promised>segment.daily_tasks || budget>segment.daily_budget) invalid("Industry commitments overbook the same customers");
    }
    if (!deep) return;
    const auto& market = *world.simulation.market;
    const bool regional = regional_market(market);
    std::map<std::string_view, std::size_t, std::less<>> indices;
    for (std::size_t i = 0; i < market.segments.size(); ++i) indices.emplace(market.segments[i].id, i);
    for (const auto* seller : participants) {
        if (!seller->market || seller->market->segments.size() != market.segments.size())
            invalid("A seller changed the common customer catalog");
        for (std::size_t i = 0; i < market.segments.size(); ++i) {
            auto own = seller->market->segments[i];
            auto base = market.segments[i];
            own.preferences.clear();
            base.preferences.clear();
            if (own != base) invalid("A seller changed common customer or hardware terms");
        }
    }
    struct DayTotals {
        std::uint64_t delivered = 0;
        std::uint64_t promised = 0;
        Cents charged{0};
        MilliCU local_work{0};
    };
    std::vector<DayTotals> totals(market.segments.size());
    std::vector<std::vector<InstalledShare>> installed(market.segments.size());
    std::vector<MarketAdoption> expected;
    for (const auto& record : market.days) {
        std::fill(totals.begin(), totals.end(), DayTotals{});
        for (const auto* seller : participants) {
            const auto* day = settled(*seller, record.day);
            if (!day) continue;
            for (const auto& row : day->segments) {
                auto& total = totals[indices.at(row.segment_id)];
                total.delivered += row.contract_tasks + row.discretionary_tasks;
                total.charged = checked_unit_add(total.charged, row.charged);
            }
            for (const auto& row : day->contracts)
                totals[indices.at(find(seller->market->contracts,row.contract_id).segment_id)].promised += row.promised_tasks;
            for (const auto& sale : day->discretionary) {
                const auto index = indices.at(sale.segment_id);
                const auto& segment = market.segments[index];
                auto& total = totals[index];
                total.local_work = checked_unit_add(total.local_work, sale.customer_compute);
                const auto id = seller->market->owner + ":" + sale.product_id;
                const auto share = std::find_if(installed[index].begin(), installed[index].end(), [&](const auto& row) { return row.product_id == id; });
                const auto incumbent = share == installed[index].end() ? 0 : share->share.value();
                const auto limit = market_migration(find(seller->market->products, sale.product_id), segment).value();
                const auto demand = effective_market_demand(market, segment, record.day);
                const auto cap = mul_div_floor(demand, static_cast<std::uint64_t>(std::min<std::int64_t>(10000, incumbent + limit)), 10000).quotient;
                if (sale.tasks > cap) invalid("Industry delivery bypassed its shared monthly migration limit");
            }
        }
        for (std::size_t i = 0; i < totals.size(); ++i) {
            const auto& total = totals[i];
            if (total.delivered == 0 && total.promised == 0 && total.charged.value() == 0) continue;
            const auto& segment = market.segments[i];
            const auto demand = effective_market_demand(market, segment, record.day);
            if (total.delivered > demand || total.promised > demand || total.charged > segment.daily_budget ||
                total.local_work.value() > segment.customer_compute.value())
                invalid("Industry history duplicates useful work, buyer hardware or customer budget");
        }
        const auto civil = from_ordinal(record.day);
        if (civil.day == days_in_month(civil.year,civil.month)) {
            auto rows = adoption(world,record.day);
            if (regional) for (auto& shares : installed) shares.clear();
            for (const auto& row : rows) installed[indices.at(row.segment_id)] = row.shares;
            expected.insert(expected.end(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
        }
    }
    if (expected!=world.competition->adoptions) invalid("Industry adoption differs from actual work across all sellers");
    std::map<std::pair<std::string,std::string>,OrdinalDay> first,released;
    for (const auto* seller:participants) if (seller->market) for (const auto& day:seller->market->days) {
        for (const auto& row:day.discretionary) if (row.tasks>0) first.try_emplace({seller->market->owner,row.product_id},day.day);
        for (const auto& row:day.contracts) if (row.delivered_tasks>0) first.try_emplace({seller->market->owner,find(seller->market->contracts,row.contract_id).product_id},day.day);
    }
    for (const auto& release:world.competition->releases)
        if (!released.emplace(std::pair{release.company_id,release.product_id},release.day).second) invalid("Public release has a duplicate owner/product identity");
    if (first!=released) invalid("Public release history differs from first actual customer deliveries");
}
}
void validate_public_insight(const WorldState& world,const ResearchInsight& insight) {
    if (!world.competition || !world.competition->setup) invalid("This scenario has no public research diffusion contract");
    const auto& setup=*world.competition->setup;const auto* publisher=competition_detail::company(world,insight.publisher);
    if (!publisher || !publisher->lab || insight.credit!=setup.publication_credit || setup.publication_credit.value()==0 ||
        insight.observed_on<advance_days(insight.published_on,setup.publication_delay) ||
        std::none_of(publisher->lab->publications.begin(),publisher->lab->publications.end(),[&](const auto& row){return row.research_id==insight.research_id && row.day==insight.published_on;}))
        invalid("Research insight lacks an actual public result and its adoption delay");
}
}
