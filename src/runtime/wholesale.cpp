#include "competition_internal.hpp"
#include <pgp/runtime/society.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* explanation) { throw std::invalid_argument(explanation); }

template<class Rows> auto& source(Rows& rows,std::string_view id) {
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row) {return row.id==id;});
    if (found==rows.end()) invalid("The capacity source does not exist");
    return *found;
}
const content::WholesalePlan& terms(std::span<const content::WholesalePlan> plans,std::string_view id) {
    const auto found=std::find_if(plans.begin(),plans.end(),[&](const auto& row) {return row.id==id;});
    if (found==plans.end()) invalid("Choose an authored wholesale offer");
    return *found;
}
template<class World> auto& buyer(World& world,std::string_view id) {
    const auto found=std::find_if(world.competition->companies.begin(),world.competition->companies.end(),
        [&](const auto& row) {return row.definition_id==id;});
    if (found==world.competition->companies.end() || found->response || !found->planning ||
        !found->planning->entered_on || !found->simulation.capacity || !found->simulation.accounts)
        invalid("Wholesale requires an entered independent operating buyer");
    return *found;
}
template<class State> auto& escrow(State& state,std::uint64_t id) {
    const auto found=std::lower_bound(state.reservations.begin(),state.reservations.end(),id,
        [](const auto& row,std::uint64_t key) {return row.id<key;});
    if (found==state.reservations.end() || found->id!=id) invalid("The wholesale buyer lost its funded reservation");
    return *found;
}
std::string imported_id(const WholesaleContract& contract) { return "import/"+contract.id; }
OrdinalDay last_boundary(const WholesaleContract& contract) { return contract.terminated_on.value_or(contract.ends_before); }
Cents ceiling(Cents daily,Days days) {
    return Cents{mul_div_floor(static_cast<std::uint64_t>(daily.value()),static_cast<std::uint64_t>(days.value()),1).quotient};
}
Days duration(OrdinalDay start,OrdinalDay end) { return Days{checked_subtract(end.value(),start.value())}; }
bool closed(const SimulationState& state) {
    return state.phase==DayPhase::finished || state.phase==DayPhase::closed;
}
CapacitySource imported_source(const CapacitySource& owned,const content::WholesalePlan& plan,
    const WholesaleContract& contract,std::string_view seller,MilliCUPerDay available) {
    return {imported_id(contract),"slice/"+contract.id,contract.buyer_id,std::string(seller),CapacitySourceKind::imported,
        plan.capacity,available,owned.memory_mib,owned.interconnect,plan.energy_included,contract.starts_on};
}
bool compatible(const CapacitySource& owned,const content::WholesalePlan& plan) {
    return owned.kind==CapacitySourceKind::owned && owned.memory_mib>=plan.minimum_memory_mib &&
        owned.interconnect==plan.interconnect;
}
Cents daily_ceiling(const SimulationState& seller,const CapacitySource& owned,const content::WholesalePlan& plan) {
    return checked_unit_add(plan.daily_fee,plan.energy_included?Cents{0}:wholesale_electricity(seller,owned.id,MilliCU{plan.capacity.value()}));
}
WholesaleDay usage(const SimulationState& seller,const content::WholesalePlan& plan,std::string_view source_id,
    MilliCUPerDay available,MilliCU used) {
    if (available>plan.capacity || used.value()>available.value()) invalid("Wholesale work exceeds deliverable capacity");
    const auto guarantee=mul_div_floor(static_cast<std::uint64_t>(plan.capacity.value()),
        static_cast<std::uint64_t>(plan.availability.value()),10000);
    const auto guaranteed=guarantee.quotient+(guarantee.remainder>0?1:0);
    const auto billed=mul_div_floor(static_cast<std::uint64_t>(plan.daily_fee.value()),
        std::min(static_cast<std::uint64_t>(available.value()),guaranteed),guaranteed);
    return {available,used,checked_unit_subtract(plan.daily_fee,Cents{billed.quotient}),
        plan.energy_included?Cents{0}:wholesale_electricity(seller,source_id,used),0,0};
}
Cents payment(const content::WholesalePlan& plan,const WholesaleDay& row) {
    return checked_unit_add(checked_unit_subtract(plan.daily_fee,row.service_credit),row.electricity);
}
const SourceDayUsage& recorded_source(const CapacityDay& day,std::string_view id) {
    const auto found=std::lower_bound(day.sources.begin(),day.sources.end(),id,
        [](const auto& row,std::string_view key) {return row.source_id<key;});
    if (found==day.sources.end() || found->source_id!=id) invalid("Wholesale work has no dated source record");
    return *found;
}
const CapacityDay& recorded_day(const SimulationState& state,OrdinalDay date) {
    const auto& capacity=*state.capacity;
    if (date<capacity.starts_on || !capacity.closed_through || date>*capacity.closed_through)
        invalid("Wholesale work must be settled on the same day by both companies");
    return capacity.days.at(static_cast<std::size_t>(date.value()-capacity.starts_on.value()));
}
void finish_imported_work(RivalCompany& rival,std::string_view source_id) {
    if (closed(rival.simulation)) return;
    std::vector<std::string> unfinished;
    for (const auto& project:rival.simulation.capacity->projects)
        if (project.source_id==source_id && project.status==ProjectStatus::active)
            unfinished.push_back(project.id);
    for (const auto& id:unfinished) if (!competition_detail::submit_rival(rival,CancelProject{id}))
        invalid("An expired wholesale slice could not release its unfinished work");
}
}

