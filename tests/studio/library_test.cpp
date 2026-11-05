#include <pgp/studio/library.hpp>
#include <pgp/studio/draft.hpp>
#include <pgp/studio/flow_forms.hpp>

#include <pgp/persistence/storage.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs = std::filesystem;
using namespace pgp;
std::size_t checks = 0;
void require(bool valid, const char* detail) {
    ++checks;
    if (!valid) throw std::runtime_error(detail);
}
struct Scratch {
    fs::path root = fs::path{PGP_STUDIO_SCRATCH_PATH} /
        ("library-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        if (!fs::create_directory(root)) throw std::runtime_error("Studio scratch already exists");
    }
    ~Scratch() { std::error_code error; fs::remove_all(root, error); }
};
void put(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary);
    output << bytes;
    output.close();
    if (!output) throw std::runtime_error("Cannot write the fixture");
}
void marker(const fs::path& root, int schema) {
    put(root / "project-versions.json",
        "{\"manifest_format\":1,\"project_id\":\"project-glowing-pancake\",\"compatibility\":{\"content_schema\":" +
        std::to_string(schema) + "}}");
}
constexpr auto authoring_source = "data/runtime/current.json";
void prepare_authoring_checkout(const fs::path& root) {
    marker(root, 1);
    fs::create_directory(root / ".git");
    put(root / "CMakeLists.txt", "# development fixture\n");
    put(root / "src/content/collection.cpp", "// source tree marker\n");
    put(root / authoring_source, persistence::read_bounded(PGP_STUDIO_FIXTURE_PATH));
}
const studio::LibraryDocument& document(const studio::Library& library, const fs::path& path) {
    const auto found = std::find_if(library.documents.begin(), library.documents.end(),
        [&](const auto& entry) { return entry.relative_path == path; });
    if (found == library.documents.end()) throw std::runtime_error("Missing expected library document");
    return *found;
}
const studio::DefinitionEntry& definition(const studio::LibraryDocument& data, std::string_view id) {
    const auto found = std::find_if(data.definitions.begin(), data.definitions.end(),
        [&](const auto& entry) { return entry.id == id; });
    if (found == data.definitions.end()) throw std::runtime_error("Missing expected definition");
    return *found;
}
bool includes(const std::vector<std::string>& values, std::string_view target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}
template<class Action> void rejected(Action action, const char* detail) {
    try { action(); }
    catch (const std::invalid_argument&) { require(true, detail); return; }
    throw std::runtime_error(detail);
}
template<class Action> void refused_write(Action action, std::string_view code) {
    try { action(); }
    catch (const persistence::PersistenceError& error) { require(error.code() == code, error.what()); return; }
    throw std::runtime_error("Expected draft refusal " + std::string(code));
}
void inspect_source_and_drafts() {
    Scratch scratch;
    const auto root = scratch.root / "Author checkout";
    marker(root, 1);
    rejected([&] { (void)studio::inspect_checkout(root); }, "A marker-only package must be refused");
    put(root / ".git", "gitdir: ../git-metadata/worktrees/fixture\n");
    put(root / "CMakeLists.txt", "# development fixture\n");
    put(root / "src/content/collection.cpp", "// source tree marker\n");
    const auto bytes = persistence::read_bounded(PGP_STUDIO_FIXTURE_PATH);
    put(root / "data/runtime/current.json", bytes);
    put(root / "data/drafts/working draft.json", bytes + "\n");
    put(root / "data/drafts/broken.json", "{");
    put(root / "data/drafts/future.json", "{\"content_schema\":2,\"content_version\":\"0.0.0-dev\","
        "\"currency_basis\":\"constant_2014_cents\",\"definitions\":[]}");
    put(root / "tests/content/fixtures/foundation.json", bytes);
    put(root / "unrelated.json", bytes);
    const auto outside = scratch.root / "outside.json";
    put(outside, bytes);
    std::error_code error;
    fs::create_symlink(outside, root / "data/drafts/escaped.json", error);
    const bool linked = !error;

    const auto opened = studio::inspect_checkout(root);
    require(opened.checkout == fs::canonical(root) && opened.checkout_schema == 1, "The selected project identity must be verified");
    require(opened.documents.size() == (linked ? 6U : 5U), "Only configured collection roots may appear in the library");
    require(std::is_sorted(opened.documents.begin(), opened.documents.end(), [](const auto& a, const auto& b) {
        return a.relative_path.generic_u8string() < b.relative_path.generic_u8string();
    }), "Stable library order must be independent of directory iteration");
    const auto& current = document(opened, "data/runtime/current.json");
    const auto& draft = document(opened, "data/drafts/working draft.json");
    require(current.origin == studio::Origin::reviewed && draft.origin == studio::Origin::draft &&
            current.validation == studio::Validation::validated && draft.validation == studio::Validation::validated,
            "Reviewed and draft copies must be separately labeled and share the compiler");
    require(current.source_hash != draft.source_hash && current.canonical_hash == draft.canonical_hash &&
            current.content_version == "0.0.0-dev", "Source fingerprints must distinguish a nonsemantic external edit");
    const auto& scenario = definition(current, "scenario.foundation");
    const auto& offer = definition(current, "flow.supplier.exclusivity_offer");
    const auto& deal = definition(current, "deal.compute_exclusivity");
    require(scenario.kind == "scenario" && scenario.revision == 1 && includes(scenario.uses, offer.id) &&
            includes(offer.used_by, scenario.id) && includes(offer.uses, deal.id) && includes(deal.used_by, offer.id),
            "Reference edges must follow the validated typed content in both directions");
    require(document(opened, "data/drafts/broken.json").validation == studio::Validation::invalid &&
            document(opened, "data/drafts/future.json").validation == studio::Validation::unsupported &&
            document(opened, "tests/content/fixtures/foundation.json").origin == studio::Origin::engineering_fixture,
            "Broken, future and engineering documents must stay visible and separate");
    if (linked) {
        const auto& blocked = document(opened, "data/drafts/escaped.json");
        require(blocked.validation == studio::Validation::blocked && blocked.source_hash.empty() && blocked.definitions.empty(),
                "The library must not open a symlink escaping the selected checkout");
    } else std::cout << "Skipped symlink fixture: " << error.message() << '\n';
    require(persistence::read_bounded(root / "data/drafts/working draft.json") == bytes + "\n",
            "Opening the library must preserve exact authored bytes");
    marker(root, 2);
    const auto future = studio::inspect_checkout(root);
    require(future.checkout_schema == 2 && document(future, "data/runtime/current.json").validation == studio::Validation::unsupported &&
            document(future, "data/runtime/current.json").source_hash.empty(),
            "Unknown checkout schema must list documents without opening them");
    rejected([&] { (void)studio::inspect_checkout("relative checkout"); }, "A checkout must be selected explicitly by absolute path");
}
void typed_drafts_and_external_conflicts() {
    Scratch scratch;
    const auto root = scratch.root / "author";
    marker(root, 1);
    fs::create_directory(root / ".git");
    put(root / "CMakeLists.txt", "# development fixture\n");
    put(root / "src/content/collection.cpp", "// source tree marker\n");
    const auto bytes = persistence::read_bounded(PGP_STUDIO_FIXTURE_PATH);
    const fs::path source{"data/runtime/current.json"};
    put(root / source, bytes);
    studio::DraftDocument draft{root, source};
    require(!draft.is_draft() && !draft.dirty() && !fs::exists(root / "data/drafts"),
            "Opening a draft working copy must preserve the reviewed source and create no folders");
    refused_write([&] { draft.save(); }, "path");
    constexpr auto id = "flow.supplier.exclusivity_offer";
    auto edit = draft.collection();
    auto& target = edit.definitions.at(id);
    auto& flow = std::get<content::FlowDefinition>(target.data);
    flow.tags.push_back("reviewed_terms");
    require(draft.replace(edit) && draft.dirty() && draft.can_undo() && !draft.can_redo(),
            "A valid typed change must add one undo entry");
    require(!draft.replace(edit), "A no-op edit must not enter history");
    auto invalid = edit;
    invalid.definitions.at(id).revision += 1;
    refused_write([&] { (void)draft.replace(invalid); }, "version");
    require(draft.undo() && !draft.dirty() && draft.can_redo() && !draft.can_undo(),
            "Undo must restore the saved semantics and retain redo");
    require(draft.redo() && draft.dirty() && draft.can_undo() && !draft.can_redo(),
            "Redo must restore the exact typed change");
    refused_write([&] { draft.save("../escape.json"); }, "path");
    put(root / source, bytes + "\n");
    refused_write([&] { draft.save("working.json"); }, "conflict");
    require(draft.dirty() && !fs::exists(root / "data/drafts"), "A stale source must leave edits staged without creating a draft");
    put(root / source, bytes);
    draft.save("working.json");
    const auto path = root / "data/drafts/working.json";
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";
    const auto saved = persistence::read_bounded(path);
    require(draft.is_draft() && !draft.dirty() && draft.can_undo() && persistence::read_bounded(root / source) == bytes,
            "Explicit Save As must keep the reviewed collection untouched and preserve undo");
    studio::DraftDocument reopened{root, "data/drafts/working.json"};
    require(includes(std::get<content::FlowDefinition>(reopened.collection().definitions.at(id).data).tags, "reviewed_terms") &&
            reopened.collection().definitions.at(id).revision == target.revision &&
            reopened.collection().content_version == edit.content_version,
            "Reopening a typed draft must retain tags and the original version identities");
    refused_write([&] { reopened.save("working.json"); }, "conflict");
    require(draft.undo() && draft.dirty(), "Undo after saving must produce a new unsaved draft change");
    put(path, saved + " ");
    refused_write([&] { draft.save(); }, "conflict");
    require(draft.dirty() && persistence::read_bounded(path) == saved + " ",
            "An external nonsemantic edit must be detected and preserved");
    fs::remove(path);
    refused_write([&] { draft.save(); }, "conflict");
    require(!fs::exists(path), "Deleting a draft externally must not cause the editor to recreate it");
    put(path, saved);
    put(temporary, "unrelated pending data");
    refused_write([&] { draft.save(); }, "pending_write");
    require(persistence::read_bounded(temporary) == "unrelated pending data", "Pending work must remain intact");
    fs::remove(temporary);
    draft.save();
    require(!draft.dirty() && persistence::read_bounded(backup) == saved,
            "Replacing a draft must preserve the previous valid version");
    const auto rewritten = persistence::read_bounded(path);
    draft.save();
    require(persistence::read_bounded(path) == rewritten && persistence::read_bounded(backup) == saved,
            "A no-op save must preserve both exact current bytes and the recovery copy");
    marker(root, 2);
    refused_write([&] { draft.save(); }, "conflict");
    refused_write([&] { (void)studio::DraftDocument{root, source}; }, "unsupported_schema");

    marker(root, 1);
    studio::DraftDocument escaped{root, source};
    fs::rename(root / "data/drafts", root / "data/draft-hold");
    const auto outside = scratch.root / "outside";
    fs::create_directory(outside);
    std::error_code error;
    fs::create_directory_symlink(outside, root / "data/drafts", error);
    if (!error) {
        rejected([&] { escaped.save("unsafe.json"); }, "The draft writer must refuse a redirected parent directory");
        require(fs::is_empty(outside), "A redirected parent must not receive a file");
    }
}
void typed_trigger_forms() {
    Scratch scratch;
    const auto root = scratch.root;
    prepare_authoring_checkout(root);
    studio::DraftDocument draft{root, authoring_source};
    constexpr auto id = "flow.supplier.exclusivity_offer";
    auto open = [&](std::string_view row) { return studio::FlowForm::open(draft, id, row); };
    auto change = [&](std::string_view row, std::string_view field, std::string_view value = {}) {
        require(open(row).apply(draft, field, value), "Expected typed form mutation");
    };
    auto trigger = [&]() -> const content::Condition& {
        return std::get<content::FlowDefinition>(draft.collection().definitions.at(id).data).trigger;
    };
    const auto field = [](const studio::FlowForm& form, std::string_view key) -> const studio::FormField& {
        const auto selected = std::find_if(form.fields().begin(), form.fields().end(), [&](const auto& entry) { return entry.key == key; });
        if (selected == form.fields().end()) throw std::runtime_error("Expected authoring field " + std::string(key));
        return *selected;
    };

    const auto first = open("trigger/0");
    require(field(first, "value").help.find("constant-2014 cents") != std::string::npos &&
            field(first, "value").value == "2000000", "Comparison should show the exact registry unit and value");
    rejected([&] { (void)first.apply(draft, "fact", "world.public_breakthrough_count"); }, "Reject a fact from another scope");
    rejected([&] { (void)first.apply(draft, "value", "2e6"); }, "Reject exponent input instead of coercing it");
    require(!draft.dirty() && draft.edit_revision() == 0, "Rejected input must leave the draft and history untouched");
    change("trigger/0", "value", "9007199254740993");
    require(std::get<std::int64_t>(trigger().children[0].clause->value) == 9'007'199'254'740'993LL,
            "Authoring must keep integers above the exact JavaScript range");
    rejected([&] { (void)first.apply(draft, "comparison", "lt"); }, "Reject a stale revision-bound form");
    rejected([&] { (void)open("trigger/1").apply(draft, "comparison", "lt"); }, "Boolean comparisons cannot order values");
    change("trigger/0", "operand", "expression");
    change("trigger/0/expression", "kind", "add");
    change("trigger/0/expression/1", "kind", "fact");
    rejected([&] { (void)open("trigger/0/expression/1").apply(draft, "fact", "company.is_operating"); },
             "Expression fact picker must reject incompatible types");
    change("trigger/0/expression/1", "fact", "company.monthly_compute_bill_cents");
    change("trigger/0/expression/1", "kind", "scale");
    change("trigger/0/expression/1", "numerator", "2");
    rejected([&] { (void)open("trigger/0/expression/1").apply(draft, "denominator", "0"); }, "Refuse an invalid scale ratio");
    change("trigger/0/expression/1", "denominator", "3");

    change("trigger", "append");
    change("trigger/2", "kind", "related");
    change("trigger/2", "relation", "company.projects");
    change("trigger/2", "matching", "count");
    change("trigger/2", "count", "2");
    const auto member = open("trigger/2/0");
    require(field(member, "fact").value.starts_with("project."), "Related predicates use the selected member scope");
    rejected([&] { (void)member.apply(draft, "kind", "duration"); }, "Related members cannot acquire duration history");
    change("trigger", "kind", "duration");
    change("trigger", "days", "15");
    change("trigger/0", "kind", "any");
    const auto outline = studio::flow_outline(draft, id);
    require(std::any_of(outline.begin(), outline.end(), [](const auto& row) {
        return row.key == "trigger/0/0/expression/1/0" && row.depth == 5;
    }), "Outline should expose the nested condition and arithmetic operand path");
    require(draft.undo() && trigger().children.front().kind == content::ConditionKind::all_of,
            "Undo should restore the previous group operation");
    require(draft.redo() && trigger().children.front().kind == content::ConditionKind::any_of,
            "Redo should restore the new group operation");
    draft.save("trigger-forms.json");
    studio::DraftDocument reopened{root, "data/drafts/trigger-forms.json"};
    const auto canonical = [&](const auto& document) {
        return runtime::compile_collection(document.collection(), content::foundation_registry()).canonical_content_bytes;
    };
    require(canonical(reopened) == canonical(draft) && !reopened.dirty(),
            "Save and reopen must retain the exact shared compiler representation");
}
void lifecycle_forms_and_references() {
    using namespace content;
    Scratch scratch;
    prepare_authoring_checkout(scratch.root);
    studio::DraftDocument draft{scratch.root, authoring_source};
    auto id = std::string("flow.form.review");
    require(studio::create_flow(draft, id, "company"), "Create a typed company flow in the working collection");
    auto select = [&]() -> const FlowDefinition& {
        return std::get<FlowDefinition>(draft.collection().definitions.at(id).data);
    };
    const auto change = [&](std::string_view row, std::string_view field, std::string_view value) {
        require(studio::FlowForm::open(draft, id, row).apply(draft, field, value), "Expected lifecycle or definition edit");
    };
    require(select().scope == Scope::company && select().nodes.at("end").kind == NodeKind::end &&
            select().lifecycle.mode == LifecycleMode::once, "New flows need a valid once lifecycle, guard and terminal");
    rejected([&] { (void)studio::create_flow(draft, id, "world"); }, "Reject duplicate flow IDs");
    rejected([&] { (void)studio::create_flow(draft, "flow.invalid", "project"); }, "Member scope cannot become a flow root");
    change("flow", "scenario/scenario.foundation", "true");
    change("lifecycle", "mode", "recurring");
    change("lifecycle", "maximum", "4");
    change("lifecycle", "cooldown", "17");
    require(select().lifecycle.cooldown_days == 17 && select().lifecycle.max_occurrences == 4,
            "Recurring form must retain exact cooldown and occurrence bound");
    change("lifecycle", "mode", "interval");
    change("lifecycle", "interval_unit", "months");
    change("lifecycle", "interval", "1");
    change("lifecycle", "anchor", "fixed_date");
    change("lifecycle", "anchor_date", "2016-01-31");
    change("lifecycle", "offset_unit", "months");
    change("lifecycle", "offset", "1");
    const auto interval = studio::FlowForm::open(draft, id, "lifecycle");
    require(std::any_of(interval.fields().begin(), interval.fields().end(), [](const auto& field) {
        return field.label == "First due at scenario opening" && field.value == "2016-02-29";
    }), "Fixed month anchors must use the real scheduler's leap-day clamp");
    change("lifecycle", "mode", "scheduled");
    change("lifecycle", "due", "2016-02-29");
    try {
        (void)studio::FlowForm::open(draft, id, "lifecycle").apply(draft, "due", "2015-02-29");
        throw std::runtime_error("Accepted an impossible scheduled date");
    } catch (const std::out_of_range&) {
        require(core::iso_date(*select().lifecycle.due_date) == "2016-02-29", "Invalid date leaves the draft unchanged");
    }
    change("trigger", "kind", "duration");
    change("lifecycle", "mode", "continuous");
    change("lifecycle", "reentry", "true");
    const auto& exit = *select().lifecycle.exit_when;
    require(exit.kind == ConditionKind::not_condition && exit.children.front().kind == ConditionKind::holds_for_days &&
            exit.children.front().predicate_id != select().trigger.predicate_id && select().lifecycle.allow_reentry,
            "Continuous defaults need an independent, editable exit timer");
    change("exit/0", "days", "3");
    change("lifecycle", "mode", "once");
    require(!select().lifecycle.exit_when && select().exit_node.empty() && select().nodes.size() == 1,
            "Switching away from continuous drops only the unused terminal");
    rejected([&] {
        (void)studio::FlowForm::open(draft, "flow.hidden.sustained_workload", "lifecycle").apply(draft, "mode", "once");
    }, "Lifecycle conversion must preserve existing exit actions and owned modifier authoring");

    require(studio::create_flow(draft, "flow.form.caller", "company"), "Create a refactor fixture flow");
    auto candidate = draft.collection();
    auto& caller = std::get<FlowDefinition>(candidate.definitions.at("flow.form.caller").data);
    Node nested{};
    nested.kind = NodeKind::subflow;
    nested.subflow_id = id;
    nested.next = "end";
    caller.nodes.emplace("call", std::move(nested));
    caller.entry_node = "call";
    require(draft.replace(candidate), "Install the typed reference fixture");
    change("flow", "id", "flow.form.renamed");
    const auto scenario_has = [&](std::string_view target) {
        const auto& scenario = std::get<ScenarioDefinition>(draft.collection().definitions.at("scenario.foundation").data);
        return includes(scenario.flows, target);
    };
    require(!draft.collection().find(id) && draft.collection().find("flow.form.renamed") &&
            scenario_has("flow.form.renamed") && !scenario_has(id) &&
            std::get<FlowDefinition>(draft.collection().definitions.at("flow.form.caller").data).nodes.at("call").subflow_id == "flow.form.renamed",
            "Rename must atomically update scenario and subflow references");
    require(draft.undo() && scenario_has(id), "Undo restores both a definition ID and its uses");
    require(draft.redo() && scenario_has("flow.form.renamed"), "Redo reapplies the refactor atomically");
}
} // namespace

int main() {
    try {
        inspect_source_and_drafts();
        typed_drafts_and_external_conflicts();
        typed_trigger_forms();
        lifecycle_forms_and_references();
        std::cout << checks << " Studio library checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
