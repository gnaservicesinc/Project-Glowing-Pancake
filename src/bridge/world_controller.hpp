#pragma once
#include "campaign_snapshot.hpp"
#include <QObject>
#include <QVariantMap>
#include <QVariantList>

namespace pgp::bridge {
// A read-only projection of the real campaign plus locally persisted navigation.
// Places never mutate travel, inventory, staffing or simulation RNG.
class WorldController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString location READ location NOTIFY changed)
    Q_PROPERTY(QString setting READ setting NOTIFY changed)
    Q_PROPERTY(bool atHeadquarters READ atHeadquarters NOTIFY changed)
    Q_PROPERTY(QVariantMap founder READ founder NOTIFY changed)
    Q_PROPERTY(QVariantList people READ people NOTIFY changed)
    Q_PROPERTY(QVariantList applicants READ applicants NOTIFY changed)
    Q_PROPERTY(QVariantMap recruitingTerms READ recruitingTerms NOTIFY changed)
    Q_PROPERTY(QString recruitingSummary READ recruitingSummary NOTIFY changed)
    Q_PROPERTY(QVariantList branches READ branches NOTIFY changed)
    Q_PROPERTY(QVariantList countyRows READ countyRows NOTIFY changed)
    Q_PROPERTY(QString countySummary READ countySummary NOTIFY changed)
    Q_PROPERTY(QString countyState READ countyState WRITE setCountyState NOTIFY changed)
    Q_PROPERTY(QVariantList rooms READ rooms NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantList scales READ scales NOTIFY changed)
    Q_PROPERTY(QVariantMap worldGeometry READ worldGeometry CONSTANT)
    Q_PROPERTY(QString officeKind READ officeKind NOTIFY changed)
    Q_PROPERTY(QVariantMap navigation READ navigation NOTIFY navigationChanged)
    Q_PROPERTY(QString headquartersSummary READ headquartersSummary NOTIFY changed)
    Q_PROPERTY(QVariantList roomOffers READ roomOffers NOTIFY changed)
    Q_PROPERTY(QVariantList headquartersRooms READ headquartersRooms NOTIFY changed)
    Q_PROPERTY(QVariantList blueprints READ blueprints NOTIFY changed)
    Q_PROPERTY(QVariantList blueprintHardware READ blueprintHardware NOTIFY changed)
public:
    explicit WorldController(std::filesystem::path directory, QObject* parent=nullptr);
    void update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram&, bool replace);
    QString location() const;
    QString setting() const;
    bool atHeadquarters() const;
    QVariantMap founder() const;
    QVariantList people() const;
    QVariantList applicants() const;
    QVariantList branches() const;
    QString recruitingSummary() const;
    QVariantMap recruitingTerms() const;
    QVariantList countyRows() const;
    QString countySummary() const;
    QString countyState() const {return county_state_;}
    void setCountyState(QString value);
    Q_INVOKABLE void hireEmployee(QString id);
    Q_INVOKABLE void setRecruitingTerms(int payPercent,QString mission);
    QVariantList rooms() const;
    QVariantList devices() const;
    QVariantList scales() const;
    QVariantMap worldGeometry() const {return world_geometry_;}
    QString officeKind() const;
    QVariantMap navigation() const {return navigation_;}
    Q_INVOKABLE void navigate(QString place,QString scale,QString device,QString app,QString section);
    Q_INVOKABLE QString draft(QString key,QString fallback) const;
    Q_INVOKABLE void setDraft(QString key,QString value);
    QString headquartersSummary() const;
    QVariantList roomOffers() const;
    QVariantList headquartersRooms() const;
    QVariantList blueprints() const;
    QVariantList blueprintHardware() const;
    Q_INVOKABLE void orderHeadquartersRoom(QString offer,QString revision);
    Q_INVOKABLE void createBlueprint(QString name,QString hardware,QString gridKw,QString operators,bool evaporative);
    Q_INVOKABLE QVariantMap blueprintQuote(QString blueprint,QString state) const;
    Q_INVOKABLE void buildBlueprint(QVariantMap reviewed);
signals:
    void actionRequested(core::CommandAction);
    void changed();
    void navigationChanged();
    void draftsReset();
    void noticeRaised(QString);
private:
    void restore();
    CampaignSnapshotPtr snapshot_;
    const runtime::CompiledProgram* program_=nullptr;
    QString county_state_="US-CA";
    std::filesystem::path directory_,path_;
    QVariantMap navigation_{{"place","workspace"},{"scale","hq"},{"device",""},{"app",""},{"section",""}};
    QVariantMap world_geometry_;
    QVariantMap drafts_;
};
}
