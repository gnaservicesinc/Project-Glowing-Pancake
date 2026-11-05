#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::studio {
struct AssetRecord {
    std::string id, path, sha256, kind, title, creator, source_url, retrieved_on;
    std::string license_id, license_url, license_evidence_path, modifications, purpose;
    bool ai_generated = false;
    std::string generation_record, credit_text, credit_group, review_status;
    // Existing non-editable source metadata is retained when a record is saved.
    std::string source_metadata;
};
struct AssetSeal {
    AssetRecord record;
    std::string fingerprint, evidence_hash;
};
class AssetRegister {
public:
    explicit AssetRegister(const std::filesystem::path& checkout);
    [[nodiscard]] const std::vector<AssetRecord>& records() const { return records_; }
    [[nodiscard]] std::string inspect(std::string_view id) const;
    [[nodiscard]] AssetSeal seal(std::string_view id) const;
    [[nodiscard]] std::string manifest_hash() const;
    // An empty import uses an existing ordinary file under assets. A local
    // import first creates an immutable content-named copy in that checkout.
    void save(AssetRecord record, const std::filesystem::path& import = {});
private:
    std::filesystem::path root_;
    std::string original_, marker_;
    std::vector<AssetRecord> records_;
};
} // namespace pgp::studio
