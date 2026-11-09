#pragma once
#include "campaign_model.hpp"
#include "campaign_snapshot.hpp"
#include <QVariantList>

namespace pgp::bridge {
// Player-facing evidence, public notices and explicitly requested due diligence.
// Commands return to the campaign's serialized worker.
class ManagementController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(StableListModel* dataOffers READ dataOffers CONSTANT)
    Q_PROPERTY(bool groundedResearch READ groundedResearch NOTIFY changed)
    Q_PROPERTY(QVariantList researchGraph READ researchGraph NOTIFY changed)
    Q_PROPERTY(QVariantList researchWorkers READ researchWorkers NOTIFY changed)
    Q_PROPERTY(QVariantList researchSources READ researchSources NOTIFY changed)
    Q_PROPERTY(QVariantList researchRentals READ researchRentals NOTIFY changed)
    Q_PROPERTY(StableListModel* researchTopics READ researchTopics CONSTANT)
    Q_PROPERTY(StableListModel* models READ models CONSTANT)
    Q_PROPERTY(StableListModel* evidence READ evidence CONSTANT)
    Q_PROPERTY(StableListModel* offers READ offers CONSTANT)
    Q_PROPERTY(StableListModel* products READ products CONSTANT)
    Q_PROPERTY(StableListModel* contracts READ contracts CONSTANT)
    Q_PROPERTY(StableListModel* rivals READ rivals CONSTANT)
    Q_PROPERTY(StableListModel* heavyweights READ heavyweights CONSTANT)
    Q_PROPERTY(StableListModel* competitiveOffers READ competitiveOffers CONSTANT)
    Q_PROPERTY(StableListModel* payables READ payables CONSTANT)
    Q_PROPERTY(StableListModel* funding READ funding CONSTANT)
    Q_PROPERTY(StableListModel* specialists READ specialists CONSTANT)
    Q_PROPERTY(StableListModel* policies READ policies CONSTANT)
    Q_PROPERTY(StableListModel* decisions READ decisions CONSTANT)
    Q_PROPERTY(StableListModel* alerts READ alerts CONSTANT)
    Q_PROPERTY(QString scoreText READ scoreText NOTIFY changed)
    Q_PROPERTY(QVariantMap policyDefaults READ policyDefaults NOTIFY changed)
    Q_PROPERTY(QVariantList renewableLeases READ renewableLeases NOTIFY changed)
    Q_PROPERTY(QVariantList pricedProducts READ pricedProducts NOTIFY changed)
    Q_PROPERTY(QVariantList replacementRacks READ replacementRacks NOTIFY changed)
    Q_PROPERTY(QString operatingReserveText READ operatingReserveText NOTIFY changed)
    Q_PROPERTY(int decisionPage READ decisionPage NOTIFY changed)
    Q_PROPERTY(int decisionPageCount READ decisionPageCount NOTIFY changed)
    Q_PROPERTY(QString decisionPageLabel READ decisionPageLabel NOTIFY changed)
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY changed)
    Q_PROPERTY(QVariantList evaluations READ evaluations NOTIFY changed)
    Q_PROPERTY(QVariantList evaluationSpecialists READ evaluationSpecialists NOTIFY changed)
    Q_PROPERTY(QVariantList synthesisRecipes READ synthesisRecipes NOTIFY changed)
    Q_PROPERTY(QVariantList collectionOffers READ collectionOffers NOTIFY changed)
    Q_PROPERTY(QVariantList collectionJobs READ collectionJobs NOTIFY changed)
    Q_PROPERTY(QString staffWorkload READ staffWorkload NOTIFY changed)
    Q_PROPERTY(QVariantList synthesisTeachers READ synthesisTeachers NOTIFY changed)
    Q_PROPERTY(QVariantList synthesisJobs READ synthesisJobs NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantList launchRegions READ launchRegions NOTIFY changed)
    Q_PROPERTY(StableListModel* regionalMarkets READ regionalMarkets CONSTANT)
    Q_PROPERTY(QVariantMap recommended READ recommended NOTIFY changed)
    Q_PROPERTY(QString briefing READ briefing NOTIFY changed)
    Q_PROPERTY(QVariantMap financialStatements READ financialStatements NOTIFY changed)
    Q_PROPERTY(bool corporateTaxAvailable READ corporateTaxAvailable NOTIFY changed)
    Q_PROPERTY(QVariantMap corporateTax READ corporateTax NOTIFY changed)
    Q_PROPERTY(QString guide READ guide NOTIFY changed)
    Q_PROPERTY(QString ending READ ending NOTIFY changed)
    Q_PROPERTY(bool plannedCompetition READ plannedCompetition NOTIFY changed)
    Q_PROPERTY(QString competitionSummary READ competitionSummary NOTIFY changed)
    Q_PROPERTY(bool governanceAvailable READ governanceAvailable NOTIFY changed)
    Q_PROPERTY(QString ownershipText READ ownershipText NOTIFY changed)
    Q_PROPERTY(QString boardText READ boardText NOTIFY changed)
    Q_PROPERTY(QString publicListingText READ publicListingText NOTIFY changed)
    Q_PROPERTY(int publicFilingCount READ publicFilingCount NOTIFY changed)
    Q_PROPERTY(StableListModel* corporateFinance READ corporateFinance CONSTANT)
    Q_PROPERTY(StableListModel* executives READ executives CONSTANT)
    Q_PROPERTY(StableListModel* charters READ charters CONSTANT)
    Q_PROPERTY(StableListModel* boardMotions READ boardMotions CONSTANT)
    Q_PROPERTY(StableListModel* corporateDisputes READ corporateDisputes CONSTANT)
    Q_PROPERTY(StableListModel* mergerTargets READ mergerTargets CONSTANT)
