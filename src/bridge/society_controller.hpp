#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>

namespace pgp::bridge {
class SocietyController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(StableListModel* regions READ regions CONSTANT)
    Q_PROPERTY(QString assumptions READ assumptions NOTIFY changed)
    Q_PROPERTY(QString global READ global NOTIFY changed)
    Q_PROPERTY(QString forum READ forum NOTIFY changed)
    Q_PROPERTY(QString audiences READ audiences NOTIFY changed)
    Q_PROPERTY(QString media READ media NOTIFY changed)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantList evidence READ evidence NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
    Q_PROPERTY(bool hasUpcoming READ hasUpcoming NOTIFY changed)
public:
    explicit SocietyController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr,bool replace);
    bool available() const;
    StableListModel* regions() {return &regions_;}
    QString assumptions() const;
    QString global() const;
    QString forum() const;
    QString audiences() const;
    QString media() const;
    QVariantList services() const;
    QVariantList evidence() const;
    QString reviewText() const {return review_text_;}
    bool hasUpcoming() const;
    Q_INVOKABLE void reviewMedia(const QString& service,int audience);
    Q_INVOKABLE void reviewEndorsement(const QString& evidence,bool support);
    Q_INVOKABLE void confirmAction();
    Q_INVOKABLE void clearReview();
signals:
    void changed();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    const runtime::WorldState& world() const {return snapshot_->checkpoint.world;}
    const core::SocietyState& society() const {return *world().society;}
    bool editable() const;
    void updateRows(bool replace);
    CampaignSnapshotPtr snapshot_;
    StableListModel regions_;
    QString review_text_;
    std::optional<core::CommandAction> pending_;
    std::uint64_t reviewed_revision_=0;
};
}
