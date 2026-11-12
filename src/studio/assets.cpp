#include <pgp/studio/assets.hpp>

#include "checkout.hpp"
#include "snapshot_internal.hpp"
#include <pgp/content/registry.hpp>
#include <pgp/content/collection.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/persistence/sha256.hpp>
#include <algorithm>
#include <set>

namespace pgp::studio {
namespace {
namespace fs = std::filesystem;
using Json = detail::SnapshotJson;
constexpr auto manifest = "assets/manifest.json";
constexpr std::size_t asset_limit = 16U * 1024U * 1024U;

std::string local_bytes(const fs::path& root, const fs::path& path, std::size_t bound) {
    if (!path.generic_string().starts_with("assets/") || !fs::is_regular_file(detail::checked_path(root,path)))
        throw std::invalid_argument("Choose an ordinary file under the checkout's assets folder.");
    return persistence::read_bounded(root/path,bound);
}
std::string field(const Json& row, const char* key, std::size_t limit, bool optional = false) {
    if (optional && (!row.contains(key) || row.at(key).is_null())) return {};
    if (!row.contains(key) || !row.at(key).is_string()) throw std::invalid_argument(std::string("Asset needs a text field named ") + key + ".");
    auto text = row.at(key).get<std::string>();
    if ((!optional && text.empty()) || text.size() > limit || text.find('\0') != std::string::npos)
        throw std::invalid_argument(std::string("Asset field is empty or too long: ") + key);
    return text;
}
AssetRecord record(const Json& row) {
    if (!row.is_object() || !row.contains("ai_generated") || !row.at("ai_generated").is_boolean())
        throw std::invalid_argument("Asset needs an explicit provenance record.");
    AssetRecord result;
    result.id = field(row,"id",128);
    result.path = field(row,"path",512);
    result.sha256 = field(row,"sha256",64);
    result.kind = field(row,"kind",16);
    result.title = field(row,"title",256);
    result.creator = field(row,"creator",512);
    result.source_url = field(row,"source_url",2048,true);
    result.retrieved_on = field(row,"retrieved_on",10);
    result.license_id = field(row,"license_id",128);
    result.license_url = field(row,"license_url",2048,true);
    result.license_evidence_path = field(row,"license_evidence_path",512);
    result.modifications = field(row,"modifications",4096,true);
    result.purpose = field(row,"purpose",512);
    result.ai_generated = row.at("ai_generated").get<bool>();
    result.generation_record = field(row,"generation_record",4096,true);
    result.credit_text = field(row,"credit_text",2048);
    result.credit_group = field(row,"credit_group",128);
    result.review_status = field(row,"review_status",128);
    result.source_metadata = row.dump();
    if (!content::valid_id(result.id) || result.sha256.size()!=64 ||
        result.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos ||
        (result.kind!="image" && result.kind!="audio" && result.kind!="font" && result.kind!="data"))
        throw std::invalid_argument("Asset identity, hash or media kind is invalid.");
    (void)core::parse_iso_date(result.retrieved_on);
    if (result.ai_generated != !result.generation_record.empty())
        throw std::invalid_argument("AI origin and the generation record must agree.");
    for (const auto& url : {result.source_url,result.license_url})
        if (!url.empty() && !url.starts_with("https://") && !url.starts_with("http://"))
            throw std::invalid_argument("Source and license URLs must be ordinary HTTP or HTTPS attribution links.");
    return result;
}
Json record_json(const AssetRecord& r, Json source) {
    source.update({{"id",r.id},{"path",r.path},{"sha256",r.sha256},{"kind",r.kind},{"title",r.title},
        {"creator",r.creator},{"source_url",r.source_url},{"retrieved_on",r.retrieved_on},{"license_id",r.license_id},
        {"license_url",r.license_url},{"license_evidence_path",r.license_evidence_path},{"modifications",r.modifications},
        {"purpose",r.purpose},{"ai_generated",r.ai_generated},{"generation_record",r.ai_generated?Json(r.generation_record):Json(nullptr)},
        {"credit_text",r.credit_text},{"credit_group",r.credit_group},{"review_status",r.review_status}});
    return source;
}
std::vector<AssetRecord> parse(std::string_view bytes) {
    const auto document = detail::snapshot_parse(bytes,content::maximum_document_bytes);
    detail::snapshot_fields(document,{"format","assets"});
    if (document.at("format") != 1 || !document.at("assets").is_array() || document.at("assets").size() > 512)
        throw std::invalid_argument("Unsupported asset register format or size.");
    std::set<std::string> ids, paths;
    std::vector<AssetRecord> result;
    for (const auto& row : document.at("assets")) {
        auto parsed = record(row);
        if (!ids.insert(parsed.id).second || !paths.insert(parsed.path).second)
            throw std::invalid_argument("Each asset ID and relative file path must be unique.");
        result.push_back(std::move(parsed));
    }
    return result;
}
void media_type(const AssetRecord& r, std::string_view bytes, const fs::path& path) {
    const auto ext = path.extension().string();
    const bool matches =
        (r.kind == "image" && ((ext==".png" && bytes.starts_with("\x89PNG\r\n\x1a\n")) ||
                               ((ext==".jpeg" || ext==".jpg") && bytes.starts_with("\xff\xd8\xff")))) ||
        (r.kind == "audio" && ((ext==".ogg" && bytes.starts_with("OggS")) ||
                              (ext==".wav" && bytes.size()>=12 && bytes.starts_with("RIFF") && bytes.substr(8,4)=="WAVE"))) ||
        (r.kind == "font" && ((ext==".otf" && bytes.starts_with("OTTO")) ||
                             (ext==".ttf" && bytes.starts_with(std::string_view{"\x00\x01\x00\x00",4})))) ||
        (r.kind == "data" && ext==".json" && bytes.find_first_not_of(" \t\r\n")!=bytes.npos &&
             (bytes[bytes.find_first_not_of(" \t\r\n")]=='{' || bytes[bytes.find_first_not_of(" \t\r\n")]=='['));
    if (!matches) throw std::invalid_argument("File header or extension differs from the selected asset kind.");
}
std::string evidence(const fs::path& root, const AssetRecord& r) {
    if (r.license_evidence_path==r.path || r.license_evidence_path==manifest || r.path==manifest)
        throw std::invalid_argument("Use a separate local file for license evidence.");
    const auto terms = local_bytes(root,r.license_evidence_path,content::maximum_document_bytes);
    if (terms.empty()) throw std::invalid_argument("Record the actual license evidence in the checkout.");
    return persistence::sha256(terms);
}
} // namespace

AssetRegister::AssetRegister(const fs::path& checkout) : root_{detail::checkout_root(checkout)} {
    if (detail::checkout_schema(root_) != content::content_schema) throw std::invalid_argument("This checkout is read only.");
    marker_ = persistence::read_bounded(root_/"project-versions.json",16384);
    if (const auto found = detail::snapshot_read(root_,manifest,content::maximum_document_bytes)) {
        original_ = *found;
        records_ = parse(original_);
    }
}
std::string AssetRegister::inspect(std::string_view id) const {
    const auto found = std::find_if(records_.begin(),records_.end(),[&](const auto& row) { return row.id == id; });
    if (found == records_.end()) throw std::invalid_argument("Select an asset record.");
    const auto bytes = local_bytes(root_,found->path,asset_limit);
    media_type(*found,bytes,found->path);
    evidence(root_,*found);
    if (persistence::sha256(bytes) != found->sha256) throw std::invalid_argument("The file bytes differ from the recorded asset hash.");
    return found->credit_text + "\n" + found->license_id + " · " + found->license_evidence_path;
}
AssetSeal AssetRegister::seal(std::string_view id) const {
    const auto found = std::find_if(records_.begin(),records_.end(),[&](const auto& row) { return row.id == id; });
    if (found == records_.end()) throw std::invalid_argument("Select an asset record.");
    if (found->review_status!="reviewed" && found->review_status!="source-and-terms-verified-for-visual-display")
        throw std::invalid_argument("Record the completed provenance review before pinning this asset.");
    const auto bytes = local_bytes(root_,found->path,asset_limit);
    media_type(*found,bytes,found->path);
    if (persistence::sha256(bytes)!=found->sha256) throw std::invalid_argument("The file bytes differ from the recorded asset hash.");
    auto terms_hash=evidence(root_,*found);
    auto digest=persistence::sha256(found->source_metadata+"\n"+terms_hash);
    return {*found,std::move(digest),std::move(terms_hash)};
}
std::string AssetRegister::manifest_hash() const { return persistence::sha256(original_); }
void AssetRegister::save(AssetRecord r, const fs::path& import) {
    if (!content::valid_id(r.id)) throw std::invalid_argument("Choose a stable lowercase asset ID.");
    const auto current = detail::snapshot_read(root_,manifest,content::maximum_document_bytes);
    if (current.value_or("") != original_ || persistence::read_bounded(root_/"project-versions.json",16384) != marker_)
        throw std::invalid_argument("The register or checkout changed externally. Reopen it before saving.");
    if (fs::exists(detail::checked_path(root_,".pgp-editor/snapshot-transaction")))
        throw std::invalid_argument("Recover the pending content transaction before saving the asset register.");
    const auto existing = std::find_if(records_.begin(),records_.end(),[&](const auto& row) {return row.id==r.id;});
    std::string bytes;
    if (!import.empty()) {
        if (!import.is_absolute() || !fs::is_regular_file(fs::symlink_status(import)))
            throw std::invalid_argument("Choose an absolute local file for the import.");
        bytes = persistence::read_bounded(import,asset_limit);
        media_type(r,bytes,import);
        r.sha256 = persistence::sha256(bytes);
        r.path = "assets/imported/" + r.id + "-" + r.sha256.substr(0,16) + import.extension().string();
        if (existing == records_.end()) r.review_status = "pending";
    } else {
        bytes = local_bytes(root_,r.path,asset_limit);
        media_type(r,bytes,r.path);
        const auto digest = persistence::sha256(bytes);
        if (existing != records_.end() && (existing->path != r.path || existing->sha256 != digest))
            throw std::invalid_argument("An existing asset identity owns immutable file bytes. Register the replacement under a new ID.");
        r.sha256 = digest;
    }
    if (!import.empty() && existing != records_.end() && (existing->path != r.path || existing->sha256 != r.sha256))
        throw std::invalid_argument("Register a new asset ID when importing different bytes.");
    const auto source = existing == records_.end() ? Json::object() : detail::snapshot_parse(existing->source_metadata);
    const auto edited = record_json(r,source);
    r = record(edited);
    evidence(root_,r);
    Json rows = Json::array();
    for (const auto& row : records_) if (row.id != r.id) rows.push_back(detail::snapshot_parse(row.source_metadata));
    rows.push_back(edited);
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b) {return a.at("id")<b.at("id");});
    const auto next = Json{{"format",1},{"assets",std::move(rows)}}.dump(2)+"\n";
    auto parsed = parse(next);
    if (!import.empty()) {
        const auto destination = fs::path{r.path};
        const auto status = detail::checked_path(root_,destination);
        if (fs::exists(status)) {
            if (local_bytes(root_,destination,asset_limit) != bytes) throw std::invalid_argument("An import path already has different bytes.");
        } else persistence::atomic_write(root_/destination,bytes,
            [](std::string_view) {throw std::invalid_argument("An import path appeared during copy.");},asset_limit,false);
    }
    persistence::atomic_write(root_/manifest,next,[&](std::string_view prior) {
        if (prior != original_) throw std::invalid_argument("An external asset register edit was preserved.");
    },content::maximum_document_bytes,current.has_value());
    original_ = next;
    records_ = std::move(parsed);
}
} // namespace pgp::studio
