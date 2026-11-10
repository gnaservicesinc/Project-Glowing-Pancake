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
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY changed)
    Q_PROPERTY(QVariantMap personalLife READ personalLife NOTIFY changed)
    Q_PROPERTY(QVariantMap familyHome READ familyHome NOTIFY changed)
    Q_PROPERTY(QVariantMap personalEvents READ personalEvents NOTIFY changed)
    Q_PROPERTY(bool contactsAvailable READ contactsAvailable NOTIFY changed)
    Q_PROPERTY(QVariantList applicants READ applicants NOTIFY changed)
    Q_PROPERTY(QVariantMap recruitingTerms READ recruitingTerms NOTIFY changed)
    Q_PROPERTY(QString recruitingSummary READ recruitingSummary NOTIFY changed)
    Q_PROPERTY(QVariantList branches READ branches NOTIFY changed)
    Q_PROPERTY(QVariantList countyRows READ countyRows NOTIFY changed)
    Q_PROPERTY(QString countySummary READ countySummary NOTIFY changed)
    Q_PROPERTY(QString countyState READ countyState WRITE setCountyState NOTIFY changed)
    Q_PROPERTY(QVariantList rooms READ rooms NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantList deviceInventory READ deviceInventory NOTIFY changed)
    Q_PROPERTY(QVariantList deviceOffers READ deviceOffers NOTIFY changed)
    Q_PROPERTY(QString deviceSummary READ deviceSummary NOTIFY changed)
    Q_PROPERTY(bool companyArchive READ companyArchive NOTIFY changed)
    Q_PROPERTY(bool nationalAccess READ nationalAccess NOTIFY changed)
    Q_PROPERTY(bool worldAccess READ worldAccess NOTIFY changed)
    Q_PROPERTY(QString accessSummary READ accessSummary NOTIFY changed)
    Q_PROPERTY(QVariantList overseasOfficeOffers READ overseasOfficeOffers NOTIFY changed)
    Q_PROPERTY(QVariantList overseasOffices READ overseasOffices NOTIFY changed)
    Q_PROPERTY(QVariantList managerCandidates READ managerCandidates NOTIFY changed)
    Q_PROPERTY(QVariantList managementSources READ managementSources NOTIFY changed)
    Q_PROPERTY(QVariantMap operationsManagement READ operationsManagement NOTIFY changed)
    Q_PROPERTY(QVariantList scales READ scales NOTIFY changed)
    Q_PROPERTY(QVariantMap worldGeometry READ worldGeometry CONSTANT)
    Q_PROPERTY(QString officeKind READ officeKind NOTIFY changed)
    Q_PROPERTY(QVariantMap navigation READ navigation NOTIFY navigationChanged)
    Q_PROPERTY(QString reviewToken READ reviewToken NOTIFY changed)
    Q_PROPERTY(QString headquartersSummary READ headquartersSummary NOTIFY changed)
    Q_PROPERTY(QVariantMap headquarters READ headquarters NOTIFY changed)
    Q_PROPERTY(QVariantList hqEquipmentOffers READ hqEquipmentOffers NOTIFY changed)
    Q_PROPERTY(QVariantList hqMarkets READ hqMarkets NOTIFY changed)
    Q_PROPERTY(QVariantList hqProperties READ hqProperties NOTIFY changed)
    Q_PROPERTY(QVariantList hqEmployees READ hqEmployees NOTIFY changed)
    Q_PROPERTY(QVariantList roomOffers READ roomOffers NOTIFY changed)
    Q_PROPERTY(QVariantList headquartersRooms READ headquartersRooms NOTIFY changed)
    Q_PROPERTY(QVariantList blueprints READ blueprints NOTIFY changed)
    Q_PROPERTY(QVariantList blueprintHardware READ blueprintHardware NOTIFY changed)
