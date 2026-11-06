#include "endurance_run.hpp"
#include "content_file.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/session/slice_run.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ostream>
#include <stdexcept>

namespace {
using namespace pgp;
using namespace core;
using runtime::Runtime;

void require(const runtime::RuntimeResult& result) {
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void apply(Runtime& engine, CommandAction action) {
    require(engine.submit({engine.current_state().world.simulation.revision, std::move(action)}));
}

bool acknowledge_required(Runtime& engine) {
    const auto& checkpoint = engine.current_state();
    const auto& world = checkpoint.world;
    const auto& state = world.simulation;
    const auto& presentations = checkpoint.runtime.presentations;
    const auto pending = std::ranges::find_if(presentations, [](const auto& row) {
        return row.kind == runtime::PresentationKind::dialogue && !row.resolved;
    });
    if (const auto breach = runtime::required_objective_review(world)) {
        apply(engine, AcknowledgeObjectiveBreach{*breach});
    } else if (pending != presentations.end()) {
        require(engine.choose(state.revision, pending->id, ""));
    } else if (const auto warning = liquidity_report(*state.liquidity).pending_warning) {
        apply(engine, AcknowledgeLiquidityWarning{*warning});
    } else if (const auto* notice = required_succession_notice(state)) {
        apply(engine, AcknowledgeSuccession{notice->id});
    } else if (const auto* alert = required_site_alert(state)) {
        apply(engine, AcknowledgeSiteAlert{alert->id});
    } else if (required_governance_notice(state)) {
        const auto& disputes = state.management->governance->disputes;
        const auto open = std::ranges::find(disputes, false, &CorporateDispute::acknowledged);
        apply(engine, RespondCorporateDispute{open->id, false});
    } else if (required_management_notice(state)) {
        const auto& notices = state.management->notices;
        const auto open = std::ranges::find_if(notices, [](const auto& n) { return n.required && !n.acknowledged; });
        apply(engine, AcknowledgeManagementNotice{open->id});
    } else if (runtime::required_public_case(world)) {
        const auto& cases = world.authority->cases;
        const auto open = std::ranges::find_if(cases, [](const auto& row) { return !row.acknowledged; });
        apply(engine, AcknowledgePublicCase{open->id});
    } else {
        return false;
    }
    return true;
}

using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}
void report(const runtime::Checkpoint& checkpoint, std::size_t required, Clock::time_point start,
            std::ostream& output) {
    const auto& world = checkpoint.world;
    const auto& player = world.simulation;
    const auto& ledger = *player.accounts;
    const auto entered = std::ranges::count_if(world.competition->companies, [](const auto& row) {
        return row.planning->entered_on.has_value();
    });
    const auto closed = std::ranges::count_if(world.competition->companies, [](const auto& row) {
        return row.simulation.phase == DayPhase::closed;
    });
    output << iso_date(player.date) << " seconds=" << elapsed(start)
           << " cash_2014_cents=" << player.cash.value()
           << " profit_2014_cents=" << accounts_report(ledger).profit.value()
           << " rivals_entered=" << entered << " rivals_closed=" << closed
           << " postings=" << ledger.postings.size() << " invoices=" << ledger.invoices.size()
           << " live_invoices=" << ledger.invoices.live_rows().size()
           << " market_days=" << player.market->days.size()
           << " activations=" << checkpoint.runtime.activations.size()
           << " required_followups_after_opening=" << required << '\n' << std::flush;
}
}

void run_endurance(const char* path, std::string_view scenario_id, std::string_view through_date,
                   const char* seed, std::ostream& output) {
    const auto start = Clock::now();
    const auto registry = content::foundation_registry();
    auto collection = content::load_collection(read_content_file(path), registry);
    const auto definition = collection.definitions.find(scenario_id);
    auto* scenario = definition == collection.definitions.end() ? nullptr :
        std::get_if<content::ScenarioDefinition>(&definition->second.data);
    if (!scenario || !scenario->competition || !scenario->objectives)
        throw std::invalid_argument("Choose an authored century competition scenario");
    if (seed) {
        const std::string_view input{seed};
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), scenario->seed);
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size())
            throw std::invalid_argument("Seed must be an unsigned 64-bit integer");
    }
    const auto through = parse_iso_date(through_date);
    if (through <= scenario->starts_on) throw std::invalid_argument("Endurance date must follow the scenario start");
    const auto program = runtime::compile_collection(collection, registry, runtime::supported_effects());
    output << "Specialist " << scenario_id << " content=" << collection.content_version
           << " seed=" << scenario->seed << " through=" << through_date << '\n' << std::flush;
    const auto opening_day = std::min(through, advance_days(scenario->starts_on, Days{181}));
    auto opening = session::run_competition_specialist(program, scenario_id, opening_day);
    if (opening.stop != session::SliceStop::requested_boundary)
        throw std::runtime_error("The specialist opening ended before its requested boundary");
    Runtime engine{program, std::move(opening.checkpoint)};
    std::size_t required = 0;
    try {
        while (engine.current_state().world.simulation.date < through) {
            const auto& world = engine.current_state().world;
            if (world.simulation.phase == DayPhase::closed)
                throw std::runtime_error("The company closed before the requested boundary");
            if (const auto breach = runtime::required_objective_review(world)) {
                apply(engine, AcknowledgeObjectiveBreach{*breach});
                ++required;
                continue;
            }
            if (runtime::objective_time_stopped(world)) apply(engine, ContinueAfterEnding{});
            require(engine.prepare_opening(engine.current_state().world.simulation.revision));
            for (unsigned n = 0; acknowledge_required(engine); ++n) {
                ++required;
                if (n == 127) throw std::runtime_error("Required decisions exceeded the engineering morning budget");
            }
            apply(engine, AdvanceDay{});
            const auto date = from_ordinal(engine.current_state().world.simulation.date);
            if (date.month == 1 && date.day == 1 && engine.current_state().world.simulation.date < through)
                report(engine.current_state(), required, start, output);
        }
        report(engine.current_state(), required, start, output);
        const auto save_start = Clock::now();
        const auto bytes = persistence::encode_save(program, engine.current_state());
        const auto save_seconds = elapsed(save_start);
        const auto load_start = Clock::now();
        const auto loaded = persistence::decode_save(program, bytes);
        const auto load_seconds = elapsed(load_start);
        if (loaded.checkpoint != engine.current_state()) throw std::runtime_error("Endurance save changed the checkpoint");
        output << "Exact checkpoint " << bytes.size() << " bytes encode_seconds=" << save_seconds
               << " decode_seconds=" << load_seconds << '\n';
    } catch (const std::exception& error) {
        throw std::runtime_error("At " + iso_date(engine.current_state().world.simulation.date) + " " + error.what());
    }
}
