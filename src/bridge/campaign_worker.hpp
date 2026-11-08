#pragma once

#include "campaign_snapshot.hpp"
#include <pgp/session/milestone.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/current_campaign.hpp>
#include <QLockFile>

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

namespace pgp::bridge {
// Move to one QThread before start. Every operation, including local IO and
// preferences, runs serially there; no UI callback changes campaign state.
class CampaignWorker final : public QObject {
    Q_OBJECT
public:
    CampaignWorker(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
                   persistence::LocalPaths paths, persistence::Settings settings,
                   QObject* parent = nullptr, bool continuous = false);
    void newCampaign(std::uint64_t token,std::string headquarters,bool stories,std::string scenario_id = {},
        std::optional<core::ObjectiveSelection> objectives = {}, runtime::ScenarioOptions options = {});
    void advanceTo(std::uint64_t token,std::uint64_t revision,std::string kind);
    void setSpeed(int days_per_second);
    void start();
    void stop();
    void retryPersistence(std::uint64_t token);
    void submitCommand(std::uint64_t token, core::Command command);
    void respondCompetitiveOffer(std::uint64_t token,std::uint64_t revision,std::string id,bool accept);
    void proposeAcquisition(std::uint64_t token,std::uint64_t revision,std::string id,bool equity);
    void signWholesale(std::uint64_t token,std::uint64_t revision,std::string plan,std::string source,std::string buyer);
    void transferSubsidiaryFunds(std::uint64_t token,std::uint64_t revision,std::string id,pgp::core::Cents amount,bool funding);
    void choose(std::uint64_t token, std::uint64_t expected_revision,
                std::uint64_t presentation_id, std::string choice_id);
    void saveTo(std::uint64_t token, std::filesystem::path path);
    void loadFrom(std::uint64_t token, std::filesystem::path path);
    void changeSettings(std::uint64_t token, persistence::Settings settings);
    void reviewOriginalContent(std::uint64_t token,std::filesystem::path source);
    void confirmOriginalContent(std::uint64_t token);
    void cancelOriginalContent();
signals:
    void snapshotReady(pgp::bridge::CampaignSnapshotPtr snapshot);
    void operationCompleted(qulonglong token, bool accepted, QString explanation);
    // Required notices are independent of snapshot coalescing.
    void noticeRaised(QString explanation);
    void originalContentReviewReady(QString review);
private:
    void requireOwningThread() const;
    void publish();
    void tick();
    void pauseDriver();
    void complete(std::uint64_t token, const session::OperationResult& result, bool advanced_day = false);
    bool persist(bool advanced_day = false);
    std::shared_ptr<const runtime::CompiledProgram> installed_,active_content_;
    session::Campaign campaign_;
    persistence::LocalPaths paths_;
    persistence::CurrentCampaign current_;
    std::unique_ptr<QLockFile> writer_lock_;
    bool continuous_ = false, active_ = false, storage_blocked_ = false;
    QTimer* timer_;
    QElapsedTimer last_publish_;
    std::uint64_t generation_ = 1;
    bool halted_ = false;
    std::optional<session::Milestone> milestone_;
    std::string missing_content_hash_;
    std::filesystem::path content_target_;
    bool content_target_current_=true;
    struct ContentReview {
        std::shared_ptr<const runtime::CompiledProgram> source;
        runtime::Checkpoint checkpoint;
        std::unique_ptr<persistence::CurrentCampaign> continuing;
        std::filesystem::path target;
        std::string fingerprint;
        bool current=false;
    };
    std::optional<ContentReview> content_review_;
};
}
