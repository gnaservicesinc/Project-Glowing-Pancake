#include <pgp/studio/revision_review.hpp>

#include "checkout.hpp"
#include "snapshot_version.hpp"
#include "structured_document.hpp"
#include <pgp/core/build_identity.hpp>
#include <pgp/persistence/sha256.hpp>

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

namespace pgp::studio {
namespace {
namespace fs = std::filesystem;
using Json = detail::SnapshotJson;
constexpr auto state_path = "authoring/content-state.json";
constexpr auto history_path = "authoring/content-history.json";
constexpr auto versions_path = "project-versions.json";

std::string required(const fs::path& root, const fs::path& path,
                     std::size_t limit = 4U * 1024U * 1024U) {
    auto value = detail::snapshot_read(root, path, limit);
    if (!value) detail::snapshot_refuse("missing_snapshot", "Missing finalized revision evidence: " + path.generic_string());
    return std::move(*value);
}

std::string display_value(const Json& value) {
    return value.is_null() ? "(absent)" : value.is_string() ? value.get<std::string>() : value.dump();
}

void differences(const Json& before, const Json& after, const std::string& section,
                 const std::string& path, std::vector<RevisionDifference>& out) {
    if (before == after) return;
    static const Json absent;
    if ((before.is_object() || before.is_null()) && (after.is_object() || after.is_null())) {
        std::set<std::string, std::less<>> keys;
        if (before.is_object()) for (const auto& item : before.items()) keys.insert(item.key());
        if (after.is_object()) for (const auto& item : after.items()) keys.insert(item.key());
        for (const auto& key : keys)
            differences(before.contains(key) ? before.at(key) : absent, after.contains(key) ? after.at(key) : absent,
                section, path + "/" + detail::pointer_escape(key), out);
        if (!keys.empty()) return;
    }
    if ((before.is_array() || before.is_null()) && (after.is_array() || after.is_null())) {
        const auto size = std::max(before.size(), after.size());
        for (std::size_t i = 0; i < size; ++i)
            differences(i < before.size() ? before.at(i) : absent, i < after.size() ? after.at(i) : absent,
                section, path + "/" + std::to_string(i), out);
        if (size != 0) return;
    }
    out.push_back({section, path, display_value(before), display_value(after)});
}

ProbeFixture fixture(std::string_view scenario, std::string_view headquarters, std::uint64_t seed) {
    return {std::string(scenario), std::string(headquarters), {}, seed};
}
} // namespace

RevisionArchive::RevisionArchive(const fs::path& checkout) : root_(detail::checkout_root(checkout)) {
    const auto marker_bytes = required(root_, state_path);
    const auto versions_bytes = required(root_, versions_path, 65536);
    const auto current = check_snapshot(root_);
    const auto marker = detail::snapshot_parse(marker_bytes);
    const auto versions = detail::snapshot_parse(versions_bytes);
    const auto history_bytes = required(root_, history_path);
    const auto history = detail::snapshot_parse(history_bytes, detail::snapshot_history_bytes, detail::snapshot_history_tokens);
    if (marker.at("generation") != current.generation || marker.at("collection_hash") != current.collection_hash ||
        marker.at("history_hash") != persistence::sha256(history_bytes) ||
        marker_bytes != required(root_, state_path) || versions_bytes != required(root_, versions_path, 65536) ||
        fs::exists(detail::checked_path(root_, ".pgp-editor/snapshot-transaction")))
        detail::snapshot_refuse("conflict", "The finalized history changed while opening it. Refresh the review.");
    const auto& build = core::build_identity();
    const Json compatibility{{"save_format", build.save_format}, {"content_schema", build.content_schema},
        {"simulation_rules", build.simulation_rules}};
    const auto& batches = history.at("batches");
    if (batches.size() != current.generation)
        detail::snapshot_refuse("history", "The finalized generation sequence is incomplete.");
    for (const auto& row : batches) {
        const auto generation = revisions_.size() + 1;
        std::ostringstream name;
        name << "authoring/snapshots/" << std::setw(6) << std::setfill('0') << generation << ".collection.json";
        const Json previous = revisions_.empty() ? Json{} : Json(revisions_.back().content_version);
        if (row.at("generation") != generation || row.at("snapshot") != name.str() ||
            row.at("previous_content_version") != previous)
            detail::snapshot_refuse("history", "A finalized generation has an invalid history link.");
        const auto game = generation == current.generation ? versions.at("components").at("game") : row.at("game_version_at_finalization");
        auto& info = revisions_.emplace_back(RevisionInfo{generation, row.at("content_version"), game,
            row.at("collection_hash"), row.at("summary")});
        (void)detail::snapshot_version(info.content_version);
        (void)detail::snapshot_version(info.game_version);
        snapshots_.emplace_back(name.str());
        supported_.push_back(row.at("compatibility") == compatibility);
    }
    stamp_ = persistence::sha256(marker_bytes + "\n" + versions_bytes);
}

FinalizedRevision RevisionArchive::open(std::uint64_t generation) const {
    if (generation == 0 || generation > revisions_.size())
        detail::snapshot_refuse("generation", "Choose a finalized generation from this checkout.");
    const auto index = static_cast<std::size_t>(generation - 1);
    if (!supported_[index])
        detail::snapshot_refuse("compatibility", "The generation's schema, simulation rules or save format differs from this build.");
    const auto& info = revisions_[index];
    const auto bytes = required(root_, snapshots_[index], content::maximum_document_bytes);
    auto program = detail::compile_source(bytes, content::foundation_registry());
    if (program.collection.content_version != info.content_version ||
        persistence::sha256(program.canonical_content_bytes) != info.collection_hash)
        detail::snapshot_refuse("snapshot", "The finalized collection differs from its recorded generation hash.");
    return {info, std::move(program)};
}

std::vector<RevisionDifference> compare_finalized(const FinalizedRevision& before, const FinalizedRevision& after) {
    const auto left = detail::structured_document(before.program), right = detail::structured_document(after.program);
    std::vector<RevisionDifference> out;
    for (const auto& [key, definition] : left.at("definitions").items()) {
        const auto& next = right.at("definitions");
        differences(definition, next.contains(key) ? next.at(key) : Json{}, key,
            "/definitions/" + detail::pointer_escape(key), out);
    }
    for (const auto& [key, definition] : right.at("definitions").items())
        if (!left.at("definitions").contains(key))
            differences(Json{}, definition, key, "/definitions/" + detail::pointer_escape(key), out);
    for (const auto* key : {"messages", "translations", "variables", "asset_records"})
        differences(left.value(key, Json{}), right.value(key, Json{}), key, "/" + std::string(key), out);
    return out;
}

MatchedFixture::MatchedFixture(const FinalizedRevision& before, const FinalizedRevision& after,
    std::string_view before_scenario, std::string_view after_scenario,
    std::string_view headquarters, std::uint64_t seed)
    : before_(before.program, fixture(before_scenario, headquarters, seed)),
      after_(after.program, fixture(after_scenario, headquarters, seed)) {
    if (this->before().world.campaign_start != this->after().world.campaign_start)
        throw std::invalid_argument("The chosen scenario starts on different dates. Select a common opening date.");
}

template<class Before, class After> MatchedSteps MatchedFixture::apply(Before before, After after) {
    auto left = before_;
    auto right = after_;
    MatchedSteps steps{before(left), after(right)};
    if (steps.before.trace.accepted() && steps.after.trace.accepted() &&
        steps.before.after_date == steps.after.after_date) {
        before_ = std::move(left);
        after_ = std::move(right);
    } else {
        const auto reject = [](ProbeStep& step) {
            if (!step.trace.accepted()) return;
            step.trace.reason = runtime::RuntimeReason::invalid_state;
            step.trace.explanation = "The paired operation did not commit because the other fixture refused or reached a different date.";
            step.trace.committed = false;
            for (auto& path : step.trace.paths) path.committed = false;
            step.after_date = step.before_date;
            step.after_cash = step.before_cash;
            step.effects.clear();
            step.postings.clear();
        };
        reject(steps.before);
        reject(steps.after);
        steps.before.active_modifiers = this->before().runtime.modifiers;
        steps.after.active_modifiers = this->after().runtime.modifiers;
    }
    return steps;
}

MatchedSteps MatchedFixture::prepare() {
    const auto run = [](FlowProbe& side) { return side.prepare(); };
    return apply(run, run);
}

MatchedSteps MatchedFixture::advance() {
    const auto run = [](FlowProbe& side) { return side.submit(core::AdvanceDay{}); };
    return apply(run, run);
}

MatchedSteps MatchedFixture::choose(const FixtureChoice& before, const FixtureChoice& after) {
    if (before.choice_id != after.choice_id)
        throw std::invalid_argument("Choose the same authored response ID in both fixture revisions.");
    return apply([&](FlowProbe& side) { return side.choose(before.presentation_id, before.choice_id); },
        [&](FlowProbe& side) { return side.choose(after.presentation_id, after.choice_id); });
}
} // namespace pgp::studio
