#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString money(core::Cents value) {return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(value.value())/100);}
QString percent(core::BasisPoints value) {return QString::number(static_cast<double>(value.value())/100,'f',2)+"%";}
core::BasisPoints heldBy(const std::vector<core::EquityHolding>& holdings,std::string_view holder) {
    const auto found=std::ranges::find(holdings,holder,&core::EquityHolding::holder);
    return found==holdings.end()?core::BasisPoints{0}:found->shares;
}
const core::CorporateFinanceTerms& terms(const core::SimulationState& state,std::string_view id) {
    const auto& offers=state.management->catalog.governance->financing;
    return *std::ranges::find(offers,id,&core::CorporateFinanceTerms::id);
}
const core::PeopleContact* contact(const core::SimulationState& state,const QString& id) {
    const auto& contacts=state.management->people->contacts;
    const auto found=std::ranges::find(contacts,id.toStdString(),&core::PeopleContact::person);
    return found==contacts.end()?nullptr:&*found;
}
QVariantMap offerRow(const core::SimulationState& state,const core::CorporateFinanceTerms& offer) {
    const auto holder=state.succession?state.succession->founder_holder:std::string{"founder"};
    auto after=state.accounts->holdings;
    if(!offer.donation)core::dilute_holdings(after,"investor/"+offer.id,offer.acquired);
    const auto before_shares=heldBy(state.accounts->holdings,holder);
    const auto after_shares=heldBy(after,holder);
    return {{"id",text(offer.id)},{"name",text(offer.name)},{"cash",money(offer.proceeds)},
        {"equity",percent(offer.acquired)},{"donation",offer.donation},{"seat",offer.board_seat},{"founderBefore",percent(before_shares)},
        {"founderAfter",percent(after_shares)},{"fraction",static_cast<double>(after_shares.value())/10000}};
}
} // namespace

QVariantMap WorldController::contactFunding(QString person) const {
    if(!contactsAvailable()) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto* known=contact(state,person);
    if(!known || !core::contact_funding_available(state,known->person)) return {};
    const auto& policy=*state.management->catalog.governance->contact_finance;
    const bool donor=core::nonprofit_organization(state);
    const auto next=known->funding.empty()?state.date:core::advance_days(known->funding.back().requested_on,policy.retry);
    QVariantList discussions;
    for(auto it=known->funding.rbegin();it!=known->funding.rend();++it) {
        const auto& discussion=*it;
        auto status=text(discussion.status);
        bool open=discussion.status=="offered" || discussion.status=="held";
        if(open && state.date>=discussion.expires_on) {open=false;status="expired";}
        if(discussion.status=="submitted") {
            const auto& motions=state.management->governance->motions;
            const auto& motion=*std::ranges::find(motions,discussion.motion,&core::BoardMotion::id);
            status=motion.executed?QStringLiteral("funded"):motion.status==core::BoardMotionStatus::pending?QStringLiteral("board"):
                motion.status==core::BoardMotionStatus::rejected?QStringLiteral("rejected"):QStringLiteral("vote_expired");
        }
        QVariantList offers;
        for(const auto& id:discussion.offers) offers.push_back(offerRow(state,terms(state,id)));
        const auto label=status=="offered"?tr("Terms offered"):status=="held"?tr("Thinking it over"):
            status=="not_ready"?tr("No offer yet"):status=="expired"?tr("Offer expired"):
            status=="declined"?tr("You declined"):status=="funded"?(donor?tr("Gift received"):tr("Round funded")):
            status=="board"?tr("Awaiting board vote"):status=="rejected"?tr("Board rejected"):tr("Board vote expired");
        const auto reply=status=="not_ready"?
            (donor?tr("We have no available gift for this record. Review the publication and independent evaluation requirements before applying again."):
                tr("We have no available terms for this company record. Build your reputation and publish completed research, or resolve any pending round before pitching again.")):
            status=="funded"?(donor?tr("The trustees approved the gift. The funds belong to the institution. The donor received no ownership and personal savings are unchanged."):
                tr("The board approved the round. The company received the agreed capital and ownership changed under those terms.")):
            status=="board"?tr("The terms are with your board. No capital changes hands until approval."):
            (donor?tr("These gift terms follow our review of the institution's record. Each program can fund once, with no shares sold."):
                tr("These were the terms offered after reviewing your company record. Alternatives in the same round can be funded once."));
        discussions.push_back(QVariantMap{{"id",discussion.id},{"status",status},{"label",label},{"reply",reply},
            {"date",text(core::iso_date(discussion.requested_on))},{"expires",text(core::iso_date(discussion.expires_on))},
            {"record",donor?tr("At this proposal: reputation %1/100 · %2 publications · %3 completed independent evaluations").arg(discussion.reputation).arg(discussion.publications).arg(discussion.evaluations):
                tr("Reputation %1/100 · %2 published research results at this pitch").arg(discussion.reputation).arg(discussion.publications)},
            {"open",open},{"held",discussion.status=="held"},{"submitted",discussion.status=="submitted"},
            {"offers",offers},{"selected",text(discussion.selected)}});
    }
    QStringList requirements;
    for(const auto& offer:policy.offers) if(offer.person==known->person) {
        auto requirement=tr("%1: reputation %2/100 and %3 publications").arg(text(terms(state,offer.terms).name)).arg(offer.minimum_reputation).arg(offer.minimum_publications);
        if(donor || offer.minimum_evaluations)requirement+=tr("; %1 completed independent evaluations").arg(offer.minimum_evaluations);
        requirements.push_back(requirement);
    }
    const auto reputation=core::contact_funding_reputation(state,state.date);
    return {{"available",true},{"donor",donor},{"canRequest",state.date>=next && known->funding.size()<core::contact_funding_limit},
        {"next",text(core::iso_date(next))},{"reputation",reputation},
        {"record",tr("Reputation %1/100: founding prestige, community public interest and published research, capped at 100.").arg(reputation)},
        {"requirements",requirements.join("\n")},{"discussions",discussions}};
}