public:
    explicit ManagementController(QObject* parent=nullptr);
    void update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace);
    bool available() const;
    bool plannedCompetition() const;
    QString competitionSummary() const;
    bool governanceAvailable() const;
    QString ownershipText() const {return ownership_text_;}
    QString boardText() const {return board_text_;}
    QString publicListingText() const;
    int publicFilingCount() const;
    Q_INVOKABLE QString publicFiling(int newestOffset) const;
    Q_INVOKABLE QVariantMap listingPreview(QString offer) const;
    StableListModel* corporateFinance() const {return corporate_finance_;}
    StableListModel* executives() const {return executives_;}
    StableListModel* charters() const {return charters_;}
    StableListModel* boardMotions() const {return board_motions_;}
    StableListModel* corporateDisputes() const {return corporate_disputes_;}
    StableListModel* mergerTargets() const {return merger_targets_;}
    Q_INVOKABLE void negotiateFinance(QString id);
    Q_INVOKABLE void appointExecutive(QString id,bool hybrid);
    Q_INVOKABLE void proposeCharter(QString id);
    Q_INVOKABLE void proposePool(QString basisPoints);
    Q_INVOKABLE void voteBoard(QString id,bool support);
    Q_INVOKABLE void respondDispute(QString id,bool waiver);
    Q_INVOKABLE QString acquisitionPreview(QString id,bool equity);
    Q_INVOKABLE void proposeAcquisition(QString id,bool equity);
    Q_INVOKABLE void transferSubsidiaryFunds(QString id,QString dollars,bool funding);
    StableListModel* dataOffers() const { return data_; }
    StableListModel* researchTopics() const { return research_; }
    bool groundedResearch() const;
    QVariantList researchGraph() const;
    QVariantList researchWorkers() const;
    QVariantList researchSources() const;
    QVariantList researchRentals() const;
    Q_INVOKABLE QVariantMap researchTopic(QString id) const;
    Q_INVOKABLE QString researchBlocker(QString id,QString source,QString rate,QStringList workers) const;
    Q_INVOKABLE void beginGroundedResearch(QString id,QString source,QString rate,QStringList workers);
    StableListModel* models() const { return models_; }
    StableListModel* evidence() const { return evidence_; }
    StableListModel* offers() const { return offers_; }
    StableListModel* products() const { return products_; }
    StableListModel* contracts() const { return contracts_; }
    StableListModel* regionalMarkets() const { return regional_markets_; }
    QVariantList launchRegions() const { return launch_regions_; }
    StableListModel* rivals() const { return rivals_; }
    StableListModel* heavyweights() const {return heavyweights_;}
    StableListModel* competitiveOffers() const {return competitive_offers_;}
    StableListModel* payables() const { return payables_; }
    StableListModel* funding() const { return funding_; }
    StableListModel* specialists() const { return specialists_; }
    StableListModel* policies() const { return policies_; }
    StableListModel* decisions() const { return decisions_; }
    StableListModel* alerts() const { return alerts_; }
    QString scoreText() const;
    QVariantMap policyDefaults() const;
    QVariantList renewableLeases() const;
    QVariantList pricedProducts() const;
    QVariantList replacementRacks() const;
    QString operatingReserveText() const;
    Q_INVOKABLE QVariantList replacementFamilies(QString rack) const;
    Q_INVOKABLE void authorizeOperatingReserve(QString target);
    Q_INVOKABLE void authorizeHardwareReplacement(QString rack,QString family,QString purchase,QString monthly,QString reserve);
    int decisionPage() const {return decision_page_;}
    int decisionPageCount() const;
    QString decisionPageLabel() const;
    Q_INVOKABLE void setDecisionPage(int page);
    Q_INVOKABLE void authorizePricing(QString product,QString floor,QString ceiling,QString step,QString low,QString high,QString interval,QString runs,QString reserve);
    Q_INVOKABLE QString pricingPreview(QString product,QString floor,QString ceiling,QString step,QString low,QString high) const;
    QString alertDetail(const core::ManagementNotice& notice) const;
    Q_INVOKABLE void takeFinancing(QString id);
    Q_INVOKABLE void hireSpecialist(QString id);
    Q_INVOKABLE void authorizeEvaluation(QString model,QString plan,QString source,QString rate,QString staff,QString budget,QString reserve,QString interval,QString runs);
    Q_INVOKABLE void authorizeRenewal(QString offer,QString budget,QString reserve,QString monthly,QString runs);
    Q_INVOKABLE void setPolicyEnabled(QString id,bool enabled);
    Q_INVOKABLE void renewLease(QString id);
    Q_INVOKABLE void acknowledgeAlert(QString id);
    QVariantList recipes() const;
    QVariantList evaluations() const;
    QVariantList evaluationSpecialists() const;
    QVariantList synthesisRecipes() const;
    QVariantList collectionOffers() const;
    QVariantList collectionJobs() const;
    QString staffWorkload() const;
    Q_INVOKABLE QVariantMap collectionQuote(QString offer,QString source,QString rate,QString staff) const;
    Q_INVOKABLE void startCollection(QString offer,QString source,QString rate,QString staff);
    QVariantList synthesisTeachers() const;
    QVariantList synthesisJobs() const;
    Q_INVOKABLE QVariantList synthesisEvidence(QString teacher) const;
    Q_INVOKABLE QVariantList synthesisVerification(QString recipe,QString teacher) const;
    Q_INVOKABLE QVariantMap synthesisQuote(QString recipe,QString teacher,QString evidence,QString verification,
        QString units,QString source,QString rate,QString staff) const;
    Q_INVOKABLE void generateData(QString recipe,QString teacher,QString evidence,QString verification,
        QString units,QString source,QString rate,QString staff);
    QVariantList sources() const;
    QVariantList services() const;
    QVariantMap recommended() const;
    QString briefing() const;
    QVariantMap financialStatements() const;
    bool corporateTaxAvailable() const;
    QVariantMap corporateTax() const;
    Q_INVOKABLE QVariantMap cashScenarios() const;
    QString guide() const;
    QString ending() const;
    Q_INVOKABLE QVariantMap trainingPreview(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent={}) const;
    Q_INVOKABLE QString servicePreview(QString service,QString model,bool review,QString tools={},QString oversight={}) const;
    Q_INVOKABLE QVariantList weightAudiences(QString service,QString region) const;
    Q_INVOKABLE QVariantMap publicationQuote(QString service,QString model,QString evidence,QString source,
        bool review,QString region,QString audience) const;
    Q_INVOKABLE QVariantMap agentQuote(QString service,QString model,QString evidence,QString source,
        bool review,QString region,QString audience,QString tools,QString oversight) const;
    Q_INVOKABLE QVariantMap rolloutPreview(QString service,QString model,QString evidence,QString source,
        bool review,QString share,QString days,QString audience={},QString tools={},QString oversight={}) const;
    Q_INVOKABLE QVariantMap compatibilityQuote(QString service,QString days) const;
    Q_INVOKABLE QVariantMap retirementQuote(QString service,QString days) const;
    Q_INVOKABLE QVariantMap waitlistQuote(QString service,QString tasks,bool enabled) const;
    Q_INVOKABLE QVariantMap deploymentResponseQuote(QString incident,QString source,QString rate,QString staff) const;
    Q_INVOKABLE void buyData(QString id);
    Q_INVOKABLE void research(QString id,QString source,QString rate,QString staff);
    Q_INVOKABLE void publishResearch(QString id);
    Q_INVOKABLE void acquirePublication(QString id);
    Q_INVOKABLE void setProductPrice(QString id,QString cents);
    Q_INVOKABLE void train(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent={});
    Q_INVOKABLE void clone(QString id);
    Q_INVOKABLE void checkpoint(QString id);
    Q_INVOKABLE void evaluate(QString model,QString plan,QString source,QString rate,QString staff,QString specialist={});
    Q_INVOKABLE void deploy(QString service,QString model,QString evidence,QString source,bool review,QString region,QString audience={},QString tools={},QString oversight={});
    Q_INVOKABLE void stageRollout(QString service,QString model,QString evidence,QString source,bool review,QString share,QString days,QString audience={},QString tools={},QString oversight={});
    Q_INVOKABLE void finishRollout(QString service,bool promote);
    Q_INVOKABLE void rollbackService(QString service);
    Q_INVOKABLE void commitCompatibility(QString service,QString days);
    Q_INVOKABLE void announceRetirement(QString service,QString days);
    Q_INVOKABLE void setServiceWaitlist(QString service,QString tasks,bool enabled);
    Q_INVOKABLE void respondDeploymentIncident(QString incident,QString source,QString rate,QString staff);
    Q_INVOKABLE void acceptOffer(QString id);
    Q_INVOKABLE void respondCompetitiveOffer(QString id,bool accept);
    Q_INVOKABLE void cancelContract(QString id);
    Q_INVOKABLE void payInvoice(QString id);
    Q_INVOKABLE void repayDebt(QString id);
