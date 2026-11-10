#include "campaign_controller.hpp"
#include <QMetaProperty>
#include <QUuid>
#include <pgp/runtime/authority.hpp>
#include "flow_presentation.hpp"

#include <pgp/core/integer_codec.hpp>
#include <pgp/session/campaign.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <set>
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
    std::set<std::string_view> lab_projects,protected_projects;
    if(state.lab) {
        for(const auto& r:state.lab->research) lab_projects.insert(r.project_id);
        for(const auto& r:state.lab->models) if(!r.project_id.empty()) lab_projects.insert(r.project_id);
        for(const auto& r:state.lab->evaluations) lab_projects.insert(r.project_id);
        for(const auto& r:state.lab->generated) lab_projects.insert(r.project_id);
        for(const auto& r:state.lab->acquisitions) lab_projects.insert(r.project_id);
    }
    if(state.deployments && state.deployments->risk)
        for(const auto& response:state.deployments->risk->responses) lab_projects.insert(response.project_id);
    if(state.management && state.management->governance) for(const auto& d:state.management->governance->disputes)
        if(!d.resolved_on && !d.review_project.empty()) protected_projects.insert(d.review_project);
    for (const auto& project : state.capacity->projects) {
        QString kind;
        switch (project.kind) {
        case core::ProjectKind::training: kind = QStringLiteral("Training"); break;
        case core::ProjectKind::research: kind = QStringLiteral("Research"); break;
        case core::ProjectKind::management: kind = QStringLiteral("HQ operations management"); break;
        case core::ProjectKind::evaluation: kind = QStringLiteral("Evaluation"); break;
        case core::ProjectKind::serving: kind = QStringLiteral("Serving"); break;
        case core::ProjectKind::replication: kind = QStringLiteral("Replica synchronization"); break;
        case core::ProjectKind::data_acquisition: kind = QStringLiteral("Collect and review data"); break;
        case core::ProjectKind::data_generation: kind = QStringLiteral("Generate and verify data"); break;
        case core::ProjectKind::incident_response: kind = QStringLiteral("Deployment incident response"); break;
        }
        QString status;
        switch (project.status) {
        case core::ProjectStatus::active: status = QStringLiteral("Active"); break;
        case core::ProjectStatus::completed: status = QStringLiteral("Completed"); break;
        case core::ProjectStatus::cancelled: status = QStringLiteral("Cancelled"); break;
        case core::ProjectStatus::paused: status = QStringLiteral("Paused"); break;
        }
        const auto progress = project.kind==core::ProjectKind::replication ?
            QStringLiteral("%1 CU synchronized").arg(cu(project.completed_work.value())):project.kind == core::ProjectKind::serving ?
            QStringLiteral("%1 useful tasks delivered").arg(integer(project.useful_tasks)) :
            QStringLiteral("%1 / %2 CU completed").arg(cu(project.completed_work.value()),cu(project.total_work.value()));
        const auto detail = QStringLiteral("%1\n%2\nAllocation %3 CU/day · %4 staff\n%5\nSupport: %6/month\nStarted %7")
            .arg(text(project.cause),kind,cu(project.requested.value()),QString::number(project.staff),progress,
                 money(project.monthly_support_cost),text(core::iso_date(project.starts_on)));
        auto lifecycle=detail;
        if(project.status==core::ProjectStatus::paused) lifecycle+=QStringLiteral("\nPaused: compute and staff are released. Resume requests the same source and allocation. Existing bills and spent costs remain. A model checkpoint can continue on another source.");
        rows.push_back({text(project.id),readable(project.id),kind + QStringLiteral(" · ") + progress,lifecycle,status,
            project.kind!=core::ProjectKind::management && (project.status == core::ProjectStatus::active || project.status==core::ProjectStatus::paused),
            {{"canPause",lab_projects.contains(project.id) && !protected_projects.contains(project.id) && project.pauses.size()<128 && project.status==core::ProjectStatus::active},
             {"canResume",lab_projects.contains(project.id) && project.status==core::ProjectStatus::paused}}});
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
    persistence::LocalPaths paths,persistence::Settings initial_settings,QObject* parent,bool welcome,bool continuous)
    : QObject(parent),program_(program),management_(new ManagementController(this)),infrastructure_(new InfrastructureController(this)),
      civic_(new CivicController(this)),society_(new SocietyController(this)),institutions_(new InstitutionsController(this)),
      frontier_(new FrontierController(this)),
      leadership_(new SuccessionController(this)),
      objectives_(new ObjectivesController(this)),
      founding_(new FoundingController(program_.collection,paths.saves_directory.parent_path()/"founding-draft.json",this)),
      world_(new WorldController(paths.saves_directory.parent_path()/"world-navigation",this)),
      welcome_(welcome),projects_(new StableListModel(this)),inbox_(new StableListModel(this,true)),settings_(initial_settings),
      save_folder_(QUrl::fromLocalFile(QString::fromStdU16String(paths.saves_directory.u16string()))) {
    const auto nameModels=[](QObject* owner,const QString& prefix) {
        const auto* meta=owner->metaObject();
        for(int index=meta->propertyOffset();index<meta->propertyCount();++index) {
            const auto property=meta->property(index);
            if(property.metaType()!=QMetaType::fromType<StableListModel*>()) continue;
            if(auto* model=property.read(owner).value<StableListModel*>()) model->setObjectName(prefix+QLatin1Char('/')+QString::fromLatin1(property.name()));
        }
    };
    nameModels(this,QStringLiteral("campaign"));nameModels(management_,QStringLiteral("management"));
    nameModels(infrastructure_,QStringLiteral("infrastructure"));nameModels(civic_,QStringLiteral("civic"));
    nameModels(society_,QStringLiteral("society"));nameModels(institutions_,QStringLiteral("institutions"));
    nameModels(frontier_,QStringLiteral("frontier"));nameModels(leadership_,QStringLiteral("leadership"));nameModels(objectives_,QStringLiteral("objectives"));
    connect(world_,&WorldController::actionRequested,this,&CampaignController::submit);
    connect(world_,&WorldController::noticeRaised,this,&CampaignController::setNotice);
    connect(management_,&ManagementController::actionRequested,this,&CampaignController::submit);
    connect(infrastructure_,&InfrastructureController::actionRequested,this,&CampaignController::submit);
    connect(infrastructure_,&InfrastructureController::noticeRaised,this,&CampaignController::setNotice);
    connect(civic_, &CivicController::actionRequested, this, &CampaignController::submit);
    connect(civic_, &CivicController::noticeRaised, this, &CampaignController::setNotice);
    connect(society_, &SocietyController::actionRequested, this, &CampaignController::submit);
    connect(society_, &SocietyController::noticeRaised, this, &CampaignController::setNotice);
    connect(institutions_, &InstitutionsController::actionRequested, this, &CampaignController::submit);
    connect(institutions_, &InstitutionsController::noticeRaised, this, &CampaignController::setNotice);
    connect(frontier_, &FrontierController::actionRequested, this, &CampaignController::submit);
    connect(frontier_, &FrontierController::noticeRaised, this, &CampaignController::setNotice);
    connect(leadership_,&SuccessionController::actionRequested,this,&CampaignController::submit);
    connect(leadership_,&SuccessionController::noticeRaised,this,&CampaignController::setNotice);
    connect(objectives_,&ObjectivesController::actionRequested,this,&CampaignController::submit);
    connect(objectives_,&ObjectivesController::noticeRaised,this,&CampaignController::setNotice);
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
    connect(infrastructure_,&InfrastructureController::wholesaleRequested,this,[this](QString plan,QString source,QString buyer) {
        if (!paused() || !beginOperation()) return;
        const auto revision=snapshot_->checkpoint.world.simulation.revision;
        QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,revision,
            plan=plan.toStdString(),source=source.toStdString(),buyer=buyer.toStdString()] {
            worker->signWholesale(token,revision,plan,source,buyer);
        },Qt::QueuedConnection);
    });
    (void)persistence::encode_settings(settings_);
    worker_ = new CampaignWorker(std::move(program),std::move(checkpoint),std::move(paths),settings_,nullptr,continuous);
    worker_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater);
    connect(worker_,&CampaignWorker::snapshotReady,this,&CampaignController::acceptSnapshot);
    connect(worker_,&CampaignWorker::noticeRaised,this,&CampaignController::setNotice);
    connect(worker_,&CampaignWorker::originalContentReviewReady,this,[this](QString review){original_content_review_=std::move(review);emit stateChanged();});
    connect(worker_,&CampaignWorker::operationCompleted,this,[this](qulonglong token,bool accepted,QString explanation) {
        if (token != static_cast<qulonglong>(token_)) return;
        if (accepted && starting_founder_) founding_->reset();
        starting_founder_ = false;
        busy_ = false; emit stateChanged();
        if(resetting_local_data_) {
            resetting_local_data_=false;
            if(accepted) {
                founding_->reset();welcome_=true;original_content_review_.clear();
                emit stateChanged();emit developmentResetFinished();
            }
        }
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
    if(snapshot->content || replace) active_content_=snapshot->content;
    const auto& content=activeProgram();
    auto projects = project_rows(snapshot->checkpoint);
    world_->update(snapshot,content,replace);
    management_->update(snapshot,content,replace);
    infrastructure_->update(snapshot,content,replace);
    civic_->update(snapshot, replace);
    society_->update(snapshot, replace);
    institutions_->update(snapshot, replace);
    frontier_->update(snapshot, replace);
    leadership_->update(snapshot,replace);
    objectives_->update(snapshot,content,replace);
    auto inbox = inbox_rows(snapshot->checkpoint,content.collection,*management_);
    if ((!first && replace) || snapshot->current_campaign) welcome_=false;
    if (replace) { projects_->beginCampaign(std::move(projects)); inbox_->beginCampaign(std::move(inbox)); }
    else { projects_->setRows(std::move(projects)); inbox_->setRows(std::move(inbox)); }
    const bool changed_settings = settings_ != snapshot->settings;
    settings_ = snapshot->settings; snapshot_ = std::move(snapshot);
    if (first) busy_ = false;
    if (changed_settings) emit settingsChanged();
    emit stateChanged(); emit snapshotChanged(); emit selectionChanged();
}
bool CampaignController::canStartNew() const {
    return ready() && !busy_ && !storageBlocked() && (!currentCampaign() ||
        runtime::objective_time_stopped(snapshot_->checkpoint.world) ||
        snapshot_->checkpoint.world.simulation.phase==core::DayPhase::closed ||
        snapshot_->checkpoint.world.simulation.phase==core::DayPhase::finished);
}
void CampaignController::retryPersistence() {
    if(busy_ || !ready()) return;
    busy_=true;++token_;emit stateChanged();
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_]{worker->retryPersistence(token);},Qt::QueuedConnection);
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
                .arg(setup.catchup_cash.value()>0?QStringLiteral(" · rival catch-up equity"):QString{});
            std::uint64_t demand=0;for (const auto& segment:scenario->market->segments) demand+=segment.daily_tasks;
            detail=QStringLiteral("Up to three US and three international startups enter initially; later startups enter on the authored schedule. Rivals pay for work and use public information. Shared demand is %1 tasks/day. %2")
                .arg(demand).arg(setup.catchup_cash.value()>0?QStringLiteral("Each rival may take one disclosed %1 equity round for %2.%3% ownership; no recurring catch-up subsidy.").arg(money(setup.catchup_cash)).arg(setup.catchup_acquired.value()/100).arg(setup.catchup_acquired.value()%100,2,10,QLatin1Char('0')):QStringLiteral("No catch-up funding is available."));
        }
        if (const auto* frontier=content::frontier_catalog(program_.collection,*scenario)) {
            label=text(frontier->name);
            detail=text(frontier->assumptions)+QStringLiteral(" Campaign dates %1 through %2.")
                .arg(text(core::iso_date(scenario->starts_on)),text(core::iso_date(core::OrdinalDay{scenario->ends_before.value()-1})));
        }
        QVariantMap choice{{QStringLiteral("id"),text(id)},{QStringLiteral("label"),label},{QStringLiteral("detail"),detail}};
        choice.insert(QStringLiteral("seed"),integer(scenario->seed));
        choice.insert(QStringLiteral("opening"),QStringLiteral("Opening cash %1. Team %2. Leased compute %3 milli-CU/day.")
            .arg(money(scenario->cash)).arg(scenario->team_people).arg(scenario->leased_capacity.value()));
        QVariantList backgrounds,charters;
        for(const auto& b:scenario->backgrounds) {
            QString detail=text(b.description)+QStringLiteral("\nOpening cash change %1. Staff change %2. Payroll change %3/month. Independent compute %4 milli-CU/day at %5/month. Opening commercial contacts delayed %6 days.")
                .arg(money(b.cash_adjustment)).arg(b.staff_adjustment).arg(money(b.monthly_payroll_adjustment)).arg(b.independent_capacity.value()).arg(money(b.monthly_compute_cost)).arg(b.commercial_delay.value());
            for(const auto& offer:b.contacts) detail+=QStringLiteral("\nContact offer: %1. Available %2 through %3 exclusive. Setup %4; %5/month; %6-day term; %7 milli-CU/day; %8 tasks/day. Review eligibility and accept the offer in Office.")
                .arg(text(offer.name),text(core::iso_date(offer.available_from)),text(core::iso_date(offer.available_before)),money(offer.setup_cost),money(offer.monthly_amount)).arg(offer.term.value()).arg(offer.capacity.value()).arg(offer.tasks_per_day);
            backgrounds.push_back(QVariantMap{{"id",text(b.id)},{"label",text(b.name)},{"detail",detail}});
        }
        if(scenario->management && scenario->management->governance) for(const auto& c:scenario->management->governance->charters) {
            const auto detail=QStringLiteral("%1\nEmployees expect %2\nBoard floor %3 cash; dilution ceiling %4.%5%. Employee program %6/month. Existing stricter board positions still apply. Amendments require a board majority.")
                .arg(text(c.priorities),text(c.employee_expectations),money(c.cash_floor)).arg(c.maximum_dilution.value()/100).arg(c.maximum_dilution.value()%100,2,10,QLatin1Char('0')).arg(money(c.monthly_employee_budget));
            charters.push_back(QVariantMap{{"id",text(c.id)},{"label",text(c.name)},{"detail",detail}});
        }
        choice.insert("backgrounds",backgrounds);choice.insert("charters",charters);
        QVariantList difficulties;
        for(const auto& d:scenario->difficulties) {
            auto detail=text(d.description)+QStringLiteral("\nStarting capital %1% after the background adjustment. Player financing proceeds %2% for the same equity, interest rate and term. Amounts round down once to cents. Training forecast intervals add %3 basis points of uncertainty on each side; held-out evidence and model outcomes use the same rules.")
                .arg(d.starting_cash_percent).arg(d.financing_percent).arg(d.forecast_padding.value());
            if(scenario->competition) detail+=QStringLiteral("\nOperating rivals review at most one feasible strategic action every %1 days. Entry, heavyweight warnings and pressure reviews keep their authored dates. Rivals retain their ordinary cash, staff, compute and bounded public information.").arg(d.rival_review_interval.value());
            else detail+=QStringLiteral("\nThis introduction uses scripted rivals: their initial action delays are %1% of the authored delay, rounded up to whole days. Warning lead times and actual funded work keep their ordinary rules.").arg(d.scripted_delay_percent);
            difficulties.push_back(QVariantMap{{"id",text(d.id)},{"label",text(d.name)},{"detail",detail}});
        }
        choice.insert("difficulties",difficulties);
        if(scenario->objectives) {
            const auto& opening=scenario->objectives->opening;
            const bool asi=content::frontier_catalog(program_.collection,*scenario)->asi_route;
            QStringList secondary;for(auto kind:opening.secondary) secondary.push_back(text(core::objective_key(kind)));
            choice.insert(QStringLiteral("objectives"),objectiveOptions(asi));
            choice.insert(QStringLiteral("primary"),text(core::objective_key(opening.primary)));
            choice.insert(QStringLiteral("secondary"),secondary);
            if(!asi) choice.insert(QStringLiteral("objectiveNote"),QStringLiteral("This plateau has no feasible ASI route. The other five objectives are available."));
        }
        result.push_back(std::move(choice));
    }
    return result;
}
QString CampaignController::setupPreview(QString scenario,QString background,QString charter,QString difficulty) const {
    try {
        const auto* definition=program_.collection.find(scenario.toStdString());
        const auto* s=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
        if(!s) return {};
        const auto opening=content::scenario_opening(*s,background.toStdString(),"company.player",difficulty.toStdString());
        core::Cents monthly{0};core::MilliCUPerDay compute{0};
        for(const auto& [id,amount]:opening.monthly_commitments) { (void)id;monthly=core::checked_unit_add(monthly,amount); }
        for(const auto& source:opening.capacity_sources) compute=core::checked_unit_add(compute,source.effective);
        if(!charter.isEmpty()) {
            if(!s->management || !s->management->governance) return {};
            const auto& rows=s->management->governance->charters;
            const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.id==charter.toStdString();});
            if(it==rows.end()) return {};
            monthly=core::checked_unit_add(monthly,it->monthly_employee_budget);
        }
        return QStringLiteral("Opening cash %1. Team %2. Leased compute %3 milli-CU/day. Total starting commitments %4/month, including the selected employee program. Offer setup, future hires and project costs are additional. Backgrounds change this opening package; later research, hiring and product choices remain available.")
            .arg(money(opening.cash)).arg(opening.team_people).arg(compute.value()).arg(money(monthly));
    } catch(const std::exception& error) {return QString::fromUtf8(error.what());}
}
QString CampaignController::headquartersText() const {
    if (!snapshot_) return {};
    const auto& id=snapshot_->checkpoint.world.state_id;
    return id=="state.us_ca"?QStringLiteral("California"):id=="state.us_tx"?QStringLiteral("Texas"):QStringLiteral("Washington");
}
QString CampaignController::companyName() const {
    return snapshot_ && snapshot_->checkpoint.world.identity ? text(snapshot_->checkpoint.world.identity->company_name) : QStringLiteral("Your company");
}
QString CampaignController::identitySummary() const {
    if (!snapshot_) return {};
    const auto& world = snapshot_->checkpoint.world;
    auto summary = QStringLiteral("World seed %1").arg(integer(world.root_seed));
    if (world.identity) {
        summary = QStringLiteral("Founded by %1%2 · %3").arg(text(world.identity->founder_name),
            world.identity->founder_pronouns.empty() ? QString{} : QStringLiteral(" (%1)").arg(text(world.identity->founder_pronouns)), summary);
    }
    if(!world.background.empty()) {
        if(const auto* d=activeProgram().collection.find(world.scenario_id)) if(const auto* s=std::get_if<content::ScenarioDefinition>(&d->data))
            for(const auto& b:s->backgrounds) if(b.id==world.background) summary+=QStringLiteral(" · %1").arg(text(b.name));
    }
    if(!world.difficulty.empty()) {
        if(const auto* def=activeProgram().collection.find(world.scenario_id)) if(const auto* s=std::get_if<content::ScenarioDefinition>(&def->data))
            summary+=QStringLiteral(" · %1").arg(text(content::scenario_difficulty(*s,world.difficulty)->name));
    }
    if(const auto* charter=core::active_charter(world.simulation)) summary+=QStringLiteral(" · %1").arg(text(charter->name));
    return summary;
}
QVariantMap CampaignController::generateCampaignNames() const {
    const auto names = runtime::generate_campaign_identity(QRandomGenerator::global()->generate64());
    return {{QStringLiteral("company"),text(names.company_name)},{QStringLiteral("founder"),text(names.founder_name)}};
}
QString CampaignController::generateWorldSeed() const {
    return integer(QRandomGenerator::global()->generate64());
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
    newCampaignWithObjectives(std::move(hq),stories,std::move(scenario),{},{});
}
void CampaignController::newCampaignWithObjectives(QString hq,bool stories,QString scenario,QString primary,QStringList secondary) {
    startCampaign(std::move(hq),stories,std::move(scenario),std::move(primary),std::move(secondary),{});
}
void CampaignController::newCampaignSetup(QString hq,bool stories,QString scenario,QString primary,QStringList secondary,
    QString company,QString founder,QString pronouns,QString seed,QString background,QString charter,QString difficulty) {
    runtime::ScenarioOptions options;
    options.background=background.toStdString();options.charter=charter.toStdString();options.difficulty=difficulty.toStdString();
    try {
        options.identity = runtime::CampaignIdentity{company.trimmed().toStdString(),founder.trimmed().toStdString(),pronouns.trimmed().toStdString()};
        runtime::validate_campaign_identity(*options.identity);
        options.seed = runtime::parse_world_seed(seed.trimmed().toStdString());
    } catch (const std::exception& error) { setNotice(QString::fromUtf8(error.what())); return; }
    startCampaign(std::move(hq),stories,std::move(scenario),std::move(primary),std::move(secondary),std::move(options));
}
void CampaignController::startCampaign(QString hq,bool stories,QString scenario,QString primary,QStringList secondary,
    runtime::ScenarioOptions options) {
    std::optional<core::ObjectiveSelection> selection;
    try {
        if(!primary.isEmpty()) selection=parseObjectiveSelection(primary,secondary);
    } catch(const std::exception& error) {setNotice(QString::fromUtf8(error.what()));return;}
    if (!beginOperation()) return;
    if(options.identity && options.identity->instance_id.empty())
        options.identity->instance_id=QUuid::createUuid().toString(QUuid::Id128).toStdString();
    starting_founder_ = options.founding.has_value();
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,hq=hq.toStdString(),stories,scenario=scenario.toStdString(),selection,options=std::move(options)]{
        worker->newCampaign(token,hq,stories,scenario,selection,options);
    },Qt::QueuedConnection);
}
void CampaignController::openWelcome() {
    if(currentCampaign() && !canStartNew()) {setNotice(tr("Your company is still in progress. Plan a handover in Leadership."));return;}
    if (!management_->available() || busy_) return;
    if (!paused()) submit(core::SetPaused{true});
    clearNotice();
    founding_->reset();
    welcome_=true;emit stateChanged();
}
void CampaignController::startFounderCampaign() {
    try {
        if(const auto error=founding_->signingError();!error.isEmpty()) throw std::invalid_argument(error.toStdString());
        const auto form=founding_->form();
        runtime::ScenarioOptions options;
        options.founding=founding_->selection();
        const auto& selection=*options.founding;
        const auto* definition=program_.collection.find("scenario.founder_campaign");
        if(!definition) throw std::invalid_argument("The campaign introduction is missing from this content collection.");
        const auto& scenario=std::get<content::ScenarioDefinition>(definition->data);
        if(!scenario.founding) throw std::invalid_argument("This content collection has no founding choices.");
        (void)content::founding_terms(*scenario.founding,selection);
        options.identity=runtime::CampaignIdentity{form.value("company").toString().trimmed().toStdString(),
            selection.first_name+" "+selection.last_name,form.value("pronouns").toString().trimmed().toStdString()};
        runtime::validate_campaign_identity(*options.identity);
        options.seed=runtime::parse_world_seed(form.value("seed").toString().trimmed().toStdString());
        options.difficulty={};
        startCampaign(form.value("headquarters").toString(),true,QStringLiteral("scenario.founder_campaign"),
            form.value("mission").toString(),{},std::move(options));
    } catch(const std::exception& e) {setNotice(QString::fromUtf8(e.what()));}
}
void CampaignController::setShowGuide(bool value) { auto settings=settings_;settings.show_guide=value;changeSettings(settings); }
QString CampaignController::date() const { return snapshot_ ? text(core::iso_date(runtime::campaign_day(snapshot_->checkpoint.world))) : QStringLiteral("Preparing campaign…"); }
QString CampaignController::revision() const { return snapshot_ ? integer(snapshot_->checkpoint.world.simulation.revision) : QString{}; }
QString CampaignController::cashText() const { return snapshot_ ? money(snapshot_->checkpoint.world.simulation.cash) : QStringLiteral("—"); }
QString CampaignController::availableText() const { return snapshot_ ? money(core::discretionary_cash(snapshot_->checkpoint.world.simulation)) : QStringLiteral("—"); }
QString CampaignController::phaseText() const {
    if (!snapshot_) return QStringLiteral("Preparing");
    if(runtime::observing_world(snapshot_->checkpoint.world)) return paused()?QStringLiteral("Observer paused"):QStringLiteral("Observing");
    if(runtime::objective_time_stopped(snapshot_->checkpoint.world)) return QStringLiteral("Campaign ended");
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
bool CampaignController::paused() const { return !snapshot_ || snapshot_->driver_halted || runtime::campaign_paused(snapshot_->checkpoint.world); }
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
    const auto& world=snapshot_->checkpoint.world;
    if(runtime::objective_time_stopped(world) || runtime::observing_world(world)) return QStringLiteral("none");
    if(runtime::required_objective_review(world)) return QStringLiteral("objective");
    if (snapshot_->checkpoint.world.simulation.pending_choice) return QStringLiteral("core");
    if (waitingPresentation()) return QStringLiteral("content");
    if (needsWarning()) return QStringLiteral("liquidity");
    if (core::required_deployment_incident(snapshot_->checkpoint.world.simulation)) return QStringLiteral("deploymentIncident");
    if (core::required_management_notice(snapshot_->checkpoint.world.simulation)) return QStringLiteral("management");
    if (core::required_site_alert(snapshot_->checkpoint.world.simulation)) return QStringLiteral("site");
    if (core::required_succession_notice(snapshot_->checkpoint.world.simulation)) return QStringLiteral("leadership");
    if (runtime::required_public_case(snapshot_->checkpoint.world)) return QStringLiteral("institution");
    if (core::required_governance_notice(snapshot_->checkpoint.world.simulation)) {
        const auto& disputes=snapshot_->checkpoint.world.simulation.management->governance->disputes;
        const auto row=std::ranges::find_if(disputes,[](const auto& d){return !d.acknowledged;});
        return institutions_->available() && row->kind=="publication"?QStringLiteral("executive"):QStringLiteral("governance");
    }
    return QStringLiteral("none");
}
QString CampaignController::requiredTitle() const {
    const auto kind = requiredKind();
    if(kind==QStringLiteral("objective")) return QStringLiteral("An objective is now out of reach");
    if(kind==QStringLiteral("leadership")) return QStringLiteral("Leadership planning and continuity");
    if (kind == QStringLiteral("core")) return QStringLiteral("Delivery decision");
    if (kind == QStringLiteral("content")) return presentation_heading(*waitingPresentation(),activeProgram().collection);
    if (kind == QStringLiteral("liquidity")) return QStringLiteral("Unpaid obligations need a response");
    if (kind == QStringLiteral("deploymentIncident")) {
        const auto* alert=core::required_deployment_incident(snapshot_->checkpoint.world.simulation);
        return alert->recovery?tr("Service recovery completed"):tr("Deployment incident needs attention");
    }
    if (kind == QStringLiteral("management")) {
        const auto& notices=snapshot_->checkpoint.world.simulation.management->notices;
        const auto next=std::ranges::find_if(notices,[](const auto& row) { return row.required && !row.acknowledged; });
        if (next->kind==core::service_monitoring_limit_notice) return tr("Automatic service warnings paused");
        return next->kind==core::service_workload_notice?tr("Agent service reliability dropped"):tr("Temporary compute setback");
    }
    if (kind==QStringLiteral("site")) return QStringLiteral("Owned site disruption");
    if (kind==QStringLiteral("institution")) return QStringLiteral("Public case needs attention");
    if (kind==QStringLiteral("executive") || kind==QStringLiteral("governance")) return QStringLiteral("Corporate response needs attention");
    return {};
}
QString CampaignController::requiredText() const {
    const auto kind = requiredKind();
    if(kind==QStringLiteral("objective")) return objectives_->breachText();
    if(kind==QStringLiteral("leadership")) return leadership_->noticeText();
    if(kind==QStringLiteral("deploymentIncident")) {
        const auto& state=snapshot_->checkpoint.world.simulation;
        return text(core::deployment_incident_description(state,*core::required_deployment_incident(state)))+
            tr("\n\nReview the affected service in Office → Services. Acknowledgment records that you read this alert; it does not pay compensation or repair the service.");
    }
    if (kind==QStringLiteral("site")) return text(core::required_site_alert(snapshot_->checkpoint.world.simulation)->cause);
    if (kind==QStringLiteral("institution")) return institutions_->requiredCaseText();
    if (kind==QStringLiteral("executive") || kind==QStringLiteral("governance")) {
        const auto& disputes=snapshot_->checkpoint.world.simulation.management->governance->disputes;
        const auto row=std::ranges::find_if(disputes,[](const auto& d){return !d.acknowledged;});
        return management_->corporateDisputes()->row(text(row->id)).value(QStringLiteral("detail")).toString();
    }
    if (kind == QStringLiteral("management")) {
        for (const auto& n:snapshot_->checkpoint.world.simulation.management->notices) if (n.required && !n.acknowledged) return management_->alertDetail(n);
    }
    if (kind == QStringLiteral("core")) return QStringLiteral("A scheduled delivery awaits your decision. Accepting consumes its reserved funds; declining does not refund earlier purchases.");
    if (kind == QStringLiteral("content")) {
        const auto* row=waitingPresentation();
        auto result=presentation_detail(snapshot_->checkpoint,*row,&activeProgram().collection);
        const auto& s=snapshot_->checkpoint.world.simulation;
        if (s.business_offers && (row->flow_id=="slice.supplier_offer" || row->flow_id=="slice.recovery_opportunity"))
            for (const auto& offer:s.business_offers->catalog)
                if ((row->flow_id=="slice.supplier_offer" && offer.id=="offer.supplier_small") ||
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
QVariantList CampaignController::requiredParticipants() const {
    const auto kind=requiredKind();if(kind=="none") return {};
    const auto* presentation=kind=="content"?waitingPresentation():nullptr;
    return world_->decisionParticipants(kind,presentation?presentation->id:0);
}
QVariantList CampaignController::requiredChoices() const {
    QVariantList result;
    if (const auto* presentation = waitingPresentation())
        for (const auto& choice : presentation->choices)
            result.push_back(QVariantMap{{QStringLiteral("choiceId"),text(choice.id)},{QStringLiteral("label"),choice_label(choice,&activeProgram().collection)}});
    return result;
}
bool CampaignController::canAdvance() const {
    return paused() && canPlay();
}
bool CampaignController::companyEditable() const {
    if(!snapshot_) return false;
    const auto& w=snapshot_->checkpoint.world;
    return !runtime::objective_time_stopped(w) && !runtime::observing_world(w) &&
        w.simulation.phase!=core::DayPhase::closed && w.simulation.phase!=core::DayPhase::finished;
}
bool CampaignController::canPlay() const {
    if (welcome_ || !snapshot_ || busy_ || requiredKind() != QStringLiteral("none")) return false;
    const auto& world=snapshot_->checkpoint.world;
    if(runtime::objective_time_stopped(world)) return false;
    if(runtime::observing_world(world)) return runtime::campaign_day(world)<runtime::campaign_calendar_limit(world);
    const auto& state = world.simulation;
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
bool CampaignController::beginOperation(bool recovery) {
    if(storageBlocked() && !recovery) {setNotice(tr("Automatic saving needs recovery. Retry before continuing."));return false;}
    if (busy_ || !snapshot_) return false;
    if (token_ == std::numeric_limits<std::uint64_t>::max()) { setNotice(QStringLiteral("The command identifier range is exhausted.")); return false; }
    ++token_; busy_ = true; emit stateChanged(); return true;
}
void CampaignController::submit(core::CommandAction action) {
    if(storageBlocked()) {setNotice(tr("Progress needs recovery. Retry automatic saving before making another decision."));return;}
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
void CampaignController::acknowledgeDeploymentIncident() {
    if (!snapshot_) return;
    if (const auto* alert=core::required_deployment_incident(snapshot_->checkpoint.world.simulation))
        submit(core::AcknowledgeDeploymentIncident{alert->id});
}
void CampaignController::acknowledgeSiteAlert() {
    if (snapshot_) if (const auto* alert=core::required_site_alert(snapshot_->checkpoint.world.simulation))
        submit(core::AcknowledgeSiteAlert{alert->id});
}
void CampaignController::acknowledgeInstitutionalNotice() {
    if(!snapshot_) return;
    const auto& world=snapshot_->checkpoint.world;
    if(world.authority) for(const auto& row:world.authority->cases) if(!row.acknowledged) {
        submit(core::AcknowledgePublicCase{row.id});
        return;
    }
    if(core::required_governance_notice(world.simulation))
        for(const auto& row:world.simulation.management->governance->disputes) if(!row.acknowledged) {
            submit(core::RespondCorporateDispute{row.id,false});
            return;
        }
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
void CampaignController::projectPause(QString id) {if(!id.isEmpty()) submit(core::PauseProject{id.toStdString()});}
void CampaignController::projectResume(QString id) {if(!id.isEmpty()) submit(core::ResumeProject{id.toStdString()});}
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
        if (!beginOperation(true)) return;
        QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,path] { worker->loadFrom(token,path); },Qt::QueuedConnection);
    } catch (const std::exception& error) { setNotice(QString::fromUtf8(error.what())); }
}
void CampaignController::changeSettings(persistence::Settings settings) {
    if (settings == settings_ || !beginOperation(true)) return;
    QMetaObject::invokeMethod(worker_,[worker = worker_,token = token_,settings] { worker->changeSettings(token,settings); },Qt::QueuedConnection);
}
void CampaignController::setTextScalePercent(int value) {
    if (value < 100 || value > 200) { setNotice(QStringLiteral("Text scale must be between 100% and 200%.")); return; }
    auto settings = settings_; settings.text_scale_percent = static_cast<std::uint32_t>(value); changeSettings(settings);
}
void CampaignController::setCompactManagement(bool value) { auto settings = settings_; settings.compact_management = value; changeSettings(settings); }
void CampaignController::setReducedMotion(bool value) { auto settings = settings_; settings.reduced_motion = value; changeSettings(settings); }
void CampaignController::setAutosaveEnabled(bool value) { auto settings = settings_; settings.autosave_enabled = value; changeSettings(settings); }
void CampaignController::setMusicEnabled(bool value) { auto settings = settings_; settings.music_enabled = value; changeSettings(settings); }
void CampaignController::setMusicVolume(int value) {
    if (value < 0 || value > 100) return;
    auto settings = settings_;
    settings.volume_percent = static_cast<std::uint32_t>(value);
    changeSettings(settings);
}
void CampaignController::applyPresentationSettings(bool full_screen,int width,int height,bool music,
    int music_volume,int effects_volume,int text_scale,bool reduced_motion,bool motion_background) {
    if (width < 800 || width > 7680 || height < 600 || height > 4320 || music_volume < 0 || music_volume > 100 ||
        effects_volume < 0 || effects_volume > 100 || text_scale < 100 || text_scale > 200) {
        setNotice(QStringLiteral("Choose a supported window size, volume and text size.")); return;
    }
    auto settings=settings_;
    settings.full_screen=full_screen;
    settings.window_width=static_cast<std::uint32_t>(width);
    settings.window_height=static_cast<std::uint32_t>(height);
    settings.music_enabled=music;
    settings.volume_percent=static_cast<std::uint32_t>(music_volume);
    settings.effects_volume_percent=static_cast<std::uint32_t>(effects_volume);
    settings.text_scale_percent=static_cast<std::uint32_t>(text_scale);
    settings.reduced_motion=reduced_motion;
    settings.motion_background=motion_background;
    changeSettings(settings);
}
}