std::span<const content::WholesalePlan> wholesale_plans(const WorldState& world,const CompiledProgram& program) {
    if (!world.competition || !world.competition->setup || !world.simulation.infrastructure) return {};
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    const auto* catalog=scenario?content::regional_markets(program.collection,*scenario):nullptr;
    return catalog?std::span<const content::WholesalePlan>{catalog->wholesale}:std::span<const content::WholesalePlan>{};
}

WholesaleQuote wholesale_quote(const WorldState& world,const CompiledProgram& program,std::string_view plan_id,
    std::string_view source_id,std::string_view buyer_id) {
    const auto& plan=terms(wholesale_plans(world,program),plan_id);
    const auto& seller=world.simulation;
    const auto& rival=buyer(world,buyer_id);
    const auto& customer=rival.simulation;
    const auto day=seller.date;
    const auto end=advance_days(day,plan.term);
    if (!competition_detail::operating(seller) || !competition_detail::operating(customer) ||
        customer.date!=day || end>seller.end_date || !rival.parent_company.empty() ||
        customer.capacity->available_staff==0 || customer.liquidity->status!=LiquidityStatus::operating)
        invalid("The buyer needs an operating team, sufficient runway and time for the full term");
    const auto& state=*world.competition;
    if (state.wholesale.size()>=wholesale_contract_limit || customer.capacity->sources.size()>=capacity_source_limit)
        invalid("The bounded wholesale register is full");
    if (std::any_of(state.wholesale.begin(),state.wholesale.end(),[&](const auto& row) {
        return row.buyer_id==buyer_id && day<last_boundary(row);
    })) invalid("The buyer already has a live wholesale agreement");
    const auto& owned=source(seller.capacity->sources,source_id);
    if (!compatible(owned,plan) || owned.owner!=world.company_id || owned.provider!=world.company_id)
        invalid("Only compatible commissioned hardware owned by the seller can be reserved");
    auto free=company_capacity(owned);
    for (const auto& work:seller.capacity->projects) if (work.status==ProjectStatus::active && work.source_id==source_id)
        free=MilliCUPerDay{std::max<std::int64_t>(0,checked_subtract(free.value(),work.requested.value()))};
    if (free<plan.capacity) invalid("This rack does not have enough powered capacity after existing reservations");
    const auto funded=ceiling(daily_ceiling(seller,owned,plan),plan.term);
    const auto& policy=*rival_definition(program,buyer_id).planning;
    Cents bills{0},reserved{0};
    for (const auto& row:customer.accounts->recurring)
        if (row.starts_on<=day && (!row.ends_before || day<*row.ends_before)) bills=checked_unit_add(bills,row.monthly_amount);
    for (const auto& row:customer.reservations) reserved=checked_unit_add(reserved,row.reserved);
    const auto runway=checked_unit_add(policy.reserve_floor,ceiling(bills,Days{policy.runway_months}));
    if (checked_unit_subtract(customer.cash,reserved)<checked_unit_add(funded,runway))
        invalid("The buyer cannot fund this full term and protect its operating runway");
    return {end,funded};
}

