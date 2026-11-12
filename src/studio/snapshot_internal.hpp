#pragma once
#include <pgp/studio/snapshot.hpp>
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <map>
namespace pgp::studio::detail {
using SnapshotJson=nlohmann::json;
inline constexpr std::string_view snapshot_history_path = "authoring/content-history.json";
inline constexpr std::size_t snapshot_history_bytes = 8U * 1024U * 1024U;
inline constexpr std::size_t snapshot_history_tokens = 600'000;
inline constexpr std::size_t snapshot_history_generations = 512;
struct EditedSnapshot {
    std::string_view collection;
    std::string_view expected_source;
    std::string_view expected_versions;
};
struct SnapshotFile {
    std::filesystem::path path;
    std::optional<std::string> before;
    std::string after;
};
struct PreparedSnapshot {
    SnapshotReview review;
    SnapshotResult result;
    std::vector<SnapshotFile> writes;
    std::map<std::filesystem::path,std::optional<std::string>> observed;
    std::map<std::filesystem::path,std::pair<std::string,std::size_t>> asset_observations;
};
[[noreturn]] void snapshot_refuse(std::string code,std::string message);
[[nodiscard]] SnapshotJson snapshot_parse(std::string_view bytes,std::size_t maximum_bytes=4U*1024U*1024U,
    std::size_t maximum_tokens=200000);
void snapshot_fields(const SnapshotJson& value,std::initializer_list<std::string_view> required,std::initializer_list<std::string_view> optional = {});
[[nodiscard]] std::optional<std::string> snapshot_read(const std::filesystem::path& root,const std::filesystem::path& path,std::size_t limit=4U*1024U*1024U);
[[nodiscard]] PreparedSnapshot prepare_snapshot(const std::filesystem::path& root,const SnapshotRequest& request,
    std::optional<EditedSnapshot> edited = {});
void write_snapshot_batch(const std::filesystem::path& root,const PreparedSnapshot& plan,const SnapshotWriteObserver& observer);
}
