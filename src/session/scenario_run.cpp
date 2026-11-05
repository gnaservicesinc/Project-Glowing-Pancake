#include <pgp/session/scenario_run.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace pgp::session {
namespace {
using namespace core;
constexpr std::string_view license_name = "Scenario data license";
constexpr std::string_view rescue_key = "scenario.rescue.equity";

void require(bool valid, const char* explanation) {
    if (!valid) throw std::invalid_argument(explanation);
}
void accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void apply(runtime::Runtime& engine, CommandAction action) {
    accepted(engine.submit({engine.checkpoint().world.simulation.revision,std::move(action)}));
}
template<class Row> const Row* row_with_id(const std::vector<Row>& rows, std::string_view id) {
    const auto found = std::find_if(rows.begin(),rows.end(),[&](const auto& row) { return row.id == id; });
    return found == rows.end() ? nullptr : &*found;
}
bool has_decision(const runtime::Checkpoint& checkpoint) {
    return checkpoint.world.simulation.phase == DayPhase::awaiting_choice ||
        std::any_of(checkpoint.runtime.presentations.begin(),checkpoint.runtime.presentations.end(),[](const auto& row) {
            return row.kind == runtime::PresentationKind::dialogue && !row.resolved;
        });
}
void startup(runtime::Runtime& engine, const content::BusinessPlan& plan, OrdinalDay starts_on) {
    auto checkpoint = engine.checkpoint();
    auto& state = checkpoint.world.simulation;
    require(state.accounts && state.capacity && state.market && state.liquidity,
            "Business scenario requires accounts, capacity, market and liquidity state");
    auto license = std::find_if(state.reservations.begin(),state.reservations.end(),[](const auto& row) {
        return row.name == license_name;
    });
    if (license == state.reservations.end()) {
        require(state.date == starts_on,"Resumed scenario is missing its committed data license");
        apply(engine,ReserveFunds{std::string(license_name),plan.data_license,false});
        checkpoint = engine.checkpoint();
        license = std::find_if(checkpoint.world.simulation.reservations.begin(),
            checkpoint.world.simulation.reservations.end(),[](const auto& row) { return row.name == license_name; });
    }
    require(license->consumed == Cents{0} || license->consumed == plan.data_license,
            "Scenario data license differs from its authored delivered cost");
    if (license->consumed == Cents{0}) {
        require(checkpoint.world.simulation.date == starts_on && license->reserved == plan.data_license,
                "Resumed scenario data delivery is inconsistent");
        apply(engine,ConsumeReservation{license->id,plan.data_license});
    }
    checkpoint = engine.checkpoint();
    const auto* training = row_with_id(checkpoint.world.simulation.capacity->projects,plan.training.id);
    if (!training) {
        require(checkpoint.world.simulation.date == starts_on,"Resumed scenario is missing its authored training project");
        apply(engine,ReserveProject{plan.training});
    } else {
        require(training->kind == ProjectKind::training && training->source_id == plan.training.source_id &&
            training->requested == plan.training.requested && training->staff == plan.training.staff &&
            training->starts_on == plan.training.starts_on && training->total_work == plan.training.total_work,
            "Resumed scenario training differs from its authored plan");
    }
}
void expand(runtime::Runtime& engine, const content::BusinessPlan& plan) {
    auto checkpoint = engine.checkpoint();
    const auto day = checkpoint.world.simulation.date;
    if (day < plan.expansion_on) return;
    const auto* training = row_with_id(checkpoint.world.simulation.capacity->projects,plan.training.id);
    require(training && training->status == ProjectStatus::completed,
            "Scenario expansion requires completed finite training work");
    for (const auto& offer : plan.expansion.offers) {
        checkpoint = engine.checkpoint();
        if (!row_with_id(checkpoint.world.simulation.capacity->projects,offer.serving.id)) {
            require(day == plan.expansion_on,"Resumed scenario omitted a due expansion project");
            apply(engine,ReserveProject{offer.serving});
        }
        checkpoint = engine.checkpoint();
        if (!row_with_id(checkpoint.world.simulation.market->products,offer.product.id)) {
            require(day == plan.expansion_on,"Resumed scenario omitted a due expansion product");
            apply(engine,AddMarketProduct{offer.product});
        }
    }
    for (const auto& segment : plan.expansion.segments) {
        checkpoint = engine.checkpoint();
        if (!row_with_id(checkpoint.world.simulation.market->segments,segment.id)) {
            require(day == plan.expansion_on,"Resumed scenario omitted a due expansion customer");
            apply(engine,AddMarketSegment{segment});
        }
    }
    for (const auto& contract : plan.expansion.contracts) {
        checkpoint = engine.checkpoint();
        const auto* existing = row_with_id(checkpoint.world.simulation.market->contracts,contract.terms.id);
        if (!existing) {
            require(day == plan.expansion_on,"Resumed scenario omitted a due expansion agreement");
            apply(engine,AcceptContract{contract.terms});
            checkpoint = engine.checkpoint();
            existing = row_with_id(checkpoint.world.simulation.market->contracts,contract.terms.id);
        }
        if (contract.start_immediately && existing->status == ContractStatus::pending) {
            require(day == plan.expansion_on,"Resumed scenario omitted its due contract activation");
            apply(engine,StartContract{contract.terms.id});
        }
    }
}
bool rescued_before(const AccountsState& accounts) {
    return std::any_of(accounts.postings.begin(),accounts.postings.end(),[](const auto& row) {
        return row.key == rescue_key;
    });
}
void rescue(runtime::Runtime& engine, const content::BusinessPlan& plan) {
    require(plan.rescue_proceeds.value() > 0 && plan.rescue_shares.value() > 0,
            "Scenario rescue requires authored financing terms");
    apply(engine,InvestEquity{"scenario.rescue.investor",plan.rescue_proceeds,plan.rescue_shares,std::string(rescue_key)});
    auto checkpoint = engine.checkpoint();
    auto report = core::Simulation{checkpoint.world.simulation}.snapshot();
    const auto closed = checkpoint.world.simulation.accounts->closed_through;
    const auto invoices = checkpoint.world.simulation.accounts->invoices;
    for (const auto& invoice : invoices) {
        if (invoice.kind != InvoiceKind::payable || !closed || invoice.due > *closed || invoice.settled == invoice.amount) continue;
        const auto amount = std::min(checked_unit_subtract(invoice.amount,invoice.settled),report->available);
        if (amount.value() <= 0) break;
        apply(engine,SettleInvoice{invoice.id,amount,"scenario.rescue.invoice:" + invoice.id});
        report = core::Simulation{engine.checkpoint().world.simulation}.snapshot();
    }
    const auto debts = checkpoint.world.simulation.accounts->debts;
    for (const auto& debt : debts) {
        if (!closed || debt.due > *closed || debt.principal.value() == 0) continue;
        const auto amount = std::min(debt.principal,report->available);
        if (amount.value() <= 0) break;
        apply(engine,RepayDebt{debt.id,amount,"scenario.rescue.principal:" + debt.id});
        report = core::Simulation{engine.checkpoint().world.simulation}.snapshot();
    }
}
std::uint64_t add_tasks(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
        throw std::overflow_error("Scenario report task total overflow");
    return left + right;
}
std::vector<BusinessMonthReport> monthly_reports(const runtime::Checkpoint& checkpoint) {
    const auto& ledger = *checkpoint.world.simulation.accounts;
    const auto& market = *checkpoint.world.simulation.market;
    if (!ledger.closed_through) return {};
    std::array<Cents,9> balances{Cents{0},Cents{0},Cents{0},Cents{0},Cents{0},Cents{0},Cents{0},Cents{0},Cents{0}};
    std::vector<BusinessMonthReport> result;
    std::size_t posting = 0, market_day = 0;
    auto cursor = ledger.starts_on;
    Cents previous_revenue{0}, previous_expenses{0};
    while (cursor <= *ledger.closed_through) {
        const auto civil = from_ordinal(cursor);
        const auto through = std::min(to_ordinal({civil.year,civil.month,days_in_month(civil.year,civil.month)}),*ledger.closed_through);
        while (posting < ledger.postings.size() && ledger.postings[posting].day <= through) {
            const auto& row = ledger.postings[posting++];
            const auto update = [&](Account account, bool debit) {
                const bool debit_normal = account == Account::cash || account == Account::receivables ||
                    account == Account::assets || account == Account::expenses;
                auto& balance = balances[static_cast<std::size_t>(account)];
                balance = checked_unit_add(balance,debit == debit_normal ? row.amount : Cents{-row.amount.value()});
            };
            update(row.debit,true); update(row.credit,false);
        }
        BusinessMonthReport month{through,balances[0],checked_unit_subtract(balances[7],previous_revenue),
            checked_unit_subtract(balances[8],previous_expenses),balances[4],balances[5],0,0};
        while (market_day < market.days.size() && market.days[market_day].day <= through) {
            for (const auto& segment : market.days[market_day++].segments) {
                month.delivered_tasks = add_tasks(month.delivered_tasks,add_tasks(segment.contract_tasks,segment.discretionary_tasks));
                month.outside_tasks = add_tasks(month.outside_tasks,segment.outside_tasks);
            }
        }
        result.push_back(month); previous_revenue = balances[7]; previous_expenses = balances[8];
        if (through == *ledger.closed_through) break;
        cursor = advance_days(through,Days{1});
    }
    return result;
}
}

