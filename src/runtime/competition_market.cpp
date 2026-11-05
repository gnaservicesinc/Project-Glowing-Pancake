#include "competition_internal.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (it==rows.end()) invalid("Industry allocation refers to an unknown product or segment");return *it;
}
bool operating(const SimulationState& state) {
    if (state.phase==DayPhase::closed || state.phase==DayPhase::finished) return false;
    if (state.liquidity && state.liquidity->status==LiquidityStatus::workout && state.date>=*liquidity_report(*state.liquidity).deadline) return false;
    return true;
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
    std::vector<MarketAdoption> result;
    for (const auto& segment:world.simulation.market->segments) {
        std::map<std::string,std::uint64_t> observed{{"outside",0}};std::uint64_t demand=0,delivered=0;
        for (auto date=begin;date<=day;date=advance_days(date,Days{1})) {
            const auto* player=settled(world.simulation,date);if (!player) continue;
            for (const auto& row:player->segments) if (row.segment_id==segment.id) demand+=row.demand;
            for (const auto* seller:sellers(world)) {
                const auto* record=settled(*seller,date);if (!record) continue;
                for (const auto& row:record->discretionary) if (row.segment_id==segment.id) {observed[seller->market->owner+":"+row.product_id]+=row.tasks;delivered+=row.tasks;}
                for (const auto& row:record->contracts) {
                    const auto& contract=find(seller->market->contracts,row.contract_id);
                    if (contract.segment_id==segment.id) {observed[seller->market->owner+":"+contract.product_id]+=row.delivered_tasks;delivered+=row.delivered_tasks;}
                }
            }
        }
        if (delivered>demand) invalid("Industry useful work duplicates customer demand");
        if (demand==0) continue;
        observed["outside"]=demand-delivered;std::vector<WeightedQuota> quotas;
        for (const auto& [id,tasks]:observed) quotas.push_back({id,tasks});
        MarketAdoption row{day,segment.id,demand,{}};
        for (const auto& share:apportion_units(10000,quotas)) row.shares.push_back({share.id,BasisPoints{share.units}});
        result.push_back(std::move(row));
    }
    return result;
}
}
IndustryAllocations prepare_competition_day(WorldState& world,const CompiledProgram& program) {
    if (!world.competition || !world.competition->setup) return {};
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
        opportunities.emplace(seller->market->owner,market_opportunity(*seller->market,*seller->capacity,day,&installed));
    std::vector<AllocationProduct> products;std::vector<AllocationSegment> segments;
    for (const auto& row:world.simulation.market->segments) {
        const auto& own=find(opportunities.at(world.company_id).segments,row.id);
        AllocationSegment segment{row.id,effective_market_demand(*world.simulation.market,row,day),row.daily_budget,own.outside_weight,{}};
        std::uint64_t promised=0;Cents charged{0};
        for (const auto& [owner,opportunity]:opportunities) {
            for (const auto& claim:opportunity.contracts) if (claim.segment_id==row.id) {promised+=claim.promised;charged=checked_unit_add(charged,claim.charged);}
            const auto& source=find(opportunity.segments,row.id);
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
std::vector<MarketAdoption> industry_adoption_snapshot(const WorldState& world) {
    std::vector<MarketAdoption> result;std::set<std::string> segments;
    if (world.competition && world.competition->setup)
        for (auto row=world.competition->adoptions.rbegin();row!=world.competition->adoptions.rend();++row) {
            if (segments.insert(row->segment_id).second) result.push_back(*row);
            if (segments.size()==world.simulation.market->segments.size()) break;
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
    std::vector<MarketAdoption> expected;
    for (const auto& record:world.simulation.market->days) {
        for (const auto& segment:record.segments) {
            std::uint64_t delivered=0,promised=0;Cents charged{0};
            for (const auto* seller:participants) {
                const auto* day=settled(*seller,record.day);if (!day) continue;
                const auto& own=find(seller->market->segments,segment.segment_id);
                const auto& base=find(world.simulation.market->segments,segment.segment_id);
                if (own.daily_tasks!=base.daily_tasks || own.daily_budget!=base.daily_budget || own.outside_utility!=base.outside_utility ||
                    own.reservation_threshold!=base.reservation_threshold || own.maximum_monthly_migration!=base.maximum_monthly_migration)
                    invalid("Rival market changed common customer terms");
                for (const auto& row:day->segments) if (row.segment_id==segment.segment_id) {
                    if (row.demand!=segment.demand || row.budget!=segment.budget) invalid("A seller duplicated or changed the shared customer snapshot");
                    delivered+=row.contract_tasks+row.discretionary_tasks;charged=checked_unit_add(charged,row.charged);
                }
                for (const auto& work:day->discretionary) if (work.segment_id==segment.segment_id) {
                    std::uint64_t installed=0;const auto id=seller->market->owner+":"+work.product_id;
                    for (auto prior=expected.rbegin();prior!=expected.rend();++prior) if (prior->segment_id==segment.segment_id) {
                        for (const auto& share:prior->shares) if (share.product_id==id) installed=static_cast<std::uint64_t>(share.share.value());
                        break;
                    }
                    const auto cap=mul_div_floor(segment.demand,std::min<std::uint64_t>(10000,installed+static_cast<std::uint64_t>(base.maximum_monthly_migration.value())),10000).quotient;
                    if (work.tasks>cap) invalid("Industry delivery bypassed the actual shared monthly migration limit");
                }
                for (const auto& row:day->contracts) if (find(seller->market->contracts,row.contract_id).segment_id==segment.segment_id) promised+=row.promised_tasks;
            }
            if (delivered>segment.demand || promised>segment.demand || charged>segment.budget) invalid("Industry history duplicates useful work or customer budget");
        }
        const auto civil=from_ordinal(record.day);
        if (civil.day==days_in_month(civil.year,civil.month)) {auto rows=adoption(world,record.day);expected.insert(expected.end(),rows.begin(),rows.end());}
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
