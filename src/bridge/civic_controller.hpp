#pragma once

#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>

namespace pgp::bridge {
class CivicController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(StableListModel* dockets READ dockets CONSTANT)
    Q_PROPERTY(QVariantList jurisdictions READ jurisdictions NOTIFY catalogChanged)
    Q_PROPERTY(QString jurisdiction READ jurisdiction WRITE setJurisdiction NOTIFY changed)
    Q_PROPERTY(QString institutions READ institutions NOTIFY changed)
    Q_PROPERTY(QString calendar READ calendar NOTIFY changed)
    Q_PROPERTY(QString assumptions READ assumptions NOTIFY catalogChanged)
    Q_PROPERTY(bool hasUpcoming READ hasUpcoming NOTIFY changed)
    Q_PROPERTY(QString advocacy READ advocacy NOTIFY changed)
    Q_PROPERTY(bool canAdvocate READ canAdvocate NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
    Q_PROPERTY(QVariantList evidence READ evidence NOTIFY changed)
public:
    explicit CivicController(QObject* parent = nullptr);
    void update(CampaignSnapshotPtr snapshot, bool replace);
    [[nodiscard]] bool available() const;
    [[nodiscard]] StableListModel* dockets() { return &dockets_; }
    [[nodiscard]] QVariantList jurisdictions() const { return jurisdictions_; }
    [[nodiscard]] QString jurisdiction() const { return jurisdiction_; }
    [[nodiscard]] QString institutions() const;
    [[nodiscard]] QString calendar() const;
    [[nodiscard]] QString assumptions() const;
    [[nodiscard]] bool hasUpcoming() const;
    [[nodiscard]] QString advocacy() const;
    [[nodiscard]] bool canAdvocate() const;
    [[nodiscard]] QString reviewText() const { return review_text_; }
    [[nodiscard]] QVariantList evidence() const;
    void setJurisdiction(const QString& jurisdiction);
    Q_INVOKABLE void reviewAdvocacy(bool support);
    Q_INVOKABLE void confirmAdvocacy();
    Q_INVOKABLE void clearReview();
    Q_INVOKABLE void discloseEvidence(const QString& id, bool support);
signals:
    void changed();
    void catalogChanged();
    void actionRequested(core::CommandAction action);
    void noticeRaised(QString message);
private:
    const core::SimulationState& state() const;
    const core::CivicDocket* selected() const;
    void update_rows(bool replace);
    [[nodiscard]] bool editable() const;
    std::optional<core::CivicAdvocacyTerms> terms() const;
    CampaignSnapshotPtr snapshot_;
    StableListModel dockets_;
    QVariantList jurisdictions_;
    QString jurisdiction_ = QStringLiteral("all"), review_text_;
    std::optional<core::FundPolicyAdvocacy> pending_;
    std::uint64_t reviewed_revision_ = 0;
};
} // namespace pgp::bridge
