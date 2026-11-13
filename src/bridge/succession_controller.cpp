#include "succession_controller.hpp"
#include "departure_presentation.hpp"
#include "flow_presentation.hpp"
#include "lab_inputs.hpp"
#include <pgp/runtime/legacy.hpp>
#include <pgp/runtime/objectives.hpp>
#include <pgp/core/society.hpp>
#include <algorithm>

namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString date(OrdinalDay value) {return text(iso_date(value));}
QString money(Cents value) {
    const auto n=value.value();const auto magnitude=n<0?static_cast<std::uint64_t>(-(n+1))+1:static_cast<std::uint64_t>(n);
    auto whole=QString::number(static_cast<qulonglong>(magnitude/100));
    for(auto i=whole.size()-3;i>0;i-=3) whole.insert(i,u',');
    return QStringLiteral("%1$%2.%3").arg(n<0?QStringLiteral("−"):QString{},whole).arg(magnitude%100,2,10,QLatin1Char('0'));
}
QString role(FounderRole role) {
    switch(role) {
    case FounderRole::executive:return QStringLiteral("Founder is chief executive");
    case FounderRole::chair:return QStringLiteral("Founder is chair. The successor leads operations");
    case FounderRole::trust:return QStringLiteral("Founder trust represented by its institutional committee");
    }
    return {};
}
QString status(SuccessionStatus state) {
    switch(state) {
    case SuccessionStatus::proposed:return QStringLiteral("Awaiting board vote");
    case SuccessionStatus::approved:return QStringLiteral("Funded preparation");
    case SuccessionStatus::rejected:return QStringLiteral("Board declined");
    case SuccessionStatus::expired:return QStringLiteral("Vote expired");
    case SuccessionStatus::cancelled:return QStringLiteral("Cancelled");
    case SuccessionStatus::activated:return QStringLiteral("Handover completed");
    case SuccessionStatus::obsolete:return QStringLiteral("Preparation stopped or executive changed");
    }
    return {};
}
QString notice(const SuccessionNotice& n,const SuccessionState& actor) {
    const auto& term=actor.terms.at(n.term-1);
    const auto deadline=date(n.deadline);
    if(n.kind=="planning") return QStringLiteral("%1's executive term ends on %2. Nominate a successor and fund the transition before that date. The charter appoints a paid caretaker if the term ends without a ready plan.").arg(text(term.name),deadline);
    if(n.kind=="handover") return QStringLiteral("%1 took office on %2 after the approved preparation. Existing company duties and the founder trust continue under the recorded board rights.").arg(text(term.name),date(n.day));
    if(n.kind=="caretaker") return QStringLiteral("The previous executive term ended. %1 is the charter's paid caretaker through %2. Review the next leadership plan and current financial commitments.").arg(text(term.name),deadline);
    if(n.kind=="chair_retired") return QStringLiteral("The founder's personal chair role ended on %1. The retained trust now uses its institutional representative and keeps its recorded beneficial ownership.").arg(date(n.day));
    if(n.kind=="transition_delayed") return QStringLiteral("The reviewed handover date arrived while transition exercises remain unfinished. The current executive's retirement deadline is %1. Review the funded project and staffing.").arg(deadline);
    return QStringLiteral("Leadership preparation stopped or its incumbent changed. Existing service obligations continue. Review a replacement plan before %1.").arg(deadline);
}
}
SuccessionController::SuccessionController(QObject* parent):QObject(parent),plans_(this),terms_(this) {
    connect(&plans_,&StableListModel::selectedIdChanged,this,&SuccessionController::clearReview);
}
bool SuccessionController::available() const {return snapshot_ && company().succession.has_value();}
bool SuccessionController::editable() const {return available() && company().phase==DayPhase::work_ready &&
    (snapshot_->driver_halted || company().paused) && snapshot_->checkpoint.runtime.prepared_day==company().date;}
