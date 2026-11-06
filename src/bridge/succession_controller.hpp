#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"

namespace pgp::bridge {
class SuccessionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(bool founderExecutive READ founderExecutive NOTIFY changed)
    Q_PROPERTY(bool founderChair READ founderChair NOTIFY changed)
    Q_PROPERTY(QString overview READ overview NOTIFY changed)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY changed)
    Q_PROPERTY(QString recommendedDate READ recommendedDate NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
    Q_PROPERTY(StableListModel* plans READ plans CONSTANT)
    Q_PROPERTY(StableListModel* terms READ terms CONSTANT)
public:
    explicit SuccessionController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr,bool replace);
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool editable() const;
    [[nodiscard]] bool founderExecutive() const;
    [[nodiscard]] bool founderChair() const;
    [[nodiscard]] QString overview() const;
    [[nodiscard]] QString noticeText() const;
    [[nodiscard]] QString recommendedDate() const;
    [[nodiscard]] QString reviewText() const {return review_text_;}
    StableListModel* plans() {return &plans_;}
    StableListModel* terms() {return &terms_;}
    Q_INVOKABLE QVariantList candidatesAt(QString day) const;
    Q_INVOKABLE void reviewNomination(QString person,QString day,bool chair,QString source,QString rate,QString staff);
    Q_INVOKABLE void reviewVote(bool support);
    Q_INVOKABLE void reviewCancellation();
    Q_INVOKABLE void reviewRetirement();
    Q_INVOKABLE void acknowledge();
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void clearReview();
signals:
    void changed();
    void reviewReady();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    [[nodiscard]] const core::SimulationState& company() const {return snapshot_->checkpoint.world.simulation;}
    [[nodiscard]] const core::SuccessionState& actor() const {return *company().succession;}
    void stage(core::CommandAction,QString);
    CampaignSnapshotPtr snapshot_;
    StableListModel plans_,terms_;
    std::optional<core::CommandAction> pending_;
    std::uint64_t reviewed_revision_=0;
    QString review_text_;
};
}
