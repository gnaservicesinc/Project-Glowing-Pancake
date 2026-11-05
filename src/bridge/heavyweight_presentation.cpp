#include "heavyweight_presentation.hpp"
#include "flow_presentation.hpp"
#include <algorithm>
#include <QCoreApplication>
namespace pgp::bridge {
namespace {
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString number(std::uint64_t value) {return QString::number(static_cast<qulonglong>(value));}
QString percent(core::BasisPoints value) {return QStringLiteral("%1.%2%").arg(value.value()/100).arg(value.value()%100,2,10,QLatin1Char('0'));}
QString money(core::Cents value) {
    auto whole=QString::number(value.value()/100);
    for (auto position=whole.size()-3;position>0;position-=3) whole.insert(position,QLatin1Char(','));
    return QStringLiteral("$%1.%2").arg(whole).arg(value.value()%100,2,10,QLatin1Char('0'));
}
QString date(core::OrdinalDay day) {return text(core::iso_date(day));}
}
HeavyweightModels heavyweight_models(const runtime::Checkpoint& checkpoint,const runtime::CompiledProgram& program) {
    HeavyweightModels result;const auto& world=checkpoint.world;if (!world.competition) return result;
    for (const auto& rival:world.competition->companies) if (rival.response) {
        const auto& state=*rival.response;const auto& definition=runtime::rival_definition(program,rival.definition_id);const auto& policy=*definition.response;
        QString detail=QCoreApplication::translate("HeavyweightBriefings","Reviews every %1 days; two reviews at or above %2 start a visible %3-day warning. Two reviews at or below %4 cause consolidation. Next public review: %5.\n")
            .arg(policy.review_interval.value()).arg(percent(policy.entry_pressure)).arg(policy.warning_lead.value()).arg(percent(policy.exit_pressure),date(state.next_review));
        QString score=QCoreApplication::translate("HeavyweightBriefings","Public review pending");
        if (!state.reviews.empty()) {
            const auto& review=state.reviews.back();score=percent(review.score)+QCoreApplication::translate("HeavyweightBriefings"," pressure · ")+date(review.day);
            detail+=QCoreApplication::translate("HeavyweightBriefings","Public pressure %1 on %2.\nRevenue substitution %3 (weight %4): %5 of delivered customer work at public service prices, compared with a published %6 monthly incumbent baseline.\nDemonstrated capability %7 (weight %8): %9 services delivered real customer work.\nAdoption %10 (weight %11): %12 / %13 customer tasks.\nDisclosed expansion %14 (weight %15): %16 publicly announced financing rounds. Private balances, checkpoints and undisclosed funding are absent from this briefing.\n")
                .arg(percent(review.score),date(review.day),percent(review.components[0])).arg(percent(core::BasisPoints{policy.weights[0]})).arg(money(review.substituted_revenue),money(policy.revenue_at_risk),percent(review.components[1])).arg(percent(core::BasisPoints{policy.weights[1]})).arg(review.public_services)
                .arg(percent(review.components[2])).arg(percent(core::BasisPoints{policy.weights[2]})).arg(number(review.delivered_tasks),number(review.customer_demand),percent(review.components[3])).arg(percent(core::BasisPoints{policy.weights[3]})).arg(review.disclosed_rounds);
        }
        detail+=QCoreApplication::translate("HeavyweightBriefings","Capability target: %1 services; disclosed-financing target: %2 rounds. Public quote values are normalized to a 30-day month. Component scores are capped at 100%; weighted integer terms round down.\n").arg(policy.capability_target).arg(policy.financed_target);
        detail+=QCoreApplication::translate("HeavyweightBriefings","One finite response allocation: %1. Mobilization pays for a specialist and a fixed-term compute order; capacity has a %2-day delivery lead. Further fundraising is unavailable to this response program.\n").arg(money(definition.cash)).arg(policy.delivery_lead.value());
        if (state.warned_on && !state.mobilized_on) detail+=QCoreApplication::translate("HeavyweightBriefings","Warning issued %1; earliest mobilization %2.\n").arg(date(*state.warned_on),date(core::advance_days(*state.warned_on,policy.warning_lead)));
        if (state.mobilized_on) detail+=QCoreApplication::translate("HeavyweightBriefings","Program funded %1; delivery boundary %2. Training and independent qualification still take actual resources and time.\n").arg(date(*state.mobilized_on),date(*state.ready_on));
        if (state.retired_on) detail+=QCoreApplication::translate("HeavyweightBriefings","Operations withdrew %1. Released staff, past costs and remaining fixed-term lease bills are recorded; the program receives no replacement capital.\n").arg(date(*state.retired_on));
        detail+=QCoreApplication::translate("HeavyweightBriefings","Player responses: retain a funded specialist position, compare released prices, expand through paid capacity, publish deliberately, or inspect the nonexclusive funded offers below.");
        result.responses.push_back({text(rival.definition_id),text(definition.name),score,detail,readable(std::string(runtime::to_string(state.phase))),false,
            {{QStringLiteral("phase"),text(runtime::to_string(state.phase))},{QStringLiteral("pressure"),state.reviews.empty()?QStringLiteral("0"):QString::number(state.reviews.back().score.value())}}});
    }
    for (const auto& offer:world.competition->offers) {
        const auto& definition=runtime::rival_definition(program,offer.company_id);
        QString detail=QCoreApplication::translate("HeavyweightBriefings","%1 reserves %2 from its own finite program cash. Reply before %3; ignoring the offer releases the reservation. This nonexclusive agreement pays once when its actual milestone is met.\n")
            .arg(text(definition.name),money(offer.amount),date(offer.respond_by));
        if (offer.kind==runtime::CompetitiveOfferKind::publication) detail+=QCoreApplication::translate("HeavyweightBriefings","Publish completed %1 research after acceptance and before the stated fulfillment date. Completion and publication must be real. Publication enables the existing delayed knowledge diffusion.\n").arg(readable(offer.subject));
        else detail+=QCoreApplication::translate("HeavyweightBriefings","Qualify %1, then deliver %2 additional customer tasks after acceptance. The fixed evaluation fee comes from this heavyweight; existing customer demand, invoices and useful-work totals stay in the shared market. Progress: %3 tasks.\n").arg(readable(offer.subject),number(offer.required_tasks),number(offer.delivered_tasks));
        detail+=QCoreApplication::translate("HeavyweightBriefings","Fulfillment term: %1 days, bounded by the campaign's last settlement morning. Unmet terms expire without a fee; past work and costs are retained.").arg(definition.response->offer_term.value());
        if (!offer.due_on) detail+=QCoreApplication::translate("HeavyweightBriefings","\nIf accepted today, fulfill before %1.").arg(date(std::min(core::OrdinalDay{world.simulation.end_date.value()-1},core::advance_days(world.simulation.date,definition.response->offer_term))));
        if (offer.due_on) detail+=QCoreApplication::translate("HeavyweightBriefings","\nAccepted %1; fulfill before %2.").arg(date(*offer.accepted_on),date(*offer.due_on));
        if (offer.resolved_on) detail+=QCoreApplication::translate("HeavyweightBriefings","\n%1 on %2.").arg(readable(std::string(runtime::to_string(offer.status))),date(*offer.resolved_on));
        const bool pending=offer.status==runtime::CompetitiveOfferStatus::pending && world.simulation.date<offer.respond_by;
        result.offers.push_back({text(offer.id),text(definition.name)+QCoreApplication::translate("HeavyweightBriefings"," · ")+(offer.kind==runtime::CompetitiveOfferKind::publication?QCoreApplication::translate("HeavyweightBriefings","Publication partnership"):QCoreApplication::translate("HeavyweightBriefings","Customer pilot evaluation")),money(offer.amount)+QCoreApplication::translate("HeavyweightBriefings"," funded fee"),detail,readable(std::string(runtime::to_string(offer.status))),pending,
            {{QStringLiteral("kind"),text(runtime::to_string(offer.kind))},{QStringLiteral("status"),text(runtime::to_string(offer.status))},{QStringLiteral("amountCents"),QString::number(offer.amount.value())}}});
    }
    return result;
}
QString heavyweight_notice_detail(const runtime::WorldState& world,const runtime::CompiledProgram& program,const runtime::RivalNotice& notice) {
    if (!world.competition) return {};
    const auto company=std::find_if(world.competition->companies.begin(),world.competition->companies.end(),[&](const auto& row){return row.definition_id==notice.company_id;});
    if (company==world.competition->companies.end() || !company->response) return {};
    if (notice.kind=="offer") return QCoreApplication::translate("HeavyweightBriefings","A funded nonexclusive agreement was offered on %1. Review its actual fee, milestone, response deadline and fulfillment term in Office → Rivals.").arg(date(notice.day));
    if (notice.kind!="response" && notice.kind!="warning") return {};
    const auto& rows=company->response->transitions;
    for (std::size_t i=0;i<rows.size();++i) if (notice.subject=="response."+std::to_string(i+1)) {
        const auto& event=rows[i];const auto& policy=*runtime::rival_definition(program,company->definition_id).response;
        QString result=QCoreApplication::translate("HeavyweightBriefings","Public response: %1 · %2 on %3.\n").arg(readable(std::string(runtime::to_string(event.phase))),readable(event.cause),date(event.day));
        if (event.due) result+=QCoreApplication::translate("HeavyweightBriefings","Announced boundary: %1.\n").arg(date(*event.due));
        result+=QCoreApplication::translate("HeavyweightBriefings","The pressure components and thresholds are visible in the public response briefing. Mobilization requires at least %1 days of warning, a finite budget, a paid specialist/compute order and the actual delivery lead time.").arg(policy.warning_lead.value());return result;
    }
    return {};
}
}
