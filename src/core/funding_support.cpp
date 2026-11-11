#include <pgp/core/simulation.hpp>
#include <pgp/core/workplace_research.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}

template<class Rows> const auto* find(const Rows& rows,std::string_view id) {
    const auto it=std::ranges::find_if(rows,[&](const auto& row){return row.id==id;});
    return it==rows.end()?nullptr:&*it;
}

OrdinalDay activated(OrdinalDay claimed) {return advance_days(claimed,Days{1});}
bool award_active(const FundingAward& award,const FundingSupportOffer& offer,OrdinalDay day) {
    return day>award.accepted_on && day.value()-award.accepted_on.value()<=offer.use_days.value();
}
bool active_bill(const RecurringObligation* bill,OrdinalDay day) {
    return bill && bill->starts_on<=day && (!bill->ends_before || day<*bill->ends_before);
}

Cents award_contribution(const FundingSupportOffer& offer,Cents unused,Cents gross) {
    const auto limit=mul_div_floor(static_cast<std::uint64_t>(gross.value()),
                                  static_cast<std::uint64_t>(offer.coverage.value()),10000).quotient;
    return std::min(unused,Cents{limit});
}

Cents contribution(const FundingSupportOffer& offer,const FundingAward& award,OrdinalDay day,Cents gross) {
    if (!award_active(award,offer,day)) return Cents{0};
    return award_contribution(offer,checked_unit_subtract(offer.limit,award.used),gross);
}

struct GrantQuote {
    Cents gross{0},sponsored{0};
    std::size_t award=0;
};

GrantQuote research_quote(const SimulationState& state,std::string_view research) {
    if (!state.lab) invalid("This company has no research laboratory");
    const auto* topic=find(state.lab->catalog.research,research);
    if (!topic) invalid("Research topic is unavailable");
    GrantQuote result{topic->cash};
    if (!state.management) return result;
    const auto& management=*state.management;
    for (std::size_t i=0;i<management.funding_support.awards.size();++i) {
        const auto& award=management.funding_support.awards[i];
        const auto* offer=find(management.catalog.support_offers,award.offer_id);
        if (!offer || offer->kind!=FundingSupportKind::research_grant ||
            std::ranges::find(offer->targets,research)==offer->targets.end()) continue;
        result.sponsored=contribution(*offer,award,state.date,result.gross);
        result.award=i;
        break;
    }
    return result;
}

void record_use(FundingSupportState& support,std::size_t award,std::string target,OrdinalDay day,Cents amount) {
    if (support.uses.size()>=funding_use_limit) invalid("The funding redemption history is full");
    auto& claimed=support.awards[award];
    claimed.used=checked_unit_add(claimed.used,amount);
    support.uses.push_back({claimed.offer_id,std::move(target),day,amount});
}

std::string supplier_credit_key(std::string_view id,OrdinalDay day) {
    return std::string(supplier_credit_prefix)+std::string(id)+"/"+iso_date(day);
}

bool supported_bill(const SimulationState& state,std::string_view id) {
    if (id=="compute_lease" || id=="founder_compute") return true;
    if (!id.starts_with("offer.lease/") || !state.business_offers) return false;
    const auto* offer=find(state.business_offers->catalog,id.substr(std::string_view{"offer.lease/"}.size()));
    return offer && offer->kind==BusinessOfferKind::compute_lease;
}

void validate_credit_posting(const AccountsState& accounts,const FundingSupportOffer& offer,OrdinalDay day,Cents gross,Cents credit) {
    const auto date=from_ordinal(day);
    const auto invoice="recurring/"+offer.targets.front()+"/"+iso_date(to_ordinal({date.year,date.month,1}));
    const auto credit_key=supplier_credit_key(offer.id,day);
    const auto charge_key="accrue/"+invoice+"/"+iso_date(day);
    auto row=std::ranges::lower_bound(accounts.postings,day,{},&Posting::day);
    bool charged=false,applied=false;
    for (;row!=accounts.postings.end() && row->day==day;++row) {
        if (row->key==charge_key && row->document_id==invoice && row->amount==gross &&
            row->debit==Account::expenses && row->credit==Account::payables) charged=true;
        if (row->key==credit_key && row->document_id==invoice && row->amount==credit &&
            row->debit==Account::payables && row->credit==Account::expenses) applied=charged;
    }
    if (!applied) invalid("Cloud credit lacks its matching supplier expense and noncash settlement");
}
}

