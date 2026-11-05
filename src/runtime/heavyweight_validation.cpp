#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
const RivalCompany& owner(const WorldState& world,std::string_view id) {
    const auto& rows=world.competition->companies;const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.definition_id==id;});
    if (it==rows.end() || !it->response || !it->planning || !it->planning->entered_on) invalid("Offer owner is not an activated heavyweight program");return *it;
}
void offers(const WorldState& world,const CompiledProgram& program) {
    std::set<std::string> ids;std::set<std::pair<std::string,std::uint64_t>> reservations;
    std::set<std::string> paid_keys;
    for (const auto& offer:world.competition->offers) {
        const auto& rival=owner(world,offer.company_id);const auto& policy=*rival_definition(program,offer.company_id).response;
        const auto amount=offer.kind==CompetitiveOfferKind::publication?policy.publication_fee:policy.pilot_fee;
        const auto subject=offer.kind==CompetitiveOfferKind::publication?policy.publication_node:policy.pilot_product;
        const auto cutoff=OrdinalDay{world.simulation.end_date.value()-1};
        if (!ids.insert(offer.id).second || !reservations.emplace(offer.company_id,offer.reservation_id).second || offer.id!=offer.company_id+"."+std::string(to_string(offer.kind)) || offer.amount!=amount || amount.value()==0 || offer.subject!=subject ||
            offer.required_tasks!=(offer.kind==CompetitiveOfferKind::pilot?policy.pilot_tasks:0) || offer.issued_on<*rival.response->ready_on || offer.issued_on>world.simulation.date || offer.respond_by!=std::min(cutoff,advance_days(offer.issued_on,policy.offer_response)) || offer.respond_by<=offer.issued_on)
            invalid("Competitive offer changed its bounded identity, disclosed terms or issuance date");
        const auto found=std::find_if(rival.simulation.reservations.begin(),rival.simulation.reservations.end(),[&](const auto& row){return row.id==offer.reservation_id;});
        if (found==rival.simulation.reservations.end() || found->name!="Funded "+std::string(to_string(offer.kind))+" agreement" || !found->reversible) invalid("Competitive offer has no real owned cash reservation");
        if (std::count_if(world.competition->notices.begin(),world.competition->notices.end(),[&](const auto& row){return row.kind=="offer" && row.subject==offer.id && row.company_id==offer.company_id && row.day==offer.issued_on && row.response_due==offer.respond_by;})!=1)
            invalid("Competitive offer lost its unique public response notice");
        const bool pending=offer.status==CompetitiveOfferStatus::pending,active=offer.status==CompetitiveOfferStatus::active,paid=offer.status==CompetitiveOfferStatus::paid,failed=offer.status==CompetitiveOfferStatus::failed;
        if (to_string(offer.status)=="invalid" || (pending && (offer.accepted_on || world.simulation.date>=offer.respond_by)) || (active && !offer.accepted_on) ||
            ((pending || active)?offer.resolved_on.has_value():!offer.resolved_on.has_value()) || (offer.resolved_on && (*offer.resolved_on<offer.issued_on || *offer.resolved_on>world.simulation.date))) invalid("Competitive offer status/deadline history is inconsistent");
        if (offer.accepted_on) {
            if (*offer.accepted_on<offer.issued_on || *offer.accepted_on>=offer.respond_by || offer.due_on!=std::min(cutoff,advance_days(*offer.accepted_on,policy.offer_term)) || *offer.due_on<=*offer.accepted_on ||
                (offer.resolved_on && *offer.resolved_on<*offer.accepted_on) || (active && world.simulation.date>=*offer.due_on) || offer.status==CompetitiveOfferStatus::declined || offer.status==CompetitiveOfferStatus::cancelled)
                invalid("Accepted competitive offer lost its finite fulfillment window");
        } else if (offer.due_on || offer.last_delivery_day || offer.delivered_tasks!=0 || active || paid) invalid("Unaccepted offer manufactured work, a deadline or a payout");
        const auto expected=(pending || active || failed)?ReservationStatus::active:paid?ReservationStatus::completed:ReservationStatus::cancelled;
        if (found->status!=expected || found->reserved!=((pending || active || failed)?amount:Cents{0}) || found->consumed!=(paid?amount:Cents{0})) invalid("Competitive escrow differs from the actual reserved/consumed cash");
        if (failed && rival.simulation.phase!=DayPhase::closed) invalid("A failed counterparty has no actual company closure");
        if (offer.status==CompetitiveOfferStatus::expired && *offer.resolved_on<(offer.accepted_on?*offer.due_on:offer.respond_by)) invalid("Competitive offer expired before its actual deadline");
        if (offer.status==CompetitiveOfferStatus::declined && *offer.resolved_on>=offer.respond_by) invalid("Competitive decline occurred after the response window");
        if (offer.kind==CompetitiveOfferKind::pilot && offer.accepted_on) {
            std::uint64_t tasks=0;std::optional<OrdinalDay> last;
            const auto through=offer.resolved_on?OrdinalDay{offer.resolved_on->value()-1}:OrdinalDay{world.simulation.date.value()-1};
            for (const auto& day:world.simulation.market->days) if (day.day>=*offer.accepted_on && day.day<*offer.due_on && day.day<=through) {
                last=day.day;for (const auto& row:day.discretionary) if (row.product_id==offer.subject) tasks+=row.tasks;
                for (const auto& row:day.contracts) if (row.delivered_tasks>0) {
                    const auto& contracts=world.simulation.market->contracts;const auto contract=std::find_if(contracts.begin(),contracts.end(),[&](const auto& c){return c.id==row.contract_id;});
                    if (contract!=contracts.end() && contract->product_id==offer.subject) tasks+=row.delivered_tasks;
                }
            }
            if (offer.last_delivery_day!=last || offer.delivered_tasks!=tasks || (paid && tasks<offer.required_tasks)) invalid("Pilot payout/progress differs from actual additional customer deliveries");
        } else if (offer.delivered_tasks!=0 || offer.last_delivery_day) invalid("Publication agreement carries invented customer work");
        const auto key="competitive/"+offer.id;const auto& postings=world.simulation.accounts->postings;
        const auto receipt=std::find_if(postings.begin(),postings.end(),[&](const auto& row){return row.key==key;});
        if (paid) {
            paid_keys.insert(key);
            if (receipt==postings.end() || receipt->day!=offer.resolved_on || receipt->debit!=Account::cash || receipt->credit!=Account::revenue || receipt->amount!=amount) invalid("Competitive payment lacks the player's actual commercial receipt");
            const auto& receipts=rival.simulation.receipts;
            const auto expense=std::find_if(receipts.begin(),receipts.end(),[&](const auto& row){return row.reservation_id==offer.reservation_id && row.cause=="consume_reservation";});
            if (expense==receipts.end() || expense->day!=offer.resolved_on || expense->cash_delta!=Cents{-amount.value()}) invalid("Competitive payment has no equal actual counterparty expense");
            if (offer.kind==CompetitiveOfferKind::publication && std::none_of(world.simulation.lab->publications.begin(),world.simulation.lab->publications.end(),[&](const auto& row){return row.research_id==offer.subject && row.day>=*offer.accepted_on && row.day<*offer.due_on && row.day<=*offer.resolved_on;})) invalid("Publication agreement paid without the promised real public research");
        } else if (receipt!=postings.end()) invalid("Unpaid competitive agreement manufactured player cash");
    }
    for (const auto& posting:world.simulation.accounts->postings) if (posting.key.starts_with("competitive/") && !paid_keys.contains(posting.key)) invalid("Competitive revenue has no unique funded agreement");
    for (const auto& rival:world.competition->companies) if (rival.response && rival.planning->entered_on)
        for (const auto& row:rival.simulation.reservations) if (!reservations.contains({rival.definition_id,row.id})) invalid("Heavyweight has an undeclared cash commitment");
    for (const auto& rival:world.competition->companies) if (rival.response && rival.planning->entered_on)
        for (const auto& row:rival.simulation.receipts) if (row.cause=="reserve_funds" && !reservations.contains({rival.definition_id,row.reservation_id})) invalid("Heavyweight dropped a real funded offer from its continuation");
    for (const auto& row:world.competition->notices) if (row.kind=="offer" && !ids.contains(row.subject)) invalid("Competitive notice refers to an absent funded offer");
}
void replay(const WorldState& world,const CompiledProgram& program,const RivalCompany& rival) {
    const auto& saved=*rival.response;const auto& policy=*rival_definition(program,rival.definition_id).response;
    HeavyweightState state;state.next_review=world.campaign_start;std::size_t review_index=0,transition_index=0;
    std::optional<OrdinalDay> first_warning;
    const auto emit=[&](OrdinalDay day,HeavyweightPhase phase,std::string_view cause,std::optional<OrdinalDay> due={}) {
        if (transition_index>=saved.transitions.size() || saved.transitions[transition_index]!=HeavyweightTransition{day,phase,std::string(cause),due}) invalid("Heavyweight transition differs from its actual pressure, warning or resource boundary");
        const auto subject="response."+std::to_string(transition_index+1);
        if (std::count_if(world.competition->notices.begin(),world.competition->notices.end(),[&](const auto& n){return n.company_id==rival.definition_id && n.subject==subject && n.day==day && n.kind==(cause=="warning"?"warning":"response") && n.response_due==due;})!=1) invalid("Heavyweight transition lost its unique public notice");
        ++transition_index;state.phase=phase;
    };
    const auto end=std::min(world.simulation.date,OrdinalDay{world.simulation.end_date.value()-1});
    for (auto day=world.campaign_start;day<=end;day=advance_days(day,Days{1})) {
        const bool closed=rival.planning->entered_on && rival.simulation.phase==DayPhase::closed && day>rival.simulation.date;
        if (state.mobilized_on && !state.retired_on && closed && state.phase!=HeavyweightPhase::consolidate) emit(day,HeavyweightPhase::consolidate,"insolvency");
        if (day==state.next_review) {
            if (review_index>=saved.reviews.size() || saved.reviews[review_index]!=observe_heavyweight_pressure(world,program,rival.definition_id,day)) invalid("Heavyweight pressure differs from last-settled public information");
            const auto& review=saved.reviews[review_index++];state.next_review=advance_days(state.next_review,policy.review_interval);
            state.high_reviews=review.score>=policy.entry_pressure?state.high_reviews+1:0;state.low_reviews=review.score<=policy.exit_pressure?state.low_reviews+1:0;
            if (state.phase==HeavyweightPhase::watch && !state.mobilized_on && day>=advance_days(world.campaign_start,rival_definition(program,rival.definition_id).start_after)) {
                if (!state.warned_on && state.high_reviews>=2) {state.warned_on=day;if (!first_warning) first_warning=day;emit(day,HeavyweightPhase::watch,"warning",advance_days(day,policy.warning_lead));}
                else if (state.warned_on && state.low_reviews>=2) {state.warned_on.reset();emit(day,HeavyweightPhase::watch,"warning_withdrawn");}
            } else if ((state.phase==HeavyweightPhase::mobilize || state.phase==HeavyweightPhase::compete) && state.low_reviews>=2) emit(day,HeavyweightPhase::consolidate,"low_pressure");
        }
        if (state.phase==HeavyweightPhase::watch && state.warned_on && !state.mobilized_on && day>=advance_days(*state.warned_on,policy.warning_lead)) {
            state.mobilized_on=day;state.ready_on=advance_days(day,policy.delivery_lead);emit(day,HeavyweightPhase::mobilize,"mobilize",state.ready_on);
            const auto& simulation=rival.simulation;const auto lease=policy.lease_offer_id+"."+rival.definition_id;
            const bool hired=simulation.management && std::any_of(simulation.management->specialists.begin(),simulation.management->specialists.end(),[&](const auto& row){return row.offer_id==policy.specialist_id && row.day==day;});
            const bool leased=simulation.business_offers && std::any_of(simulation.business_offers->accepted.begin(),simulation.business_offers->accepted.end(),[&](const auto& row){return row.offer_id==lease && row.accepted_on==day;});
            if (!hired || !leased) emit(day,HeavyweightPhase::consolidate,"resources_unaffordable");
        }
        if (state.phase==HeavyweightPhase::mobilize && !closed && state.ready_on && day>=*state.ready_on) emit(day,HeavyweightPhase::compete,"compete");
        if (state.phase==HeavyweightPhase::compete && !closed && day.value()<world.simulation.end_date.value()-1) state.offers_issued=true;
        if (state.phase==HeavyweightPhase::consolidate) {
            bool committed=false;
            for (const auto& offer:world.competition->offers) if (offer.company_id==rival.definition_id && offer.accepted_on && *offer.accepted_on<=day && (!offer.resolved_on || *offer.resolved_on>day)) committed=true;
            if (!state.retired_on && state.mobilized_on && !closed && !committed) {
                state.retired_on=day;emit(day,HeavyweightPhase::consolidate,"withdrawn",advance_days(day,policy.cooldown));
            }
            if (state.retired_on && day>=advance_days(*state.retired_on,policy.cooldown)) emit(day,HeavyweightPhase::watch,"program_ended");
        }
    }
    if (review_index!=saved.reviews.size() || transition_index!=saved.transitions.size() || state.phase!=saved.phase || state.next_review!=saved.next_review || state.high_reviews!=saved.high_reviews || state.low_reviews!=saved.low_reviews || state.warned_on!=saved.warned_on || state.mobilized_on!=saved.mobilized_on || state.ready_on!=saved.ready_on || state.retired_on!=saved.retired_on || state.offers_issued!=saved.offers_issued || rival.warned_on!=first_warning || rival.planning->entered_on!=state.mobilized_on) invalid("Heavyweight continuation does not resume its complete public response history");
    if (static_cast<std::size_t>(std::count_if(world.competition->notices.begin(),world.competition->notices.end(),[&](const auto& row){return row.company_id==rival.definition_id && (row.kind=="response" || row.kind=="warning");}))!=saved.transitions.size()) invalid("Heavyweight has an invented public response notice");
    if (rival.planning->entered_on) for (const auto& day:rival.simulation.capacity->days)
        if ((state.ready_on && day.day<*state.ready_on) || (state.retired_on && day.day>=*state.retired_on)) {
            if (!day.projects.empty() || std::any_of(day.sources.begin(),day.sources.end(),[](const auto& row){return row.effective.value()!=0 || row.used.value()!=0;})) invalid("Heavyweight performed work before delivery or after releasing operations");
        }
    if (state.retired_on && (rival.simulation.capacity->available_staff!=0 || std::any_of(rival.simulation.capacity->projects.begin(),rival.simulation.capacity->projects.end(),[](const auto& row){return row.status==ProjectStatus::active;}) ||
        std::none_of(rival.simulation.receipts.begin(),rival.simulation.receipts.end(),[&](const auto& row){return row.cause=="withdraw_operations" && row.day==*state.retired_on;}))) invalid("Heavyweight withdrawal lacks actual released staff and stopped operations");
}
}
void validate_heavyweight_responses(const WorldState& world,const CompiledProgram& program,bool deep) {
    if (world.competition->offers.size()>40) invalid("Competitive offers exceed their finite bound");
    bool any=false;
    for (const auto& rival:world.competition->companies) {
        const auto& definition=rival_definition(program,rival.definition_id);
        if (rival.response.has_value()!=definition.response.has_value()) invalid("Heavyweight policy and saved response presence differ");
        if (!rival.response) continue;any=true;const auto& r=*rival.response;
        if (to_string(r.phase)=="invalid" || r.reviews.size()>8192 || r.transitions.size()>512 || r.next_review<world.campaign_start || r.high_reviews>8192 || r.low_reviews>8192) invalid("Heavyweight response exceeds its typed history bounds");
        if (deep) replay(world,program,rival);
    }
    if (!any && !world.competition->offers.empty()) invalid("Offers have no heavyweight response program");
    if (deep && any) offers(world,program);
}
}
