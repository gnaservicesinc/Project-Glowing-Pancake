#include "campaign_controller.hpp"
#include "flow_presentation.hpp"

#include <pgp/core/integer_codec.hpp>
#include <pgp/session/campaign.hpp>

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size())); }
QString integer(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString money(core::Cents amount) {
    const auto raw = amount.value();
    const auto magnitude = raw < 0 ? static_cast<std::uint64_t>(-(raw + 1)) + 1 : static_cast<std::uint64_t>(raw);
    QString whole = integer(magnitude / 100);
    for (qsizetype position = whole.size() - 3; position > 0; position -= 3) whole.insert(position,QLatin1Char(','));
    return (raw < 0 ? QStringLiteral("−$") : QStringLiteral("$")) + whole + QLatin1Char('.') +
        QStringLiteral("%1").arg(static_cast<qulonglong>(magnitude % 100),2,10,QLatin1Char('0'));
}
core::Cents available(const core::SimulationState& state) {
    auto result = state.cash;
    for (const auto& row : state.reservations) result = core::checked_unit_subtract(result,row.reserved);
    return result;
}
std::filesystem::path local_path(const QUrl& url) {
    if (!url.isLocalFile() || !url.host().isEmpty() || url.hasQuery() || url.hasFragment())
        throw std::invalid_argument("Choose an absolute local save file.");
    const auto file = url.toLocalFile();
    if (file.isEmpty() || !QFileInfo{file}.isAbsolute()) throw std::invalid_argument("Choose an absolute local save file.");
    return std::filesystem::path{file.toStdU16String()};
}
std::vector<CampaignRow> project_rows(const runtime::Checkpoint& checkpoint) {
    std::vector<CampaignRow> rows;
    const auto& state = checkpoint.world.simulation;
    if (!state.capacity) return rows;
    for (const auto& project : state.capacity->projects) {
        QString kind;
        switch (project.kind) {
        case core::ProjectKind::training: kind = QStringLiteral("Training"); break;
        case core::ProjectKind::research: kind = QStringLiteral("Research"); break;
        case core::ProjectKind::evaluation: kind = QStringLiteral("Evaluation"); break;
        case core::ProjectKind::serving: kind = QStringLiteral("Serving"); break;
        }
        QString status;
        switch (project.status) {
        case core::ProjectStatus::active: status = QStringLiteral("Active"); break;
        case core::ProjectStatus::completed: status = QStringLiteral("Completed"); break;
        case core::ProjectStatus::cancelled: status = QStringLiteral("Cancelled"); break;
        }
        const auto progress = project.kind == core::ProjectKind::serving ?
            QStringLiteral("%1 useful tasks delivered").arg(integer(project.useful_tasks)) :
            QStringLiteral("%1 / %2 CU completed").arg(cu(project.completed_work.value()),cu(project.total_work.value()));
        const auto detail = QStringLiteral("%1\n%2\n%3 CU/day reserved · %4 staff\n%5\nSupport: %6/month\nStarted %7")
            .arg(text(project.cause),kind,cu(project.requested.value()),QString::number(project.staff),progress,
                 money(project.monthly_support_cost),text(core::iso_date(project.starts_on)));
        rows.push_back({text(project.id),readable(project.id),kind + QStringLiteral(" · ") + progress,detail,status,
            project.status == core::ProjectStatus::active});
    }
    return rows;
}
std::vector<CampaignRow> inbox_rows(const runtime::Checkpoint& checkpoint,const content::Collection& collection,const ManagementController& management) {
    std::vector<CampaignRow> rows;
    for (const auto& row : checkpoint.runtime.presentations) {
        if (row.kind == runtime::PresentationKind::news) continue;
        rows.push_back({QStringLiteral("flow:") + integer(row.id),presentation_heading(row,collection),text(core::iso_date(row.day)),
            presentation_detail(checkpoint,row,&collection),row.kind == runtime::PresentationKind::dialogue && !row.resolved ? QStringLiteral("Response required") : QStringLiteral("Recorded"),
            row.kind == runtime::PresentationKind::dialogue && !row.resolved});
    }
    constexpr std::size_t headlines_per_part = 8;
    for (const auto& [week, entries] : session::group_weekly_news(checkpoint)) {
        const auto date = text(core::iso_date(week));
        const auto parts = (entries.size() + headlines_per_part - 1) / headlines_per_part;
        for (std::size_t part = 0; part < parts; ++part) {
            const auto first = part * headlines_per_part;
            const auto limit = std::min(first + headlines_per_part, entries.size());
            QString detail;
            for (std::size_t index = first; index < limit; ++index) {
                const auto& news = *entries[index];
                if (!detail.isEmpty()) detail += QStringLiteral("\n\n");
                detail += QStringLiteral("%1\n%2").arg(text(core::iso_date(news.day)), presentation_context(checkpoint, news,&collection));
            }
            const auto status = parts == 1 ? (entries.size() == 1 ? QStringLiteral("1 update") : QStringLiteral("%1 updates").arg(integer(entries.size()))) :
                QStringLiteral("Part %1 of %2 · %3 updates").arg(integer(part + 1), integer(parts), integer(entries.size()));
            const auto id = QStringLiteral("digest:%1:%2").arg(date).arg(static_cast<qulonglong>(part), 5, 10, QLatin1Char('0'));
            rows.push_back({id, QStringLiteral("Weekly news digest"), QStringLiteral("Week of %1").arg(date),
                            std::move(detail), status, false});
        }
    }
    const auto& state = checkpoint.world.simulation;
    if (state.management) for (const auto& n:state.management->notices)
        rows.push_back({QStringLiteral("management:")+text(n.id),readable(n.kind),text(core::iso_date(n.day))+(n.deadline?QStringLiteral(" · deadline ")+text(core::iso_date(*n.deadline)):QString{}),
            management.alertDetail(n),n.acknowledged?QStringLiteral("Acknowledged"):n.required?QStringLiteral("Required response"):QStringLiteral("Company alert"),n.required && !n.acknowledged,
            {{QStringLiteral("noticeId"),text(n.id)},{QStringLiteral("acknowledged"),n.acknowledged}}});
    if (state.liquidity) for (const auto& workout : state.liquidity->workouts) {
        const auto ended = workout.resolved_on.has_value() || workout.closed_on.has_value();
        QString status = workout.resolved_on ? QStringLiteral("Resolved") : workout.closed_on ? QStringLiteral("Closed") : QStringLiteral("Workout active");
        rows.push_back({QStringLiteral("liquidity:") + integer(workout.id),QStringLiteral("Liquidity response window"),
            text(core::iso_date(workout.warned_on)),
            QStringLiteral("%1\nInitial unpaid obligations: %2\nRespond by %3. Financing must be followed by payment; partial payments preserve this deadline.")
                .arg(text(workout.trigger.cause),money(workout.initial_shortfall),text(core::iso_date(workout.deadline))),
            status,!ended});
    }
    return rows;
}
}
CampaignController::CampaignController(runtime::CompiledProgram program,runtime::Checkpoint checkpoint,
    persistence::LocalPaths paths,persistence::Settings initial_settings,QObject* parent,bool welcome)
    : QObject(parent),program_(program),management_(new ManagementController(this)),welcome_(welcome),projects_(new StableListModel(this)),inbox_(new StableListModel(this,true)),settings_(initial_settings),
      save_folder_(QUrl::fromLocalFile(QString::fromStdU16String(paths.saves_directory.u16string()))) {
    connect(management_,&ManagementController::actionRequested,this,&CampaignController::submit);
    connect(management_,&ManagementController::competitiveResponseRequested,this,[this](QString id,bool accept) {
        if (!paused() || !beginOperation()) return;
        const auto revision=snapshot_->checkpoint.world.simulation.revision;
        QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,id=id.toStdString(),accept]{worker->respondCompetitiveOffer(token,revision,id,accept);},Qt::QueuedConnection);
    });
    connect(management_,&ManagementController::noticeRaised,this,&CampaignController::setNotice);
    connect(management_,&ManagementController::groupTransferRequested,this,[this](QString id,core::Cents amount,bool funding) {
        if (!paused() || !beginOperation()) return;
        const auto revision=snapshot_->checkpoint.world.simulation.revision;
        QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,id=id.toStdString(),amount,funding]{worker->transferSubsidiaryFunds(token,revision,id,amount,funding);},Qt::QueuedConnection);
    });
    connect(management_,&ManagementController::acquisitionRequested,this,[this](QString id,bool equity) {
        if (!paused() || !beginOperation()) return;
        const auto revision=snapshot_->checkpoint.world.simulation.revision;
        QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,id=id.toStdString(),equity]{worker->proposeAcquisition(token,revision,id,equity);},Qt::QueuedConnection);
    });
    (void)persistence::encode_settings(settings_);
    worker_ = new CampaignWorker(std::move(program),std::move(checkpoint),std::move(paths),settings_);
    worker_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater);
    connect(worker_,&CampaignWorker::snapshotReady,this,&CampaignController::acceptSnapshot);
    connect(worker_,&CampaignWorker::noticeRaised,this,&CampaignController::setNotice);
    connect(worker_,&CampaignWorker::operationCompleted,this,[this](qulonglong token,bool accepted,QString explanation) {
        if (token != static_cast<qulonglong>(token_)) return;
        busy_ = false; emit stateChanged();
        if (!accepted || !explanation.isEmpty()) setNotice(explanation);
        emit commandFinished(accepted,explanation);
    });
    connect(projects_,&StableListModel::selectedIdChanged,this,&CampaignController::selectionChanged);
    connect(projects_,&StableListModel::selectedRowChanged,this,&CampaignController::selectionChanged);
    connect(inbox_,&StableListModel::selectedIdChanged,this,&CampaignController::selectionChanged);
    connect(inbox_,&StableListModel::selectedRowChanged,this,&CampaignController::selectionChanged);
    thread_.start();
    QMetaObject::invokeMethod(worker_,[worker = worker_] { worker->start(); },Qt::QueuedConnection);
}
CampaignController::~CampaignController() {
    if (thread_.isRunning()) {
        QMetaObject::invokeMethod(worker_,[worker = worker_] { worker->stop(); },Qt::BlockingQueuedConnection);
        thread_.quit(); thread_.wait();
    }
}
void CampaignController::acceptSnapshot(CampaignSnapshotPtr snapshot) {
    if (!snapshot) return;
    if (snapshot_ && (snapshot->generation < snapshot_->generation ||
        (snapshot->generation == snapshot_->generation && snapshot->checkpoint.world.simulation.revision < snapshot_->checkpoint.world.simulation.revision))) return;
    const bool first = !snapshot_;
    const bool replace = first || snapshot_->generation != snapshot->generation;
    auto projects = project_rows(snapshot->checkpoint);
    management_->update(snapshot,program_,replace);
    auto inbox = inbox_rows(snapshot->checkpoint,program_.collection,*management_);
    if (!first && replace) welcome_=false;
    if (replace) { projects_->beginCampaign(std::move(projects)); inbox_->beginCampaign(std::move(inbox)); }
    else { projects_->setRows(std::move(projects)); inbox_->setRows(std::move(inbox)); }
    const bool changed_settings = settings_ != snapshot->settings;
    settings_ = snapshot->settings; snapshot_ = std::move(snapshot);
    if (first) busy_ = false;
    if (changed_settings) emit settingsChanged();
    emit stateChanged(); emit snapshotChanged(); emit selectionChanged();
}
QVariantList CampaignController::headquarters() const {
    QVariantList result;
    for (const auto& [id,definition]:program_.collection.definitions) {
        (void)id; const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);
        if (!scenario || !scenario->service_catalog) continue;
        for (const auto& hq:scenario->headquarters_choices)
            result.push_back(QVariantMap{{QStringLiteral("id"),text(hq)},{QStringLiteral("label"),hq=="US-CA"?QStringLiteral("California"):hq=="US-TX"?QStringLiteral("Texas"):QStringLiteral("Washington")}});
        break;
    }
    return result;
}
QVariantList CampaignController::scenarios() const {
    QVariantList result;
    for (const auto& [id,definition]:program_.collection.definitions) {
        const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);if (!scenario || !scenario->service_catalog) continue;
        QString label=QStringLiteral("Introductory roster · 2 startups and 1 heavyweight");
        QString detail=QStringLiteral("Small 2014–2017 introduction with bounded rival routines.");
        if (scenario->competition) {
            const auto& setup=*scenario->competition;
            label=QStringLiteral("%1 US + %2 international startups · %3 heavyweights%4").arg(setup.domestic).arg(setup.international).arg(setup.heavyweights)
                .arg(setup.catchup_cash.value()>0?QStringLiteral(" · forgiving financing"):QString{});
            std::uint64_t demand=0;for (const auto& segment:scenario->market->segments) demand+=segment.daily_tasks;
            detail=QStringLiteral("Up to three US and three international startups enter initially; later startups enter on the authored schedule. Rivals pay for work and use public information. Shared demand is %1 tasks/day. %2")
                .arg(demand).arg(setup.catchup_cash.value()>0?QStringLiteral("Each rival may take one disclosed %1 equity round for %2.%3% ownership; no recurring catch-up subsidy.").arg(money(setup.catchup_cash)).arg(setup.catchup_acquired.value()/100).arg(setup.catchup_acquired.value()%100,2,10,QLatin1Char('0')):QStringLiteral("No catch-up funding is available."));
        }
        result.push_back(QVariantMap{{QStringLiteral("id"),text(id)},{QStringLiteral("label"),label},{QStringLiteral("detail"),detail}});
    }
    return result;
}
QString CampaignController::headquartersText() const {
    if (!snapshot_) return {};
    const auto& id=snapshot_->checkpoint.world.state_id;
    return id=="state.us_ca"?QStringLiteral("California"):id=="state.us_tx"?QStringLiteral("Texas"):QStringLiteral("Washington");
}
void CampaignController::setDaysPerSecond(int value) {
    if (value!=1 && value!=10 && value!=30 && value!=60) return;
    days_per_second_=value;
    QMetaObject::invokeMethod(worker_,[worker=worker_,value]{worker->setSpeed(value);},Qt::QueuedConnection);
    emit stateChanged();
}
void CampaignController::newCampaign(QString hq,bool stories) {
    newCampaignOptions(std::move(hq),stories,{});
}
void CampaignController::newCampaignOptions(QString hq,bool stories,QString scenario) {
    if (!beginOperation()) return;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,hq=hq.toStdString(),stories,scenario=scenario.toStdString()]{worker->newCampaign(token,hq,stories,scenario);},Qt::QueuedConnection);
}
void CampaignController::openWelcome() {
    if (!management_->available() || busy_) return;
    if (!paused()) submit(core::SetPaused{true});
    welcome_=true;emit stateChanged();
}
void CampaignController::setShowGuide(bool value) { auto settings=settings_;settings.show_guide=value;changeSettings(settings); }
QString CampaignController::date() const { return snapshot_ ? text(core::iso_date(snapshot_->checkpoint.world.simulation.date)) : QStringLiteral("Preparing campaign…"); }
QString CampaignController::revision() const { return snapshot_ ? integer(snapshot_->checkpoint.world.simulation.revision) : QString{}; }
QString CampaignController::cashText() const { return snapshot_ ? money(snapshot_->checkpoint.world.simulation.cash) : QStringLiteral("—"); }
QString CampaignController::availableText() const { return snapshot_ ? money(available(snapshot_->checkpoint.world.simulation)) : QStringLiteral("—"); }
QString CampaignController::phaseText() const {
    if (!snapshot_) return QStringLiteral("Preparing");
    switch (snapshot_->checkpoint.world.simulation.phase) {
    case core::DayPhase::opening: return QStringLiteral("Morning briefing");
    case core::DayPhase::awaiting_choice: return QStringLiteral("Decision required");
    case core::DayPhase::work_ready: return paused() ? QStringLiteral("Paused") : QStringLiteral("Running");
    case core::DayPhase::finished: return QStringLiteral("Scenario complete");
    case core::DayPhase::closed: return QStringLiteral("Company closed");
    }
    return {};
}
QString CampaignController::capacityText() const {
    if (!snapshot_ || !snapshot_->checkpoint.world.simulation.capacity) return QStringLiteral("No capacity assigned");
    const auto report = core::capacity_report(*snapshot_->checkpoint.world.simulation.capacity);
    return QStringLiteral("%1 CU/day · %2 / %3 staff assigned").arg(cu(report.effective.value()))
        .arg(report.assigned_staff).arg(report.available_staff);
}
bool CampaignController::paused() const { return !snapshot_ || snapshot_->driver_halted || snapshot_->checkpoint.world.simulation.paused; }
const runtime::Presentation* CampaignController::waitingPresentation() const {
    if (!snapshot_) return nullptr;
    const auto& rows = snapshot_->checkpoint.runtime.presentations;
    const auto found = std::find_if(rows.begin(),rows.end(),[](const auto& row) { return row.kind == runtime::PresentationKind::dialogue && !row.resolved; });
    return found == rows.end() ? nullptr : &*found;
}
bool CampaignController::needsWarning() const {
    if (!snapshot_ || !snapshot_->checkpoint.world.simulation.liquidity) return false;
    return core::liquidity_report(*snapshot_->checkpoint.world.simulation.liquidity).pending_warning.has_value();
}
QString CampaignController::requiredKind() const {
    if (!snapshot_) return QStringLiteral("none");
    if (snapshot_->checkpoint.world.simulation.pending_choice) return QStringLiteral("core");
    if (waitingPresentation()) return QStringLiteral("content");
    if (needsWarning()) return QStringLiteral("liquidity");
    if (core::required_management_notice(snapshot_->checkpoint.world.simulation)) return QStringLiteral("management");
    return QStringLiteral("none");
}
QString CampaignController::requiredTitle() const {
    const auto kind = requiredKind();
    if (kind == QStringLiteral("core")) return QStringLiteral("Delivery decision");
    if (kind == QStringLiteral("content")) return presentation_heading(*waitingPresentation(),program_.collection);
    if (kind == QStringLiteral("liquidity")) return QStringLiteral("Unpaid obligations need a response");
    if (kind == QStringLiteral("management")) return QStringLiteral("Temporary compute setback");
    return {};
}
QString CampaignController::requiredText() const {
    const auto kind = requiredKind();
    if (kind == QStringLiteral("management")) {
        for (const auto& n:snapshot_->checkpoint.world.simulation.management->notices) if (n.required && !n.acknowledged) return management_->alertDetail(n);
    }
    if (kind == QStringLiteral("core")) return QStringLiteral("A scheduled delivery awaits your decision. Accepting consumes its reserved funds; declining does not refund earlier purchases.");
    if (kind == QStringLiteral("content")) {
        const auto* row=waitingPresentation();
        auto result=presentation_detail(snapshot_->checkpoint,*row,&program_.collection);
        const auto& s=snapshot_->checkpoint.world.simulation;
        if (s.business_offers && (row->flow_id=="slice.supplier_offer" || row->flow_id=="slice.recovery_opportunity"))
            for (const auto& offer:s.business_offers->catalog)
                if ((row->flow_id=="slice.supplier_offer" && offer.kind==core::BusinessOfferKind::compute_lease) ||
                    (row->flow_id=="slice.recovery_opportunity" && offer.kind==core::BusinessOfferKind::recovery_pilot && offer.id=="offer.recovery_pilot"))
                    result+=QStringLiteral("\n\n")+management_->offers()->row(text(offer.id)).value(QStringLiteral("detail")).toString();
        return result;
    }
    if (kind == QStringLiteral("liquidity")) {
        const auto report = core::liquidity_report(*snapshot_->checkpoint.world.simulation.liquidity);
        return QStringLiteral("%1\nOverdue payables %2 · matured principal %3\nRespond by %4. Acknowledging keeps this deadline; financing must be followed by payment.")
            .arg(report.oldest ? text(report.oldest->cause) : QStringLiteral("Unserviced obligations"),
                 money(report.unpaid_payables),money(report.matured_principal),text(core::iso_date(*report.deadline)));
    }
    return {};
}
QVariantList CampaignController::requiredChoices() const {
    QVariantList result;
    if (const auto* presentation = waitingPresentation())
        for (const auto& choice : presentation->choices)
            result.push_back(QVariantMap{{QStringLiteral("choiceId"),text(choice.id)},{QStringLiteral("label"),choice_label(choice,&program_.collection)}});
    return result;
}
bool CampaignController::canAdvance() const {
    return paused() && canPlay();
}
bool CampaignController::canPlay() const {
    if (welcome_ || !snapshot_ || busy_ || requiredKind() != QStringLiteral("none")) return false;
    const auto& state = snapshot_->checkpoint.world.simulation;
    return state.phase == core::DayPhase::work_ready && snapshot_->checkpoint.runtime.prepared_day == state.date;
}
void CampaignController::setSelectedProjectId(QString id) {
    projects_->setSelectedId(std::move(id));
}
void CampaignController::setSelectedInboxId(QString id) {
    inbox_->setSelectedId(std::move(id));
}
void CampaignController::setNotice(QString explanation) { if (explanation == notice_) return; notice_ = std::move(explanation); emit noticeChanged(); }
void CampaignController::clearNotice() { setNotice({}); }
bool CampaignController::beginOperation() {
    if (busy_ || !snapshot_) return false;
    if (token_ == std::numeric_limits<std::uint64_t>::max()) { setNotice(QStringLiteral("The command identifier range is exhausted.")); return false; }
    ++token_; busy_ = true; emit stateChanged(); return true;
}
void CampaignController::submit(core::CommandAction action) {
    if (!beginOperation()) return;
    if (welcome_) { busy_=false;emit stateChanged();return; }
    const auto command = core::Command{snapshot_->checkpoint.world.simulation.revision,std::move(action)};
    QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,command] { worker->submitCommand(token,command); },Qt::QueuedConnection);
}
void CampaignController::step() { if (canAdvance()) submit(core::AdvanceDay{}); }
void CampaignController::togglePaused() {
    if (!snapshot_) return;
    if (!paused() || canPlay()) submit(core::SetPaused{!paused()});
}
void CampaignController::choose(QString choice_id) {
    const auto* presentation = waitingPresentation();
    if (!presentation) return;
    const auto id = presentation->id;
    if (!beginOperation()) return;
    const auto observed = snapshot_->checkpoint.world.simulation.revision;
    QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,observed,id,choice = choice_id.toStdString()] {
        worker->choose(token,observed,id,choice);
    },Qt::QueuedConnection);
}
void CampaignController::advanceTo(QString kind) {
    if (!paused() || !canPlay() || !beginOperation()) return;
    const auto revision=snapshot_->checkpoint.world.simulation.revision;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,kind=kind.toStdString()]{worker->advanceTo(token,revision,kind);},Qt::QueuedConnection);
}
void CampaignController::acknowledgeManagement() {
    if (!snapshot_ || !snapshot_->checkpoint.world.simulation.management) return;
    for (const auto& n:snapshot_->checkpoint.world.simulation.management->notices) if (n.required && !n.acknowledged) {submit(core::AcknowledgeManagementNotice{n.id});return;}
}
void CampaignController::setAutoPauseProjects(bool value) {auto settings=settings_;settings.auto_pause_projects=value;changeSettings(settings);}
void CampaignController::setAutoPauseEvaluations(bool value) {auto settings=settings_;settings.auto_pause_evaluations=value;changeSettings(settings);}
void CampaignController::setAutoPauseAlerts(bool value) {auto settings=settings_;settings.auto_pause_alerts=value;changeSettings(settings);}
void CampaignController::acknowledgeWarning() {
    if (!needsWarning()) return;
    submit(core::AcknowledgeLiquidityWarning{*core::liquidity_report(*snapshot_->checkpoint.world.simulation.liquidity).pending_warning});
}
void CampaignController::resolveCoreChoice(bool accept) {
    if (snapshot_ && snapshot_->checkpoint.world.simulation.pending_choice)
        submit(core::ResolveOpeningChoice{*snapshot_->checkpoint.world.simulation.pending_choice,accept});
}
void CampaignController::projectCancel(QString id) { if (!id.isEmpty()) submit(core::CancelProject{id.toStdString()}); }
void CampaignController::saveTo(QUrl url) {
    try {
        const auto path = local_path(url);
        if (!beginOperation()) return;
        QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,path] { worker->saveTo(token,path); },Qt::QueuedConnection);
    } catch (const std::exception& error) { setNotice(QString::fromUtf8(error.what())); }
}
void CampaignController::loadFrom(QUrl url) {
    try {
        const auto path = local_path(url);
        if (!beginOperation()) return;
        QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,path] { worker->loadFrom(token,path); },Qt::QueuedConnection);
    } catch (const std::exception& error) { setNotice(QString::fromUtf8(error.what())); }
}
void CampaignController::changeSettings(persistence::Settings settings) {
    if (settings == settings_ || !beginOperation()) return;
    QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,settings] { worker->changeSettings(token,settings); },Qt::QueuedConnection);
}
void CampaignController::setTextScalePercent(int value) {
    if (value < 100 || value > 200) { setNotice(QStringLiteral("Text scale must be between 100% and 200%.")); return; }
    auto settings = settings_; settings.text_scale_percent = static_cast<std::uint32_t>(value); changeSettings(settings);
}
void CampaignController::setCompactManagement(bool value) { auto settings = settings_; settings.compact_management = value; changeSettings(settings); }
void CampaignController::setReducedMotion(bool value) { auto settings = settings_; settings.reduced_motion = value; changeSettings(settings); }
void CampaignController::setAutosaveEnabled(bool value) { auto settings = settings_; settings.autosave_enabled = value; changeSettings(settings); }
}