void validate_funding_offers(const std::vector<FundingSupportOffer>& offers) {
    if (offers.size()>funding_offer_limit) invalid("Too many funding support offers");
    const auto label=[](std::string_view value,std::size_t maximum) {
        return !value.empty() && value.size()<=maximum && std::ranges::none_of(value,[](unsigned char ch){return ch<32 || ch==127;});
    };
    std::set<std::string_view> ids;
    std::set<std::pair<FundingSupportKind,std::string_view>> targets;
    for (const auto& offer:offers) {
        if (!label(offer.id,64) || !ids.insert(offer.id).second || !label(offer.name,128) ||
            offer.limit.value()<=0 || offer.limit.value()>1'000'000'000 || offer.coverage.value()==0 ||
            offer.opens_after.value()>36524 || offer.claim_days.value()==0 || offer.claim_days.value()>3660 ||
            offer.use_days.value()==0 || offer.use_days.value()>366 || offer.targets.empty() || offer.targets.size()>8 ||
            (offer.kind==FundingSupportKind::cloud_credit && offer.targets.size()!=1) ||
            (offer.kind!=FundingSupportKind::research_grant && offer.kind!=FundingSupportKind::cloud_credit))
            invalid("Funding support needs a distinct offer, eligible targets, finite dates and a bounded share and amount");
        for (const auto& target:offer.targets)
            if (!label(target,128) || !targets.emplace(offer.kind,target).second)
                invalid("Funding support cannot overlap another award for the same expense");
    }
}

FundingSupportStatus funding_support_status(const SimulationState& state,const FundingSupportOffer& offer) {
    FundingSupportStatus result;
    if (!state.management || !state.accounts) return result;
    result.opens_on=advance_days(state.accounts->starts_on,offer.opens_after);
    result.claim_before=advance_days(result.opens_on,offer.claim_days);
    const auto& awards=state.management->funding_support.awards;
    const auto award=std::ranges::find(awards,offer.id,&FundingAward::offer_id);
    if (award!=awards.end()) {
        result.active_on=activated(award->accepted_on);
        result.expires_before=advance_days(*result.active_on,offer.use_days);
        result.unused=checked_unit_subtract(offer.limit,award->used);
        return result;
    }
    const auto start=activated(state.date);
    result.claimable=state.date>=result.opens_on && state.date<result.claim_before && start<state.end_date;
    if (result.claimable && offer.kind==FundingSupportKind::cloud_credit) {
        const auto* bill=find(state.accounts->recurring,offer.targets.front());
        result.claimable=active_bill(bill,start);
    }
    return result;
}

void accept_funding_support(SimulationState& state,const FundingSupportOffer& offer) {
    if (!funding_support_status(state,offer).claimable)
        invalid("The funding award was already claimed, is outside its window or lacks its eligible lease");
    auto& awards=state.management->funding_support.awards;
    if (awards.size()>=funding_offer_limit) invalid("Funding award history is full");
    (void)advance_days(activated(state.date),offer.use_days);
    awards.push_back({offer.id,state.date,Cents{0}});
}

Cents research_cash_fee(const SimulationState& state,std::string_view research) {
    const auto quote=research_quote(state,research);
    return checked_unit_subtract(quote.gross,quote.sponsored);
}

Cents start_funded_research(SimulationState& state,std::string_view research,const LabAllocation& allocation,Cents protected_cash,const ResearchCooperation* cooperation) {
    if (!state.accounts || !state.capacity) invalid("Research needs company accounts and compute capacity");
    require_workplace_research(state,research,allocation);
    const auto quote=research_quote(state,research);
    const auto team=state.lab->catalog.creative_research.contains(std::string{research})?research_team(state,allocation.researchers):TeamDynamics{};
    start_research(*state.lab,*state.accounts,*state.capacity,state.date,research,allocation,protected_cash,quote.sponsored,team.creativity,team.compatibility_bp,
        cooperation?std::optional{*cooperation}:std::nullopt);
    if (quote.sponsored.value()>0)
        record_use(state.management->funding_support,quote.award,state.lab->research.back().project_id,state.date,quote.sponsored);
    state.cash=accounts_report(*state.accounts).cash;
    return checked_unit_subtract(quote.gross,quote.sponsored);
}

std::vector<RecurringCredit> redeem_cloud_credits(SimulationState& state) {
    std::vector<RecurringCredit> credits;
    if (!state.management) return credits;
    auto& support=state.management->funding_support;
    for (std::size_t i=0;i<support.awards.size();++i) {
        const auto& award=support.awards[i];
        const auto* offer=find(state.management->catalog.support_offers,award.offer_id);
        if (!offer || offer->kind!=FundingSupportKind::cloud_credit || award.used>=offer->limit || !award_active(award,*offer,state.date)) continue;
        const auto* bill=find(state.accounts->recurring,offer->targets.front());
        if (!bill) invalid("Claimed cloud credit lost its supplier obligation");
        const auto amount=contribution(*offer,award,state.date,recurring_charge_on(*bill,state.date));
        if (amount.value()==0) continue;
        record_use(support,i,bill->id,state.date,amount);
        credits.push_back({bill->id,supplier_credit_key(offer->id,state.date),amount});
    }
    return credits;
}

