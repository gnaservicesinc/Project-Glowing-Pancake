#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>

namespace pgp::bridge {
// Player-facing adapters. Only measured lab evidence and public rival notices
// enter these models; commands return to the campaign's serialized worker.
class ManagementController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(StableListModel* dataOffers READ dataOffers CONSTANT)
    Q_PROPERTY(StableListModel* researchTopics READ researchTopics CONSTANT)
    Q_PROPERTY(StableListModel* models READ models CONSTANT)
    Q_PROPERTY(StableListModel* evidence READ evidence CONSTANT)
    Q_PROPERTY(StableListModel* offers READ offers CONSTANT)
    Q_PROPERTY(StableListModel* products READ products CONSTANT)
    Q_PROPERTY(StableListModel* contracts READ contracts CONSTANT)
    Q_PROPERTY(StableListModel* rivals READ rivals CONSTANT)
    Q_PROPERTY(StableListModel* payables READ payables CONSTANT)
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY changed)
    Q_PROPERTY(QVariantList evaluations READ evaluations NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantMap recommended READ recommended NOTIFY changed)
    Q_PROPERTY(QString briefing READ briefing NOTIFY changed)
    Q_PROPERTY(QString guide READ guide NOTIFY changed)
    Q_PROPERTY(QString ending READ ending NOTIFY changed)
public:
    explicit ManagementController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace);
    bool available() const;
    StableListModel* dataOffers() const { return data_; }
    StableListModel* researchTopics() const { return research_; }
    StableListModel* models() const { return models_; }
    StableListModel* evidence() const { return evidence_; }
    StableListModel* offers() const { return offers_; }
    StableListModel* products() const { return products_; }
    StableListModel* contracts() const { return contracts_; }
    StableListModel* rivals() const { return rivals_; }
    StableListModel* payables() const { return payables_; }
    QVariantList recipes() const;
    QVariantList evaluations() const;
    QVariantList sources() const;
    QVariantList services() const;
    QVariantMap recommended() const;
    QString briefing() const;
    QString guide() const;
    QString ending() const;
    Q_INVOKABLE QVariantMap trainingPreview(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent={}) const;
    Q_INVOKABLE QString servicePreview(QString service,QString model,bool review) const;
    Q_INVOKABLE void buyData(QString id);
    Q_INVOKABLE void research(QString id,QString source,QString rate,QString staff);
    Q_INVOKABLE void train(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent={});
    Q_INVOKABLE void clone(QString id);
    Q_INVOKABLE void evaluate(QString model,QString plan,QString source,QString rate,QString staff);
    Q_INVOKABLE void deploy(QString service,QString model,QString evidence,QString source,bool review);
    Q_INVOKABLE void acceptOffer(QString id);
    Q_INVOKABLE void cancelContract(QString id);
    Q_INVOKABLE void payInvoice(QString id);
    Q_INVOKABLE void repayDebt(QString id);
signals:
    void changed();
    void actionRequested(pgp::core::CommandAction action);
    void noticeRaised(QString text);
private:
    bool editable();
    const core::SimulationState& state() const;
    QString nextId(QString prefix) const;
    CampaignSnapshotPtr snapshot_;
    StableListModel *data_,*research_,*models_,*evidence_,*offers_,*products_,*contracts_,*rivals_,*payables_;
};
}
