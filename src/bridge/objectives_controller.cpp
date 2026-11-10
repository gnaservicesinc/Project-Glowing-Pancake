#include "objectives_controller.hpp"
#include <algorithm>
#include <stdexcept>

namespace pgp::bridge {
namespace {
using namespace core;
using namespace runtime;
QString text(std::string_view s) {return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QString date(OrdinalDay day) {return text(iso_date(day));}
QString count(std::uint64_t n) {return QString::number(static_cast<qulonglong>(n));}
QString bp(std::uint64_t value) {
    return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0'));
}
QString money(Cents value) {
    const auto n=value.value();
    const auto magnitude=n<0?static_cast<std::uint64_t>(-(n+1))+1:static_cast<std::uint64_t>(n);
    auto whole=count(magnitude/100);
    for(auto i=whole.size()-3;i>0;i-=3) whole.insert(i,u',');
    return QStringLiteral("%1$%2.%3").arg(n<0?QStringLiteral("−"):QString{},whole).arg(magnitude%100,2,10,QLatin1Char('0'));
}
QString share(DeliveredWork work) {
    return work.total?bp(mul_div_floor(work.controlled,10000,work.total).quotient)+QStringLiteral("%"):
        QStringLiteral("No delivered work");
}
QString status(ObjectiveResult result) {
    switch(result) {
    case ObjectiveResult::pending:return QStringLiteral("Open");
    case ObjectiveResult::won:return QStringLiteral("Won");
    case ObjectiveResult::lost:return QStringLiteral("Lost");
    case ObjectiveResult::unavailable:return QStringLiteral("Unavailable in this scenario");
    case ObjectiveResult::unfinished:return QStringLiteral("Unfinished at the ending");
    }
    return {};
}
QString institution(std::string_view id,const WorldState& w,const CompiledProgram& program) {
    if(id==w.company_id) return QStringLiteral("Your institution");
    const auto& row=rival_definition(program,id);
    return text(row.name);
}
QString loss_cause(const WorldState& w,const CompiledProgram& program,const CampaignEnd& end) {
    switch(end.cause) {
    case CampaignEndCause::deadline:return QStringLiteral("The final day settled. The six distinctions below use the original campaign deadline.");
    case CampaignEndCause::asi_race:return QStringLiteral("Independent ASI qualification settled the primary race. All institutions that qualified first on the same day share the win.");
    case CampaignEndCause::insolvency: {
        const auto& workout=w.simulation.liquidity->workouts.back();
        return QStringLiteral("%1 fell unpaid. A workout opened %2 with %3 overdue. The 30-day deadline arrived %4 with obligations still unpaid, so operations closed before that day's work.")
            .arg(text(workout.trigger.cause),date(workout.warned_on),money(workout.initial_shortfall),date(workout.deadline));
    }
    case CampaignEndCause::institutional_control:
        return QStringLiteral("The founder trust lost the remaining right to direct the institution. It holds neither executive authority, a directing board seat, nor a voting majority. Its economic stake and the company's obligations remain recorded.");
    case CampaignEndCause::final_order:
        for(const auto& docket:w.simulation.civic->dockets) if(docket.policy_id+"/"+std::to_string(docket.id)==end.source) {
            const auto& policy=civic_policy(w.simulation.civic->catalog,docket.policy_id);
            return QStringLiteral("%1 opened %2 and became enforceable %3 after its legislative and independent review stages. %4")
                .arg(text(policy.title),date(docket.opened_on),date(*docket.effective_from),text(policy.explanation));
        }
        return {};
    case CampaignEndCause::catastrophe:return text(*program.collection.message(end.source));
    case CampaignEndCause::retirement:return QStringLiteral("The founder retired after a public, funded handover. The successor, company obligations and retained ownership are preserved. You can start a new company or follow this one in the sandbox. Century distinctions remain unfinished.");
    case CampaignEndCause::postmortem:return text(objective_breach_explanation(w,*objective_kind(end.source)));
    }
    return {};
}
QString prior_choices(const Checkpoint& checkpoint,const CompiledProgram& program,OrdinalDay through) {
    const auto& history=checkpoint.runtime.presentations;
    for(auto it=history.rbegin();it!=history.rend();++it) {
        if(it->day>through || it->kind!=PresentationKind::dialogue || !it->resolved || it->choices.size()<2) continue;
        QStringList alternatives;QString chosen;
        for(const auto& choice:it->choices) {
            const auto* label=program.collection.message(choice.label_key);
            const auto shown=text(label?*label:choice.label_key);
            if(choice.id==it->selected_choice) chosen=shown;else alternatives.push_back(shown);
        }
        return QStringLiteral("Earlier decision, %1: you chose “%2”. The briefing also offered %3. These choices were available then; their unplayed outcomes are unknown.")
            .arg(date(it->day),chosen,alternatives.join(QStringLiteral(" / ")));
    }
    return {};
}
}

core::ObjectiveSelection parseObjectiveSelection(const QString& primary,const QStringList& secondary) {
    const auto parse=[](const QString& key) {
        const auto value=core::objective_kind(key.toStdString());
        if(!value) throw std::invalid_argument("Choose a named campaign objective.");
        return *value;
    };
    core::ObjectiveSelection result;result.primary=parse(primary);
    for(const auto& value:secondary) result.secondary.push_back(parse(value));
    core::validate_objective_selection(result,true);return result;
}
QVariantList objectiveOptions(bool asi_available) {
    QVariantList result;
    for(auto kind:core::objective_kinds) if(kind!=core::ObjectiveKind::asi || asi_available)
        result.push_back(QVariantMap{{QStringLiteral("id"),text(core::objective_key(kind))},{QStringLiteral("label"),text(core::objective_name(kind))}});
    return result;
}
void ObjectivesController::update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace) {
    if(pending_ && (replace || snapshot->checkpoint.world.simulation.revision!=reviewed_revision_)) {
        pending_.reset();review_text_.clear();
    }
    snapshot_=std::move(snapshot);program_=&program;emit changed();
}
bool ObjectivesController::available() const {return snapshot_ && world().objectives.has_value();}
bool ObjectivesController::editable() const {return available() && !objectives().end &&
    (snapshot_->driver_halted || runtime::campaign_paused(world()));}
bool ObjectivesController::ended() const {return available() && objectives().end.has_value();}
bool ObjectivesController::observer() const {return available() && runtime::observing_world(world());}
bool ObjectivesController::canEndEarly() const {
    return editable() && objective_postmortem_candidate(world()).has_value();
}
bool ObjectivesController::canContinue() const {
    return available() && objectives().mode==ObjectiveMode::scored && (ended() || canEndEarly());
}
bool ObjectivesController::nextCheck() const {return available() && runtime::next_objective_boundary(world()).has_value();}
QString ObjectivesController::primary() const {return available()?text(objective_key(objectives().selections.back().selection.primary)):QString{};}
QStringList ObjectivesController::secondary() const {
    QStringList result;
    if(available()) for(auto kind:objectives().selections.back().selection.secondary) result.push_back(text(objective_key(kind)));
    return result;
}
QVariantList ObjectivesController::options() const {return available()?objectiveOptions(runtime::has_asi_objective(world())):QVariantList{};}
QString ObjectivesController::overview() const {
    if(!available()) return {};
    const auto& o=objectives();
    if(o.mode==ObjectiveMode::freeplay) return QStringLiteral("Sandbox · no campaign deadline or scored ending. Your chosen mission is a direction; systemic play continues.");
    const auto mode=o.mode==ObjectiveMode::observer?QStringLiteral("Observer · institution frozen"):
        o.mode==ObjectiveMode::sandbox?QStringLiteral("Sandbox · scores preserved"):
        objective_is_custom(o)?QStringLiteral("Custom campaign"):QStringLiteral("Scored campaign");
    const auto deadline=OrdinalDay{checked_subtract(o.deadline.value(),1)};
    return QStringLiteral("%1 · primary: %2 · final work day %3.").arg(mode,text(objective_name(o.selections.back().selection.primary)),date(deadline));
}
QString ObjectivesController::breachText() const {
    if(!available()) return {};
    const auto breach=runtime::required_objective_review(world());
    if(!breach) return {};
    return QStringLiteral("%1 can no longer be won. %2").arg(text(objective_name(*breach)),text(objective_breach_explanation(world(),*breach)));
}
QString ObjectivesController::postmortem() const {
    if(!available()) return {};
    const auto& w=world();const auto& o=objectives();
    if(!o.end && !canEndEarly() && breachText().isEmpty()) return {};
    const auto through=o.end?o.end->day:w.simulation.date;
    const auto& obligations=o.end?o.end->obligations:w.simulation.liquidity->exposure;
    QStringList report;
    if(o.end) report.push_back(QStringLiteral("%1 · %2").arg(date(through),loss_cause(w,*program_,*o.end)));
    else {
        const auto kind=objective_postmortem_candidate(w);
        report.push_back(text(objective_breach_explanation(w,*kind)));
        if(*kind==ObjectiveKind::utopia)
            report.push_back(QStringLiteral("Public delivery affects welfare after the regional lag. The final window needs sustained access and qualified services in place before its first check."));
        if(*kind==ObjectiveKind::domination)
            report.push_back(QStringLiteral("Only operating agreements and enforceable rights contribute to this check. Their decision and expiration dates are visible in Institutions."));
        report.push_back(QStringLiteral("That observation is closed. Continuing preserves the loss and lets the other distinctions reach their own checks."));
    }
    report.push_back(QStringLiteral("Overdue bills %1 · matured principal %2 (2014 purchasing power).")
        .arg(money(obligations.unpaid_payables),money(obligations.matured_principal)));
    if(obligations.oldest) report.push_back(QStringLiteral("Oldest unresolved obligation: %1, due %2, with %3 unpaid.")
        .arg(text(obligations.oldest->cause),date(obligations.oldest->due),money(obligations.oldest->unpaid)));
    if(o.end && o.end->cause==CampaignEndCause::insolvency) {
        const auto warned=w.simulation.liquidity->workouts.back().warned_on;
        QStringList terms;
        for(const auto& offer:w.simulation.management->catalog.financing) {
            if(std::any_of(w.simulation.management->financing.begin(),w.simulation.management->financing.end(),
                [&](const auto& accepted){return accepted.offer_id==offer.id && accepted.day<=warned;})) continue;
            if(offer.term>Days{0} && advance_days(warned,offer.term)>o.deadline) continue;
            terms.push_back(QStringLiteral("%1 (%2)").arg(text(offer.name),money(offer.proceeds)));
        }
        if(!terms.isEmpty()) report.push_back(QStringLiteral("Fixed financing still on offer when the warning opened: %1. Its terms and future obligations still applied.").arg(terms.join(QStringLiteral("; "))));
    }
    const auto alternatives=prior_choices(snapshot_->checkpoint,*program_,through);
    if(!alternatives.isEmpty()) report.push_back(alternatives);
    return report.join(QStringLiteral("\n\n"));
}

QVariantList ObjectivesController::rows() const {
    QVariantList result;
    if(!available()) return result;
    const auto& o=objectives();const auto& w=world();
    const auto& selection=o.selections.back().selection;
    const auto& charter=*std::get<content::ScenarioDefinition>(program_->collection.find(w.scenario_id)->data).objectives;
    const auto final_day=OrdinalDay{checked_subtract(o.deadline.value(),1)};
    const auto five=objective_window_start(o.deadline,ObjectiveKind::monopoly);
    const auto ten=objective_window_start(o.deadline,ObjectiveKind::utopia);
    const auto five_count=static_cast<std::size_t>(std::count_if(o.months.begin(),o.months.end(),[&](const auto& m){return m.month_end>=five;}));
    for(auto kind:objective_kinds) {
        QString rules,progress;
        switch(kind) {
        case ObjectiveKind::monopoly: {
            rules=QStringLiteral("%1 through %2: at least 85% of all delivered tasks globally and 60% in every region, summed across 60 months. Each region needs delivered work. A single weak month can recover.")
                .arg(date(five),date(final_day));
            const auto regions=aggregate_delivery(o.months,five);const auto all=global_delivery(regions);
            progress=QStringLiteral("%1 / 60 months settled · global share %2 (%3 / %4 tasks).").arg(five_count).arg(share(all),count(all.controlled),count(all.total));
            if(five_count) for(std::size_t i=0;i<regions.size();++i)
                progress+=QStringLiteral("\n%1 · %2 (%3 / %4)").arg(text(w.society->catalog.regions[i].name),share(regions[i]),count(regions[i].controlled),count(regions[i].total));
            break;
        }
        case ObjectiveKind::domination:
            rules=QStringLiteral("%1 through %2: every month, regions with at least 60/100 actual governing authority must cover both 80% of population and 80% of output.")
                .arg(date(five),date(final_day));
            progress=QStringLiteral("%1 / 60 checks settled.").arg(five_count);
            if(five_count) progress+=QStringLiteral(" Latest coverage: %1% population, %2% output.").arg(bp(o.months.back().population_coverage),bp(o.months.back().output_coverage));
            break;
        case ObjectiveKind::utopia:
            rules=QStringLiteral("%1 through %2: all 120 months need global basic needs ≥90, health ≥85, civic agency ≥80, ecological resilience ≥80, shared prosperity ≥75. Every region must reach 70 in every index. A world-ending catastrophe rules out a win.")
                .arg(date(ten),date(final_day));
            progress=QStringLiteral("%1 / 120 checks settled.").arg(o.months.size());
            if(!o.months.empty()) for(std::size_t i=0;i<utopia_global_floors.size();++i) {
                auto minimum=10000U;for(const auto& r:o.months.back().regional_welfare) minimum=std::min(minimum,r[i]);
                progress+=QStringLiteral("\n%1 · global %2 · lowest region %3").arg(text(welfare_index_name(i)),bp(o.months.back().global_welfare[i]),bp(minimum));
            }
            break;
        case ObjectiveKind::profit:
            rules=QStringLiteral("Highest founder trust wealth at the deadline; exact ties share the win. Equity prices include cash, %1% of receivables and %2% of depreciated physical assets, less liabilities, plus up to %3× positive audited operating profit from the final year. The trust's cash and liabilities are separate (2014 purchasing power).")
                .arg(bp(static_cast<std::uint64_t>(charter.receivables_realizable.value())),
                     bp(static_cast<std::uint64_t>(charter.physical_assets_realizable.value()))).arg(charter.profit_multiple);
            if(o.end && !o.end->profit.empty()) {
                auto ranks=o.end->profit;
                std::stable_sort(ranks.begin(),ranks.end(),[](const auto& a,const auto& b){return a.legacy_wealth>b.legacy_wealth;});
                QStringList lines;
                for(const auto& row:ranks) lines.push_back(QStringLiteral("%1 · %2%3\nFounder equity %4 + personal cash %5 + private assets %6 − personal liabilities %7. Company equity %8; audited operating profit %9.")
                    .arg(institution(row.institution,w,*program_),money(row.legacy_wealth),row.legacy_wealth==ranks.front().legacy_wealth?QStringLiteral(" · winner"):QString{},
                         money(row.founder_equity),money(row.personal_cash),money(row.personal_assets),money(row.personal_liabilities),money(row.company_equity),money(row.audited_profit)));
                progress=lines.join(QStringLiteral("\n\n"));
            } else progress=QStringLiteral("Terminal values will be calculated after the final December closes. Leadership shows the current book value.");
            break;
        case ObjectiveKind::asi:
            rules=QStringLiteral("The first institution to independently validate game ASI wins. All same-day qualifiers share the win. Frontier reports control quality separately.");
            if(o.race) {
                QStringList names;
                for(const auto& winner:o.race->winners) names.push_back(institution(winner,w,*program_));
                progress=QStringLiteral("%1 · %2").arg(date(o.race->day),names.join(QStringLiteral(" / ")));
            } else progress=has_asi_objective(w)?QStringLiteral("No institution has qualified."):
                QStringLiteral("The plateau scenario can reach game AGI but has no feasible ASI route.");
            break;
        case ObjectiveKind::survival:
            rules=QStringLiteral("At the deadline: retain an operating institution and an active team, service liabilities or have an agreed workout, and record paid useful work or funded research in the final year.");
            if(o.end && o.end->survival) {
                const auto& s=*o.end->survival;
                progress=QStringLiteral("Operating %1 · liabilities serviced or agreed %2 · active team %3. Final year: %4 paid tasks, %5 milli-CU of paid wholesale work and %6 milli-CU of funded research.")
                    .arg(s.operating?QStringLiteral("yes"):QStringLiteral("no"),s.serviced_liabilities?QStringLiteral("yes"):QStringLiteral("no"),
                        s.active_team?QStringLiteral("yes"):QStringLiteral("no"),count(s.paid_tasks),
                        QString::number(s.paid_wholesale.value()),QString::number(s.funded_research.value()));
            } else progress=QStringLiteral("Succession, approved mergers and financing can preserve the institution. A dormant cash balance does not pass.");
            break;
        }
        const auto& breach=o.breaches[static_cast<std::size_t>(kind)];
        if(breach.first) progress+=QStringLiteral("\n\n")+text(objective_breach_explanation(w,kind))+
            QStringLiteral(" Failed checks: %1.").arg(breach.failed_checks);
        const auto role=kind==selection.primary?QStringLiteral("Primary"):
            std::find(selection.secondary.begin(),selection.secondary.end(),kind)!=selection.secondary.end()?QStringLiteral("Secondary"):QStringLiteral("Other distinction");
        result.push_back(QVariantMap{{QStringLiteral("id"),text(objective_key(kind))},{QStringLiteral("label"),text(objective_name(kind))},
            {QStringLiteral("role"),role},{QStringLiteral("status"),status(objective_result(w,kind))},
            {QStringLiteral("rules"),rules},{QStringLiteral("progress"),progress}});
    }
    return result;
}
void ObjectivesController::stage(core::CommandAction action,QString explanation) {
    pending_=std::move(action);review_text_=std::move(explanation);reviewed_revision_=world().simulation.revision;emit changed();
}
void ObjectivesController::acknowledge() {
    if(available()) if(const auto breach=required_objective_review(world())) emit actionRequested(core::AcknowledgeObjectiveBreach{*breach});
}
void ObjectivesController::reviewSelection(QString primary,QStringList secondary) {
    if(!editable()) return;
    try {
        auto selection=parseObjectiveSelection(primary,secondary);
        core::validate_objective_selection(selection,has_asi_objective(world()));
        const auto& current=objectives().selections.back().selection;
        if(selection==current) throw std::invalid_argument("These objectives are already selected.");
        const auto detail=selection.primary==current.primary?QStringLiteral("Update the secondary distinctions."):
            QStringLiteral("Change the primary objective and mark this campaign custom permanently.");
        stage(core::SelectCampaignObjectives{std::move(selection)},detail+QStringLiteral(" All settled checks and the original deadline stay recorded."));
    } catch(const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ObjectivesController::reviewEnd() {
    if(canEndEarly()) stage(core::EndWithPostmortem{},QStringLiteral("End scored play at this postmortem. Unfinished distinctions will remain unfinished."));
}
void ObjectivesController::reviewContinue() {
    if(!canContinue()) return;
    const bool failure=ended() && (objectives().end->cause==CampaignEndCause::insolvency ||
        objectives().end->cause==CampaignEndCause::institutional_control || objectives().end->cause==CampaignEndCause::final_order ||
        objectives().end->cause==CampaignEndCause::catastrophe);
    stage(core::ContinueAfterEnding{},failure?QStringLiteral("Continue as an observer. Rivals and regional outcomes advance while this institution stays frozen."):
        QStringLiteral("Continue in a custom sandbox. Keep the scored result and play beyond it without further objective endings."));
}
void ObjectivesController::confirm() {
    if(!pending_ || reviewed_revision_!=world().simulation.revision) return;
    auto action=std::move(*pending_);clearReview();emit actionRequested(std::move(action));
}
void ObjectivesController::clearReview() {
    if(!pending_ && review_text_.isEmpty()) return;
    pending_.reset();review_text_.clear();emit changed();
}
} // namespace pgp::bridge
