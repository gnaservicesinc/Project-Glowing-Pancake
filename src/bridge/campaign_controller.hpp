#pragma once

#include "campaign_model.hpp"
#include "campaign_worker.hpp"
#include "management_controller.hpp"
#include "infrastructure_controller.hpp"

#include <QThread>
#include <QUrl>
#include <QVariantList>

namespace pgp::bridge {
class CampaignController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ManagementController* management READ management CONSTANT)
    Q_PROPERTY(InfrastructureController* infrastructure READ infrastructure CONSTANT)
    Q_PROPERTY(int daysPerSecond READ daysPerSecond WRITE setDaysPerSecond NOTIFY stateChanged)
    Q_PROPERTY(bool welcome READ welcome NOTIFY stateChanged)
    Q_PROPERTY(QVariantList headquarters READ headquarters CONSTANT)
    Q_PROPERTY(QVariantList scenarios READ scenarios CONSTANT)
    Q_PROPERTY(QString headquartersText READ headquartersText NOTIFY stateChanged)
    Q_PROPERTY(bool autoPauseProjects READ autoPauseProjects WRITE setAutoPauseProjects NOTIFY settingsChanged)
    Q_PROPERTY(bool autoPauseEvaluations READ autoPauseEvaluations WRITE setAutoPauseEvaluations NOTIFY settingsChanged)
    Q_PROPERTY(bool autoPauseAlerts READ autoPauseAlerts WRITE setAutoPauseAlerts NOTIFY settingsChanged)
    Q_PROPERTY(bool showGuide READ showGuide WRITE setShowGuide NOTIFY settingsChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString date READ date NOTIFY stateChanged)
    Q_PROPERTY(QString revision READ revision NOTIFY stateChanged)
    Q_PROPERTY(QString cashText READ cashText NOTIFY stateChanged)
    Q_PROPERTY(QString availableText READ availableText NOTIFY stateChanged)
    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
    Q_PROPERTY(QString capacityText READ capacityText NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool canAdvance READ canAdvance NOTIFY stateChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY stateChanged)
    Q_PROPERTY(QUrl saveFolder READ saveFolder CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString requiredKind READ requiredKind NOTIFY stateChanged)
    Q_PROPERTY(QString requiredTitle READ requiredTitle NOTIFY stateChanged)
    Q_PROPERTY(QString requiredText READ requiredText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList requiredChoices READ requiredChoices NOTIFY stateChanged)
    Q_PROPERTY(bool needsWarning READ needsWarning NOTIFY stateChanged)
    Q_PROPERTY(StableListModel* projects READ projects CONSTANT)
    Q_PROPERTY(StableListModel* inbox READ inbox CONSTANT)
    Q_PROPERTY(QString selectedProjectId READ selectedProjectId WRITE setSelectedProjectId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedInboxId READ selectedInboxId WRITE setSelectedInboxId NOTIFY selectionChanged)
    Q_PROPERTY(int textScalePercent READ textScalePercent WRITE setTextScalePercent NOTIFY settingsChanged)
    Q_PROPERTY(bool compactManagement READ compactManagement WRITE setCompactManagement NOTIFY settingsChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY settingsChanged)
    Q_PROPERTY(bool autosaveEnabled READ autosaveEnabled WRITE setAutosaveEnabled NOTIFY settingsChanged)
