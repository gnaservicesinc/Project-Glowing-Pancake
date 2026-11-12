#include "campaign_controller.hpp"

namespace pgp::bridge {
QVariantMap CampaignController::playtestFounder() const {
    if(!snapshot_ || welcome_ || !snapshot_->checkpoint.world.developer_playtest ||
       !snapshot_->checkpoint.world.founding) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    if(!core::social_rules(state) || !core::talent_rules(state)) return {};
    const auto& people=*state.management->people;
    QStringList traits;
    for(const auto& trait:people.social->founder_traits) traits.push_back(QString::fromStdString(trait));
    return {{"traits",traits},{"genius",people.talent->founder_genius}};
}

void CampaignController::editPlaytestFounder(const QStringList& traits,bool genius) {
    if(playtestFounder().isEmpty() || !companyEditable() || !beginOperation()) return;
    const auto revision=snapshot_->checkpoint.world.simulation.revision;
    std::vector<std::string> values;
    for(const auto& trait:traits) values.push_back(trait.toStdString());
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,values=std::move(values),genius]() mutable {
        worker->editPlaytestFounder(token,revision,std::move(values),genius);
    },Qt::QueuedConnection);
}

void CampaignWorker::editPlaytestFounder(std::uint64_t token,std::uint64_t revision,
                                         std::vector<std::string> traits,bool genius) {
    requireOwningThread();
    if(!active_ || !writer_lock_ || !writer_lock_->isLocked() || storage_blocked_) {
        emit operationCompleted(static_cast<qulonglong>(token),false,tr("The playtest save is unavailable."));
        return;
    }
    pauseDriver();
    complete(token,campaign_.editPlaytestFounder(revision,std::move(traits),genius));
}
}
