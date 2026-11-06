#include "endurance_run.hpp"
#include "content_file.hpp"
#include "endurance_strategy.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/session/slice_run.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <map>
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
unsigned option_number(const char* value, unsigned maximum) {
    const std::string_view input{value};
    unsigned number = 0;
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || number == 0 || number > maximum)
        throw std::invalid_argument("Endurance interval/size must be a positive whole number within its supported bound");
    return number;
}
void save_checkpoint(const runtime::CompiledProgram& program, const runtime::Checkpoint& checkpoint,
                     const char* path, persistence::SaveLimits limits, std::ostream& output) {
    const auto save_start = Clock::now();
    const auto bytes = [&] {
        if (!path) return persistence::encode_save(program, checkpoint, 0, limits);
        persistence::save_file(program, checkpoint, path, 0, limits);
        return persistence::read_bounded(path, limits.maximum_bytes);
    }();
    const auto save_seconds = elapsed(save_start);
    const auto load_start = Clock::now();
    const auto loaded = persistence::decode_save(program, bytes, limits);
    const auto load_seconds = elapsed(load_start);
    if (loaded.checkpoint != checkpoint) throw std::runtime_error("Endurance save changed the checkpoint");
    output << "Exact checkpoint " << iso_date(checkpoint.world.simulation.date) << " bytes=" << bytes.size()
           << " over_default=" << (bytes.size() > persistence::default_maximum_save_bytes)
           << " encode_seconds=" << save_seconds << " decode_seconds=" << load_seconds << '\n' << std::flush;
}
void report_concentration(const runtime::WorldState& world, std::ostream& output) {
    std::map<std::string, std::uint64_t, std::less<>> providers;
    std::uint64_t total = 0;
    for (const auto& segment : runtime::industry_adoption_snapshot(world)) for (const auto& share : segment.shares) {
        if (share.product_id == "outside" || share.share.value() == 0) continue;
        const auto weight = mul_div_floor(segment.observed_tasks, static_cast<std::uint64_t>(share.share.value()), 1).quotient;
        const auto id = share.product_id.substr(0, share.product_id.find(':'));
        providers[id] += weight;
        total += weight;
    }
    std::uint64_t hhi = 0, largest = 0, player_share = 0;
    for (const auto& [id, weight] : providers) {
        const auto share = total ? mul_div_floor(weight, 10000, total).quotient : 0;
        hhi += share * share;
        largest = std::max(largest, share);
        if (id == world.company_id) player_share = share;
    }
    output << " installed_ai_providers=" << providers.size() << " installed_ai_hhi_bp=" << hhi / 10000
           << " largest_ai_provider_bp=" << largest << " player_ai_share_bp=" << player_share;
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
           << " required_followups_after_opening=" << required;
    if (player.lab) output << " publications=" << player.lab->publications.size();
    if (player.deployments) output << " deployments=" << player.deployments->records.size();
    if (world.competition) output << " wholesale_contracts=" << world.competition->wholesale.size();
    report_concentration(world, output);
    if (player.frontier && !player.frontier->systems.empty()) {
        const auto qualification = frontier_qualification(*player.frontier, player.frontier->systems.front().id);
        output << " frontier_assessments=" << qualification.assessments
               << " frontier_improvements=" << qualification.improvements
               << " game_agi=" << (qualification.agi_on ? iso_date(*qualification.agi_on) : "pending")
               << " game_asi=" << (qualification.asi_on ? iso_date(*qualification.asi_on) : "pending");
    }
    output << '\n' << std::flush;
}
}

void run_endurance(const char* path, std::string_view scenario_id, std::string_view through_date,
                   EnduranceOptions options, std::ostream& output) {
    const auto start = Clock::now();
    const auto strategy_name = std::string_view{options.strategy ? options.strategy : "specialist"};
    const auto strategy = parse_endurance_strategy(strategy_name);
    persistence::SaveLimits limits;
    if (options.save_limit_mib) limits.maximum_bytes = static_cast<std::size_t>(option_number(options.save_limit_mib, 64)) * 1024U * 1024U;
    const auto checkpoint_years = options.checkpoint_years ? option_number(options.checkpoint_years, 500) : 0;
    if (checkpoint_years != 0 && !options.save) throw std::invalid_argument("Checkpoint intervals require an explicit --save path");
    const auto registry = content::foundation_registry();
    auto collection = content::load_collection(read_content_file(path), registry);
    const auto definition = collection.definitions.find(scenario_id);
    auto* scenario = definition == collection.definitions.end() ? nullptr :
        std::get_if<content::ScenarioDefinition>(&definition->second.data);
    if (!scenario || !scenario->competition || !scenario->objectives)
        throw std::invalid_argument("Choose an authored century competition scenario");
    if (options.seed) {
        const std::string_view input{options.seed};
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), scenario->seed);
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size())
            throw std::invalid_argument("Seed must be an unsigned 64-bit integer");
    }
    const auto through = parse_iso_date(through_date);
    if (through <= scenario->starts_on) throw std::invalid_argument("Endurance date must follow the scenario start");
    const auto program = runtime::compile_collection(collection, registry, runtime::supported_effects());
    output << (options.resume ? "Resume " : "Start ") << scenario_id << " strategy=" << strategy_name << " content=" << collection.content_version
           << " seed=" << scenario->seed << " through=" << through_date << '\n' << std::flush;
    const auto initial = [&] {
        if (options.resume) {
            auto loaded = persistence::load_file(program, options.resume, limits);
            if (loaded.checkpoint.world.scenario_id != scenario_id || loaded.checkpoint.world.simulation.date > through)
                throw std::invalid_argument("Resume requires the same scenario and a date at or after the checkpoint");
            return std::move(loaded.checkpoint);
        }
        return begin_endurance_strategy(program, scenario_id, through, strategy);
    };
    Runtime engine{program, initial()};
    auto next_checkpoint_year = from_ordinal(engine.current_state().world.simulation.date).year + static_cast<int>(checkpoint_years);
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
            review_endurance_strategy(engine, strategy, output);
            apply(engine, AdvanceDay{});
            const auto date = from_ordinal(engine.current_state().world.simulation.date);
            if (date.month == 1 && date.day == 1 && engine.current_state().world.simulation.date < through) {
                report(engine.current_state(), required, start, output);
                if (checkpoint_years != 0 && date.year >= next_checkpoint_year) {
                    save_checkpoint(program, engine.current_state(), options.save, limits, output);
                    next_checkpoint_year += static_cast<int>(checkpoint_years);
                }
            }
        }
        report(engine.current_state(), required, start, output);
        save_checkpoint(program, engine.current_state(), options.save, limits, output);
    } catch (const std::exception& error) {
        if (checkpoint_years != 0) {
            try { save_checkpoint(program, engine.current_state(), options.save, limits, output); }
            catch (const std::exception& save_error) { output << "Checkpoint failed " << save_error.what() << '\n' << std::flush; }
        }
        throw std::runtime_error("At " + iso_date(engine.current_state().world.simulation.date) + " " + error.what());
    }
}
