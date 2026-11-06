#include <pgp/studio/revision_review.hpp>

#include "checkout.hpp"
#include "snapshot_internal.hpp"
#include "structured_document.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/studio/catalog.hpp>

#include <algorithm>
#include <set>
#include <tuple>

namespace pgp::studio {
std::string_view migration_kind_name(MigrationKind kind) {
    switch (kind) {
    case MigrationKind::flow: return "flow";
    case MigrationKind::node: return "node";
    case MigrationKind::scenario: return "scenario";
    case MigrationKind::deal: return "deal";
    case MigrationKind::variable: return "variable";
    case MigrationKind::message: return "text";
    }
    throw std::invalid_argument("Unknown save mapping family.");
}

namespace {
namespace fs = std::filesystem;
using Json = detail::StructuredJson;
using Key = std::tuple<MigrationKind, std::string, std::string>;
constexpr std::size_t maximum_receipt_bytes = 8U * 1024U * 1024U;
constexpr auto exports_path = ".work/revision-review";

[[noreturn]] void refuse(std::string code, std::string detail) {
    throw persistence::PersistenceError(std::move(code), std::move(detail));
}

Json variables_by_id(const Json& collection) {
    Json result = Json::object();
    for (const auto& variable : collection.value("variables", Json::array()))
        result[variable.at("id").get<std::string>()] = variable;
    return result;
}

const content::FlowDefinition& source_flow(const runtime::CompiledProgram& source, const std::string& id) {
    return std::get<content::FlowDefinition>(source.collection.definitions.at(id).data);
}

class SaveRemapper {
public:
    SaveRemapper(const runtime::CompiledProgram& source, const runtime::CompiledProgram& target,
        const std::vector<IdentityMapping>& mappings)
        : source_(source), target_(target), before_(detail::structured_document(source)),
          after_(detail::structured_document(target)) {
        if (mappings.size() > 4096) refuse("budget", "Save review supports at most 4096 explicit mappings.");
        std::set<Key> destinations;
        for (const auto& row : mappings) {
            if (!content::valid_id(row.from) || !content::valid_id(row.to) || row.from == row.to ||
                (row.kind == MigrationKind::node ? !content::valid_id(row.owner) : !row.owner.empty()) ||
                !maps_.emplace(Key{row.kind, row.owner, row.from}, row.to).second ||
                !destinations.emplace(row.kind, row.owner, row.to).second)
                refuse("mapping", "Each typed source and destination must occur once with different valid IDs.");
        }
        const auto prior_variables = variables_by_id(before_);
        const auto next_variables = variables_by_id(after_);
        for (const auto& [key, destination] : maps_) {
            const auto& [kind, owner, from] = key;
            const auto next_owner = mapped(MigrationKind::flow, owner);
            if (!exists(before_, prior_variables, kind, owner, from) ||
                !exists(after_, next_variables, kind, next_owner, destination) ||
                exists(before_, prior_variables, kind, owner, destination))
                refuse("mapping", "Mapping needs an existing typed source and a distinct destination without an older owner: " +
                    (owner.empty() ? "" : owner + "/") + from + " → " + destination);
        }
        for (const auto& [id, variable] : prior_variables.items()) {
            const auto next_id = mapped(MigrationKind::variable, id);
            auto expected = variable;
            expected["id"] = next_id;
            if (!next_variables.contains(next_id) || next_variables.at(next_id) != expected)
                refuse("variable_contract", "Saved variable defaults, scope, units and type must remain compatible: " + id);
        }

        const auto index = catalog_index(source);
        const std::map<std::string_view, MigrationKind> families{{"flow", MigrationKind::flow},
            {"deal", MigrationKind::deal}, {"variable", MigrationKind::variable}, {"text", MigrationKind::message}};
        for (const auto& reference : index.references) {
            const auto kind = families.find(reference.family);
            if (kind == families.end()) continue;
            const auto found = maps_.find(Key{kind->second, {}, reference.id});
            if (found != maps_.end()) before_.at(Json::json_pointer{reference.path}) = found->second;
        }
    }

    [[nodiscard]] std::size_t validate_saved_flows(const runtime::Checkpoint& checkpoint) const {
        std::set<std::string> visited;
        std::vector<std::string> pending;
        for (const auto& activation : checkpoint.runtime.activations)
            pending.push_back(activation.definition_id);
        while (!pending.empty()) {
            auto id = std::move(pending.back());
            pending.pop_back();
            if (!visited.insert(id).second) continue;
            compare_flow(id, true);
            for (const auto& [node_id, node] : source_flow(source_, id).nodes) {
                (void)node_id;
                if (node.kind == content::NodeKind::subflow) pending.push_back(node.subflow_id);
            }
        }
        for (const auto& lifecycle : checkpoint.runtime.lifecycles)
            if (!visited.contains(lifecycle.definition_id)) compare_flow(lifecycle.definition_id, false);
        for (const auto& id : std::set(checkpoint.world.negotiations.begin(), checkpoint.world.negotiations.end())) {
            const auto next_id = mapped(MigrationKind::deal, id);
            if (!after_.at("definitions").contains(next_id) ||
                body(before_.at("definitions").at(id)) != body(after_.at("definitions").at(next_id)))
                refuse("deal_contract", "Retain the terms of a recorded negotiation: " + id);
        }
        return visited.size();
    }

