#pragma once

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/settings.hpp>

#include <functional>
#include <map>

namespace pgp::session {
struct StorageNotice {
    std::string code;
    std::string explanation;
};
struct OperationResult {
    runtime::RuntimeResult command;
    std::optional<StorageNotice> storage_notice;
    // A driver stops its timer on a content/storage failure. It must retain
    // the notice independently of coalesced presentation snapshots.
    bool halt_requested = false;
    std::optional<std::string> pause_notice{};
};
struct LoadResult {
    bool loaded;
    std::optional<StorageNotice> notice;
};
// Borrows presentation records from the given immutable snapshot. Required
// dialogue and decision results remain individual rows.
using WeeklyNews = std::map<core::OrdinalDay, std::vector<const runtime::Presentation*>>;
[[nodiscard]] WeeklyNews group_weekly_news(const runtime::Checkpoint& checkpoint);
WeeklyNews group_weekly_news(runtime::Checkpoint&& checkpoint) = delete;

// The application-facing owner combines the pure runtime with local persistence
// policy. Its explicitly selected directory may be portable or OS-standard;
// construction performs no filesystem writes.
class Campaign {
public:
    Campaign(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
             std::filesystem::path autosave_directory, persistence::Settings settings = {});
    [[nodiscard]] const runtime::Checkpoint& checkpoint() const noexcept { return engine_.checkpoint(); }
    [[nodiscard]] const runtime::CompiledProgram& program() const noexcept { return engine_.program(); }
    [[nodiscard]] const persistence::Settings& settings() const noexcept { return settings_; }
    void set_settings(persistence::Settings settings);
    [[nodiscard]] OperationResult prepare_opening(std::uint64_t expected_revision);
    [[nodiscard]] OperationResult submit(const core::Command& command);
    [[nodiscard]] OperationResult respond_competitive_offer(std::uint64_t expected_revision,std::string_view offer_id,bool accept);
    [[nodiscard]] OperationResult propose_acquisition(std::uint64_t expected_revision,std::string_view target,bool equity);
    [[nodiscard]] OperationResult sign_wholesale(std::uint64_t expected_revision,std::string_view plan,std::string_view source,std::string_view buyer);
    [[nodiscard]] OperationResult pursue_romance(std::uint64_t expected_revision,const runtime::RomanceRequest&);
    [[nodiscard]] OperationResult set_romantic_preferences(std::uint64_t expected_revision,bool interested,const std::vector<std::string>& genders);
    [[nodiscard]] OperationResult collaborate_research(std::uint64_t expected_revision,const runtime::CollaborationRequest&);
    [[nodiscard]] OperationResult publish_collaboration(std::uint64_t expected_revision,std::string_view id);
    [[nodiscard]] OperationResult transfer_subsidiary_funds(std::uint64_t expected_revision,std::string_view target,core::Cents amount,bool funding);
    [[nodiscard]] OperationResult choose(std::uint64_t expected_revision, std::uint64_t presentation_id,
                                        std::string_view choice_id);
    void save(const std::filesystem::path& path) const;
    [[nodiscard]] LoadResult load(const std::filesystem::path& path);
private:
    OperationResult operate(const std::function<runtime::RuntimeResult(runtime::Runtime&)>& operation,
                            bool before_commit);
    runtime::Runtime engine_;
    persistence::AutosaveRing autosaves_;
    persistence::Settings settings_;
};
} // namespace pgp::session