public:
    explicit WorldController(std::filesystem::path directory, QObject* parent=nullptr);
    void update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram&, bool replace);
    QString location() const;
    Q_INVOKABLE QString referenceCity(QString state) const;
    QString setting() const;
    bool atHeadquarters() const;
    QVariantMap founder() const;
    QVariantList people() const;
    QVariantList contacts() const;
    QVariantMap personalLife() const;
    QVariantMap familyHome() const;
    QVariantMap personalEvents() const;
    Q_INVOKABLE QVariantMap personalEventQuote(QString offer,int property,QString day,QStringList guests) const;
    Q_INVOKABLE void bookPersonalEvent(QString offer,int property,QString day,QStringList guests,QString revision);
    Q_INVOKABLE void cancelPersonalEvent(int event,QString revision);
    Q_INVOKABLE QVariantMap personalFloor(int property) const;
    Q_INVOKABLE QVariantMap personalPlacement(int item,int property,int x,int y,bool rotated) const;
    Q_INVOKABLE void buyPersonalProperty(QString offer,QString revision);
    Q_INVOKABLE void sellPersonalProperty(int property,QString revision);
    Q_INVOKABLE void buyPersonalDecor(QString offer,QString revision);
    Q_INVOKABLE void placePersonalDecor(int item,int property,int x,int y,bool rotated,QString revision);
    Q_INVOKABLE void storePersonalDecor(int item,QString revision);
    Q_INVOKABLE void sellPersonalDecor(int item,QString revision);
    Q_INVOKABLE void payPersonalUpkeep(QString revision);
    bool contactsAvailable() const;
    Q_INVOKABLE QVariantList roomContacts(int room) const;
    Q_INVOKABLE QVariantMap contactThread(QString person) const;
    Q_INVOKABLE void exchangeContact(QString person,int room,QString revision);
    Q_INVOKABLE void messageContact(QString person,QString topic,int room,QString revision);
    Q_INVOKABLE QVariantMap dismissalQuote(QString person,int room) const;
    Q_INVOKABLE void dismissContact(QString person,int room,QString revision);
    Q_INVOKABLE QVariantMap contactFunding(QString person) const;
    Q_INVOKABLE QVariantMap contactFundingQuote(QString person,int discussion,QString terms) const;
    Q_INVOKABLE void requestContactFunding(QString person,QString revision);
    Q_INVOKABLE void respondContactFunding(QString person,int discussion,QString reply,QString terms,QString revision);
    QVariantMap person(QString id,core::OrdinalDay day) const;
    QVariantList decisionParticipants(QString kind,std::uint64_t presentation=0) const;
    QVariantList applicants() const;
    QVariantList branches() const;
    QString recruitingSummary() const;
    QVariantMap recruitingTerms() const;
    QVariantList countyRows() const;
    QString countySummary() const;
    QString countyState() const {return county_state_;}
    void setCountyState(QString value);
    Q_INVOKABLE void hireEmployee(QString id);
    Q_INVOKABLE QVariantMap recruitmentOffer(QString id) const;
    Q_INVOKABLE void proposeEmployment(QString id,QString salary,QString signing,QString revision);
    Q_INVOKABLE void acceptEmploymentCounter(QString id,QString revision);
    Q_INVOKABLE void declineApplicant(QString id,QString revision);
    Q_INVOKABLE void hireNegotiatedEmployee(QString id,int desk,QString revision);
    Q_INVOKABLE void setRecruitingTerms(int payPercent,QString mission);
    QVariantList rooms() const;
    QVariantList devices() const;
    QVariantList deviceInventory() const;
    QVariantList deviceOffers() const;
    QString deviceSummary() const;
    bool companyArchive() const;
    bool nationalAccess() const;
    bool worldAccess() const;
    QString accessSummary() const;
    QVariantList overseasOfficeOffers() const;
    QVariantList overseasOffices() const;
    QVariantList managerCandidates() const;
    QVariantList managementSources() const;
    QVariantMap operationsManagement() const;
    Q_INVOKABLE QVariantMap managerQuote(QString person,QString source) const;
    Q_INVOKABLE void assignOperationsManager(QVariantMap reviewed);
    Q_INVOKABLE void openOverseasOffice(QString offer,QString revision);
    Q_INVOKABLE void closeOverseasOffice(QString id,QString revision);
    Q_INVOKABLE void buyDevice(QString offer,QString revision);
    Q_INVOKABLE void serviceDevice(QString id,QString revision);
    Q_INVOKABLE void powerDevice(QString id,bool powered);
    Q_INVOKABLE void retireDevice(QString id,QString revision);
    Q_INVOKABLE bool appAvailable(QString app,QString preferred) const;
    Q_INVOKABLE bool appAvailableOn(QString app,QString device) const;
    QVariantList scales() const;
    QVariantMap worldGeometry() const {return world_geometry_;}
    QString officeKind() const;
    QVariantMap navigation() const {return navigation_;}
    QString reviewToken() const;
    Q_INVOKABLE bool navigate(QString place,QString scale,QString device,QString app,QString section);
    Q_INVOKABLE QString draft(QString key,QString fallback) const;
    Q_INVOKABLE void setDraft(QString key,QString value);
    QString headquartersSummary() const;
    QVariantMap headquarters() const;
    QVariantList hqEquipmentOffers() const;
    QVariantList hqMarkets() const;
    QVariantList hqProperties() const;
    QVariantList hqEmployees() const;
    Q_INVOKABLE QVariantList hqListings(QString market) const;
    Q_INVOKABLE QVariantMap hqPropertyQuote(QString market,QString plan,bool land) const;
    Q_INVOKABLE QVariantMap hqMoveQuote(QString construction) const;
    Q_INVOKABLE QVariantMap hqHomeQuote() const;
    Q_INVOKABLE void returnHomeHQ(QString revision);
    Q_INVOKABLE QVariantList hqBids(QString land,int width,int depth,QVariantList rooms) const;
    Q_INVOKABLE void buyHQEquipment(QString offer,QString revision);
    Q_INVOKABLE QVariantMap hqPlacement(int item,int x,int y,bool rotated,int target) const;
    Q_INVOKABLE void assignHQLaptop(int item,QString person,QString revision);
    Q_INVOKABLE void placeHQEquipment(int item,int x,int y,bool rotated,int mount,QString revision);
    Q_INVOKABLE void storeHQEquipment(int item,QString revision);
    Q_INVOKABLE void assignHQDesk(int item,QString person,QString revision);
    Q_INVOKABLE void powerHQEquipment(int item,bool powered,QString revision);
    Q_INVOKABLE void leaseHQ(QString market,QString plan,QString revision);
    Q_INVOKABLE void buyHQLand(QString market,QString plan,QString revision);
    Q_INVOKABLE void buildHQ(QString land,QString contractor,int width,int depth,QVariantList rooms,QString revision);
    Q_INVOKABLE void moveToOwnedHQ(QString construction,QString revision);
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
    void personalAction(core::PersonalLifeAction,QString revision);
    bool hqReview(QString revision);
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