signals:
    void changed();
    void actionRequested(pgp::core::CommandAction action);
    void competitiveResponseRequested(QString id,bool accept);
    void acquisitionRequested(QString id,bool equity);
    void groupTransferRequested(QString id,pgp::core::Cents amount,bool funding);
    void noticeRaised(QString text);
private:
    bool editable();
    std::vector<CampaignRow> policyDecisionRows() const;
    int decision_page_=0;
    const core::SimulationState& state() const;
    QString nextId(QString prefix) const;
    CampaignSnapshotPtr snapshot_;
    StableListModel *funding_,*specialists_,*policies_,*decisions_,*alerts_;
    StableListModel *data_,*research_,*models_,*evidence_,*offers_,*products_,*contracts_,*rivals_,*payables_;
    StableListModel *heavyweights_,*competitive_offers_;
    StableListModel *corporate_finance_,*executives_,*board_motions_,*corporate_disputes_,*merger_targets_,*charters_;
    StableListModel *regional_markets_;
    QVariantList launch_regions_;
    QString ownership_text_,board_text_;
    const runtime::CompiledProgram* program_=nullptr;
    std::optional<std::uint64_t> acquisition_preview_revision_;
    QString acquisition_preview_target_;
    bool acquisition_preview_equity_=false;
};
}
