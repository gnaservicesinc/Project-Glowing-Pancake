#include "departure_presentation.hpp"
#include <QObject>
#include <QStringList>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString money(core::Cents value) {
    const auto amount=value.value();
    const auto magnitude=amount<0?static_cast<std::uint64_t>(-(amount+1))+1:static_cast<std::uint64_t>(amount);
    auto whole=QString::number(static_cast<qulonglong>(magnitude/100));
    for(auto pos=whole.size()-3;pos>0;pos-=3) whole.insert(pos,QLatin1Char(','));
    return QStringLiteral("%1$%2.%3").arg(amount<0?QStringLiteral("−"):QString{},whole)
        .arg(magnitude%100,2,10,QLatin1Char('0'));
}
}
QVariantMap departureArticle(const core::SimulationState& state,const core::DepartureRelease& release) {
    const auto& catalog=core::succession_catalog(state);
    const auto& plan=state.succession->plans.at(release.plan-1);
    const auto& message=core::departure_message(catalog,release.message);
    const auto person=core::leadership_candidate(state,plan.candidate,plan.proposed_on);
    QStringList statement;
    if(!release.reason.empty()) statement.push_back(text(core::departure_reason(catalog,release.reason).copy));
    statement.push_back(text(message.copy));
    statement.push_back(QObject::tr("The approved handover to %1 is dated %2.").arg(text(person.name),text(core::iso_date(plan.handover_on))));
    if(plan.retain_chair) statement.push_back(QObject::tr("I will stay on as chair."));
    const auto assessment=core::departure_assessment(state,release);
    const auto claim=message.claim.value_or("");
    QString headline,response,focus=QStringLiteral("handover");
    if(assessment=="challenged") {
        focus=QStringLiteral("claim");
        headline=QObject::tr("Company record challenges founder's resignation statement");
        if(claim=="profit") response=QObject::tr("The statement calls the company profitable. Its accounts show no cumulative profit at the announcement.");
        if(claim=="research") response=QObject::tr("The statement points to an established research record. The lab had no publications before the announcement day.");
        if(claim=="delivery") response=QObject::tr("The statement says the company met every customer start date. Missed starts in the contract register: %1.")
            .arg(release.evidence->missed_starts);
        if(claim=="publication_commitment") response=QObject::tr("The statement denies any executive departure over publication. The company has already recorded one.");
        if(claim=="safety_record") response=QObject::tr("The statement claims a spotless deployment safety record. The risk register shows %1 disclosed service incident(s).")
            .arg(release.evidence->service_incidents);
        if(claim=="workforce_trust") response=QObject::tr("The statement describes unified workplace backing. Personnel records show %1 resignation(s) before the announcement.")
            .arg(release.evidence->workforce_resignations);
    } else if(assessment=="supported") {
        headline=QObject::tr("Founder announces handover to %1").arg(text(person.name));
        response=QObject::tr("The claim is supported by the dated company record.");
    } else if(assessment=="unsubstantiated" && claim=="delivery") {
        focus=QStringLiteral("claim");
        headline=QObject::tr("Delivery claim awaits a completed contract");
        response=QObject::tr("The register shows zero missed starts and zero contracts that reached the end of their term.");
    } else {
        headline=QObject::tr("%1 named as founder prepares to step down").arg(text(person.name));
        response=QObject::tr("Coverage follows the funded handover and the commitments made to the incoming leader.");
    }
    if(const auto* outside=core::outside_leadership_candidate(catalog,person.id)) {
        headline=QObject::tr("Board turns to %1 for founder succession").arg(text(person.name));
        response=QObject::tr("%1 comes from outside the company. %2 The board approved the handover to the chief executive office.")
            .arg(text(person.name),text(outside->background))+QStringLiteral("\n\n")+response;
        focus=QStringLiteral("outside_successor");
    } else if(person.id.starts_with("internal.")) {
        headline=person.internal_surprise
            ?QObject::tr("Company stuns observers by naming %1 as internal successor").arg(text(person.name))
            :QObject::tr("Company promotes insider %1 to chief executive").arg(text(person.name));
        response=QObject::tr("%1 rises from within the workplace (%2). %3")
            .arg(text(person.name),text(person.internal_role),text(person.priorities))+QStringLiteral("\n\n")+response;
        focus=QStringLiteral("internal_successor");
    }
    if(release.evidence && (release.evidence->service_incidents>0 || release.evidence->workforce_resignations>0)) {
        QStringList items;
        if(release.evidence->service_incidents>0)
            items<<QObject::tr("%1 deployment incident(s)").arg(release.evidence->service_incidents);
        if(release.evidence->workforce_resignations>0)
            items<<QObject::tr("%1 employee resignation(s)").arg(release.evidence->workforce_resignations);
        headline=QObject::tr("Public controversy shadows founder's resignation");
        response=QObject::tr("Unresolved company controversies (%1) dominate coverage of the departure statement.")
            .arg(items.join(QObject::tr(" and ")))+QStringLiteral("\n\n")+response;
        focus=QStringLiteral("public_controversy");
    }
    if(release.evidence && release.evidence->public_dispute && state.management && state.management->governance) {
        const auto& governance=*state.management->governance;
        const auto dispute=std::ranges::find(governance.disputes,*release.evidence->public_dispute,&core::CorporateDispute::id);
        if(dispute!=governance.disputes.end() && dispute->resolved_on) {
            const auto& executives=state.management->catalog.governance->executives;
            const auto executive=std::ranges::find(executives,dispute->subject,&core::ExecutiveCandidate::id);
            const auto name=executive==executives.end()?QObject::tr("An executive"):text(executive->name);
            headline=dispute->kind=="publication"
                ?QObject::tr("Publication dispute follows founder’s resignation")
                :QObject::tr("Public governance dispute follows founder’s resignation");
            response=QObject::tr("%1 left on %2 after a %3 dispute went unresolved. That departure shapes how reporters read the founder’s statement.")
                .arg(name,text(core::iso_date(*dispute->resolved_on)),text(dispute->kind))+QStringLiteral("\n\n")+response;
            focus=QStringLiteral("public_dispute");
        }
    }
    if(plan.asi) {
        const bool weak=plan.asi->control<catalog.asi->control_scrutiny_below;
        const auto lead=weak?QObject::tr("ASI succession draws scrutiny over control"):
            QObject::tr("Company chooses its own ASI, %1, as chief executive").arg(text(person.name));
        const auto detail=QObject::tr("The approved appointment of %1 leads the coverage. Independent ASI qualification was established on %2. "
            "The control assessment recorded at nomination was %3/100. %4")
            .arg(text(person.name),text(core::iso_date(plan.asi->qualified_on)),QString::number(plan.asi->control/100.0,'f',2),
                plan.retain_chair?QObject::tr("The founder plans to remain as chair."):QObject::tr("The founder is leaving the executive office."));
        response=detail+QStringLiteral("\n\n")+response;headline=lead;
        focus=weak?QStringLiteral("ai_control"):QStringLiteral("ai_successor");
    }
    QString facts;
    if(release.evidence) {
        const auto& record=*release.evidence;
        facts=QObject::tr("At announcement · cash %1 · debt %2 · profit since opening %3 · publications before that day %4")
            .arg(money(record.cash),money(record.debt),money(record.profit)).arg(record.publications);
        facts+=QObject::tr("\nCustomer contracts that reached term end: %1 · missed start dates: %2")
            .arg(record.completed_contracts).arg(record.missed_starts);
        if(record.service_incidents>0 || record.workforce_resignations>0)
            facts+=QObject::tr("\nDisclosed service incidents: %1 · employee resignations: %2")
                .arg(record.service_incidents).arg(record.workforce_resignations);
    }
    return {{"headline",headline},{"statement",statement.join(QStringLiteral("\n\n"))},{"response",response},
        {"facts",facts},{"assessment",text(assessment)},{"focus",focus},{"successor",text(person.name)},{"date",text(core::iso_date(release.day))}};
}
}