QVariantMap WorldController::contactFundingQuote(QString person,int discussion,QString offer) const {
    if(!contactsAvailable() || discussion<1) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    try {
        const auto device=navigation_.value("device").toString().toStdString();
        core::Simulation preview{state};
        const auto result=preview.submit({state.revision,core::ManagePeopleContact{core::RespondContactFunding{
            person.toStdString(),device,static_cast<std::uint32_t>(discussion),"submit",offer.toStdString(),0}}});
        if(!result.accepted()) return {{"allowed",false},{"detail",text(result.explanation)}};
        const auto& quote=terms(state,offer.toStdString());
        const auto row=offerRow(state,quote);
        const auto detail=quote.donation?tr("%1\n\nGift to the institution on approval: %2.\nThe donor receives no ownership. The institutional endowment remains intact and your personal savings stay separate.\n\nTrustee representative: %3.\n\nSending these terms transfers no money. A majority of the board must approve. The gift is recorded as a contribution to the institution, not sales revenue or personal income. Each program can fund once.")
            .arg(text(quote.name),money(quote.proceeds),quote.board_seat?tr("one additional seat"):tr("no new seat")):
            tr("%1\n\nCompany capital on approval: %2.\nInvestor receives %3 of the company after the issue. Every existing holding and unvested claim is diluted once.\nYour current stake would change from %4 to %5.\n\nBoard representative: %6.\n\nSending these terms spends nothing and transfers no shares. A majority of the board must approve; its current cash and dilution limits still apply. The money belongs to the company, and is contributed capital rather than sales revenue.")
            .arg(text(quote.name),money(quote.proceeds),percent(quote.acquired),row.value("founderBefore").toString(),row.value("founderAfter").toString(),quote.board_seat?tr("one additional seat"):tr("no new seat"));
        return {{"allowed",true},{"donation",quote.donation},{"detail",detail},{"cash",money(quote.proceeds)},{"equity",percent(quote.acquired)}};
    } catch(const std::exception& error) {return {{"allowed",false},{"detail",text(error.what())}};}
}

void WorldController::requestContactFunding(QString person,QString revision) {
    if(!hqReview(revision)) return;
    emit actionRequested(core::ManagePeopleContact{core::RequestContactFunding{person.toStdString(),navigation_.value("device").toString().toStdString(),0}});
}
void WorldController::respondContactFunding(QString person,int discussion,QString reply,QString offer,QString revision) {
    if(!hqReview(revision) || discussion<1) return;
    emit actionRequested(core::ManagePeopleContact{core::RespondContactFunding{person.toStdString(),navigation_.value("device").toString().toStdString(),
        static_cast<std::uint32_t>(discussion),reply.toStdString(),offer.toStdString(),0}});
}
} // namespace pgp::bridge