bool SuccessionController::founderExecutive() const {return available() && founder_role(actor())==FounderRole::executive;}
bool SuccessionController::founderChair() const {return available() && founder_role(actor())==FounderRole::chair;}
QString SuccessionController::recommendedDate() const {
    if(!available()) return {};
    const auto& c=succession_catalog(company());return date(advance_days(company().date,Days{c.transition_days.value()+c.vote_window.value()}));
}
QString SuccessionController::recommendedRate() const {
    if(!available()) return {};
    const auto& c=succession_catalog(company());
    return cu((c.transition_work.value()+c.transition_days.value()-1)/c.transition_days.value());
}
bool SuccessionController::canFinish() const {
    if(!editable()) return false;
    const auto& w=snapshot_->checkpoint.world;
    return w.objectives && !w.objectives->end &&
        (w.objectives->mode==runtime::ObjectiveMode::scored || w.objectives->mode==runtime::ObjectiveMode::retrospective) &&
        runtime::completed_founder_departure(w,company().date).has_value();
}
QVariantList SuccessionController::departureReasons() const {
    QVariantList result;
    if(!available()) return result;
    const auto& departure=succession_catalog(company()).departure;
    if(!departure || !departure->press) return result;
    for(const auto& reason:departure->press->reasons)
        result.push_back(QVariantMap{{"id",text(reason.id)},{"label",text(reason.title)}});
    return result;
}
QVariantMap SuccessionController::announcementPreview(QString message,QString reason) const {
    if(!available()) return {};
    try {
        const auto release=preview_founder_departure(company(),{plans_.selectedId().toULongLong(),message.toStdString(),reason.toStdString()});
        return departureArticle(company(),release);
    } catch(const std::exception& error) {
        return {{"error",text(error.what())}};
    }
}
QVariantList SuccessionController::departureMessages() const {
    QVariantList rows;if(!available() || !succession_catalog(company()).departure) return rows;
    for(const auto& message:succession_catalog(company()).departure->messages) {
        QStringList effects;
        for(std::size_t i=0;i<message.completed_effect.size();++i) {
            const auto n=message.completed_effect[i];
            effects<<QStringLiteral("%1 %2%3/100").arg(text(society_audience_name(i)),n>=0?QStringLiteral("+"):QString{},QString::number(n/100.0,'f',2));
        }
        const auto effectLabel=message.claim?tr("If the company record supports the claim and the handover completes: %1.").arg(effects.join(QStringLiteral(" · "))):
            tr("After the handover completes: %1.").arg(effects.join(QStringLiteral(" · ")));
        rows.push_back(QVariantMap{{"id",text(message.id)},{"label",text(message.title)},
            {"detail",text(message.copy)+tr("\nCompany expense %1. %2 A delayed or cancelled handover harms confidence.")
                .arg(money(message.fee),effectLabel)}});
    }
    return rows;
}
QString SuccessionController::departureHistory() const {
    if(!available() || !actor().releases) return {};
    QStringList lines;
    for(const auto& release:*actor().releases) {
        const auto& plan=actor().plans.at(release.plan-1);
        lines<<QStringLiteral("%1 · %2 → %3 · %4").arg(date(release.day),text(departure_message(succession_catalog(company()),release.message).title),
            text(leadership_candidate(company(),plan.candidate,plan.proposed_on).name),status(plan.status));
        const auto coverage=departureArticle(company(),release);
        lines<<coverage.value("statement").toString()<<coverage.value("headline").toString()
             <<coverage.value("response").toString()<<coverage.value("facts").toString();
    }
    return lines.join(u'\n');
}
QString SuccessionController::overview() const {
    if(!available()) return {};
    const auto& c=succession_catalog(company());const auto& term=actor().terms.back();
    const auto salary=term.person=="founder"?c.founder_salary:leadership_candidate(company(),term.person,term.plan?actor().plans.at(term.plan-1).proposed_on:term.starts_on).monthly_salary;
    const auto legacy=runtime::founder_legacy(snapshot_->checkpoint.world,snapshot_->checkpoint.world.company_id);
    return QStringLiteral("%1 · %2 · %3\nExecutive term %4 through %5. Current executive cost %6/month in 2014 dollars.\n%7\n%8 · personal cash %9 · personal liabilities %10 · private assets %17.\nRetained equity at group book value %11 · founder legacy net book wealth %12. Company cash is included in that equity value.\nHuman executives have up to %13 years per term and retire at %14. The founder chair ends at %15. Preparation takes at least %16 days.")
        .arg(text(term.name),term.asi?tr("ASI"):tr("born %1").arg(term.birth_year),term.caretaker?QStringLiteral("Charter caretaker"):QStringLiteral("Chief executive"),
            date(term.starts_on),date(term.due_on),money(salary),role(founder_role(actor())),readable(legacy.holder),money(legacy.cash),money(legacy.liabilities),
            money(legacy.book_equity),money(legacy.net_book_wealth),QString::number(c.maximum_term_years),QString::number(c.executive_retirement_age),
            QString::number(c.chair_retirement_age),QString::number(c.transition_days.value()),money(legacy.assets));
}
QString SuccessionController::noticeText() const {
    if(available()) if(const auto* n=required_succession_notice(company())) return notice(*n,actor());
    return {};
}
QVariantList SuccessionController::candidatesAt(QString day) const {
    QVariantList result;if(!available()) return result;
    try {
        for(const auto& person:leadership_candidates(company(),parse_iso_date(day.toStdString()))) {
            if(person.id==actor().terms.back().person) continue;
            auto detail=tr("%1\nPreparation fee %2. %3 %4/month.").arg(text(person.priorities),money(person.signing_cost),
                person.asi?tr("Hosted operation and human oversight"):tr("Executive salary"),money(person.monthly_salary));
            if(person.asi) detail+=tr("\nASI established %1. Independent control bound %2/100. Mandate %3 years.")
                .arg(date(person.asi->qualified_on),QString::number(person.asi->control/100.0,'f',2)).arg(succession_catalog(company()).asi->maximum_term_years);
            result.push_back(QVariantMap{{"id",text(person.id)},{"label",person.asi?tr("%1 · ASI").arg(text(person.name)):
                tr("%1 · born %2").arg(text(person.name)).arg(person.birth_year)},{"detail",detail}});
        }
    }catch(const std::exception&) {}
    return result;
}
void SuccessionController::update(CampaignSnapshotPtr snapshot,bool replace) {
    snapshot_=std::move(snapshot);if(replace) clearReview();
    std::vector<CampaignRow> plans,terms;
    if(available()) {
        const auto& c=succession_catalog(company());
        for(const auto& p:actor().plans) {
            const auto person=leadership_candidate(company(),p.candidate,p.proposed_on);
            auto detail=QStringLiteral("Proposed %1 · vote before %2 · reviewed handover %3.\n%4 staff and %5 CU/day from %6. Preparation fee %7. Executive cost %8/month.\n%9")
                .arg(date(p.proposed_on),date(p.vote_before),date(p.handover_on),QString::number(p.allocation.staff),cu(p.allocation.daily_compute.value()),
                    readable(p.allocation.source_id),money(person.signing_cost),money(person.monthly_salary),p.retain_chair?QStringLiteral("Founder moves to chair."):QStringLiteral("Founder trust retains its institutional rights."));
            if(p.asi) detail+=tr("\nASI established %1 · control bound %2/100 at nomination. Hosted operation and human oversight begin at appointment.")
                .arg(date(p.asi->qualified_on),QString::number(p.asi->control/100.0,'f',2));
            for(const auto& vote:p.ballots) detail+=QStringLiteral("\n%1 · %2 seats · %3").arg(readable(vote.member),QString::number(vote.seats),vote.support?QStringLiteral("support"):QStringLiteral("decline"));
            if(!p.project.empty()) {
                const auto& rows=company().capacity->projects;const auto work=std::ranges::find(rows,p.project,&CapacityProject::id);
                detail+=QStringLiteral("\nTransition work %1 of %2 CU.").arg(cu(work->completed_work.value()),cu(work->total_work.value()));
            }
            plans.push_back({QString::number(p.id),text(person.name),date(p.handover_on),detail,status(p.status),false,
                {{"announce",c.departure && p.incumbent==1 && (p.status==SuccessionStatus::approved || p.status==SuccessionStatus::activated) && actor().releases && std::ranges::none_of(*actor().releases,[&](const auto& r){return r.plan==p.id;})},{"vote",p.status==SuccessionStatus::proposed},{"cancel",p.status==SuccessionStatus::proposed || p.status==SuccessionStatus::approved}}});
        }
        for(const auto& term:actor().terms) terms.push_back({QString::number(term.id),text(term.name),date(term.starts_on),
            QStringLiteral("%1 · executive through %2 · mandate ends %3. %4")
                .arg(term.asi?tr("ASI"):tr("Born %1").arg(term.birth_year),term.ended_on?date(*term.ended_on):QStringLiteral("current term"),date(term.due_on),
                    term.caretaker?QStringLiteral("Appointed under the caretaker charter."):term.plan?QStringLiteral("Approved, funded handover."):QStringLiteral("Founding executive.")),
            term.ended_on?QStringLiteral("Former executive"):QStringLiteral("Current executive")});
    }
    const auto rows=[&](StableListModel& model,std::vector<CampaignRow> records) {
        if(replace) model.beginCampaign(std::move(records));else model.setRows(std::move(records));
        if(model.selectedId().isEmpty() && model.rowCount()) model.setSelectedId(model.idAt(0));
    };
    rows(plans_,std::move(plans));rows(terms_,std::move(terms));emit changed();
}
void SuccessionController::stage(core::CommandAction action,QString copy) {
    pending_=std::move(action);reviewed_revision_=company().revision;review_text_=std::move(copy);emit changed();emit reviewReady();
}
void SuccessionController::reviewNomination(QString person,QString day,bool chair,QString source,QString rate,QString staff) {
    clearReview();if(!editable()) return;
    try {
        ProposeSuccession action{person.toStdString(),parse_iso_date(day.toStdString()),chair,lab_inputs::allocation(source,rate,staff)};
        auto candidate=company();propose_succession(candidate,action);
        const auto successor=leadership_candidate(company(),action.candidate);
        auto copy=tr("Nominate %1 for %2.\n%3\nPreparation fee %4. Executive cost %5/month. The approved transition uses %6 staff and %7 CU/day.")
            .arg(text(successor.name),date(action.handover_on),text(successor.priorities),money(successor.signing_cost),money(successor.monthly_salary),QString::number(action.allocation.staff),cu(action.allocation.daily_compute.value()));
        if(successor.asi) {
            copy+=tr("\nASI established %1. Independent control bound %2/100. Hosted operation and human oversight are company expenses.\nThis nominates the system for the chief executive office; the board must approve it.")
                .arg(date(successor.asi->qualified_on),QString::number(successor.asi->control/100.0,'f',2));
            if(company().management->governance) copy+=tr(" On appointment it receives the chief executive's one board seat. Your %1 keeps %2 seats.")
                .arg(chair?tr("chair role"):tr("trust representation")).arg(chair?succession_catalog(company()).chair_seats:succession_catalog(company()).trust_seats);
        }
        stage(action,copy);
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::reviewVote(bool support) {
    clearReview();if(!editable()) return;
    try {
        VoteSuccession action{plans_.selectedId().toULongLong(),support};auto candidate=company();Cents reserved{0};
        for(const auto& row:company().reservations) reserved=checked_unit_add(reserved,row.reserved);
        vote_succession(candidate,action,reserved);const auto& p=candidate.succession->plans.at(action.id-1);
        QStringList votes;for(const auto& vote:p.ballots) votes<<QStringLiteral("%1 · %2 seats · %3").arg(readable(vote.member),QString::number(vote.seats),vote.support?QStringLiteral("support"):QStringLiteral("decline"));
        stage(action,QStringLiteral("%1\n%2\nPay %3 for the approved finite preparation. The reviewed handover date is %4. Executive costs begin when the successor takes office.")
            .arg(status(p.status),votes.join(u'\n'),money(checked_unit_subtract(company().cash,candidate.cash)),date(p.handover_on)));
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::reviewCancellation() {
    clearReview();if(!editable()) return;
    try {CancelSuccession action{plans_.selectedId().toULongLong()};auto candidate=company();cancel_succession(candidate,action.id);
        stage(action,QStringLiteral("Cancel this successor plan and release its remaining preparation resources. Spent preparation fees stay in company expenses. The current executive retains the saved retirement deadline."));
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::reviewRetirement() {
    clearReview();if(!editable()) return;
    try {auto candidate=company();retire_founder_chair(candidate);
        stage(RetireFounderChair{},QStringLiteral("End the founder's personal chair role and its future stipend. The current executive continues operating. The founder trust keeps its actual beneficial holdings and institutional board rights."));
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::reviewAnnouncement(QString message,QString reason) {
    clearReview();if(!editable()) return;
    try {
        AnnounceFounderDeparture action{plans_.selectedId().toULongLong(),message.toStdString(),reason.toStdString()};auto candidate=company();Cents reserved{0};
        for(const auto& row:company().reservations) reserved=checked_unit_add(reserved,row.reserved);
        announce_founder_departure(candidate,action,reserved);
        const auto& m=departure_message(succession_catalog(candidate),action.message);
        const auto report=departureArticle(candidate,candidate.succession->releases->back());
        stage(action,tr("%1\n\n%2\n\n%3\n%4\n\nCompany expense %5.")
            .arg(report.value("statement").toString(),report.value("headline").toString(),report.value("response").toString(),
                 report.value("facts").toString(),money(m.fee)));
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::reviewFinish() {
    clearReview();if(!editable()) return;
    try {auto world=snapshot_->checkpoint.world;runtime::finish_founder_run(world);
        stage(FinishFounderRun{},QStringLiteral("Close this life story here? Read a retrospective on the founder's career so far, at their current age. This ends play in this record."));
    }catch(const std::exception& e){emit noticeRaised(text(e.what()));}
}
void SuccessionController::acknowledge() {
    if(editable()) if(const auto* n=required_succession_notice(company())) emit actionRequested(AcknowledgeSuccession{n->id});
}
void SuccessionController::confirm() {
    if(!pending_ || !editable()) return;
    if(reviewed_revision_!=company().revision) {clearReview();emit noticeRaised(QStringLiteral("The campaign changed. Review its current leadership terms again."));return;}
    auto action=std::move(*pending_);clearReview();emit actionRequested(std::move(action));
}
void SuccessionController::clearReview() {pending_.reset();review_text_.clear();emit changed();}
}
