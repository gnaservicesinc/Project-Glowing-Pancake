#include "flow_demo.hpp"
#include "content_file.hpp"

#include <pgp/content/collection.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace {
using namespace pgp;
void accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted())
        throw std::runtime_error(std::string(runtime::to_string(result.reason)) + ": " + result.explanation);
}
void resolve_choices(runtime::Runtime& engine) {
    for (unsigned count = 0; count < 32; ++count) {
        const auto checkpoint = engine.checkpoint();
        const auto waiting = std::find_if(checkpoint.runtime.presentations.begin(), checkpoint.runtime.presentations.end(),
            [](const auto& row) { return row.kind == runtime::PresentationKind::dialogue && !row.resolved; });
        if (waiting == checkpoint.runtime.presentations.end()) return;
        if (waiting->choices.empty()) throw std::runtime_error("Engineering dialogue has no choices");
        accepted(engine.choose(checkpoint.world.simulation.revision, waiting->id, waiting->choices.front().id));
    }
    throw std::runtime_error("Engineering choice budget exceeded");
}
}

void run_flow_demo(const char* content_path, const char* save_path, std::ostream& output) {
    using namespace pgp;
    const auto registry = content::foundation_registry();
    const auto collection = content::load_collection(read_content_file(content_path), registry);
    for (const auto* id : {"scenario.foundation", "flow.hidden.sequence_resume", "flow.supplier.exclusivity_offer"})
        if (!collection.find(id)) throw std::runtime_error("Flow demonstration requires the foundation engineering fixture");
    const auto program = runtime::compile_collection(collection, registry, runtime::supported_effects());
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    // Explicit engineering fixture transitions cross the authored thresholds.
    // A future business system owns these facts; this demo predicts no economy.
    initial.world.facts.at("company.monthly_compute_bill_cents") = std::int64_t{2'000'000};
    initial.world.facts.at("company.workload_stress_basis_points") = std::int64_t{8000};
    initial.world.facts.at("company.has_research_partner") = true;
    initial.world.facts.at("world.public_breakthrough_count") = std::int64_t{1};
    runtime::Runtime uninterrupted{program, initial};
    accepted(uninterrupted.prepare_opening(initial.world.simulation.revision));
    const auto suspended = uninterrupted.checkpoint();
    runtime::Checkpoint reloaded;
    if (save_path) {
        persistence::save_file(program, suspended, save_path);
        reloaded = persistence::load_file(program, save_path).checkpoint;
    } else {
        reloaded = persistence::decode_save(program, persistence::encode_save(program, suspended)).checkpoint;
    }
    if (reloaded != suspended) throw std::runtime_error("Saved opening state did not reload exactly");
    runtime::Runtime resumed{program, std::move(reloaded)};
    output << "Flow and save foundation demonstration\n"
           << "Content definition SHA256: " << persistence::save_identity(program).content_hash << '\n'
           << "Suspended morning: " << core::iso_date(suspended.world.simulation.date) << '\n'
           << "Reloaded choices, nested continuations, receipts and modifiers exactly.\n";
    resolve_choices(uninterrupted);
    resolve_choices(resumed);
    for (unsigned day = 0; day < 14; ++day) {
        accepted(uninterrupted.submit({uninterrupted.checkpoint().world.simulation.revision, core::AdvanceDay{}}));
        accepted(resumed.submit({resumed.checkpoint().world.simulation.revision, core::AdvanceDay{}}));
        resolve_choices(uninterrupted);
        resolve_choices(resumed);
        if (uninterrupted.checkpoint() != resumed.checkpoint())
            throw std::runtime_error("Resumed flow differs from uninterrupted execution");
    }
    const auto final = resumed.checkpoint();
    for (const auto* marker : {"inner_done", "outer_done"})
        if (std::count(final.world.decisions.begin(), final.world.decisions.end(), marker) != 1)
            throw std::runtime_error("Nested continuation repeated or lost a sibling effect");
    output << "Nested effects: inner_done then outer_done, once each.\n"
           << "Matched uninterrupted state on " << core::iso_date(final.world.simulation.date) << ".\n"
           << "State SHA256: " << persistence::sha256(persistence::encode_save(program, final)) << '\n'
           << "Business economics and playable rooms remain upcoming milestones.\n";
}