    void translate(runtime::Checkpoint& checkpoint) const {
        auto& world = checkpoint.world;
        auto& state = checkpoint.runtime;
        world.scenario_id = mapped(MigrationKind::scenario, world.scenario_id);
        if (world.enabled_flows) for (auto& id : *world.enabled_flows) {
            const auto next_id = mapped(MigrationKind::flow, id);
            const auto& prior = source_flow(source_, id);
            const auto* definition = target_.collection.find(next_id);
            const auto* next = definition ? std::get_if<content::FlowDefinition>(&definition->data) : nullptr;
            if (!next || prior.scope != next->scope || prior.group.has_value() != next->group.has_value())
                refuse("flow_contract", "Retain the saved root's identity, scope and role: " + id);
            id = next_id;
        }
        for (auto& id : world.negotiations) id = mapped(MigrationKind::deal, id);
        for (auto& variable : world.variables) variable.variable_id = mapped(MigrationKind::variable, variable.variable_id);
        std::sort(world.variables.begin(), world.variables.end(), [](const auto& left, const auto& right) {
            return std::tie(left.binding, left.variable_id) < std::tie(right.binding, right.variable_id);
        });
        // Receipts retain the source activation ID and, for groups, the actual
        // sequence of caller node IDs. Translate that sequence before roots.
        for (auto& receipt : state.receipts) {
            if (!receipt.group_calls.empty()) {
                const auto owner = std::lower_bound(state.activations.begin(), state.activations.end(), receipt.activation_id,
                    [](const auto& activation, auto id) { return activation.id < id; });
                auto flow = owner->definition_id;
                for (auto& call : receipt.group_calls) {
                    const auto& next = source_flow(source_, flow).nodes.at(call).subflow_id;
                    call = mapped(MigrationKind::node, call, flow);
                    flow = next;
                }
                receipt.bound_variable = mapped(MigrationKind::variable, receipt.bound_variable);
            }
            if (receipt.operation == "open_negotiation") {
                auto& deal = std::get<std::string>(receipt.after);
                deal = mapped(MigrationKind::deal, deal);
            }
            translate_location(receipt.flow_id, receipt.node_id);
        }
        for (auto& activation : state.activations) {
            activation.definition_id = mapped(MigrationKind::flow, activation.definition_id);
            activation.definition_revision = target_.collection.definitions.at(activation.definition_id).revision;
            translate_location(activation.current_flow, activation.current_node);
            for (auto& frame : activation.frames) {
                frame.return_node = mapped(MigrationKind::node, frame.return_node, frame.flow_id);
                translate_location(frame.flow_id, frame.sequence_node);
            }
        }
        for (auto& entry : state.lifecycles)
            entry.definition_id = mapped(MigrationKind::flow, entry.definition_id);
        std::sort(state.lifecycles.begin(), state.lifecycles.end(), [](const auto& left, const auto& right) {
            return std::tie(left.binding.id, left.definition_id) < std::tie(right.binding.id, right.definition_id);
        });
        for (auto& duration : state.duration_states) {
            duration.definition_id = mapped(MigrationKind::flow, duration.definition_id);
            duration.definition_revision = target_.collection.definitions.at(duration.definition_id).revision;
        }
        std::sort(state.duration_states.begin(), state.duration_states.end(), [](const auto& left, const auto& right) {
            return std::tie(left.binding.id, left.binding.scope, left.definition_id, left.predicate_id) <
                std::tie(right.binding.id, right.binding.scope, right.definition_id, right.predicate_id);
        });
        for (auto& roll : state.random_resolutions) translate_location(roll.flow_id, roll.node_id);
        for (auto& card : state.presentations) {
            translate_location(card.flow_id, card.node_id);
            card.title_key = mapped(MigrationKind::message, card.title_key);
            card.text_key = mapped(MigrationKind::message, card.text_key);
            for (auto& choice : card.choices) choice.label_key = mapped(MigrationKind::message, choice.label_key);
        }
    }

