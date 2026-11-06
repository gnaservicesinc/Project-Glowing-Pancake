#include "competition_internal.hpp"
#include <algorithm>
#include <stdexcept>
namespace pgp::runtime {
using namespace core;
std::string_view to_string(CompetitiveOfferKind kind) noexcept {switch(kind) {case CompetitiveOfferKind::publication:return "publication";case CompetitiveOfferKind::pilot:return "pilot";}return "invalid";}
std::string_view to_string(CompetitiveOfferStatus status) noexcept {
    switch(status) {case CompetitiveOfferStatus::pending:return "pending";case CompetitiveOfferStatus::active:return "active";case CompetitiveOfferStatus::paid:return "paid";case CompetitiveOfferStatus::declined:return "declined";case CompetitiveOfferStatus::expired:return "expired";case CompetitiveOfferStatus::cancelled:return "cancelled";case CompetitiveOfferStatus::failed:return "failed";}return "invalid";
}
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
RivalCompany& owner(WorldState& world,std::string_view id) {
    auto& rows=world.competition->companies;const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.definition_id==id;});
    if (it==rows.end() || !it->response || !it->planning || !it->planning->entered_on) invalid("Competitive offer has no funded heavyweight program");return *it;
}
bool open(const CompetitiveOffer& offer) {return offer.status==CompetitiveOfferStatus::pending || offer.status==CompetitiveOfferStatus::active;}
void release(WorldState& world,RivalCompany& rival,CompetitiveOffer& offer,CompetitiveOfferStatus status) {
    if (rival.simulation.phase==DayPhase::closed) {offer.status=CompetitiveOfferStatus::failed;offer.resolved_on=campaign_day(world);return;}
    if (!competition_detail::submit_rival(rival,CancelReservation{offer.reservation_id})) invalid("Funded competitive offer could not release its own escrow");
    offer.status=status;offer.resolved_on=campaign_day(world);
}
std::uint64_t delivered(const MarketState& market,const MarketDay& day,std::string_view product) {
    std::uint64_t result=0;for (const auto& row:day.discretionary) if (row.product_id==product) result+=row.tasks;
    for (const auto& row:day.contracts) if (row.delivered_tasks>0) {
        const auto contract=std::find_if(market.contracts.begin(),market.contracts.end(),[&](const auto& c){return c.id==row.contract_id;});
        if (contract!=market.contracts.end() && contract->product_id==product) result+=row.delivered_tasks;
    }
    return result;
}
}
namespace competition_detail {
void issue_competitive_offers(WorldState& world,const CompiledProgram& program,RivalCompany& rival) {
    auto& response=*rival.response;if (response.offers_issued || campaign_day(world).value()>=campaign_calendar_limit(world).value()-1) return;
    if(observing_world(world)) {response.offers_issued=true;return;}
    const auto& definition=rival_definition(program,rival.definition_id);const auto& policy=*definition.response;
    response.offers_issued=true;
    for (const auto kind:{CompetitiveOfferKind::publication,CompetitiveOfferKind::pilot}) {
        const auto subject=kind==CompetitiveOfferKind::publication?policy.publication_node:policy.pilot_product;
        const auto amount=kind==CompetitiveOfferKind::publication?policy.publication_fee:policy.pilot_fee;
        if (amount.value()==0) continue;
        if (kind==CompetitiveOfferKind::publication && std::any_of(world.simulation.lab->publications.begin(),world.simulation.lab->publications.end(),[&](const auto& row){return row.research_id==subject;})) continue;
        Cents protected_cash=definition.planning->reserve_floor;
        for (const auto& reservation:rival.simulation.reservations) if (reservation.status==ReservationStatus::active) protected_cash=checked_unit_add(protected_cash,reservation.reserved);
        if (checked_unit_add(amount,protected_cash)>rival.simulation.cash) continue;
        if (world.competition->offers.size()>=40) invalid("Competitive offer count exceeds its finite bound");
        const auto id=rival.definition_id+"."+std::string(to_string(kind));
        if (!submit_rival(rival,ReserveFunds{"Funded "+std::string(to_string(kind))+" agreement",amount,true})) invalid("A feasible competitive offer could not reserve actual cash");
        const auto reservation=rival.simulation.reservations.back().id;
        CompetitiveOffer offer;offer.id=id;offer.company_id=rival.definition_id;offer.subject=subject;offer.kind=kind;offer.amount=amount;offer.reservation_id=reservation;
        offer.issued_on=campaign_day(world);offer.respond_by=std::min(OrdinalDay{campaign_calendar_limit(world).value()-1},advance_days(offer.issued_on,policy.offer_response));
        offer.required_tasks=kind==CompetitiveOfferKind::pilot?policy.pilot_tasks:0;
        world.competition->offers.push_back(std::move(offer));
        world.competition->notices.push_back({rival.definition_id,"offer",campaign_day(world),world.competition->offers.back().respond_by,id});
    }
}
void cancel_pending_competitive_offers(WorldState& world,RivalCompany& rival) {
    for (auto& offer:world.competition->offers) if (offer.company_id==rival.definition_id && offer.status==CompetitiveOfferStatus::pending) release(world,rival,offer,CompetitiveOfferStatus::cancelled);
}
}
void respond_to_competitive_offer(WorldState& world,const CompiledProgram& program,std::string_view id,bool accept) {
    if (!world.competition || !world.competition->setup || world.simulation.phase!=DayPhase::work_ready) invalid("Competitive offers require a prepared operating company");
    auto& offers=world.competition->offers;const auto it=std::find_if(offers.begin(),offers.end(),[&](const auto& row){return row.id==id;});
    if (it==offers.end() || it->status!=CompetitiveOfferStatus::pending || campaign_day(world)>=it->respond_by) invalid("This competitive offer is absent, answered or past its response deadline");
    auto& rival=owner(world,it->company_id);
    if (rival.simulation.phase==DayPhase::closed || rival.response->phase!=HeavyweightPhase::compete) invalid("The offering program is no longer competing");
    if (!accept) {release(world,rival,*it,CompetitiveOfferStatus::declined);return;}
    if (it->kind==CompetitiveOfferKind::publication && std::any_of(world.simulation.lab->publications.begin(),world.simulation.lab->publications.end(),[&](const auto& row){return row.research_id==it->subject;})) invalid("This result is already public; the offer requests a new publication");
    if (it->kind==CompetitiveOfferKind::pilot && std::none_of(world.simulation.market->products.begin(),world.simulation.market->products.end(),[&](const auto& row){return row.id==it->subject;})) invalid("Qualify the offered pilot service before accepting its delivery commitment");
    it->status=CompetitiveOfferStatus::active;it->accepted_on=campaign_day(world);
    it->due_on=std::min(OrdinalDay{campaign_calendar_limit(world).value()-1},advance_days(*it->accepted_on,rival_definition(program,rival.definition_id).response->offer_term));
}
void update_competitive_offers(WorldState& world,const CompiledProgram&) {
    if (!world.competition || world.competition->offers.empty()) return;
    for (auto& offer:world.competition->offers) if (open(offer)) {
        auto& rival=owner(world,offer.company_id);const auto day=campaign_day(world);
        if (rival.simulation.phase==DayPhase::closed) {release(world,rival,offer,CompetitiveOfferStatus::failed);continue;}
        if (offer.status==CompetitiveOfferStatus::pending) {if (day>=offer.respond_by) release(world,rival,offer,CompetitiveOfferStatus::expired);continue;}
        bool complete=false;
        if (offer.kind==CompetitiveOfferKind::publication) {
            complete=std::any_of(world.simulation.lab->publications.begin(),world.simulation.lab->publications.end(),[&](const auto& publication){return publication.research_id==offer.subject && publication.day>=*offer.accepted_on && publication.day<*offer.due_on;});
        } else if (!world.simulation.market->days.empty()) {
            const auto& record=world.simulation.market->days.back();
            if (record.day>=*offer.accepted_on && record.day<*offer.due_on && (!offer.last_delivery_day || record.day>*offer.last_delivery_day)) {
                offer.delivered_tasks+=delivered(*world.simulation.market,record,offer.subject);offer.last_delivery_day=record.day;
            }
            complete=offer.delivered_tasks>=offer.required_tasks;
        }
        if (complete) {
            if (!competition_detail::submit_rival(rival,ConsumeReservation{offer.reservation_id,offer.amount})) invalid("A funded competitive agreement could not pay its promised amount");
            record_cash(*world.simulation.accounts,day,offer.amount,Account::revenue,"Heavyweight "+std::string(to_string(offer.kind))+" agreement", "competitive/"+offer.id);
            world.simulation.cash=accounts_report(*world.simulation.accounts).cash;offer.status=CompetitiveOfferStatus::paid;offer.resolved_on=day;
        } else if (day>=*offer.due_on) release(world,rival,offer,CompetitiveOfferStatus::expired);
    }
}
}
