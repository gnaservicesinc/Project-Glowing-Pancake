#include "frontier_controller.hpp"
#include "flow_presentation.hpp"
#include "lab_inputs.hpp"
#include <QStringList>
#include <algorithm>

namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view value) { return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size())); }
QString date(OrdinalDay value) { return text(iso_date(value)); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString points(std::int64_t value) { return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0')); }
QString money(Cents value) {
    auto whole=QString::number(value.value()/100);
    for (auto at=whole.size()-3;at>0;at-=3) whole.insert(at,u',');
    return QStringLiteral("$%1.%2").arg(whole).arg(value.value()%100,2,10,QLatin1Char('0'));
}
template<class Rows> const auto* named(const Rows& rows,std::string_view id) {
    using Row=typename Rows::value_type;
    const auto at=std::ranges::find(rows,id,&Row::id);
    return at==rows.end()?nullptr:&*at;
}
QVariantMap option(QString id,QString label) {return {{"id",std::move(id)},{"label",std::move(label)}};}
std::vector<std::string> strings(const QStringList& values) {
    std::vector<std::string> result;
    for (const auto& value:values) result.push_back(value.toStdString());
    return result;
}
FrontierBudget budget(const QVariantMap& fields,std::optional<std::uint32_t> exact_runs={}) {
    const auto value=[&](const char* name){return fields.value(QString::fromLatin1(name)).toString();};
    return {lab_inputs::allocation(value("source"),value("rate"),value("staff")),lab_inputs::policy_money(value("limit")),
        lab_inputs::policy_money(value("reserve")),Days{lab_inputs::integer(value("interval"),1096)},
        exact_runs?*exact_runs:static_cast<std::uint32_t>(lab_inputs::integer(value("runs"),frontier_policy_run_limit))};
}
QString budget_text(const FrontierBudget& terms) {
    return QStringLiteral("Reserve up to %1 CU/day and %2 staff on %3 per job. Spending limit %4 and cash floor %5 in 2014 dollars. Check again after at least %6 days.")
        .arg(cu(terms.allocation.daily_compute.value()),number(terms.allocation.staff),text(terms.allocation.source_id),
            money(terms.limit),money(terms.reserve),QString::number(terms.interval.value()));
}
QString trial_status(const SimulationState& company,const FrontierTrial& trial) {
    if (trial.result) return trial.result->valid?QStringLiteral("Recorded"):QStringLiteral("Insufficient qualifying work");
    const auto* project=named(company.capacity->projects,trial.project);
    if (project && project->status==ProjectStatus::cancelled) return QStringLiteral("Cancelled");
    return project?QStringLiteral("%1 of %2 CU").arg(cu(project->completed_work.value()),cu(project->total_work.value())):
        QStringLiteral("Pending");
}
QString protocol_name(const FrontierCatalog& catalog,const FrontierTrial& trial) {
    if (trial.kind==FrontierTrialKind::assessment) return text(named(catalog.panels,trial.protocol)->name);
    return text(named(catalog.experiments,trial.protocol)->name)+(trial.kind==FrontierTrialKind::experiment?
        QStringLiteral(" · Prototype"):QStringLiteral(" · Independent replication"));
}
}

FrontierController::FrontierController(QObject* parent)
    :QObject(parent),systems_(this),technology_(this),research_(this),refresh_(this) {
    for (auto* model:{&systems_,&research_,&refresh_})
        connect(model,&StableListModel::selectedIdChanged,this,&FrontierController::clearReview);
}
bool FrontierController::available() const {return snapshot_ && company().frontier.has_value();}
bool FrontierController::editable() const {
    return available() && company().phase==DayPhase::work_ready && (snapshot_->driver_halted || company().paused) &&
        snapshot_->checkpoint.runtime.prepared_day==company().date;
}
QString FrontierController::requirements() const {
    return QStringLiteral("Game AGI needs three independent panels across at least %1 days. The lower bounds must reach %2 in six domains, %3 in transfer, %4 in reliability and %5 in autonomous workflows. Game ASI needs %6 in all six domains, %7 in novel science and three replicated improvements across at least %8 days. Control is reported separately.")
        .arg(frontier_assessment_span.value()).arg(points(frontier_agi_domain),points(frontier_agi_transfer),
            points(frontier_agi_reliability),points(frontier_agi_workflows),points(frontier_asi_domain),points(frontier_asi_novelty))
        .arg(frontier_replication_span.value());
}
QString FrontierController::overview() const {
    if (!available()) return {};
    const auto& actor=future();
    auto description=text(actor.catalog.assumptions);
    if (company().date<actor.catalog.branches_after)
        return description+QStringLiteral("\nThe conditional era begins on %1. The Lab can prepare its prerequisites and fund research policies now.")
            .arg(date(actor.catalog.branches_after));
    const auto& path=future_path(actor);
    QStringList conditions;
    for (std::size_t i=0;i<actor.conditions.size();++i)
        conditions<<QStringLiteral("%1 %2/100").arg(text(future_condition_names[i]),points(actor.conditions[i]));
    return description+QStringLiteral("\n%1 · %2\n%3\nScenario capability ceiling %4/100.")
        .arg(text(path.name),text(path.description),conditions.join(QStringLiteral(" · ")),points(path.ceiling.value()));
}
QVariantList FrontierController::services() const {
    QVariantList result;
    if (!available()) return result;
    for (const auto& service:company().market->products) if (service.available) {
        const auto* project=named(company().capacity->projects,service.capacity_project_id);
        if (project && project->status==ProjectStatus::active) {
            auto row=option(text(service.id),readable(service.id));
            row["refreshable"]=service.channel!=ProductChannel::open_weights;
            result.push_back(row);
        }
    }
    return result;
}
QVariantList FrontierController::refreshServices() const {
    auto result=services();
    result.erase(std::remove_if(result.begin(),result.end(),[](const auto& row){return !row.toMap().value("refreshable").toBool();}),result.end());
    return result;
}
QVariantList FrontierController::protocols() const {
    QVariantList result;
    if (!available()) return result;
    for (const auto& panel:future().catalog.panels)
        result.push_back(option(QStringLiteral("assessment/")+text(panel.id),text(panel.name)));
    if (future().catalog.asi_route) for (const auto& experiment:future().catalog.experiments) {
        result.push_back(option(QStringLiteral("experiment/")+text(experiment.id),QStringLiteral("Prototype · ")+text(experiment.name)));
        result.push_back(option(QStringLiteral("replication/")+text(experiment.id),QStringLiteral("Replicate · ")+text(experiment.name)));
    }
    return result;
}
QVariantList FrontierController::researchTopics() const {
    QVariantList result;
    if (!available()) return result;
    for (const auto& row:company().lab->catalog.research) {
        auto item=option(text(row.id),readable(row.id));
        item["repeatable"]=row.repeat_limit>1;
        result.push_back(item);
    }
    return result;
}
QVariantList FrontierController::repeatableTopics() const {
    auto result=researchTopics();
    result.erase(std::remove_if(result.begin(),result.end(),[](const auto& row){return !row.toMap().value("repeatable").toBool();}),result.end());
    return result;
}
QVariantList FrontierController::licensedData() const {
    QVariantList result;
    if (!available()) return result;
    for (const auto& row:company().lab->catalog.data)
        if (row.commercial_use && std::ranges::any_of(company().lab->licenses,[&](const auto& license){return license.offer_id==row.id;}))
            result.push_back(option(text(row.id),text(row.name)));
    return result;
}
void FrontierController::update(CampaignSnapshotPtr snapshot,bool replace) {
    if (replace || (pending_ && reviewed_revision_!=snapshot->checkpoint.world.simulation.revision)) {
        pending_.reset();
        review_text_.clear();
    }
    snapshot_=std::move(snapshot);
    updateRows(replace);
    emit changed();
}
void FrontierController::updateRows(bool replace) {
    std::vector<CampaignRow> systems,technologies,research,refresh;
    if (available()) {
        const auto& actor=future();
        const auto& s=company();
        for (const auto& system:actor.systems) {
            const auto result=frontier_qualification(actor,system.id);
            QStringList detail;
            for (const auto member:system.deployments) {
                const auto& deployment=s.deployments->records.at(member-1);
                detail<<QStringLiteral("%1 · %2 deployed on %3").arg(readable(deployment.service_id),text(deployment.model_id),date(deployment.deployed_on));
            }
            detail<<QStringLiteral("%1 of 3 independent panels · %2 of 3 independently replicated improvements.")
                .arg(result.assessments).arg(result.improvements);
            if (result.assessments>0) for (std::size_t i=0;i<result.conservative.size();++i)
                detail<<QStringLiteral("%1 · lower bound %2/100").arg(text(frontier_metric_names[i]),points(result.conservative[i]));
            for (const auto& trial:actor.trials) if (trial.system==system.id) {
                auto line=QStringLiteral("%1 · %2 · %3").arg(date(trial.starts_on),protocol_name(actor.catalog,trial),trial_status(s,trial));
                if (trial.result && trial.kind!=FrontierTrialKind::assessment)
                    line+=QStringLiteral(" · verified improvement %1 points").arg(points(trial.result->gain.value()));
                detail<<line;
            }
            auto status=result.asi_on?QStringLiteral("Game ASI · %1").arg(date(*result.asi_on)):
                result.agi_on?QStringLiteral("Game AGI · %1").arg(date(*result.agi_on)):QStringLiteral("Collecting evidence");
            systems.push_back({number(system.id),text(system.name),date(system.registered_on),detail.join(u'\n'),status});
        }
        for (const auto& gate:actor.catalog.gates) {
            const auto blocked=frontier_technology_block(s,gate.kind,gate.target,s.date);
            QStringList thresholds;
            for (std::size_t i=0;i<gate.minimum.size();++i)
                thresholds<<QStringLiteral("%1 at least %2/100").arg(text(future_condition_names[i]),points(gate.minimum[i]));
            technologies.push_back({text(gate.target),readable(gate.target),readable(frontier_key(gate.kind)),
                (blocked.empty()?QStringLiteral("Future conditions are met. Use Lab or Operations to check the remaining requirements."):
                    text(blocked))+QStringLiteral("\n")+thresholds.join(QStringLiteral(" · ")),
                blocked.empty()?QStringLiteral("Conditions met"):QStringLiteral("Locked")});
        }
        for (const auto& program:actor.research_programs) {
            QStringList topics;
            for (const auto& id:program.plan.topics) topics<<readable(id);
            auto detail=budget_text(program.plan.budget);
            for (const auto& run:program.decisions)
                detail+=QStringLiteral("\n%1 · %2").arg(date(run.day),run.project.empty()?QStringLiteral("Topic already completed"):
                    QStringLiteral("Funded %1").arg(money(run.paid)));
            if (!program.waiting.empty()) detail+=QStringLiteral("\n")+text(program.waiting);
            const auto active=frontier_research_active(program);
            research.push_back({number(program.id),topics.join(QStringLiteral(" → ")),program.plan.standing?QStringLiteral("Standing track"):QStringLiteral("Research queue"),
                detail,program.cancelled_on?QStringLiteral("Cancelled"):active?QStringLiteral("Authorized"):QStringLiteral("Starts exhausted"),false,{{"active",active}}});
        }
        for (const auto& program:actor.refresh_programs) {
            auto detail=QStringLiteral("%1 · %2 CU candidate work · %3 minimum improvement points.\n%4")
                .arg(readable(program.plan.recipe),cu(program.plan.work.value()),points(program.plan.improvement.value()),budget_text(program.plan.budget));
            for (std::size_t i=0;i<program.runs.size();++i) {
                const auto& run=program.runs[i];
                detail+=QStringLiteral("\n%1 · %2").arg(date(run.starts_on),run.finished_on?text(run.outcome):
                    run.evaluated_on?QStringLiteral("Independent evaluation and rollout gate"):QStringLiteral("Candidate training"));
            }
            if (!program.waiting.empty()) detail+=QStringLiteral("\n")+text(program.waiting);
            const auto active=frontier_refresh_active(program);
            refresh.push_back({number(program.id),readable(program.plan.service),QStringLiteral("Mature refresh"),detail,
                program.cancelled_on?QStringLiteral("Cancelled"):active?QStringLiteral("Authorized"):QStringLiteral("Finished"),false,{{"active",active}}});
        }
    }
    const auto set=[&](StableListModel& model,std::vector<CampaignRow> rows) {
        if (replace) model.beginCampaign(std::move(rows)); else model.setRows(std::move(rows));
        if (model.selectedId().isEmpty() && model.rowCount()) model.setSelectedId(model.idAt(0));
    };
    set(systems_,std::move(systems));set(technology_,std::move(technologies));set(research_,std::move(research));set(refresh_,std::move(refresh));
}

void FrontierController::stage(CommandAction action,QString description) {
    pending_=std::move(action);
    reviewed_revision_=company().revision;
    review_text_=std::move(description);
    emit changed();
    emit reviewReady();
}
void FrontierController::reviewSystem(QString name,QStringList members) {
    clearReview(); if (!editable()) return;
    try {
        RegisterFrontierSystem action{name.toStdString(),strings(members)};
        auto candidate=company();
        register_frontier_system(candidate,action);
        QStringList labels;
        for (const auto& id:action.services) labels<<readable(id);
        stage(std::move(action),QStringLiteral("Register %1 with %2. Each independent panel follows these exact deployed versions and observes their actual work.")
            .arg(name,labels.join(QStringLiteral(", "))));
    } catch (const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void FrontierController::reviewTrial(QString protocol,QString source,QString rate,QString staff) {
    clearReview(); if (!editable()) return;
    try {
        const auto slash=protocol.indexOf(u'/');
        if (slash<1) throw std::invalid_argument("Choose an independent protocol.");
        const auto mode=protocol.left(slash);
        const auto id=protocol.sliced(slash+1).toStdString();
        const auto kind=mode=="assessment"?FrontierTrialKind::assessment:mode=="experiment"?FrontierTrialKind::experiment:FrontierTrialKind::replication;
        StartFrontierTrial action{systems_.selectedId().toULongLong(),kind,id,lab_inputs::allocation(source,rate,staff)};
        Cents protected_cash{0};
        for (const auto& reservation:company().reservations) protected_cash=checked_unit_add(protected_cash,reservation.reserved);
        const auto quote=quote_frontier_trial(company(),action,protected_cash);
        QString protocol_text;
        if (kind==FrontierTrialKind::assessment) {
            const auto& panel=*named(future().catalog.panels,id);
            protocol_text=QStringLiteral("%1 · %2\nHeld-out panel %3").arg(text(panel.name),text(panel.reviewer),text(panel.holdout));
        } else {
            const auto& experiment=*named(future().catalog.experiments,id);
            protocol_text=QStringLiteral("%1 · %2\n%3\nReference score %4/100. Minimum measured gain %5 points.")
                .arg(text(experiment.name),text(experiment.reviewer),text(experiment.verification),
                    points(experiment.baseline.value()),points(experiment.required_gain.value()));
        }
        stage(action,protocol_text+QStringLiteral("\nPay %1 in 2014 dollars. Commit %2 staff and %3 CU/day. The earliest completed result is %4, after %5 CU of funded work.")
            .arg(money(quote.fee),number(action.allocation.staff),cu(action.allocation.daily_compute.value()),date(quote.earliest_result),cu(quote.work.value())));
    } catch (const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void FrontierController::reviewResearch(QStringList topics,bool standing,QVariantMap limits) {
    clearReview(); if (!editable()) return;
    try {
        const auto terms=budget(limits,standing?std::nullopt:std::optional{static_cast<std::uint32_t>(topics.size())});
        AuthorizeFrontierResearch action{{strings(topics),terms,standing}};
        auto candidate=company();
        authorize_frontier_research(candidate,action.plan);
        QStringList names;
        for (const auto& topic:action.plan.topics) names<<readable(topic);
        stage(std::move(action),QStringLiteral("%1\n%2\nAuthorize up to %3 starts. Each start checks current prerequisites and available resources.\n%4")
            .arg(standing?QStringLiteral("Authorize standing research"):QStringLiteral("Authorize ordered research queue"),
                names.join(QStringLiteral(" → ")),number(terms.runs),budget_text(terms)));
    } catch (const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void FrontierController::reviewRefresh(QString service,QString recipe,QString evaluation,QStringList data,QString work,QString gain,QVariantMap limits) {
    clearReview(); if (!editable()) return;
    try {
        const auto terms=budget(limits);
        AuthorizeFrontierRefresh action{{service.toStdString(),recipe.toStdString(),evaluation.toStdString(),strings(data),terms,
            MilliCU{lab_inputs::compute(work)},BasisPoints{lab_inputs::policy_money(gain).value()}}};
        auto candidate=company();
        authorize_frontier_refresh(candidate,action.plan);
        stage(action,QStringLiteral("Authorize up to %1 candidates for %2 with %3 CU each.\n%4\nThe current service continues while each candidate trains and receives independent evaluation. Rollout requires at least %5 points of improvement, preserved evidence coverage and the current service terms.")
            .arg(number(terms.runs),readable(action.plan.service),cu(action.plan.work.value()),budget_text(terms),gain));
    } catch (const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void FrontierController::reviewCancellation(bool refresh) {
    clearReview(); if (!editable()) return;
    try {
        CancelFrontierPolicy action{(refresh?refresh_:research_).selectedId().toULongLong(),refresh};
        auto candidate=company();
        cancel_frontier_policy(candidate,action);
        stage(action,refresh?QStringLiteral("Cancel future refresh starts and stop any candidate still training or under review. The deployed service keeps operating under its current terms."):
            QStringLiteral("Cancel future starts under this program. Already funded work remains available in the Lab."));
    } catch (const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void FrontierController::confirm() {
    if (!pending_ || !editable()) return;
    if (reviewed_revision_!=company().revision) {
        clearReview();
        emit noticeRaised(QStringLiteral("The campaign changed. Review the current terms again."));
        return;
    }
    auto action=std::move(*pending_);
    clearReview();
    emit actionRequested(std::move(action));
}
void FrontierController::clearReview() {
    pending_.reset();
    review_text_.clear();
    emit changed();
}
}
