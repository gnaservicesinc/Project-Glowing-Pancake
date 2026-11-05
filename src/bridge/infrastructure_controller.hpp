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
    Q_PROPERTY(StableListModel* wholesaleContracts READ wholesaleContracts CONSTANT)
    Q_PROPERTY(QVariantList wholesalePlans READ wholesalePlans NOTIFY changed)
    Q_PROPERTY(QVariantList wholesaleSources READ wholesaleSources NOTIFY changed)
    Q_PROPERTY(QVariantList wholesaleBuyers READ wholesaleBuyers NOTIFY changed)
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
    void update(CampaignSnapshotPtr,const runtime::CompiledProgram&,bool replace);
    [[nodiscard]] bool available() const {return snapshot_ && state().infrastructure.has_value();}
    StableListModel* states() const {return states_;}
    StableListModel* sites() const {return sites_;}
    StableListModel* hardware() const {return hardware_;}
    StableListModel* calendar() const {return calendar_;}
    StableListModel* notices() const {return notices_;}
    StableListModel* wholesaleContracts() const {return wholesale_contracts_;}
    QVariantList wholesalePlans() const {return wholesale_plans_;}
    QVariantList wholesaleSources() const {return wholesale_sources_;}
    QVariantList wholesaleBuyers() const {return wholesale_buyers_;}
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
    Q_INVOKABLE void reviewWholesale(QString plan,QString source,QString buyer);
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void dismissReview();
signals:
    void changed();
    void catalogChanged();
    void reviewChanged();
    void actionRequested(core::CommandAction);
    void wholesaleRequested(QString plan,QString source,QString buyer);
    void noticeRaised(QString);
private:
    const core::SimulationState& state() const {return snapshot_->checkpoint.world.simulation;}
    template<class Make> void review(Make&&);
    void loadMap();
    struct WholesaleReview {std::uint64_t expected_revision;QString plan,source,buyer;};
    StableListModel *states_,*sites_,*hardware_,*calendar_,*notices_,*wholesale_contracts_;
    CampaignSnapshotPtr snapshot_;
    const runtime::CompiledProgram* program_ = nullptr;
    QVariantList rack_options_;
    QVariantList wholesale_plans_,wholesale_sources_,wholesale_buyers_;
    QVariantMap map_;
    QString map_terms_,review_text_;
    std::optional<std::variant<core::Command,WholesaleReview>> review_;
};
}
