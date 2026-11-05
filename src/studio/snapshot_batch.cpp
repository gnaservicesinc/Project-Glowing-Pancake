#include "snapshot_internal.hpp"
#include "checkout.hpp"
#include <pgp/persistence/sha256.hpp>
#include <algorithm>
#include <set>

namespace pgp::studio::detail {
namespace {
namespace fs=std::filesystem;
constexpr std::string_view pending=".pgp-editor/snapshot-transaction";
const fs::path marker{"authoring/content-state.json"};
bool owned_path(const fs::path& path) {
    const auto value=path.generic_string();
    return value=="project-versions.json" || value=="authoring/content-history.json" || value==marker.generic_string() ||
        value=="data/runtime/content-manifest.json" ||
        (value.starts_with("authoring/snapshots/") && path.extension()==".json") ||
        (value.starts_with("data/runtime/") && path.extension()==".json");
}
void directory(const fs::path& root,const fs::path& path) {
    auto current=fs::path{};
    for (const auto& component:path) {
        current/=component;const auto status=checked_path(root,current);
        if (fs::exists(status)) {if (!fs::is_directory(status)) snapshot_refuse("path","Snapshot folders must be ordinary local directories.");}
        else {if (!fs::create_directory(root/current)) snapshot_refuse("conflict","Snapshot folder changed during creation.");persistence::flush_local_directory((root/current).parent_path());}
    }
}
void verify_observed(const fs::path& root,const std::map<fs::path,std::optional<std::string>>& observed) {
    for (const auto& [path,before]:observed)
        if (snapshot_read(root,path)!=before) snapshot_refuse("conflict","A source, draft or version file changed after review: "+path.generic_string());
}
void clean_owned_journal(const fs::path& root,const SnapshotJson& journal) {
    // Every entry is enumerated; unexpected files are preserved for inspection.
    const auto base=fs::path{pending};std::set<fs::path> expected{base/"journal.json"};
    for (std::size_t i=0;i<journal.at("files").size();++i) {
        expected.insert(base/(std::to_string(i)+".before"));expected.insert(base/(std::to_string(i)+".after"));
        expected.insert(base/(std::to_string(i)+".replace"));
    }
    for (const auto& entry:fs::directory_iterator(root/base)) {
        const auto relative=entry.path().lexically_relative(root);
        if (!expected.contains(relative) || !fs::is_regular_file(checked_path(root,relative))) snapshot_refuse("recovery","Unexpected transaction files were preserved for inspection.");
    }
    for (const auto& path:expected) if (fs::exists(checked_path(root,path))) fs::remove(root/path);
    fs::remove(root/base);persistence::flush_local_directory((root/base).parent_path());
}
}
[[noreturn]] void snapshot_refuse(std::string code,std::string message) {throw persistence::PersistenceError(std::move(code),std::move(message));}
SnapshotJson snapshot_parse(std::string_view bytes,std::size_t maximum_bytes) {
    if (bytes.size()>maximum_bytes) snapshot_refuse("oversized","Snapshot metadata exceeds its byte bound.");
    std::size_t tokens=0;std::vector<std::set<std::string>> keys;
    return SnapshotJson::parse(bytes.begin(),bytes.end(),[&](int depth,SnapshotJson::parse_event_t event,SnapshotJson& value){
        if (depth>64 || ++tokens>200000) snapshot_refuse("budget","Snapshot metadata exceeds its structural bound.");
        if (event==SnapshotJson::parse_event_t::object_start) keys.emplace_back();
        else if (event==SnapshotJson::parse_event_t::object_end) keys.pop_back();
        else if (event==SnapshotJson::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) snapshot_refuse("duplicate_key","Duplicate snapshot metadata key.");
        else if (event==SnapshotJson::parse_event_t::value && value.is_number_float()) snapshot_refuse("integer_token","Snapshot metadata uses exact integer tokens.");
        return true;
    });
}
void snapshot_fields(const SnapshotJson& j,std::initializer_list<std::string_view> required,std::initializer_list<std::string_view> optional) {
    if (!j.is_object()) snapshot_refuse("metadata","Expected a snapshot metadata object.");
    for (const auto key:required) if (!j.contains(key)) snapshot_refuse("metadata","Required snapshot metadata field is missing.");
    for (const auto& [key,value]:j.items()) {
        (void)value;if (std::find(required.begin(),required.end(),key)==required.end() && std::find(optional.begin(),optional.end(),key)==optional.end()) snapshot_refuse("metadata","Unsupported snapshot metadata field.");
    }
}
std::optional<std::string> snapshot_read(const fs::path& root,const fs::path& path,std::size_t limit) {
    const auto status=checked_path(root,path);if (!fs::exists(status)) return {};
    if (!fs::is_regular_file(status)) snapshot_refuse("path","Snapshot files must be ordinary files inside the selected checkout.");
    return persistence::read_bounded(root/path,limit);
}
void write_snapshot_batch(const fs::path& root,const PreparedSnapshot& plan,const SnapshotWriteObserver& observer) {
    if (plan.writes.empty()) return;
    SnapshotJson files=SnapshotJson::array();
    // Build and flush the entire journal before touching a tracked file. Before
    // bytes live in the journal itself too, so an interrupted staging phase can
    // always restore the prior generation without relying on a partial backup.
    for (const auto& file:plan.writes) {
        if (!owned_path(file.path)) snapshot_refuse("path","Snapshot plan contains an unowned destination.");
        (void)checked_path(root,file.path);
        files.push_back({{"path",file.path.generic_string()},{"before",file.before?SnapshotJson(*file.before):SnapshotJson(nullptr)},
            {"after",file.after},{"before_hash",file.before?SnapshotJson(persistence::sha256(*file.before)):SnapshotJson(nullptr)},
            {"after_hash",persistence::sha256(file.after)}});
    }
    SnapshotJson journal{{"format",1},{"plan_id",plan.review.plan_id},{"files",std::move(files)}};
    journal["sha256"]=persistence::sha256(journal.dump());
    const auto journal_bytes=journal.dump()+"\n";
    if (journal_bytes.size()>64U*1024U*1024U) snapshot_refuse("oversized","Snapshot transaction exceeds its recovery byte bound.");
    directory(root,".pgp-editor");
    if (!fs::create_directory(root/fs::path{pending})) snapshot_refuse("pending_snapshot","An existing snapshot transaction must be recovered first.");
    persistence::flush_local_directory(root/".pgp-editor");
    const auto base=fs::path{pending};
    persistence::write_staged_file(root/base/"journal.json",journal_bytes);
    for (std::size_t i=0;i<plan.writes.size();++i) {
        const auto& file=plan.writes[i];
        if (file.before) persistence::write_staged_file(root/base/(std::to_string(i)+".before"),*file.before);
        persistence::write_staged_file(root/base/(std::to_string(i)+".after"),file.after);
    }
    if (observer) observer(SnapshotWritePhase::staged,0);
    verify_observed(root,plan.observed);
    for (std::size_t i=0;i<plan.writes.size();++i) {
        const auto& file=plan.writes[i];directory(root,file.path.parent_path());
        if (snapshot_read(root,file.path)!=file.before) snapshot_refuse("conflict","A reviewed destination changed before replacement: "+file.path.generic_string());
        const auto replacement=base/(std::to_string(i)+".replace");
        persistence::write_staged_file(root/replacement,file.after);
        // Recheck after staging the durable replacement, immediately before the
        // atomic single-file rename. The marker is always the final target.
        if (snapshot_read(root,file.path)!=file.before) snapshot_refuse("conflict","Concurrent edit was preserved before replacement.");
        persistence::replace_staged_file(root/replacement,root/file.path);
        if (observer) observer(file.path==marker?SnapshotWritePhase::committed:SnapshotWritePhase::replaced,i);
    }
    clean_owned_journal(root,journal);
}
}