void sign_wholesale(WorldState& world,const CompiledProgram& program,std::string_view plan_id,std::string_view source_id,std::string_view buyer_id) {
    const auto quote=wholesale_quote(world,program,plan_id,source_id,buyer_id);
    const auto& plan=terms(wholesale_plans(world,program),plan_id);
    auto& rival=buyer(world,buyer_id);
    const auto& owned=source(world.simulation.capacity->sources,source_id);
    auto& contracts=world.competition->wholesale;
    WholesaleContract contract{"wholesale."+std::to_string(contracts.size()+1),std::string(plan_id),std::string(source_id),
        std::string(buyer_id),world.simulation.date,quote.ends_before,{},rival.simulation.next_reservation_id,
        daily_ceiling(world.simulation,owned,plan),{}};
    if (!competition_detail::submit_rival(rival,ReserveFunds{contract.id,quote.funded_ceiling,false}))
        invalid("The buyer could not fund its full reservation");
    add_capacity_source(*rival.simulation.capacity,world.simulation.date,imported_source(owned,plan,contract,world.company_id,plan.capacity));
    contracts.push_back(std::move(contract));
    competition_detail::refresh_wholesale(world,program);
}

namespace competition_detail {
void refresh_wholesale(WorldState& world,const CompiledProgram& program,bool settling) {
    if (!world.competition || world.competition->wholesale.empty()) return;
    auto& seller=world.simulation;
    for (auto& owned:seller.capacity->sources) owned.wholesale_reserved=MilliCUPerDay{0};
    const auto plans=wholesale_plans(world,program);
    const auto day=seller.date;
    for (auto& contract:world.competition->wholesale) {
        auto& rival=buyer(world,contract.buyer_id);
        auto& customer=rival.simulation;
        const auto id=imported_id(contract);
        auto& imported=source(customer.capacity->sources,id);
        imported.effective=MilliCUPerDay{0};
        if (day>=last_boundary(contract)) {
            if (day==last_boundary(contract)) finish_imported_work(rival,id);
            continue;
        }
        if (closed(seller) || closed(customer) || (settling && (!operating(seller) || !operating(customer)))) {
            auto& reserve=escrow(customer,contract.reservation_id);
            reserve.reserved=Cents{0};reserve.status=ReservationStatus::cancelled;
            contract.terminated_on=day;
            finish_imported_work(rival,id);
            continue;
        }
        const auto& plan=terms(plans,contract.plan_id);
        auto& owned=source(seller.capacity->sources,contract.source_id);
        imported.effective=std::min(checked_unit_subtract(owned.effective,std::min(owned.effective,owned.wholesale_reserved)),plan.capacity);
        owned.wholesale_reserved=checked_unit_add(owned.wholesale_reserved,plan.capacity);
        if (owned.wholesale_reserved>owned.nominal) invalid("Wholesale reservations oversold the physical rack");
    }
}

std::vector<WholesaleUse> settle_wholesale(WorldState& world,const CompiledProgram& program,const IndustryAllocations& allocations) {
    std::vector<WholesaleUse> result;
    if (!world.competition || world.competition->wholesale.empty()) return result;
    auto& seller=world.simulation;
    const auto plans=wholesale_plans(world,program);
    const auto installed=industry_adoption_snapshot(world);
    const auto day=seller.date;
    std::map<std::string,MilliCU,std::less<>> work;
    for (auto& contract:world.competition->wholesale) {
        if (day>=last_boundary(contract)) continue;
        auto& customer=buyer(world,contract.buyer_id).simulation;
        const auto assignment=allocations.find(contract.buyer_id);
        if (assignment==allocations.end()) invalid("An operating wholesale buyer needs a common work allocation");
        const auto preview=preview_market_work(*customer.market,*customer.capacity,day,assignment->second,installed,
            society_market_conditions(world,customer,day));
        const auto& imported=recorded_source(preview,imported_id(contract));
        const auto& plan=terms(plans,contract.plan_id);
        auto row=usage(seller,plan,contract.source_id,imported.effective,imported.used);
        const auto charge=payment(plan,row);
        auto& reserve=escrow(customer,contract.reservation_id);
        if (charge>contract.daily_ceiling || reserve.reserved<contract.daily_ceiling || contract.days.size()>=wholesale_day_limit)
            invalid("The wholesale day exceeds its funded ceiling or term");
        reserve.reserved=checked_unit_subtract(reserve.reserved,contract.daily_ceiling);
        reserve.consumed=checked_unit_add(reserve.consumed,charge);
        if (reserve.reserved==Cents{0}) reserve.status=ReservationStatus::completed;
        customer.cash=checked_unit_subtract(customer.cash,charge);
        seller.cash=checked_unit_add(seller.cash,charge);
        row.buyer_posting=record_wholesale_cash(*customer.accounts,day,contract.id,charge,false);
        row.seller_posting=record_wholesale_cash(*seller.accounts,day,contract.id,charge,true);
        contract.days.push_back(row);
        auto& total=work.try_emplace(contract.source_id,MilliCU{0}).first->second;
        total=checked_unit_add(total,imported.used);
    }
    for (const auto& [id,used]:work) if (used.value()>0) result.push_back({id,used});
    return result;
}

void validate_wholesale(const WorldState& world,const CompiledProgram& program,bool deep) {
    const auto plans=wholesale_plans(world,program);
    const auto& seller=world.simulation;
    std::map<std::string,MilliCUPerDay,std::less<>> reserved;
    std::map<std::string,OrdinalDay,std::less<>> buyer_ends;
    std::map<std::string,std::map<std::string,OrdinalDay,std::less<>>,std::less<>> imports;
    std::map<std::string,std::set<std::uint64_t>,std::less<>> escrows,postings;
    struct ExportDay { MilliCUPerDay reserved{0}; MilliCU used{0}; };
    std::map<std::pair<OrdinalDay,std::string>,ExportDay> exports;
    const auto verify_posting=[&](const SimulationState& state,const WholesaleContract& contract,OrdinalDay day,
        std::uint64_t id,Cents charge,bool income) {
        if (charge==Cents{0}) {
            if (id!=0) invalid("A fully credited wholesale day cannot create a posting");
            return;
        }
        const auto& ledger=*state.accounts;
        if (id==0 || id>ledger.postings.size() || !postings[ledger.owner].insert(id).second)
            invalid("Wholesale postings must correspond to a single paid day");
        const auto& row=ledger.postings[static_cast<std::size_t>(id-1)];
        if (row.id!=id || row.day!=day || row.amount!=charge || row.key!="wholesale/"+contract.id+"/"+iso_date(day) ||
            row.debit!=(income?Account::cash:Account::expenses) || row.credit!=(income?Account::revenue:Account::cash) ||
            row.cause!="Owned compute reservation" || !row.document_id.empty())
            invalid("Wholesale terms, refund and balanced ledger postings differ");
    };
    if (world.competition) {
        const auto& contracts=world.competition->wholesale;
        if (contracts.size()>wholesale_contract_limit || (!contracts.empty() && (plans.empty() || !seller.capacity || !seller.accounts)))
            invalid("Wholesale contracts exceed their authored scenario or record bounds");
        auto previous=world.campaign_start;
        for (std::size_t i=0;i<contracts.size();++i) {
            const auto& contract=contracts[i];
            const auto& plan=terms(plans,contract.plan_id);
            const auto& rival=buyer(world,contract.buyer_id);
            const auto& customer=rival.simulation;
            const auto& owned=source(seller.capacity->sources,contract.source_id);
            const auto end=last_boundary(contract);
            const auto start=contract.starts_on;
            const auto earliest=buyer_ends.find(contract.buyer_id);
            if (contract.id!="wholesale."+std::to_string(i+1) || start<previous || start<*rival.planning->entered_on ||
                start>seller.date || (earliest!=buyer_ends.end() && start<earliest->second) || contract.ends_before!=advance_days(start,plan.term) ||
                contract.ends_before>seller.end_date || end<start || end>contract.ends_before || !compatible(owned,plan) ||
                owned.owner!=world.company_id || owned.provider!=world.company_id || !owned.available_from || start<*owned.available_from ||
                (contract.terminated_on && (end>=contract.ends_before || end>seller.date ||
                    !((seller.phase==DayPhase::closed && seller.date==end) || (customer.phase==DayPhase::closed && customer.date==end)))))
                invalid("Wholesale ownership, dates, compatibility or exclusivity are invalid");
            previous=start;buyer_ends.insert_or_assign(contract.buyer_id,end);
            if (contract.daily_ceiling!=daily_ceiling(seller,owned,plan)) invalid("Wholesale fee exceeds or bypasses its funded electricity terms");
            const bool active=seller.date<end;
            auto& earlier=reserved.try_emplace(owned.id,MilliCUPerDay{0}).first->second;
            const auto available=active?std::min(MilliCUPerDay{std::max<std::int64_t>(0,checked_subtract(owned.effective.value(),earlier.value()))},plan.capacity):MilliCUPerDay{0};
            if (active) earlier=checked_unit_add(earlier,plan.capacity);
            if (earlier>owned.nominal) invalid("Current wholesale contracts oversell the owned rack");
            const auto imported=imported_source(owned,plan,contract,world.company_id,available);
            if (source(customer.capacity->sources,imported.id)!=imported || !imports[contract.buyer_id].emplace(imported.id,end).second)
                invalid("Buyer capacity must match the single owned physical reservation");
            const auto count=static_cast<std::size_t>(duration(start,std::min(seller.date,end)).value());
            const auto& funds=escrow(customer,contract.reservation_id);
            if (contract.days.size()!=count || count>wholesale_day_limit || funds.reversible || funds.name!=contract.id ||
                funds.reserved!=(active?ceiling(contract.daily_ceiling,duration(seller.date,end)):Cents{0}) ||
                funds.status!=(active?ReservationStatus::active:contract.terminated_on?ReservationStatus::cancelled:ReservationStatus::completed) ||
                !escrows[contract.buyer_id].insert(contract.reservation_id).second)
                invalid("Wholesale buyer did not protect the full remaining term");
            if (!deep && count>0 && advance_days(start,Days{count})==seller.date) {
                const auto& actual=recorded_source(recorded_day(customer,OrdinalDay{seller.date.value()-1}),imported.id);
                if (actual.used!=contract.days.back().used || actual.effective!=contract.days.back().available)
                    invalid("Settled buyer work differs from the reserved export");
            }
            if (!deep) continue;
            const auto order=std::find_if(seller.infrastructure->hardware.begin(),seller.infrastructure->hardware.end(),
                [&](const auto& row) {return row.source_id==owned.id;});
            if (order==seller.infrastructure->hardware.end() || !order->commissioned_on || start<*order->commissioned_on ||
                (order->retired_on && *order->retired_on<end) || std::none_of(customer.receipts.begin(),customer.receipts.end(),[&](const auto& row) {
                    return row.day==start && row.cause=="reserve_funds" && row.reservation_id==contract.reservation_id;
                })) invalid("Wholesale capacity must come from paid commissioned hardware and an actual buyer commitment");
            Cents paid{0};
            for (std::size_t j=0;j<count;++j) {
                const auto day=advance_days(start,Days{j});
                const auto& exported=recorded_source(recorded_day(seller,day),owned.id);
                const auto& imported_work=recorded_source(recorded_day(customer,day),imported.id);
                auto& other=exports[{day,owned.id}];
                const auto capacity=std::min(plan.capacity,MilliCUPerDay{std::max<std::int64_t>(0,checked_subtract(exported.effective.value(),other.reserved.value()))});
                other.reserved=checked_unit_add(other.reserved,plan.capacity);
                other.used=checked_unit_add(other.used,imported_work.used);
                const auto& row=contract.days[j];
                auto expected=usage(seller,plan,owned.id,capacity,imported_work.used);
                expected.seller_posting=row.seller_posting;expected.buyer_posting=row.buyer_posting;
                if (other.reserved>owned.nominal || imported_work.effective!=capacity || row!=expected)
                    invalid("Historical capacity, work or credits differ from the owned reservation");
                const auto charge=payment(plan,row);
                paid=checked_unit_add(paid,charge);
                verify_posting(seller,contract,day,row.seller_posting,charge,true);
                verify_posting(customer,contract,day,row.buyer_posting,charge,false);
            }
            if (funds.consumed!=paid) invalid("Wholesale escrow consumption differs from actual buyer payments");
        }
    }
    const auto verify_company=[&](const SimulationState& state,bool owner) {
        if (!state.capacity || !state.accounts) return;
        const auto& id=state.capacity->owner;
        for (const auto& row:state.capacity->sources) {
            const auto committed=reserved.find(row.id);
            const auto expected=owner && committed!=reserved.end()?committed->second:MilliCUPerDay{0};
            if (row.wholesale_reserved!=expected || (row.kind==CapacitySourceKind::imported && !imports[id].contains(row.id)))
                invalid("Capacity bypassed the world-owned wholesale agreement");
        }
        if (!deep) return;
        if (!owner && (!world.competition->setup || std::none_of(world.competition->companies.begin(),world.competition->companies.end(),[&](const auto& rival) {
            return rival.definition_id==id && rival.response.has_value();
        }))) {
            for (const auto& row:state.reservations) if (!escrows[id].contains(row.id))
                invalid("A bounded rival cannot invent private wholesale reserves");
        }
        for (const auto& row:state.accounts->postings) if (row.key.starts_with("wholesale/") && !postings[id].contains(row.id))
            invalid("Unowned wholesale cash appears in the ledger");
        for (const auto& day:state.capacity->days) for (const auto& row:day.sources) {
            const auto it=owner?exports.find({day.day,row.source_id}):exports.end();
            const auto available=it==exports.end()?MilliCUPerDay{0}:std::min(row.effective,it->second.reserved);
            const auto used=it==exports.end()?MilliCU{0}:it->second.used;
            if (row.wholesale_available!=available || row.wholesale_used!=used)
                invalid("Physical wholesale work or withholding differs from the buyer's settled work");
            const auto imported=imports[id].find(row.source_id);
            if (imported!=imports[id].end() && day.day>=imported->second && (row.effective.value()!=0 || row.used.value()!=0))
                invalid("An expired wholesale slice kept providing work");
        }
    };
    verify_company(seller,true);
    if (world.competition) for (const auto& rival:world.competition->companies) verify_company(rival.simulation,false);
}
}
}