void validate_funding_support(const SimulationState& state,bool deep) {
    const auto credit_postings=state.accounts?state.accounts->postings.totals().supplier_credits:0;
    const auto* support=state.management?&state.management->funding_support:nullptr;
    if (support && (support->awards.size()>funding_offer_limit || support->uses.size()>funding_use_limit))
        invalid("Funding support exceeds its bounded history");
    if (!deep) return;
    const auto sponsored_runs=state.lab?std::ranges::count_if(state.lab->research,[](const auto& run){return run.sponsored_fee.value()>0;}):0;
    if (!support) {
        if (credit_postings || sponsored_runs) invalid("Funding assistance lacks its awarded terms");
        return;
    }
    const auto& offers=state.management->catalog.support_offers;
    const auto& accounts=*state.accounts;
    for (const auto& offer:offers) for (const auto& target:offer.targets) {
        if (offer.kind==FundingSupportKind::research_grant) {
            if (!find(state.lab->catalog.research,target)) invalid("A grant names an unavailable research topic");
        } else if (!supported_bill(state,target)) invalid("A cloud credit must name its actual leased compute obligation");
    }
    using UseKey=std::tuple<std::string_view,std::string_view,OrdinalDay>;
    std::map<UseKey,Cents> uses;
    auto previous=accounts.starts_on;
    for (const auto& use:support->uses) {
        if (use.day<previous || use.day>state.date || use.amount.value()<=0 ||
            !uses.emplace(UseKey{use.offer_id,use.target,use.day},use.amount).second)
            invalid("Funding redemption history contains an invalid date, amount or duplicate use");
        previous=use.day;
    }
    std::size_t cloud_uses=0;
    std::ptrdiff_t grant_uses=0;
    std::set<std::string_view> claimed;
    previous=accounts.starts_on;
    for (const auto& award:support->awards) {
        const auto* offer=find(offers,award.offer_id);
        if (!offer || !claimed.insert(award.offer_id).second || award.accepted_on<previous || award.accepted_on>state.date)
            invalid("Funding award is unknown, repeated or out of date order");
        const auto opens=advance_days(accounts.starts_on,offer->opens_after);
        const auto start=activated(award.accepted_on);
        const auto end=advance_days(start,offer->use_days);
        if (award.accepted_on<opens || award.accepted_on>=advance_days(opens,offer->claim_days))
            invalid("Funding award was claimed outside its disclosed window");
        previous=award.accepted_on;
        auto remaining=offer->limit;
        const auto verify_use=[&](std::string_view target,OrdinalDay day,Cents amount) {
            const auto found=uses.find({offer->id,target,day});
            if (found==uses.end() || found->second!=amount) invalid("Funding redemption differs from the eligible expense or remaining award");
            remaining=checked_unit_subtract(remaining,amount);
            uses.erase(found);
        };
        if (offer->kind==FundingSupportKind::research_grant) {
            for (const auto& run:state.lab->research) {
                if (std::ranges::find(offer->targets,run.research_id)==offer->targets.end()) continue;
                const auto* project=find(state.capacity->projects,run.project_id);
                if (!project) invalid("Research grant lacks its actual work");
                if (project->starts_on<start || project->starts_on>=end) continue;
                const auto amount=award_contribution(*offer,remaining,find(state.lab->catalog.research,run.research_id)->cash);
                if (run.sponsored_fee!=amount) invalid("Research co-payment differs from its remaining grant");
                if (amount.value()==0) continue;
                verify_use(run.project_id,project->starts_on,amount);
                ++grant_uses;
            }
        } else {
            const auto* bill=find(accounts.recurring,offer->targets.front());
            if (!bill || bill->starts_on>start) invalid("Cloud credit lacks its eligible supplier");
            for (auto day=start;accounts.closed_through && day<=*accounts.closed_through && day<end && remaining.value()>0;
                 day=advance_days(day,Days{1})) {
                const auto gross=recurring_charge_on(*bill,day);
                const auto amount=award_contribution(*offer,remaining,gross);
                if (amount.value()==0) continue;
                verify_use(bill->id,day,amount);
                validate_credit_posting(accounts,*offer,day,gross,amount);
                ++cloud_uses;
            }
        }
        if (award.used!=checked_unit_subtract(offer->limit,remaining)) invalid("Funding award balance differs from actual redemptions");
    }
    if (!uses.empty() || credit_postings!=cloud_uses || sponsored_runs!=grant_uses)
        invalid("An expense received funding outside its eligible award");
}
}
