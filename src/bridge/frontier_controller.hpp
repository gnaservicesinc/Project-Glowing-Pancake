#pragma once

#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>

namespace pgp::bridge {
class FrontierController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(QString overview READ overview NOTIFY changed)
    Q_PROPERTY(QString requirements READ requirements CONSTANT)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
    Q_PROPERTY(StableListModel* systems READ systems CONSTANT)
    Q_PROPERTY(StableListModel* technology READ technology CONSTANT)
    Q_PROPERTY(StableListModel* researchPrograms READ researchPrograms CONSTANT)
    Q_PROPERTY(StableListModel* refreshPrograms READ refreshPrograms CONSTANT)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantList refreshServices READ refreshServices NOTIFY changed)
    Q_PROPERTY(QVariantList protocols READ protocols NOTIFY changed)
    Q_PROPERTY(QVariantList researchTopics READ researchTopics NOTIFY changed)
    Q_PROPERTY(QVariantList repeatableTopics READ repeatableTopics NOTIFY changed)
    Q_PROPERTY(QVariantList licensedData READ licensedData NOTIFY changed)
public:
    explicit FrontierController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr,bool replace);
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool editable() const;
    [[nodiscard]] QString overview() const;
    [[nodiscard]] QString requirements() const;
    [[nodiscard]] QString reviewText() const {return review_text_;}
    StableListModel* systems() {return &systems_;}
    StableListModel* technology() {return &technology_;}
    StableListModel* researchPrograms() {return &research_;}
    StableListModel* refreshPrograms() {return &refresh_;}
    [[nodiscard]] QVariantList services() const;
    [[nodiscard]] QVariantList refreshServices() const;
    [[nodiscard]] QVariantList protocols() const;
    [[nodiscard]] QVariantList researchTopics() const;
    [[nodiscard]] QVariantList repeatableTopics() const;
    [[nodiscard]] QVariantList licensedData() const;
    Q_INVOKABLE void reviewSystem(QString name,QStringList members);
    Q_INVOKABLE void reviewTrial(QString protocol,QString source,QString rate,QString staff);
    Q_INVOKABLE void reviewResearch(QStringList topics,bool standing,QVariantMap limits);
    Q_INVOKABLE void reviewRefresh(QString service,QString recipe,QString evaluation,QStringList data,QString work,QString gain,QVariantMap limits);
    Q_INVOKABLE void reviewCancellation(bool refresh);
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void clearReview();
signals:
    void changed();
    void reviewReady();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    [[nodiscard]] const core::SimulationState& company() const {return snapshot_->checkpoint.world.simulation;}
    [[nodiscard]] const core::FrontierState& future() const {return *company().frontier;}
    void updateRows(bool replace);
    void stage(core::CommandAction,QString);
    CampaignSnapshotPtr snapshot_;
    StableListModel systems_,technology_,research_,refresh_;
    std::optional<core::CommandAction> pending_;
    std::uint64_t reviewed_revision_=0;
    QString review_text_;
};
}
