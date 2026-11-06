#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>
#include <functional>

namespace pgp::bridge {
class InstitutionsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(bool hasUpcoming READ hasUpcoming NOTIFY changed)
    Q_PROPERTY(QString coverage READ coverage NOTIFY changed)
    Q_PROPERTY(QString identity READ identity NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
    Q_PROPERTY(StableListModel* regions READ regions CONSTANT)
    Q_PROPERTY(StableListModel* mandates READ mandates CONSTANT)
    Q_PROPERTY(StableListModel* promises READ promises CONSTANT)
    Q_PROPERTY(StableListModel* cases READ cases CONSTANT)
    Q_PROPERTY(QVariantList roles READ roles NOTIFY changed)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantList reports READ reports NOTIFY changed)
    Q_PROPERTY(QVariantList research READ research NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
public:
    explicit InstitutionsController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr snapshot,bool replace);
    bool available() const;
    bool editable() const;
    bool hasUpcoming() const;
    QString coverage() const;
    QString identity() const;
    QString reviewText() const {return review_text_;}
    StableListModel* regions() {return &regions_;}
    StableListModel* mandates() {return &mandates_;}
    StableListModel* promises() {return &promises_;}
    StableListModel* cases() {return &cases_;}
    QVariantList roles() const;
    QVariantList services() const;
    QVariantList reports() const;
    QVariantList research() const;
    QVariantList sources() const;
    QString requiredCaseText() const;
    Q_INVOKABLE void selectRequiredCase();
    Q_INVOKABLE void reviewMandate(QString role,QString service,QString source);
    Q_INVOKABLE void reviewTestimony(QString report,bool forCase);
    Q_INVOKABLE void reviewPromise(QString subject,bool publication);
    Q_INVOKABLE void reviewNegotiation(QString source);
    Q_INVOKABLE void reviewProcurement();
    Q_INVOKABLE void reviewRelinquishment();
    Q_INVOKABLE void acknowledgeCase();
    Q_INVOKABLE void confirmAction();
    Q_INVOKABLE void clearReview();
signals:
    void changed();
    void reviewReady();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    const runtime::WorldState& world() const {return snapshot_->checkpoint.world;}
    const core::SimulationState& company() const {return world().simulation;}
    const core::AuthorityState& authority() const {return *world().authority;}
    const core::PublicCase* selectedPublicCase() const;
    const core::AuthorityMandate* selectedMandate() const;
    void updateRows(bool replace);
    void review(core::CommandAction,const std::function<QString(const runtime::WorldState&)>&);
    CampaignSnapshotPtr snapshot_;
    StableListModel regions_,mandates_,promises_,cases_;
    std::optional<core::CommandAction> pending_;
    std::uint64_t reviewed_revision_=0;
    QString review_text_;
};
}
