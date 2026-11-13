#include "succession_internal.hpp"
#include <set>
#include <tuple>

namespace pgp::core {
using namespace succession_detail;
namespace {
DepartureEvidence evidence_at(const SimulationState& s,OrdinalDay day,std::size_t cursor) {
    const auto report=accounts_report_at(*s.accounts,cursor);
    const auto published=s.lab?std::ranges::count_if(s.lab->publications,[&](const auto& publication){return publication.day<day;}):0;
    DepartureEvidence result{report.cash,report.profit,report.debt,static_cast<std::uint32_t>(published)};
    if(s.market) for(const auto& contract:s.market->contracts) {
        if(!contract.stopped_on || *contract.stopped_on>day) continue;
        if(contract.status==ContractStatus::completed) ++result.completed_contracts;
        if(contract.status==ContractStatus::missed && *contract.stopped_on<day) ++result.missed_starts;
    }
    // Corporate responses settle on the prepared morning. A completed
    // publication dispute is public; a private concern is not evidence.
    const CorporateDispute* latest=nullptr;
    if(s.management && s.management->governance) for(const auto& dispute:s.management->governance->disputes)
        if(dispute.kind=="publication" && dispute.status==CorporateDisputeStatus::departed &&
            dispute.resolved_on && *dispute.resolved_on<=day &&
            (!latest || std::tie(dispute.resolved_on,dispute.id)>std::tie(latest->resolved_on,latest->id))) latest=&dispute;
    if(latest) result.public_dispute=latest->id;
    return result;
}
}
std::string departure_reference(std::uint64_t plan) {return "leadership.release/"+std::to_string(plan);}
const DepartureMessage& departure_message(const SuccessionCatalog& c,std::string_view id) {
    if(!c.departure) invalid("This charter has no founder departure announcement.");
    return named(c.departure->messages,id);
}
const DepartureReason& departure_reason(const SuccessionCatalog& c,std::string_view id) {
    if(!c.departure || !c.departure->press) invalid("This charter has no resignation statement builder.");
    return named(c.departure->press->reasons,id);
}
DepartureRelease preview_founder_departure(const SimulationState& s,const AnnounceFounderDeparture& request) {
    if(!s.succession || !s.succession->releases || !s.accounts || !request.plan || request.plan>s.succession->plans.size())
        invalid("Choose a funded founder succession plan before announcing the departure.");
    const auto& releases=*s.succession->releases;const auto& plan=s.succession->plans[request.plan-1];
    if(plan.incumbent!=1 || s.succession->terms.front().person!="founder" ||
       (plan.status!=SuccessionStatus::approved && plan.status!=SuccessionStatus::activated))
        invalid("Announce an approved or completed handover from the original founder.");
    if(std::ranges::any_of(releases,[&](const auto& r){return r.plan==request.plan;}))
        invalid("This founder handover already has its public announcement.");
    const auto& catalog=succession_catalog(s);
    (void)departure_message(catalog,request.message);
    DepartureRelease release{request.plan,s.accounts->postings.size(),request.message,s.date};
    if(catalog.departure->press) {
        release.reason=request.reason.empty()?catalog.departure->press->reasons.front().id:request.reason;
        (void)departure_reason(catalog,release.reason);
        release.evidence=evidence_at(s,s.date,s.accounts->postings.size());
    } else if(!request.reason.empty()) invalid("This charter has no resignation reasons.");
    return release;
}
std::string departure_assessment(const SimulationState& s,const DepartureRelease& release) {
    const auto& message=departure_message(succession_catalog(s),release.message);
    if(!message.claim) return "framing";
    if(!release.evidence) return "unsubstantiated";
    const auto& record=*release.evidence;
    if(*message.claim=="profit") return record.profit.value()>0?"supported":"challenged";
    if(*message.claim=="research") return record.publications>0?"supported":"challenged";
    if(*message.claim=="delivery") {
        if(record.missed_starts) return "challenged";
        return record.completed_contracts?"supported":"unsubstantiated";
    }
    if(*message.claim=="publication_commitment") return record.public_dispute?"challenged":"supported";
    invalid("This departure claim has no evidence rule.");
}
void announce_founder_departure(SimulationState& s,const AnnounceFounderDeparture& request,Cents protected_cash) {
    auto release=preview_founder_departure(s,request);
    const auto& message=departure_message(succession_catalog(s),request.message);
    if(message.fee>checked_unit_subtract(s.cash,protected_cash)) invalid("Preserve funds already committed to the company.");
    if(message.fee.value()>0) record_cash(*s.accounts,s.date,Cents{-message.fee.value()},Account::expenses,
        "Founder handover announcement: "+message.title,departure_reference(request.plan));
    s.cash=accounts_report(*s.accounts).cash;s.succession->releases->push_back(std::move(release));s.paused=true;
}
void validate_founder_departure(const SimulationState& s) {
    const auto& actor=*s.succession;if(!actor.releases) return;
    if(actor.releases->size()>succession_plan_limit) invalid("Too many founder announcements.");
    std::set<std::uint64_t> seen;std::size_t paid=0;OrdinalDay previous=s.accounts->starts_on;
    for(const auto& r:*actor.releases) {
        if(!r.plan || r.plan>actor.plans.size() || !seen.insert(r.plan).second || r.day<previous || r.day>s.date ||
           r.posting_cursor>s.accounts->postings.size()) invalid("A founder announcement lost its plan, date or financial cursor.");
        previous=r.day;const auto& p=actor.plans[r.plan-1];
        if(actor.terms.front().person!="founder" || p.incumbent!=1 || !p.decided_on || r.day<*p.decided_on ||
           p.status==SuccessionStatus::proposed || p.status==SuccessionStatus::rejected || p.status==SuccessionStatus::expired ||
           (p.finished_on && p.status!=SuccessionStatus::activated && r.day>*p.finished_on))
            invalid("A founder announcement predates its funded board decision or follows its cancellation.");
        const auto& message=departure_message(succession_catalog(s),r.message);
        const auto& press=succession_catalog(s).departure->press;
        if(press) {
            (void)departure_reason(succession_catalog(s),r.reason);
            if(r.evidence!=evidence_at(s,r.day,static_cast<std::size_t>(r.posting_cursor)))
                invalid("Resignation coverage differs from the announcement's dated company record.");
        } else if(r.evidence || !r.reason.empty()) invalid("Resignation evidence needs the authored press rules.");
        if(r.posting_cursor && s.accounts->postings[r.posting_cursor-1].day>r.day)
            invalid("An announcement predates its financial history.");
        if(message.fee.value()>0) {
            if(r.posting_cursor==s.accounts->postings.size()) invalid("An announcement has no paid expense.");
            const auto& post=s.accounts->postings[r.posting_cursor];
            if(post.day!=r.day || post.key!=departure_reference(r.plan) || post.amount!=message.fee ||
               post.debit!=Account::expenses || post.credit!=Account::cash || post.cause!="Founder handover announcement: "+message.title)
                invalid("The announcement differs from its actual company expense.");
            ++paid;
        }
    }
    const auto postings=std::ranges::count_if(s.accounts->postings,[](const auto& row){return row.key.starts_with("leadership.release/");});
    if(static_cast<std::size_t>(postings)!=paid) invalid("An announcement expense has no owning release.");
}
}