BusinessRunResult run_business_scenario(const runtime::CompiledProgram& program, std::string_view scenario_id,
    std::string_view headquarters, BusinessStrategy strategy, BusinessRunOptions options,
    std::optional<runtime::Checkpoint> resume) {
    const auto* definition = program.collection.find(scenario_id);
    require(definition && std::holds_alternative<content::ScenarioDefinition>(definition->data),
            "Business runner requires a scenario definition");
    const auto& scenario = std::get<content::ScenarioDefinition>(definition->data);
    require(scenario.business_plan.has_value(),"Scenario has no authored business plan");
    const auto& plan = *scenario.business_plan;
    require(scenario.ends_before.value() - scenario.starts_on.value() <= 1096,
            "Business engineering runner is bounded to the three-year slice");
    if (options.stop_on) require(*options.stop_on >= scenario.starts_on && *options.stop_on <= scenario.ends_before,
            "Requested scenario stop boundary is outside the scenario");
    if (options.reload_on) require(*options.reload_on >= scenario.starts_on && *options.reload_on < scenario.ends_before,
            "Requested scenario reload boundary is outside the scenario");
    runtime::Runtime engine{program,resume ? std::move(*resume) : runtime::begin_scenario(program,scenario_id,headquarters)};
    auto checkpoint = engine.checkpoint();
    require(checkpoint.world.campaign_start == scenario.starts_on && checkpoint.world.simulation.end_date == scenario.ends_before,
            "Resume checkpoint does not belong to the requested scenario dates");
    require(checkpoint.world.simulation.accounts && checkpoint.world.simulation.capacity &&
            checkpoint.world.simulation.market && checkpoint.world.simulation.liquidity,
            "Business runner requires all business owning modules");
    BusinessStop stop = BusinessStop::requested_boundary;
    std::optional<OrdinalDay> reloaded_on;
    bool startup_checked = false, expansion_checked = false;
    for (unsigned steps = 0; steps <= 1096; ++steps) {
        checkpoint = engine.checkpoint();
        const auto& state = checkpoint.world.simulation;
        const auto liquidity = liquidity_report(*state.liquidity);
        if (liquidity.status == LiquidityStatus::closed) { stop = BusinessStop::closed; break; }
        if (state.phase == DayPhase::finished) { stop = BusinessStop::finished; break; }
        accepted(engine.prepare_opening(state.revision));
        checkpoint = engine.checkpoint();
        if (has_decision(checkpoint)) { stop = BusinessStop::decision_required; break; }
        const auto day = checkpoint.world.simulation.date;
        if (options.stop_on && day >= *options.stop_on) { stop = BusinessStop::requested_boundary; break; }
        if (options.reload_on && day == *options.reload_on && !reloaded_on) {
            const auto bytes = persistence::encode_save(program,checkpoint);
            const auto loaded = persistence::decode_save(program,bytes).checkpoint;
            require(loaded == checkpoint,"Scenario checkpoint did not reload exactly at its named boundary");
            engine = runtime::Runtime{program,loaded}; reloaded_on = day;
        }
        if (!startup_checked) { startup(engine,plan,scenario.starts_on); startup_checked = true; }
        if (strategy == BusinessStrategy::expansion && !expansion_checked && day >= plan.expansion_on) {
            expand(engine,plan); expansion_checked = true;
        }
        checkpoint = engine.checkpoint();
        auto warning = liquidity_report(*checkpoint.world.simulation.liquidity);
        if (warning.status == LiquidityStatus::workout) {
            const bool already_rescued = rescued_before(*checkpoint.world.simulation.accounts);
            if (strategy == BusinessStrategy::rescue && !already_rescued) {
                rescue(engine,plan);
                checkpoint = engine.checkpoint();
                warning = liquidity_report(*checkpoint.world.simulation.liquidity);
            } else if (strategy == BusinessStrategy::rescue && already_rescued) {
                stop = BusinessStop::warning; break;
            }
            if (warning.pending_warning) {
                if (!options.continue_workouts) { stop = BusinessStop::warning; break; }
                apply(engine,AcknowledgeLiquidityWarning{*warning.pending_warning});
            }
        }
        apply(engine,AdvanceDay{});
        if (steps == 1096) throw std::runtime_error("Scenario driver exceeded its finite three-year day bound");
    }
    checkpoint = engine.checkpoint();
    return {checkpoint,monthly_reports(checkpoint),stop,reloaded_on};
}
std::string_view to_string(BusinessStrategy strategy) noexcept {
    switch (strategy) {
    case BusinessStrategy::baseline: return "baseline";
    case BusinessStrategy::expansion: return "expansion";
    case BusinessStrategy::rescue: return "rescue";
    }
    return "unknown";
}
std::string_view to_string(BusinessStop stop) noexcept {
    switch (stop) {
    case BusinessStop::finished: return "finished";
    case BusinessStop::warning: return "warning";
    case BusinessStop::closed: return "closed";
    case BusinessStop::decision_required: return "decision_required";
    case BusinessStop::requested_boundary: return "requested_boundary";
    }
    return "unknown";
}
}
