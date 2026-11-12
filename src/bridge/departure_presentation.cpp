#include "departure_presentation.hpp"
#include <QObject>
#include <QStringList>

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
    QString headline,response,focus=QStringLiteral("handover");
    if(assessment=="challenged") {
        focus=QStringLiteral("claim");
        headline=QObject::tr("Company record challenges founder's resignation statement");
        response=message.claim && *message.claim=="profit"?
            QObject::tr("The statement calls the company profitable. Its accounts show no cumulative profit at the announcement."):
            QObject::tr("The statement points to an established research record. The lab had no publications before the announcement day.");
    } else if(assessment=="supported") {
        headline=QObject::tr("Founder announces handover to %1").arg(text(person.name));
        response=QObject::tr("The claim is supported by the dated company record.");
    } else {
        headline=QObject::tr("%1 named as founder prepares to step down").arg(text(person.name));
        response=QObject::tr("Coverage follows the funded handover and the commitments made to the incoming leader.");
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
    }
    return {{"headline",headline},{"statement",statement.join(QStringLiteral("\n\n"))},{"response",response},
        {"facts",facts},{"assessment",text(assessment)},{"focus",focus},{"successor",text(person.name)},{"date",text(core::iso_date(release.day))}};
}
}
