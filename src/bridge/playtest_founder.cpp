#include "campaign_controller.hpp"

namespace pgp::bridge {
QVariantList CampaignController::playtestPeople() const {
    if(!snapshot_ || welcome_ || !snapshot_->checkpoint.world.developer_playtest) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    if(!state.management || !state.management->people) return {};
    QVariantList result;
    const auto add=[&](const core::PersonProfile& person) {
        QStringList traits;
        for(const auto& trait:person.traits) traits.push_back(QString::fromStdString(trait));
        result.push_back(QVariantMap{{"id",QString::fromStdString(person.id)},
            {"label",QString::fromStdString(person.name+" · "+person.role)},{"traits",traits},
            {"experience",person.experience},{"research",person.research},{"engineering",person.engineering},
            {"evaluation",person.evaluation},{"missionFit",person.mission_fit},
            {"maxExperience",core::from_ordinal(person.applied_on).year-person.birth_year-18}});
    };
    const auto& people=*state.management->people;
    for(const auto& employee:people.employees) add(employee.terms.profile);
    for(const auto& applicant:people.applicants) add(applicant.profile);
    for(const auto& applicant:people.queued_applicants) add(applicant.profile);
    for(const auto& [id,person]:people.known_people) {
        (void)id;
        add(person);
    }
    return result;
}

void CampaignController::editPlaytestPerson(QString person,const QStringList& traits,int experience,int research,int engineering,int evaluation,int missionFit) {
    if(!snapshot_ || !snapshot_->checkpoint.world.developer_playtest || !companyEditable() || !beginOperation()) return;
    const auto revision=snapshot_->checkpoint.world.simulation.revision;
    std::vector<std::string> values;
    for(const auto& trait:traits) values.push_back(trait.toStdString());
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,person=person.toStdString(),values=std::move(values),
                                     stats=std::array{experience,research,engineering,evaluation,missionFit}]() mutable {
        worker->editPlaytestPerson(token,revision,std::move(person),std::move(values),stats);
    },Qt::QueuedConnection);
}

void CampaignWorker::editPlaytestPerson(std::uint64_t token,std::uint64_t revision,std::string person,
                                       std::vector<std::string> traits,std::array<int,5> stats) {
    requireOwningThread();
    if(!active_ || !writer_lock_ || !writer_lock_->isLocked() || storage_blocked_) {
        emit operationCompleted(static_cast<qulonglong>(token),false,tr("The playtest save is unavailable."));
        return;
    }
    pauseDriver();
    complete(token,campaign_.editPlaytestPerson(revision,person,traits,stats));
}

void CampaignController::usePlaytestTool(QString tool,QString topic) {
    if(!snapshot_ || !snapshot_->checkpoint.world.developer_playtest || !companyEditable() || !beginOperation()) return;
    const auto revision=snapshot_->checkpoint.world.simulation.revision;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,tool=tool.toStdString(),topic=topic.toStdString()]() mutable {
        worker->usePlaytestTool(token,revision,std::move(tool),std::move(topic));
    },Qt::QueuedConnection);
}

void CampaignWorker::usePlaytestTool(std::uint64_t token,std::uint64_t revision,std::string tool,std::string topic) {
    requireOwningThread();
    if(!active_ || !writer_lock_ || !writer_lock_->isLocked() || storage_blocked_) {
        emit operationCompleted(static_cast<qulonglong>(token),false,tr("The playtest save is unavailable."));
        return;
    }
    pauseDriver();
    complete(token,campaign_.usePlaytestTool(revision,tool,topic));
}

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
