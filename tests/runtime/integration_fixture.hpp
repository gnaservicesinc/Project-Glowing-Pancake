#pragma once

#include <pgp/content/collection.hpp>
#include <pgp/runtime/runtime.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace pgp::integration_fixture {

inline runtime::CompiledProgram program(const char* path, bool nested_dialogue) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open engineering content fixture");
    std::string bytes(content::maximum_document_bytes + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.bad()) throw std::runtime_error("Cannot read engineering content fixture");
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    auto registry = content::foundation_registry();
    auto collection = content::load_collection(bytes, registry);
    if (nested_dialogue) {
        auto& sequence = std::get<content::FlowDefinition>(
            collection.definitions.at("flow.hidden.sequence_resume").data);
        content::Node choice{};
        choice.kind = content::NodeKind::dialogue;
        choice.title_key = "engineering.nested.title";
        choice.text_key = "engineering.nested.choice";
        choice.choices.push_back({"continue", "engineering.choice.continue", "chosen_return"});
        choice.default_choice = "continue";
        sequence.nodes.at("wait_return") = std::move(choice);
        content::Node end{};
        end.kind = content::NodeKind::end;
        sequence.nodes.emplace("chosen_return", std::move(end));
    }
    return runtime::compile_collection(collection, registry);
}

inline runtime::Checkpoint checkpoint(const runtime::CompiledProgram& program) {
    auto checkpoint = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    checkpoint.world.facts.at("company.monthly_compute_bill_cents") = std::int64_t{2'000'000};
    checkpoint.world.facts.at("company.workload_stress_basis_points") = std::int64_t{8000};
    checkpoint.world.facts.at("company.has_compute_lease") = true;
    checkpoint.world.facts.at("company.has_research_partner") = true;
    checkpoint.world.facts.at("company.is_operating") = true;
    checkpoint.world.facts.at("world.public_breakthrough_count") = std::int64_t{1};
    return checkpoint;
}

inline void require_accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted())
        throw std::runtime_error(std::string(runtime::to_string(result.reason)) + ": " + result.explanation);
}
inline void prepare(runtime::Runtime& engine) {
    require_accepted(engine.prepare_opening(engine.checkpoint().world.simulation.revision));
}
inline void step(runtime::Runtime& engine) {
    require_accepted(engine.submit({engine.checkpoint().world.simulation.revision, core::AdvanceDay{}}));
}
inline void choose_all(runtime::Runtime& engine) {
    for (unsigned count = 0; count != 32; ++count) {
        const auto checkpoint = engine.checkpoint();
        const auto waiting = std::find_if(checkpoint.runtime.presentations.begin(),
            checkpoint.runtime.presentations.end(), [](const auto& presentation) {
                return presentation.kind == runtime::PresentationKind::dialogue && !presentation.resolved;
            });
        if (waiting == checkpoint.runtime.presentations.end()) return;
        if (waiting->choices.empty()) throw std::runtime_error("Dialogue has no choices");
        require_accepted(engine.choose(checkpoint.world.simulation.revision, waiting->id, waiting->choices.front().id));
    }
    throw std::runtime_error("Engineering choice budget exceeded");
}

} // namespace pgp::integration_fixture
