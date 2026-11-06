#pragma once

#include <pgp/studio/flow_probe.hpp>

#include <filesystem>

namespace pgp::studio {
struct RevisionInfo {
    std::uint64_t generation;
    std::string content_version, game_version, collection_hash, summary;
};
struct FinalizedRevision {
    RevisionInfo info;
    runtime::CompiledProgram program;
};

// Historical paths and canonical hashes come from the committed snapshot
// history. The current marker and compatibility contract anchor the archive.
class RevisionArchive {
public:
    explicit RevisionArchive(const std::filesystem::path& checkout);
    [[nodiscard]] const std::vector<RevisionInfo>& revisions() const noexcept { return revisions_; }
    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] const std::string& stamp() const noexcept { return stamp_; }
    [[nodiscard]] FinalizedRevision open(std::uint64_t generation) const;
private:
    std::filesystem::path root_;
    std::vector<RevisionInfo> revisions_;
    std::vector<std::filesystem::path> snapshots_;
    std::vector<bool> supported_;
    std::string stamp_;
};

struct RevisionDifference {
    std::string section, path, before, after;
};
[[nodiscard]] std::vector<RevisionDifference> compare_finalized(
    const FinalizedRevision& before, const FinalizedRevision& after);

struct MatchedSteps {
    ProbeStep before, after;
};
struct FixtureChoice {
    std::uint64_t presentation_id;
    std::string choice_id;
};
// Both sides advance together. A rejected operation leaves both fixture
// timelines intact; the traces still explain the two attempted operations.
class MatchedFixture {
public:
    MatchedFixture(const FinalizedRevision& before, const FinalizedRevision& after,
        std::string_view before_scenario, std::string_view after_scenario,
        std::string_view headquarters, std::uint64_t seed);
    [[nodiscard]] const runtime::Checkpoint& before() const noexcept { return before_.state(); }
    [[nodiscard]] const runtime::Checkpoint& after() const noexcept { return after_.state(); }
    [[nodiscard]] MatchedSteps prepare();
    [[nodiscard]] MatchedSteps advance();
    [[nodiscard]] MatchedSteps choose(const FixtureChoice& before, const FixtureChoice& after);
private:
    template<class Before, class After> MatchedSteps apply(Before before, After after);
    FlowProbe before_, after_;
};

enum class MigrationKind { flow, node, scenario, deal, variable, message };
[[nodiscard]] std::string_view migration_kind_name(MigrationKind kind);
struct IdentityMapping {
    MigrationKind kind;
    std::string owner; // Source flow ID for a node mapping.
    std::string from, to;
};
struct SaveMigrationRequest {
    std::uint64_t source_generation, target_generation;
    std::filesystem::path source_save;
    std::vector<IdentityMapping> mappings;
    std::string summary;
};
struct SaveMigrationReview {
    std::string plan_id, report_json;
};
[[nodiscard]] SaveMigrationReview review_save_migration(
    const std::filesystem::path& checkout, const SaveMigrationRequest& request);
// Exports a strictly reloaded save and its review receipt into a new checkout
// scratch directory. Never overwrites a campaign, existing export or revision.
[[nodiscard]] std::filesystem::path export_save_migration(const std::filesystem::path& checkout,
    const SaveMigrationRequest& request, std::string_view reviewed_plan, std::string_view export_name);
} // namespace pgp::studio
