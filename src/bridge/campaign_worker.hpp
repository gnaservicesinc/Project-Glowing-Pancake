#pragma once

#include "campaign_snapshot.hpp"
#include <pgp/session/milestone.hpp>

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
                   QObject* parent = nullptr);
    void newCampaign(std::uint64_t token,std::string headquarters,bool stories);
    void advanceTo(std::uint64_t token,std::uint64_t revision,std::string kind);
    void setSpeed(int days_per_second);
    void start();
    void stop();
    void submitCommand(std::uint64_t token, core::Command command);
    void choose(std::uint64_t token, std::uint64_t expected_revision,
                std::uint64_t presentation_id, std::string choice_id);
    void saveTo(std::uint64_t token, std::filesystem::path path);
    void loadFrom(std::uint64_t token, std::filesystem::path path);
    void changeSettings(std::uint64_t token, persistence::Settings settings);
signals:
    void snapshotReady(pgp::bridge::CampaignSnapshotPtr snapshot);
    void operationCompleted(qulonglong token, bool accepted, QString explanation);
    // Required notices are independent of snapshot coalescing.
    void noticeRaised(QString explanation);
private:
    void requireOwningThread() const;
    void publish();
    void tick();
    void pauseDriver();
    void complete(std::uint64_t token, const session::OperationResult& result);
    session::Campaign campaign_;
    persistence::LocalPaths paths_;
    QTimer* timer_;
    QElapsedTimer last_publish_;
    std::uint64_t generation_ = 1;
    bool halted_ = false;
    std::optional<session::Milestone> milestone_;
};
}