namespace pgp::studio {
std::string recover_snapshot(const std::filesystem::path& selected) {
    namespace fs=std::filesystem;using namespace detail;
    const auto root=checkout_root(selected);(void)checkout_schema(root);
    const fs::path base{".pgp-editor/snapshot-transaction"};
    const auto status=checked_path(root,base);if (!fs::exists(status)) return "No pending snapshot transaction.";
    if (!fs::is_directory(status)) snapshot_refuse("recovery","The transaction path is not a directory; it was preserved.");
    const auto bytes=snapshot_read(root,base/"journal.json",64U*1024U*1024U);
    if (!bytes) snapshot_refuse("recovery","Incomplete staging has no journal. No tracked files were replaced; preserve the folder for inspection.");
    auto journal=snapshot_parse(*bytes,64U*1024U*1024U);snapshot_fields(journal,{"format","plan_id","files","sha256"});
    const auto digest=journal.at("sha256");journal.erase("sha256");
    if (digest!=persistence::sha256(journal.dump()) || journal.at("format")!=1 || !journal.at("files").is_array() ||
        journal.at("files").empty() || journal.at("files").size()>16) snapshot_refuse("recovery","The transaction journal failed its integrity/count checks.");
    std::set<fs::path> paths;std::vector<SnapshotFile> files;
    for (const auto& row:journal.at("files")) {
        snapshot_fields(row,{"path","before","after","before_hash","after_hash"});
        const auto path=fs::path{row.at("path").get<std::string>()};
        if (!owned_path(path) || !paths.insert(path).second || !row.at("after").is_string()) snapshot_refuse("recovery","Recovery refused an unowned/duplicate destination.");
        (void)checked_path(root,path);
        const auto before=row.at("before").is_null()?std::nullopt:std::optional{row.at("before").get<std::string>()};
        const auto after=row.at("after").get<std::string>();
        if (row.at("after_hash")!=persistence::sha256(after) || row.at("before_hash")!=(before?SnapshotJson(persistence::sha256(*before)):SnapshotJson(nullptr))) snapshot_refuse("recovery","Recovery bytes differ from their checksums.");
        files.push_back({path,before,after});
    }
    if (files.back().path!=fs::path{"authoring/content-state.json"}) snapshot_refuse("recovery","The snapshot commit marker is missing or out of order.");
    const bool committed=snapshot_read(root,files.back().path)==files.back().after;
    for (const auto& file:files) {
        const auto current=snapshot_read(root,file.path);
        if (current!=file.before && current!=std::optional{file.after}) snapshot_refuse("conflict","Recovery preserved a concurrent third version: "+file.path.generic_string());
        if (committed && current!=std::optional{file.after}) snapshot_refuse("conflict","A committed snapshot was changed externally; no recovery overwrite was made.");
    }
    if (!committed) {
        for (std::size_t i=files.size();i>0;--i) {
            const auto& file=files[i-1];if (snapshot_read(root,file.path)==file.before) continue;
            if (file.before) {
                const auto replacement=base/(std::to_string(i-1)+".replace");
                // A crash may have left the prepared rename. It is owned only
                // if its bytes are an exact journal version.
                if (const auto existing=snapshot_read(root,replacement)) {
                    if (*existing!=file.after && *existing!=*file.before) snapshot_refuse("conflict","Unexpected recovery staging bytes were preserved.");
                    fs::remove(root/replacement);
                }
                persistence::write_staged_file(root/replacement,*file.before);
                if (snapshot_read(root,file.path)!=std::optional{file.after}) snapshot_refuse("conflict","A file changed during rollback; it was preserved.");
                persistence::replace_staged_file(root/replacement,root/file.path);
            } else {
                if (snapshot_read(root,file.path)!=std::optional{file.after}) snapshot_refuse("conflict","A file changed during rollback; it was preserved.");
                fs::remove(root/file.path);persistence::flush_local_directory((root/file.path).parent_path());
            }
        }
    }
    clean_owned_journal(root,journal);
    return committed?"Verified the committed snapshot and cleared its completed journal.":"Restored the prior complete generation; draft and concurrent edits were preserved.";
}
}
