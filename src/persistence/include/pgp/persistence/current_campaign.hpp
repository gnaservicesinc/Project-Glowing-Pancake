#pragma once
#include <pgp/persistence/campaign_content.hpp>
#include <optional>
#include <vector>

namespace pgp::persistence {
// One local continuing campaign. Callers provide exclusive writer ownership.
// Ordinary actions checkpoint; up to 31 deterministic days use a small journal.
// Construction is read-only; failures preserve all existing files.
class CurrentCampaign {
public:
    explicit CurrentCampaign(std::filesystem::path directory);
    [[nodiscard]] std::optional<runtime::Checkpoint> resume(const runtime::CompiledProgram&);
    void commit(const runtime::CompiledProgram&, const runtime::Checkpoint&, bool advanced_day = false);
    void preserve_for_recovery();
    void archive(const runtime::CompiledProgram&, const runtime::Checkpoint&);
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
    CampaignContent& content() {return content_;}
    [[nodiscard]] std::string fingerprint() const;
private:
    std::filesystem::path path_, journal_;
    CampaignContent content_;
    std::uint64_t generation_ = 0;
    bool content_ready_=false;
    std::vector<std::uint64_t> days_;
};
}
