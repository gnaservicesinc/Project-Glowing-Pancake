#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace pgp::core {
namespace {
const GovernanceCatalog& catalog(const SimulationState& state) {
    if(!state.management || !state.management->people || !state.management->governance || !state.management->catalog.governance)
        throw std::invalid_argument("This company has no investor conversations.");
    return *state.management->catalog.governance;
}
const ContactFinanceRules& rules(const SimulationState& state) {
    const auto& definitions=catalog(state);
    if(!definitions.contact_finance) throw std::invalid_argument("Follow-up funding is unavailable in this scenario.");
    return *definitions.contact_finance;
}
const CorporateFinanceTerms& terms(const GovernanceCatalog& definitions,std::string_view id) {
    const auto found=std::ranges::find(definitions.financing,id,&CorporateFinanceTerms::id);
    if(found==definitions.financing.end()) throw std::invalid_argument("Choose a quoted financing offer.");
    return *found;
}
bool used(const SimulationState& state,std::string_view id) {
    const auto& definitions=catalog(state);
    const auto& offer=terms(definitions,id);
    return std::ranges::any_of(state.management->governance->motions,[&](const auto& motion) {
        return motion.kind==BoardMotionKind::financing && (motion.executed || motion.status==BoardMotionStatus::pending) &&
            terms(definitions,motion.subject).negotiation==offer.negotiation;
    });
}
std::uint32_t publications_on(const SimulationState& state,OrdinalDay day) {
    return state.lab?static_cast<std::uint32_t>(std::ranges::count_if(state.lab->publications,[&](const auto& publication){return publication.day<=day;})):0;
}
std::uint32_t reputation(const SimulationState& state,std::uint32_t publications) {
    return std::min(100U,state.management->people->founding_prestige+publications);
}
} // namespace

std::uint32_t contact_funding_reputation(const SimulationState& state,OrdinalDay day) {
    (void)catalog(state);return reputation(state,publications_on(state,day));
}

bool contact_funding_available(const SimulationState& state,std::string_view person) {
    if(!state.management || !state.management->people || !state.management->governance || !state.management->catalog.governance ||
        !state.management->catalog.governance->contact_finance || !state.accounts) return false;
    if(std::ranges::any_of(state.accounts->holdings,[](const auto& holder){return holder.holder=="institution_endowment";})) return false;
    return std::ranges::any_of(state.management->catalog.governance->contact_finance->offers,[&](const auto& offer){return offer.person==person;});
}

void request_contact_funding(SimulationState& state,PeopleContact& contact) {
    const auto& policy=rules(state);
    if(!contact.opening || !contact_funding_available(state,contact.person))
        throw std::invalid_argument("This contact does not offer follow-up equity funding to this organization.");
    if(contact.funding.size()>=contact_funding_limit) throw std::invalid_argument("This funding conversation has reached its history limit.");
    if(!contact.funding.empty() && state.date<advance_days(contact.funding.back().requested_on,policy.retry))
        throw std::invalid_argument("This investor will review another pitch from "+iso_date(advance_days(contact.funding.back().requested_on,policy.retry))+".");
    ContactFundingDiscussion discussion;
    discussion.id=static_cast<std::uint32_t>(contact.funding.size()+1);
    discussion.requested_on=state.date;discussion.expires_on=advance_days(state.date,policy.validity);
    discussion.publications=publications_on(state,state.date);discussion.reputation=reputation(state,discussion.publications);
    for(const auto& offer:policy.offers) if(offer.person==contact.person && discussion.reputation>=offer.minimum_reputation &&
        discussion.publications>=offer.minimum_publications && !used(state,offer.terms)) discussion.offers.push_back(offer.terms);
    if(discussion.offers.empty()) discussion.status="not_ready";
    contact.funding.push_back(std::move(discussion));
}

void respond_contact_funding(SimulationState& state,PeopleContact& contact,const RespondContactFunding& response) {
    (void)rules(state);
    const auto found=std::ranges::find(contact.funding,response.discussion,&ContactFundingDiscussion::id);
    if(found==contact.funding.end() || (found->status!="offered" && found->status!="held") || state.date>=found->expires_on)
        throw std::invalid_argument("Choose an open funding discussion before its quoted expiry date.");
    if(response.reply!="defer" && response.reply!="decline" && response.reply!="submit")
        throw std::invalid_argument("Choose to hold, decline or submit these terms to the board.");
    if(response.reply!="submit" && !response.terms.empty()) throw std::invalid_argument("Only a board submission selects a financing offer.");
    if(response.reply=="defer" && found->status=="held") throw std::invalid_argument("You are already thinking this offer over.");
    found->responded_on=state.date;
    if(response.reply=="submit") {
        if(std::ranges::find(found->offers,response.terms)==found->offers.end()) throw std::invalid_argument("Select terms actually offered in this conversation.");
        negotiate_corporate_finance(state,response.terms,true);
        found->status="submitted";found->selected=response.terms;found->motion=state.management->governance->motions.back().id;
    } else found->status=response.reply=="defer"?"held":"declined";
}