    [[nodiscard]] Json evidence() const {
        Json result = Json::array();
        for (const auto& [key, destination] : maps_) {
            const auto& [kind, owner, source] = key;
            result.push_back({{"kind", migration_kind_name(kind)}, {"owner", owner}, {"from", source}, {"to", destination}});
        }
        return result;
    }

private:
    static bool exists(const Json& document, const Json& variables, MigrationKind kind,
                       const std::string& owner, const std::string& id) {
        if (kind == MigrationKind::message)
            return document.contains("messages") && document.at("messages").contains(id);
        if (kind == MigrationKind::variable) return variables.contains(id);
        const auto& definitions = document.at("definitions");
        const auto found = definitions.find(kind == MigrationKind::node ? owner : id);
        if (found == definitions.end()) return false;
        if (kind == MigrationKind::node)
            return found->at("kind") == "flow" && found->at("nodes").contains(id);
        return found->at("kind") == migration_kind_name(kind);
    }

    [[nodiscard]] std::string mapped(MigrationKind kind, const std::string& id, const std::string& owner = {}) const {
        const auto found = maps_.find(Key{kind, owner, id});
        return found != maps_.end() ? found->second : id;
    }

    void translate_location(std::string& flow, std::string& node) const {
        node = mapped(MigrationKind::node, node, flow);
        flow = mapped(MigrationKind::flow, flow);
    }

    static Json body(Json value) {
        for (const auto* key : {"id", "revision", "tags", "deprecated"}) value.erase(key);
        return value;
    }

    void compare_flow(const std::string& id, bool protect_graph) const {
        const auto next_id = mapped(MigrationKind::flow, id);
        const auto& definitions = after_.at("definitions");
        if (!definitions.contains(next_id) || definitions.at(next_id).at("kind") != "flow")
            refuse("flow_contract", "Retain or explicitly map the saved flow " + id);
        auto prior = body(before_.at("definitions").at(id));
        auto next = body(definitions.at(next_id));
        if (protect_graph) {
            const auto translate_link = [&](Json& value) { value = mapped(MigrationKind::node, value.get<std::string>(), id); };
            translate_link(prior.at("entry_node"));
            if (prior.contains("exit_node")) translate_link(prior.at("exit_node"));
            Json nodes = Json::object();
            for (auto& [name, node] : prior.at("nodes").items()) {
                if (node.contains("next")) translate_link(node.at("next"));
                if (node.contains("children")) for (auto& child : node.at("children")) translate_link(child);
                for (const auto* field : {"branches", "choices"})
                    if (node.contains(field)) for (auto& row : node.at(field)) translate_link(row.at("next"));
                nodes[mapped(MigrationKind::node, name, id)] = std::move(node);
            }
            prior["nodes"] = std::move(nodes);
        } else {
            for (const auto* key : {"entry_node", "exit_node", "nodes", "max_node_visits"}) {
                prior.erase(key);
                next.erase(key);
            }
        }
        if (prior != next)
            refuse("flow_contract", (protect_graph ? "Saved activation, continuation or history requires compatible flow behavior: " :
                "Saved occurrence state requires compatible trigger and lifecycle rules: ") + id);
    }

