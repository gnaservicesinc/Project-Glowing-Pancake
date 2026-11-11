#pragma once

#include "campaign_snapshot.hpp"
#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace pgp::bridge {
[[nodiscard]] core::ObjectiveSelection parseObjectiveSelection(const QString&,const QStringList&);
[[nodiscard]] QVariantList objectiveOptions(bool asi_available);

class ObjectivesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool retrospective READ retrospective NOTIFY changed)
    Q_PROPERTY(QVariantMap report READ report NOTIFY changed)
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(bool ended READ ended NOTIFY changed)
    Q_PROPERTY(bool observer READ observer NOTIFY changed)
    Q_PROPERTY(bool canContinue READ canContinue NOTIFY changed)
    Q_PROPERTY(bool canEndEarly READ canEndEarly NOTIFY changed)
    Q_PROPERTY(bool nextCheck READ nextCheck NOTIFY changed)
    Q_PROPERTY(QString primary READ primary NOTIFY changed)
    Q_PROPERTY(QStringList secondary READ secondary NOTIFY changed)
    Q_PROPERTY(QVariantList options READ options NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QString overview READ overview NOTIFY changed)
    Q_PROPERTY(QString breachText READ breachText NOTIFY changed)
    Q_PROPERTY(QString postmortem READ postmortem NOTIFY changed)
    Q_PROPERTY(QString reviewText READ reviewText NOTIFY changed)
public:
    explicit ObjectivesController(QObject* parent=nullptr):QObject(parent) {}
    void update(CampaignSnapshotPtr,const runtime::CompiledProgram&,bool replace);
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool retrospective() const;
    [[nodiscard]] QVariantMap report() const;
    [[nodiscard]] bool editable() const;
    [[nodiscard]] bool ended() const;
    [[nodiscard]] bool observer() const;
    [[nodiscard]] bool canContinue() const;
    [[nodiscard]] bool canEndEarly() const;
    [[nodiscard]] bool nextCheck() const;
    [[nodiscard]] QString primary() const;
    [[nodiscard]] QStringList secondary() const;
    [[nodiscard]] QVariantList options() const;
    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QString overview() const;
    [[nodiscard]] QString breachText() const;
    [[nodiscard]] QString postmortem() const;
    [[nodiscard]] QString reviewText() const {return review_text_;}
    Q_INVOKABLE void acknowledge();
    Q_INVOKABLE void reviewSelection(QString primary,QStringList secondary);
    Q_INVOKABLE void reviewEnd();
    Q_INVOKABLE void reviewContinue();
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void clearReview();
signals:
    void changed();
    void actionRequested(core::CommandAction);
    void noticeRaised(QString);
private:
    [[nodiscard]] const runtime::WorldState& world() const {return snapshot_->checkpoint.world;}
    [[nodiscard]] const runtime::ObjectivesState& objectives() const {return *world().objectives;}
    void stage(core::CommandAction,QString);
    CampaignSnapshotPtr snapshot_;
    const runtime::CompiledProgram* program_ = nullptr;
    std::optional<core::CommandAction> pending_;
    std::uint64_t reviewed_revision_ = 0;
    QString review_text_;
};
}
