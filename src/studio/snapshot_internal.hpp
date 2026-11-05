#pragma once
#include <pgp/studio/snapshot.hpp>
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <map>
namespace pgp::studio::detail {
using SnapshotJson=nlohmann::json;
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
};
[[noreturn]] void snapshot_refuse(std::string code,std::string message);
[[nodiscard]] SnapshotJson snapshot_parse(std::string_view bytes,std::size_t maximum_bytes=4U*1024U*1024U);
void snapshot_fields(const SnapshotJson& value,std::initializer_list<std::string_view> required,std::initializer_list<std::string_view> optional = {});
[[nodiscard]] std::optional<std::string> snapshot_read(const std::filesystem::path& root,const std::filesystem::path& path,std::size_t limit=4U*1024U*1024U);
[[nodiscard]] PreparedSnapshot prepare_snapshot(const std::filesystem::path& root,const SnapshotRequest& request);
void write_snapshot_batch(const std::filesystem::path& root,const PreparedSnapshot& plan,const SnapshotWriteObserver& observer);
}