    const runtime::CompiledProgram& source_;
    const runtime::CompiledProgram& target_;
    Json before_, after_;
    std::map<Key, std::string> maps_;
};

Json revision_evidence(const RevisionInfo& info) {
    return {{"generation", info.generation}, {"content_version", info.content_version},
        {"game_version", info.game_version}, {"collection_hash", info.collection_hash}};
}

Json continuation(const runtime::Activation& active) {
    Json frames = Json::array();
    for (const auto& frame : active.frames)
        frames.push_back({{"flow", frame.flow_id}, {"node", frame.sequence_node}, {"next_child", frame.next_child}, {"return", frame.return_node}});
    const auto status = active.status == runtime::ActivationStatus::awaiting_choice ? "awaiting choice" :
        active.status == runtime::ActivationStatus::delayed ? "delayed" :
        active.status == runtime::ActivationStatus::continuous_active ? "continuous active" : "ready";
    return {{"root", active.definition_id}, {"revision", active.definition_revision}, {"flow", active.current_flow},
        {"node", active.current_node}, {"status", status}, {"binding", active.binding.id}, {"frames", std::move(frames)},
        {"due", active.due ? Json(core::iso_date(*active.due)) : Json{}},
        {"presentation", active.presentation_id ? Json(*active.presentation_id) : Json{}}};
}

struct PreparedMigration {
    SaveMigrationReview review;
    std::string archive_stamp, source_hash, output;
};

PreparedMigration prepare(const RevisionArchive& archive, const SaveMigrationRequest& request) {
    if (request.source_generation >= request.target_generation || request.target_generation != archive.revisions().back().generation)
        refuse("generation", "Choose an older source generation and the checkout's current finalized destination.");
    if (request.summary.empty() || request.summary.size() > 1024 ||
        request.summary.find_first_not_of(" \t\r\n") == std::string::npos)
        refuse("summary", "Describe the reviewed continuation and identity changes in up to 1024 bytes.");
    if (!request.source_save.is_absolute() ||
        (request.source_save.extension() != ".pgpsave" &&
         !(request.source_save.extension() == ".bak" && request.source_save.stem().extension() == ".pgpsave")))
        refuse("path", "Choose an absolute path to an older .pgpsave or .pgpsave.bak file.");
    const auto prior = archive.open(request.source_generation);
    const auto next = archive.open(request.target_generation);
    const auto input = persistence::read_bounded(request.source_save);
    auto loaded = persistence::decode_revision_save(prior.program, prior.info.game_version, input);
    const SaveRemapper remapper{prior.program, next.program, request.mappings};
    const auto protected_flows = remapper.validate_saved_flows(loaded.checkpoint);
    auto candidate = loaded.checkpoint;
    remapper.translate(candidate);
    auto output = persistence::encode_revision_save(next.program, next.info.game_version, candidate, loaded.autosave_sequence);
    const auto reloaded = persistence::decode_revision_save(next.program, next.info.game_version, output);
    if (reloaded.checkpoint != candidate || reloaded.autosave_sequence != loaded.autosave_sequence)
        refuse("roundtrip", "The destination codec did not preserve the reviewed checkpoint.");
    Json pending = Json::array();
    const auto& state = candidate.runtime;
    for (std::size_t i = 0; i < state.activations.size(); ++i) {
        const auto& active = state.activations[i];
        if (active.status != runtime::ActivationStatus::completed)
            pending.push_back({{"id", active.id}, {"before", continuation(loaded.checkpoint.runtime.activations[i])},
                {"after", continuation(active)}});
    }
    const auto source_hash = persistence::sha256(input);
    Json report{{"format", 1}, {"source", revision_evidence(prior.info)}, {"target", revision_evidence(next.info)},
        {"source_save_sha256", source_hash}, {"export_save_sha256", persistence::sha256(output)},
        {"archive_sha256", archive.stamp()}, {"summary", request.summary}, {"mappings", remapper.evidence()},
        {"date", core::iso_date(candidate.world.simulation.date)}, {"scenario", candidate.world.scenario_id},
        {"protected_graphs", protected_flows}, {"occurrence_states", state.lifecycles.size()},
        {"receipts", state.receipts.size()}, {"owned_modifiers", state.modifiers.size()},
        {"resolved_random_draws", state.random_resolutions.size()}, {"pending", std::move(pending)}};
    const auto id = persistence::sha256(report.dump());
    report["plan_id"] = id;
    auto report_bytes = report.dump() + "\n";
    if (report_bytes.size() > maximum_receipt_bytes) refuse("budget", "The migration evidence exceeds its byte budget.");
    return {{id, std::move(report_bytes)}, archive.stamp(), source_hash, std::move(output)};
}
} // namespace

SaveMigrationReview review_save_migration(const fs::path& checkout, const SaveMigrationRequest& request) {
    return prepare(RevisionArchive{checkout}, request).review;
}

fs::path export_save_migration(const fs::path& checkout, const SaveMigrationRequest& request,
    std::string_view reviewed_plan, std::string_view export_name) {
    if (!content::valid_id(export_name) || export_name.back() == '.')
        refuse("path", "Use a new lowercase ID for the export directory.");
    const RevisionArchive archive{checkout};
    const auto plan = prepare(archive, request);
    if (reviewed_plan != plan.review.plan_id)
        refuse("conflict", "The save, revisions or mappings changed. Review them again before writing.");
    const auto& root = archive.root();
    const auto relative = fs::path{exports_path} / export_name;
    if (fs::exists(detail::checked_path(root, relative)))
        refuse("conflict", "The selected export directory already exists and was preserved.");
    fs::create_directories(root / exports_path);
    (void)detail::checked_path(root, relative);
    const auto destination = root / relative;
    if (!fs::create_directory(destination)) refuse("conflict", "The export directory was claimed by another operation.");
    const auto receipt = destination / "review.json";
    const auto output = destination / "campaign.pgpsave";
    const auto no_replace = [](std::string_view) { refuse("conflict", "Existing migration files were preserved."); };
    persistence::atomic_write(receipt, plan.review.report_json, no_replace, maximum_receipt_bytes, false);
    // The completed save is the publication marker. Verify both the original
    // bytes and the current finalized contract again before installing it.
    if (RevisionArchive{root}.stamp() != plan.archive_stamp ||
        persistence::sha256(persistence::read_bounded(request.source_save)) != plan.source_hash)
        refuse("conflict", "The source or finalized checkout changed during export. Review again with a new export name.");
    (void)detail::checked_path(root, relative / "campaign.pgpsave");
    persistence::atomic_write(output, plan.output, no_replace, persistence::default_maximum_save_bytes, false);
    return output;
}
} // namespace pgp::studio
