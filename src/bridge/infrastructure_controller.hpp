#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QUrl>
#include <optional>
namespace pgp::bridge {
class InfrastructureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(StableListModel* states READ states CONSTANT)
    Q_PROPERTY(StableListModel* sites READ sites CONSTANT)
    Q_PROPERTY(StableListModel* hardware READ hardware CONSTANT)
    Q_PROPERTY(StableListModel* calendar READ calendar CONSTANT)
    Q_PROPERTY(StableListModel* notices READ notices CONSTANT)
    Q_PROPERTY(QVariantList rackOptions READ rackOptions NOTIFY catalogChanged)
    Q_PROPERTY(QVariantMap mapGeometry READ mapGeometry NOTIFY catalogChanged)
    Q_PROPERTY(QString mapCredit READ mapCredit NOTIFY catalogChanged)
    Q_PROPERTY(QString mapTerms READ mapTerms NOTIFY catalogChanged)
    Q_PROPERTY(QUrl mapSource READ mapSource NOTIFY catalogChanged)
    Q_PROPERTY(QString assumptions READ assumptions NOTIFY catalogChanged)
    Q_PROPERTY(QString executiveLocation READ executiveLocation NOTIFY changed)
    Q_PROPERTY(QString leaseStatus READ leaseStatus NOTIFY changed)
    Q_PROPERTY(bool canNoticeLease READ canNoticeLease NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY reviewChanged)
    Q_PROPERTY(bool canConfirm READ canConfirm NOTIFY reviewChanged)
public:
    explicit InfrastructureController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr,bool replace);
    [[nodiscard]] bool available() const {return snapshot_ && state().infrastructure.has_value();}
    StableListModel* states() const {return states_;}
    StableListModel* sites() const {return sites_;}
    StableListModel* hardware() const {return hardware_;}
    StableListModel* calendar() const {return calendar_;}
    StableListModel* notices() const {return notices_;}
    QVariantList rackOptions() const {return rack_options_;}
    QVariantMap mapGeometry() const {return map_;}
    QString mapCredit() const;
    QString mapTerms() const {return map_terms_;}
    QUrl mapSource() const;
    QString assumptions() const;
    QString executiveLocation() const;
    QString leaseStatus() const;
    bool canNoticeLease() const;
    QString reviewText() const {return review_text_;}
    bool canConfirm() const {return review_.has_value();}
    Q_INVOKABLE QString suggestedVisit(QString site,QString mode) const;
    Q_INVOKABLE void reviewBuild(QString state,QString name,QString kilowatts,QString cooling);
    Q_INVOKABLE void reviewHardware(QString site,QString family);
    Q_INVOKABLE void reviewHire(QString site,QString people);
    Q_INVOKABLE void reviewPower(QString site,QString kilowatts);
    Q_INVOKABLE void reviewAppointment(QString site,QString date,QString mode);
    Q_INVOKABLE void reviewRetire(QString hardware);
    Q_INVOKABLE void reviewClose(QString site);
    Q_INVOKABLE void reviewCancel(QString appointment);
    Q_INVOKABLE void reviewLease();
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void dismissReview();
signals:
    void changed();
    void catalogChanged();
    void reviewChanged();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    const core::SimulationState& state() const {return snapshot_->checkpoint.world.simulation;}
    template<class Make> void review(Make&&);
    void loadMap();
    StableListModel *states_,*sites_,*hardware_,*calendar_,*notices_;
    CampaignSnapshotPtr snapshot_;
    QVariantList rack_options_;
    QVariantMap map_;
    QString map_terms_,review_text_;
    std::optional<core::Command> review_;
};
}
