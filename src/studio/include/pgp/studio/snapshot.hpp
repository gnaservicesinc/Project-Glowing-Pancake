#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace pgp::studio {
struct SnapshotRequest {
    std::filesystem::path source;
    std::filesystem::path target = "data/runtime/first-playable.json";
    std::string content_version,summary;
    // Explicitly refuse cross-content saves until a real migration exists.
    std::string save_policy = "reject-prior-content";
    std::optional<std::string> game_version;
};
struct SnapshotReview {
    std::string plan_id;
    std::string report_json;
    bool runtime_changed = false;
};
struct SnapshotResult {
    std::uint64_t generation = 0;
    std::string content_version,collection_hash,required_game_version;
    bool game_version_requirement_resolved = false;
};
enum class SnapshotWritePhase { staged, replaced, committed };
using SnapshotWriteObserver = std::function<void(SnapshotWritePhase,std::size_t)>;

// Pure review: reuses the actual compiler, scenario/runtime and save validator.
// Writes are confined to the chosen source checkout and known owned artifacts.
[[nodiscard]] SnapshotReview review_snapshot(const std::filesystem::path& checkout,const SnapshotRequest& request);
[[nodiscard]] SnapshotResult finalize_snapshot(const std::filesystem::path& checkout,const SnapshotRequest& request,
    std::string_view reviewed_plan_id,const SnapshotWriteObserver& observer = {});
// Recovery never replaces a concurrent third version. A committed marker is
// completed/verified; an incomplete generation rolls back from staged backups.
[[nodiscard]] std::string recover_snapshot(const std::filesystem::path& checkout);
[[nodiscard]] SnapshotResult check_snapshot(const std::filesystem::path& checkout);
}
