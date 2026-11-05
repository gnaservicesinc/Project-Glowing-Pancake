#include <pgp/core/business_offers.hpp>
#include <algorithm>
#include <set>
#include <map>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (found==rows.end()) invalid("Business offer refers to an unavailable record");
    return *found;
}
std::string source_id(const BusinessOffer& offer) { return "lease/"+offer.id; }
std::string contract_id(const BusinessOffer& offer) { return "pilot/"+offer.id; }
MarketContract contract(const BusinessOffer& offer,OrdinalDay day) {
    MarketContract row;
    row.id=contract_id(offer); row.segment_id=offer.segment_id; row.product_id=offer.product_id;
    row.tasks_per_day=offer.tasks_per_day; row.monthly_fee=offer.monthly_amount;
    row.accepted_on=day; row.start_by=advance_days(day,Days{1}); row.ends_before=advance_days(day,offer.term);
    row.cause="Accepted customer offer: "+offer.name;
    return row;
}
void apply(BusinessOffersState& state,AccountsState& accounts,CapacityState& capacity,std::optional<MarketState>& market,
           OrdinalDay day,const BusinessOffer& offer,Cents protected_cash) {
    if (day<offer.available_from || day>=offer.available_before ||
        std::any_of(state.accepted.begin(),state.accepted.end(),[&](const auto& row){return row.offer_id==offer.id;}))
        invalid("The offer expired, is not available yet, or was already accepted");
    if (offer.setup_cost>checked_unit_subtract(accounts_report(accounts).cash,protected_cash))
        invalid("Available cash cannot cover the disclosed setup cost");
    if (offer.setup_cost.value()>0) record_cash(accounts,day,Cents{-offer.setup_cost.value()},Account::expenses,
        "Business offer setup: "+offer.name,"offer.setup/"+offer.id);
    const auto expires=advance_days(day,offer.term);
    if (offer.kind==BusinessOfferKind::compute_lease) {
        CapacitySource source{source_id(offer),"cloud.offer/"+offer.id,capacity.owner,"supplier.slice",
            CapacitySourceKind::leased,offer.capacity,offer.capacity,0,"general",true,day};
        add_capacity_source(capacity,day,std::move(source));
        add_recurring(accounts,{"offer.lease/"+offer.id,offer.monthly_amount,day,expires,"Compute lease: "+offer.name});
    } else {
        if (!market || !find(market->products,offer.product_id).available) invalid("A recovery pilot requires an available product");
        const auto row=contract(offer,day);
        accept_contract(*market,capacity,day,row); start_contract(*market,capacity,day,row.id);
    }
    state.accepted.push_back({offer.id,day,expires});
}
}
void validate_business_offer_catalog(const std::vector<BusinessOffer>& catalog) {
    if (catalog.size()>32) invalid("Business offer catalog exceeds its bound");
    std::set<std::string_view> ids;
    for (const auto& offer:catalog) {
        if (offer.id.empty() || offer.id.size()>64 || offer.name.empty() || offer.name.size()>128 || !ids.insert(offer.id).second ||
            offer.available_before<=offer.available_from || offer.term.value()==0 || offer.term.value()>1096 ||
            offer.setup_cost.value()<0 || offer.setup_cost.value()>1'000'000'000'000LL ||
            offer.monthly_amount.value()<=0 || offer.monthly_amount.value()>1'000'000'000'000LL)
            invalid("Business offer identity, window, term or cost is invalid");
        (void)from_ordinal(offer.available_from); (void)from_ordinal(offer.available_before);
        if (offer.kind==BusinessOfferKind::compute_lease) {
            if (offer.capacity.value()==0 || offer.capacity.value()>1'000'000 || !offer.segment_id.empty() ||
                !offer.product_id.empty() || offer.tasks_per_day!=0) invalid("Compute offer has invalid capacity or pilot-only terms");
        } else if (offer.kind==BusinessOfferKind::recovery_pilot || offer.kind==BusinessOfferKind::enterprise_contract) {
            if (offer.renewable) invalid("Only compute leases offer this renewal policy");
            if (offer.capacity.value()!=0 || offer.segment_id.empty() || offer.product_id.empty() ||
                offer.tasks_per_day==0 || offer.tasks_per_day>market_task_limit) invalid("Pilot offer has invalid customer or workload terms");
        } else invalid("Unsupported business offer kind");
    }
}
bool business_offer_eligible(const BusinessOffersState& state,const BusinessOffer& offer,const AccountsState& accounts,
                             const CapacityState& capacity,const std::optional<MarketState>& market,OrdinalDay day,Cents protected_cash) {
    try {
        if (day<offer.available_from || day>=offer.available_before ||
            offer.setup_cost>checked_unit_subtract(accounts_report(accounts).cash,protected_cash) ||
            std::any_of(state.accepted.begin(),state.accepted.end(),[&](const auto& row){return row.offer_id==offer.id;})) return false;
        if (offer.kind==BusinessOfferKind::compute_lease)
            return capacity.sources.size()<capacity_source_limit && accounts.recurring.size()<accounts_terms_limit;
        if (!market || !find(market->products,offer.product_id).available) return false;
        const auto& product=find(market->products,offer.product_id);
        if (find(capacity.projects,product.capacity_project_id).status!=ProjectStatus::active) return false;
        validate_contract_acceptance(*market,capacity,day,contract(offer,day));
        return true;
    } catch (const std::exception&) { return false; }
}
void accept_business_offer(BusinessOffersState& state,AccountsState& accounts,CapacityState& capacity,
                           std::optional<MarketState>& market,OrdinalDay day,std::string_view id,Cents protected_cash) {
    const auto& offer=find(state.catalog,id);
    // The enclosing Simulation/flow transaction owns rollback across modules.
    apply(state,accounts,capacity,market,day,offer,protected_cash);
}
OrdinalDay lease_expiry(const BusinessOffersState& state,std::string_view id) {
    const auto accepted=std::find_if(state.accepted.begin(),state.accepted.end(),[&](const auto& row){return row.offer_id==id;});
    if (accepted==state.accepted.end()) invalid("Accept the compute lease before requesting renewal");
    auto expiry=accepted->expires_before;
    for (const auto& row:state.renewals) if (row.offer_id==id) expiry=row.expires_before;
    return expiry;
}
Cents lease_commitment_ceiling(const BusinessOffer& offer) {
    const auto months=static_cast<std::uint64_t>((offer.term.value()+27)/28);
    return checked_unit_add(offer.setup_cost,Cents{mul_div_floor(static_cast<std::uint64_t>(offer.monthly_amount.value()),months,1).quotient});
}
void renew_compute_lease(BusinessOffersState& state,AccountsState& accounts,CapacityState& capacity,OrdinalDay day,std::string_view id,Cents protected_cash) {
    const auto& offer=find(state.catalog,id);
    if (!offer.renewable || offer.kind!=BusinessOfferKind::compute_lease || lease_expiry(state,id)!=day || state.renewals.size()>=64)
        invalid("A renewable lease can extend only on its actual expiry morning, within the renewal history bound");
    if (offer.setup_cost>checked_unit_subtract(accounts_report(accounts).cash,protected_cash)) invalid("Available cash cannot cover lease renewal setup");
    const auto serial=std::to_string(state.renewals.size()+1);const auto expires=advance_days(day,offer.term);
    if (offer.setup_cost.value()>0) record_cash(accounts,day,Cents{-offer.setup_cost.value()},Account::expenses,
        "Lease renewal setup: "+offer.name,"offer.renew.setup/"+offer.id+"/"+serial);
    add_recurring(accounts,{"offer.renew/"+offer.id+"/"+serial,offer.monthly_amount,day,expires,"Compute lease renewal: "+offer.name});
    update_effective_capacity(capacity,day,source_id(offer),offer.capacity);
    state.renewals.push_back({offer.id,day,expires});
}
void expire_business_offers(const BusinessOffersState& state,CapacityState& capacity,OrdinalDay day) {
    for (const auto& accepted:state.accepted) {
        const auto& offer=find(state.catalog,accepted.offer_id);
        if (offer.kind!=BusinessOfferKind::compute_lease || day<lease_expiry(state,offer.id)) continue;
        const auto& source=find(capacity.sources,source_id(offer));
        if (source.effective.value()!=0) update_effective_capacity(capacity,day,source.id,MilliCUPerDay{0});
    }
}
void validate_business_offers(const BusinessOffersState& state,const AccountsState& accounts,const CapacityState& capacity,
                              const std::optional<MarketState>& market,bool deep) {
    if (deep) validate_business_offer_catalog(state.catalog);
    if (state.accepted.size()>state.catalog.size()) invalid("Offer acceptances exceed their catalog");
    const auto today=accounts.closed_through?advance_days(*accounts.closed_through,Days{1}):accounts.starts_on;
    if (state.renewals.size()>64) invalid("Lease renewal history exceeds its bound");
    std::map<std::string,OrdinalDay> latest;
    for (const auto& row:state.accepted) latest.emplace(row.offer_id,row.expires_before);
    for (std::size_t i=0;i<state.renewals.size();++i) {
        const auto& row=state.renewals[i];const auto& offer=find(state.catalog,row.offer_id);
        const auto prior=latest.find(row.offer_id);const auto serial=std::to_string(i+1);
        if (!offer.renewable || offer.kind!=BusinessOfferKind::compute_lease || prior==latest.end() || row.renewed_on!=prior->second ||
            row.renewed_on>today || row.expires_before!=advance_days(row.renewed_on,offer.term)) invalid("Lease renewal differs from its actual expiry/terms");
        const auto& term=find(accounts.recurring,"offer.renew/"+offer.id+"/"+serial);
        if (term.starts_on!=row.renewed_on || term.ends_before!=row.expires_before || term.monthly_amount!=offer.monthly_amount)
            invalid("Lease renewal lacks its actual recurring obligation");
        if (deep && offer.setup_cost.value()>0 && std::none_of(accounts.postings.begin(),accounts.postings.end(),[&](const auto& p) {
            return p.key=="offer.renew.setup/"+offer.id+"/"+serial && p.day==row.renewed_on && p.amount==offer.setup_cost && p.credit==Account::cash && p.debit==Account::expenses;
        })) invalid("Lease renewal lacks its actual setup payment");
        prior->second=row.expires_before;
    }
    std::set<std::string_view> seen;
    for (const auto& accepted:state.accepted) {
        const auto& offer=find(state.catalog,accepted.offer_id);
        if (!seen.insert(offer.id).second || accepted.accepted_on<offer.available_from || accepted.accepted_on>=offer.available_before ||
            accepted.accepted_on<accounts.starts_on || accepted.accepted_on>today ||
            accepted.expires_before!=advance_days(accepted.accepted_on,offer.term)) invalid("Offer acceptance has invalid terms or identity");
        if (deep && offer.setup_cost.value()>0 && std::none_of(accounts.postings.begin(),accounts.postings.end(),[&](const auto& row) {
            return row.key=="offer.setup/"+offer.id && row.amount==offer.setup_cost && row.credit==Account::cash &&
                row.debit==Account::expenses && row.day==accepted.accepted_on;
        })) invalid("Accepted offer lacks its actual setup payment");
        if (offer.kind==BusinessOfferKind::compute_lease) {
            const auto& source=find(capacity.sources,source_id(offer));
            const auto& obligation=find(accounts.recurring,"offer.lease/"+offer.id);
            if (source.nominal!=offer.capacity || source.available_from!=accepted.accepted_on || source.kind!=CapacitySourceKind::leased ||
                source.provider!="supplier.slice" || !source.energy_included || source.owner!=accounts.owner ||
                obligation.starts_on!=accepted.accepted_on || obligation.ends_before!=accepted.expires_before || obligation.monthly_amount!=offer.monthly_amount)
                invalid("Accepted lease differs from its actual source or obligation");
        } else {
            if (!market) invalid("Accepted pilot has no customer market");
            const auto& actual=find(market->contracts,contract_id(offer)); const auto expected=contract(offer,accepted.accepted_on);
            if (actual.product_id!=expected.product_id || actual.segment_id!=expected.segment_id || actual.monthly_fee!=expected.monthly_fee ||
                actual.tasks_per_day!=expected.tasks_per_day || actual.accepted_on!=expected.accepted_on || actual.ends_before!=expected.ends_before ||
                actual.start_by!=expected.start_by || actual.started_on!=accepted.accepted_on || actual.cause!=expected.cause)
                invalid("Accepted pilot differs from its disclosed actual contract");
        }
    }
}
}