void validate_contact_funding(const SimulationState& state) {
    const auto& contacts=state.management->people->contacts;
    const auto* definitions=state.management->catalog.governance?&*state.management->catalog.governance:nullptr;
    const auto* policy=definitions && definitions->contact_finance?&*definitions->contact_finance:nullptr;
    std::map<std::string_view,const BoardMotion*> motions;
    if(state.management->governance) for(const auto& motion:state.management->governance->motions) motions.emplace(motion.id,&motion);
    std::set<std::string> submissions;
    for(const auto& contact:contacts) {
        if(contact.funding.size()>contact_funding_limit || (!contact.funding.empty() && (!policy || !contact.opening || !contact_funding_available(state,contact.person))))
            throw std::invalid_argument("Funding history needs its known investor and authored terms.");
        std::optional<OrdinalDay> previous;
        for(std::size_t i=0;i<contact.funding.size();++i) {
            const auto& discussion=contact.funding[i];
            if(discussion.id!=i+1 || discussion.requested_on<contact.exchanged_on || discussion.requested_on>state.date ||
                discussion.expires_on!=advance_days(discussion.requested_on,policy->validity) ||
                (previous && discussion.requested_on<advance_days(*previous,policy->retry)) || discussion.offers.size()>8 ||
                discussion.publications>(state.lab?state.lab->publications.size():0) || discussion.reputation!=reputation(state,discussion.publications))
                throw std::invalid_argument("An investor pitch must retain its actual dates, reputation and publication evidence.");
            previous=discussion.requested_on;
            if(state.lab) {
                for(std::size_t n=0;n<discussion.publications;++n) if(state.lab->publications[n].day>discussion.requested_on)
                    throw std::invalid_argument("An investor pitch cannot count future publications.");
                if(discussion.publications<state.lab->publications.size() && state.lab->publications[discussion.publications].day<discussion.requested_on)
                    throw std::invalid_argument("An investor pitch omitted an earlier publication.");
            }
            std::set<std::string> offered;
            for(const auto& id:discussion.offers) {
                const auto offer=std::ranges::find_if(policy->offers,[&](const auto& row){return row.terms==id && row.person==contact.person;});
                if(offer==policy->offers.end() || !offered.insert(id).second || discussion.reputation<offer->minimum_reputation || discussion.publications<offer->minimum_publications)
                    throw std::invalid_argument("An investor offered terms without their required company record.");
            }
            const bool unready=discussion.status=="not_ready",open=discussion.status=="offered",held=discussion.status=="held",
                declined=discussion.status=="declined",submitted=discussion.status=="submitted";
            if((!unready && !open && !held && !declined && !submitted) || unready!=discussion.offers.empty() ||
                discussion.responded_on.has_value()!=(held || declined || submitted) ||
                (discussion.responded_on && (*discussion.responded_on<discussion.requested_on || *discussion.responded_on>=discussion.expires_on || *discussion.responded_on>state.date)) ||
                (!submitted && (!discussion.selected.empty() || !discussion.motion.empty())))
                throw std::invalid_argument("A funding reply must retain its quoted choices and valid response date.");
            if(submitted) {
                const auto motion=motions.find(discussion.motion);
                if(!offered.contains(discussion.selected) || !submissions.insert(discussion.motion).second || motion==motions.end() ||
                    motion->second->kind!=BoardMotionKind::financing || motion->second->subject!=discussion.selected || motion->second->opened_on!=*discussion.responded_on)
                    throw std::invalid_argument("Submitted investor terms must identify their actual board motion.");
            }
        }
    }
    if(policy) for(const auto& [id,motion]:motions) if(motion->kind==BoardMotionKind::financing &&
        std::ranges::any_of(policy->offers,[&](const auto& offer){return offer.terms==motion->subject;}) && !submissions.contains(std::string{id}))
        throw std::invalid_argument("A contact financing motion bypassed its recorded conversation.");
}
} // namespace pgp::core