public:
    CampaignController(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
                       persistence::LocalPaths paths, persistence::Settings initial_settings = {},
                       QObject* parent = nullptr, bool welcome = false);
    ~CampaignController() override;
    ManagementController* management() const { return management_; }
    InfrastructureController* infrastructure() const { return infrastructure_; }
    int daysPerSecond() const { return days_per_second_; }
    void setDaysPerSecond(int value);
    bool welcome() const { return welcome_; }
    QVariantList headquarters() const;
    QVariantList scenarios() const;
    QString headquartersText() const;
    bool autoPauseProjects() const { return settings_.auto_pause_projects; }
    bool autoPauseEvaluations() const { return settings_.auto_pause_evaluations; }
    bool autoPauseAlerts() const { return settings_.auto_pause_alerts; }
    void setAutoPauseProjects(bool value);
    void setAutoPauseEvaluations(bool value);
    void setAutoPauseAlerts(bool value);
    Q_INVOKABLE void advanceTo(QString milestone);
    Q_INVOKABLE void acknowledgeManagement();
    bool showGuide() const { return settings_.show_guide; }
    void setShowGuide(bool value);
    Q_INVOKABLE void newCampaign(QString headquarters, bool stories = true);
    Q_INVOKABLE void newCampaignOptions(QString headquarters, bool stories, QString scenario);
    Q_INVOKABLE void openWelcome();
    [[nodiscard]] QString date() const;
    [[nodiscard]] bool ready() const noexcept { return static_cast<bool>(snapshot_); }
    [[nodiscard]] QString revision() const;
    [[nodiscard]] QString cashText() const;
    [[nodiscard]] QString availableText() const;
    [[nodiscard]] QString phaseText() const;
    [[nodiscard]] QString capacityText() const;
    [[nodiscard]] bool paused() const;
    [[nodiscard]] bool canAdvance() const;
    [[nodiscard]] bool canPlay() const;
    [[nodiscard]] QUrl saveFolder() const { return save_folder_; }
    [[nodiscard]] bool busy() const noexcept { return busy_; }
    [[nodiscard]] QString notice() const { return notice_; }
    [[nodiscard]] QString requiredKind() const;
    [[nodiscard]] QString requiredTitle() const;
    [[nodiscard]] QString requiredText() const;
    [[nodiscard]] QVariantList requiredChoices() const;
    [[nodiscard]] bool needsWarning() const;
    [[nodiscard]] StableListModel* projects() const { return projects_; }
    [[nodiscard]] StableListModel* inbox() const { return inbox_; }
    [[nodiscard]] QString selectedProjectId() const { return projects_->selectedId(); }
    [[nodiscard]] QString selectedInboxId() const { return inbox_->selectedId(); }
    void setSelectedProjectId(QString id);
    void setSelectedInboxId(QString id);
    [[nodiscard]] int textScalePercent() const noexcept { return static_cast<int>(settings_.text_scale_percent); }
    [[nodiscard]] bool compactManagement() const noexcept { return settings_.compact_management; }
    [[nodiscard]] bool reducedMotion() const noexcept { return settings_.reduced_motion; }
    [[nodiscard]] bool autosaveEnabled() const noexcept { return settings_.autosave_enabled; }
    void setTextScalePercent(int value);
    void setCompactManagement(bool value);
    void setReducedMotion(bool value);
    void setAutosaveEnabled(bool value);
    Q_INVOKABLE void step();
    Q_INVOKABLE void togglePaused();
    Q_INVOKABLE void choose(QString choice_id);
    Q_INVOKABLE void acknowledgeWarning();
    Q_INVOKABLE void resolveCoreChoice(bool accept);
    Q_INVOKABLE void projectCancel(QString id);
    Q_INVOKABLE void saveTo(QUrl url);
    Q_INVOKABLE void loadFrom(QUrl url);
    Q_INVOKABLE void clearNotice();
    // C++ smoke/tests can inspect authority without exposing mutation to QML.
    [[nodiscard]] CampaignSnapshotPtr snapshot() const { return snapshot_; }
signals:
    void stateChanged();
    void settingsChanged();
    void selectionChanged();
    void noticeChanged();
    void commandFinished(bool accepted, QString explanation);
    void snapshotChanged();
private:
    bool beginOperation();
    void submit(core::CommandAction action);
    void changeSettings(persistence::Settings settings);
    void acceptSnapshot(CampaignSnapshotPtr snapshot);
    void setNotice(QString explanation);
    [[nodiscard]] const runtime::Presentation* waitingPresentation() const;
    const runtime::CompiledProgram program_;
    ManagementController* management_;
    InfrastructureController* infrastructure_;
    bool welcome_ = false;
    int days_per_second_ = 1;
    QThread thread_;
    CampaignWorker* worker_ = nullptr;
    StableListModel* projects_;
    StableListModel* inbox_;
    CampaignSnapshotPtr snapshot_;
    persistence::Settings settings_;
    const QUrl save_folder_;
    QString notice_;
    std::uint64_t token_ = 0;
    bool busy_ = true;
};
}
