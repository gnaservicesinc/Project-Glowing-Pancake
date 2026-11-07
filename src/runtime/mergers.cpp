#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <map>
#include <stdexcept>
#include <utility>
namespace pgp::runtime {
using namespace core;
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
const GovernanceCatalog& rules(const WorldState& w) {
    if (!w.simulation.management || !w.simulation.management->catalog.governance || !w.simulation.management->governance || !w.competition || !w.competition->setup) invalid("Acquisitions require the full company and board setup");
    return *w.simulation.management->catalog.governance;
}
const RivalCompany& target(const WorldState& w,std::string_view id) {
    if (!w.competition) invalid("No company is available for negotiation");
    for (const auto& r:w.competition->companies) if (r.definition_id==id) return r;
    invalid("The acquisition target is absent");
}
RivalCompany& target(WorldState& w,std::string_view id) {return const_cast<RivalCompany&>(target(std::as_const(w),id));}
Cents net_assets(const AccountsState& a,std::uint64_t count) {
    if (count>a.postings.size()) invalid("Due diligence references unavailable account history");
    return std::max(Cents{0},book_net_assets(accounts_report_at(a,static_cast<std::size_t>(count))));
}
BasisPoints equity_price(Cents buyer_value,Cents noncash,BasisPoints cap) {
    const auto denominator=checked_unit_add(buyer_value,noncash);
    const auto n=mul_div_floor(static_cast<std::uint64_t>(noncash.value()),10000,static_cast<std::uint64_t>(denominator.value()));
    const auto shares=BasisPoints{n.quotient+(n.remainder?1:0)};
    if (shares>cap || shares.value()==0) invalid("The seller's equity counteroffer exceeds the disclosed dilution ceiling; use cash or improve funded value");
    return shares;
}
std::vector<SellerSettlement> settle_sellers(const AcquisitionRecord& q) {
    std::vector<SellerSettlement> rows;std::uint64_t weights=0;
    for (const auto& h:q.sellers) if (!h.reserved_pool && h.shares.value()>0) {rows.push_back({h.holder,Cents{0},BasisPoints{0}});weights+=static_cast<std::uint64_t>(h.shares.value());}
    if (weights==0) invalid("Acquisition has no outstanding seller ownership");
    const auto distribute=[&](std::uint64_t amount,bool cash) {
        struct Remainder {std::size_t at;std::uint64_t value;std::string holder;};
        std::vector<Remainder> remainders;std::uint64_t allocated=0;
        for (std::size_t i=0;i<rows.size();++i) {
            const auto h=std::find_if(q.sellers.begin(),q.sellers.end(),[&](const auto& v){return v.holder==rows[i].holder;});
            const auto n=mul_div_floor(amount,static_cast<std::uint64_t>(h->shares.value()),weights);allocated+=n.quotient;
            if (cash) rows[i].cash=Cents{n.quotient};else rows[i].shares=BasisPoints{n.quotient};
            remainders.push_back({i,n.remainder,rows[i].holder});
        }
        std::sort(remainders.begin(),remainders.end(),[](const auto& a,const auto& b){return a.value!=b.value?a.value>b.value:a.holder<b.holder;});
        for (std::size_t i=0;i<amount-allocated;++i) {
            auto& r=rows[remainders[i].at];if (cash) r.cash=Cents{checked_add(r.cash.value(),1)};else r.shares=BasisPoints{checked_add(r.shares.value(),1)};
        }
    };
    distribute(static_cast<std::uint64_t>(q.cash.value()),true);distribute(static_cast<std::uint64_t>(q.shares.value()),false);return rows;
}
std::vector<EquityHolding> historical_sellers(const WorldState& w,const CompiledProgram& p,const AcquisitionRecord& q) {
    const auto& r=target(w,q.target);
    if (!r.planning || !r.planning->entered_on || !r.simulation.accounts || q.target_posting_cursor>r.simulation.accounts->postings.size()) invalid("Seller due diligence predates its financed company");
    auto h=competition_detail::opening_rival(w,p,rival_definition(p,q.target),*r.planning->entered_on).accounts->holdings;
    for (std::size_t i=0;i<q.target_posting_cursor;++i) {
        const auto& posting=r.simulation.accounts->postings[i];if (posting.day>q.quoted_on) invalid("Seller quote reads future financial data");
        if (posting.credit!=Account::equity || posting.key=="opening") continue;
        if (!posting.key.starts_with("financing/")) invalid("Seller ownership has an unsupported source");
        const auto id=posting.key.substr(10);const auto& terms=r.simulation.management->catalog.financing;
        const auto found=std::find_if(terms.begin(),terms.end(),[&](const auto& f){return f.id==id;});
        if (found==terms.end()) invalid("Seller financing is unavailable");
        dilute_holdings(h,"investor/"+id,found->acquired);
    }
    return h;
}
}
AcquisitionRecord quote_acquisition(const WorldState& w,const CompiledProgram& p,std::string_view id,bool equity) {
    const auto& c=rules(w);const auto& r=target(w,id);
    if (!r.planning || !r.planning->entered_on || r.response || rival_definition(p,id).planning->group=="heavyweight" ||
        !r.parent_company.empty() || r.simulation.phase!=DayPhase::work_ready || r.simulation.management->governance)
        invalid("Only an operating independent startup without outstanding executive grants offers this simple merger");
    const auto& a=*r.simulation.accounts;AcquisitionRecord q;q.target=std::string(id);q.quoted_on=w.simulation.date;
    q.target_posting_cursor=a.postings.size();q.buyer_posting_cursor=w.simulation.accounts->postings.size();q.sellers=a.holdings;
    q.value=checked_unit_add(net_assets(a,q.target_posting_cursor),c.acquisition_premium);
    if (q.value.value()<=0) invalid("The seller requires positive actual consideration");
    q.cash=equity?Cents{checked_add(q.value.value()/2,q.value.value()%2)}:q.value;
    if (equity) q.shares=equity_price(checked_unit_add(net_assets(*w.simulation.accounts,q.buyer_posting_cursor),c.acquisition_premium),checked_unit_subtract(q.value,q.cash),c.acquisition_equity);
    q.settlements=settle_sellers(q);return q;
}
void propose_acquisition(WorldState& w,const CompiledProgram& p,std::string_view id,bool equity) {
    const auto q=quote_acquisition(w,p,id,equity);auto& g=*w.simulation.management->governance;
    if (w.competition->acquisitions.size()>=32) invalid("The bounded acquisition history is full");
    for (const auto& prior:w.competition->acquisitions) if (prior.target==id && (prior.completed_on ||
        std::any_of(g.motions.begin(),g.motions.end(),[&](const auto& m){return m.id==prior.motion_id && m.status==BoardMotionStatus::pending;}))) invalid("This target already has a pending or completed merger");
    propose_acquisition_board(w.simulation,std::string(id),q.cash,q.value,q.shares);
    auto record=q;record.motion_id=g.motions.back().id;w.competition->acquisitions.push_back(std::move(record));
}
void complete_approved_acquisitions(WorldState& w,const CompiledProgram&) {
    if (!w.competition || !w.simulation.management || !w.simulation.management->governance) return;
    for (auto& q:w.competition->acquisitions) if (!q.completed_on) {
        auto& motions=w.simulation.management->governance->motions;auto m=std::find_if(motions.begin(),motions.end(),[&](const auto& b){return b.id==q.motion_id;});
        if (m==motions.end() || m->status!=BoardMotionStatus::approved) continue;
        auto& r=target(w,q.target);
        if (!r.parent_company.empty() || r.simulation.phase!=DayPhase::work_ready || r.simulation.accounts->holdings!=q.sellers || r.simulation.management->governance) invalid("The seller's control or ownership changed after its quote; review new terms");
        if (q.cash>discretionary_cash(w.simulation)) invalid("The merger exceeds buyer cash available for new spending");
        record_cash(*w.simulation.accounts,w.simulation.date,Cents{-q.cash.value()},Account::assets,"Acquisition consideration: "+q.target,"merger.cash/"+q.motion_id);
        w.simulation.cash=accounts_report(*w.simulation.accounts).cash;
        if (q.shares.value()>0) company_equity(w.simulation,"merger.sellers/"+q.target,checked_unit_subtract(q.value,q.cash),q.shares,"merger.equity/"+q.motion_id,true);
        q.acquired_posting_cursor=r.simulation.accounts->postings.size();q.acquired_profit=accounts_report(*r.simulation.accounts).profit;
        if (!competition_detail::submit_rival(r,TransferCompanyOwnership{w.company_id})) invalid("The subsidiary could not commit its conserving control transfer");
        r.parent_company=w.company_id;q.completed_on=w.simulation.date;m->executed=true;
        // Keep every license, debt, invoice, employee, contract and qualified
        // service in its operating subsidiary. Nothing is cloned or erased.
    }
}
void transfer_subsidiary_funds(WorldState& w,std::string_view id,Cents amount,bool funding) {
    const auto& c=rules(w);auto& r=target(w,id);
    if (amount.value()<=0 || r.parent_company!=w.company_id || r.simulation.phase!=DayPhase::work_ready) invalid("Group transfers require an operating wholly owned subsidiary and positive actual cash");
    auto& deals=w.competition->acquisitions;const auto at=std::find_if(deals.begin(),deals.end(),[&](const auto& q){return q.target==id && q.completed_on;});
    if (at==deals.end() || at->transfers.size()>=2048) invalid("No completed acquisition or bounded group-transfer slot is available");
    if (!funding) {
        Cents distributed{0};for (const auto& t:at->transfers) if (!t.funding) distributed=checked_unit_add(distributed,t.amount);
        const auto profit=checked_unit_subtract(accounts_report(*r.simulation.accounts).profit,at->acquired_profit);
        if (amount>checked_unit_subtract(profit,distributed)) invalid("A dividend cannot exceed actual undistributed operating profit");
        Cents protected_cash{0};for (const auto& reservation:r.simulation.reservations) protected_cash=checked_unit_add(protected_cash,reservation.reserved);
        const auto through=std::min(r.simulation.end_date,advance_days(r.simulation.date,c.subsidiary_reserve_days));
        const auto projection=project_company_cash(r.simulation,through,protected_cash);
        if (projection.overdue_payables.value()>0 || projection.due_principal.value()>0 ||
            amount>checked_unit_subtract(projection.available,c.subsidiary_cash_floor)) invalid("The subsidiary must retain its disclosed cash floor and projected bills before distributing profit");
    }
    const auto sequence=static_cast<std::uint32_t>(at->transfers.size()+1);
    const auto child=TransferGroupFunds{w.company_id,std::string(id),sequence,amount,funding,false};
    if (!competition_detail::submit_rival(r,child)) invalid("The subsidiary could not commit its actual cash-transfer leg");
    Simulation parent{std::move(w.simulation),TrustedSimulationState{}};
    const auto result=parent.submit_in_transaction({parent.checkpoint().revision,TransferGroupFunds{w.company_id,std::string(id),sequence,amount,funding,true}});
    if (!result.accepted()) throw std::invalid_argument(result.explanation);
    w.simulation=std::move(parent).release_checkpoint();at->transfers.push_back({w.simulation.date,amount,funding});
}
OwnedGroupReport owned_group_report(const WorldState& w) {
    OwnedGroupReport group;if (!w.simulation.accounts) return group;
    const auto add=[&](const AccountsState& a) {
        const auto r=accounts_report(a);group.cash=checked_unit_add(group.cash,r.cash);group.operating_profit=checked_unit_add(group.operating_profit,r.profit);
        group.net_assets=checked_unit_add(group.net_assets,book_net_assets(r));
    };
    add(*w.simulation.accounts);
    if (w.competition) for (const auto& q:w.competition->acquisitions) if (q.completed_on) {
        add(*target(w,q.target).simulation.accounts);group.net_assets=checked_unit_subtract(group.net_assets,q.value);group.operating_profit=checked_unit_subtract(group.operating_profit,q.acquired_profit);
        for (const auto& t:q.transfers) {
            if (t.funding) group.net_assets=checked_unit_subtract(group.net_assets,t.amount);
            else group.operating_profit=checked_unit_subtract(group.operating_profit,t.amount);
        }
    }
    return group;
}
namespace {
struct FinancialPoint {const Posting* posting=nullptr;Cents cash_before{0},profit_before{0};};
using FinancialIndex=std::map<std::string,FinancialPoint,std::less<>>;
FinancialIndex finance_index(const AccountsState& a) {
    FinancialIndex result;std::int64_t cash=0,revenue=0,expenses=0;
    for (const auto& posting:a.postings) {
        result.emplace(posting.key,FinancialPoint{&posting,Cents{cash},Cents{checked_subtract(revenue,expenses)}});
        if (posting.debit==Account::cash) cash=checked_add(cash,posting.amount.value());if (posting.credit==Account::cash) cash=checked_subtract(cash,posting.amount.value());
        if (posting.credit==Account::revenue) revenue=checked_add(revenue,posting.amount.value());if (posting.debit==Account::revenue) revenue=checked_subtract(revenue,posting.amount.value());
        if (posting.debit==Account::expenses) expenses=checked_add(expenses,posting.amount.value());if (posting.credit==Account::expenses) expenses=checked_subtract(expenses,posting.amount.value());
    }
    return result;
}
}
void validate_acquisitions(const WorldState& w,const CompiledProgram& p,bool deep) {
    if (!w.competition) {
        if (w.simulation.management && w.simulation.management->governance) for (const auto& m:w.simulation.management->governance->motions)
            if (m.kind==BoardMotionKind::acquisition) invalid("An acquisition motion has no world counterparty");
        return;
    }
    const auto& rows=w.competition->acquisitions;
    if (rows.size()>32 || (!rows.empty() && (!w.competition->setup || !w.simulation.management || !w.simulation.management->governance))) invalid("Acquisition continuation lacks its company/board owner");
    if (!deep) return;
    std::set<std::string> ids,completed,group_keys;
    const auto parent_index=w.simulation.accounts?finance_index(*w.simulation.accounts):FinancialIndex{};
    for (const auto& q:rows) {
        const auto& c=rules(w);const auto& r=target(w,q.target);const auto& motions=w.simulation.management->governance->motions;
        const auto m=std::find_if(motions.begin(),motions.end(),[&](const auto& b){return b.id==q.motion_id;});
        if (!ids.insert(q.motion_id).second || m==motions.end() || m->kind!=BoardMotionKind::acquisition || m->subject!=q.target || m->cash!=q.cash || m->asset_value!=q.value || m->shares!=q.shares ||
            q.quoted_on!=m->opened_on || q.quoted_on>w.simulation.date || q.sellers.size()>accounts_terms_limit || q.settlements.size()>accounts_terms_limit || r.response ||
            q.sellers!=historical_sellers(w,p,q) || q.value!=checked_unit_add(net_assets(*r.simulation.accounts,q.target_posting_cursor),c.acquisition_premium)) invalid("Merger terms differ from actual due diligence and board approval");
        if (q.buyer_posting_cursor>w.simulation.accounts->postings.size()) invalid("Buyer due diligence cursor exceeds account history");
        for (std::size_t i=0;i<q.buyer_posting_cursor;++i) if (w.simulation.accounts->postings[i].day>q.quoted_on) invalid("Buyer due diligence includes future records");
        if (q.shares.value()==0 ? q.cash!=q.value:q.cash!=Cents{checked_add(q.value.value()/2,q.value.value()%2)} ||
            q.shares!=equity_price(checked_unit_add(net_assets(*w.simulation.accounts,q.buyer_posting_cursor),c.acquisition_premium),checked_unit_subtract(q.value,q.cash),c.acquisition_equity)) invalid("Merger cash/equity package differs from its counteroffer");
        if (q.settlements!=settle_sellers(q)) invalid("Seller cash and equity do not conserve agreed consideration");
        if (q.completed_on) {
            if (!m->executed || m->resolved_on!=q.completed_on || !completed.insert(q.target).second || r.parent_company!=w.company_id) invalid("Completed merger has repeated or inconsistent control rights");
            auto expected=q.sellers;for (auto& h:expected) h.shares=BasisPoints{0};expected.push_back({"parent/"+w.company_id,BasisPoints{10000},false});
            if (r.simulation.accounts->holdings!=expected) invalid("Subsidiary retained control was diluted or duplicated");
            if (q.acquired_posting_cursor<q.target_posting_cursor || q.acquired_posting_cursor>r.simulation.accounts->postings.size()) invalid("Acquisition opening earnings lack their actual posting boundary");
            if (accounts_report_at(*r.simulation.accounts,static_cast<std::size_t>(q.acquired_posting_cursor)).profit!=q.acquired_profit ||
                std::none_of(r.simulation.receipts.begin(),r.simulation.receipts.end(),[&](const auto& receipt){return receipt.cause=="transfer_company_ownership" && receipt.day==*q.completed_on;})) invalid("Acquisition opening earnings/control do not reconcile");
            const auto paid=std::find_if(w.simulation.accounts->postings.begin(),w.simulation.accounts->postings.end(),[&](const auto& posting){return posting.key=="merger.cash/"+q.motion_id;});
            if (paid==w.simulation.accounts->postings.end() || paid->day!=*q.completed_on || paid->amount!=q.cash || paid->debit!=Account::assets || paid->credit!=Account::cash) invalid("Merger lacks actual cash consideration");
            if (q.shares.value()>0) {
                const CapitalEvent expected{"asset_equity","merger.sellers/"+q.target,"merger.equity/"+q.motion_id,*q.completed_on,q.shares,checked_unit_subtract(q.value,q.cash)};
                const auto& capital=w.simulation.management->governance->capital;
                if (std::find(capital.begin(),capital.end(),expected)==capital.end()) invalid("Merger equity lacks its conserving noncash consideration and seller claim");
            }
        } else if (m->executed || m->status==BoardMotionStatus::approved || q.acquired_posting_cursor!=0 || q.acquired_profit.value()!=0) invalid("A board-approved merger lost its atomic transfer");
    }
    for (const auto& q:rows) {
        if (q.transfers.size()>2048 || (!q.completed_on && !q.transfers.empty())) invalid("Group transfers have no completed acquisition or exceed their bound");
        const auto& r=target(w,q.target);const auto child_index=finance_index(*r.simulation.accounts);Cents distributed{0};std::optional<OrdinalDay> prior;
        for (std::size_t i=0;i<q.transfers.size();++i) {
            const auto& t=q.transfers[i];const auto key="group."+std::string(t.funding?"funding/":"dividend/")+q.target+"/"+std::to_string(i+1);
            if (t.amount.value()<=0 || t.day<*q.completed_on || t.day>w.simulation.date || t.day>r.simulation.date || (prior && t.day<*prior) || !group_keys.insert(key).second) invalid("Group transfer has invalid consideration, control or dates");
            prior=t.day;const auto parent=parent_index.find(key),child=child_index.find(key);
            if (parent==parent_index.end() || child==child_index.end()) invalid("Group transfer lost one actual cash leg");
            const auto& a=*parent->second.posting;const auto& b=*child->second.posting;
            if (a.day!=t.day || b.day!=t.day || a.amount!=t.amount || b.amount!=t.amount ||
                a.debit!=(t.funding?Account::assets:Account::cash) || a.credit!=(t.funding?Account::cash:Account::revenue) ||
                b.debit!=(t.funding?Account::cash:Account::equity) || b.credit!=(t.funding?Account::equity:Account::cash)) invalid("Group transfer does not conserve equal cash and its economic offsets");
            if (!t.funding) {
                if (t.amount>checked_unit_subtract(checked_unit_subtract(child->second.profit_before,q.acquired_profit),distributed) || t.amount>checked_unit_subtract(child->second.cash_before,rules(w).subsidiary_cash_floor)) invalid("Subsidiary distributed unearned profit or its protected cash floor");
                distributed=checked_unit_add(distributed,t.amount);
            }
        }
        for (const auto& posting:r.simulation.accounts->postings) if ((posting.key.starts_with("group.funding/") || posting.key.starts_with("group.dividend/")) && !group_keys.contains(posting.key)) invalid("Subsidiary group posting lacks an actual bilateral transfer");
    }
    for (const auto& [key,point]:parent_index) {
        (void)point;if ((key.starts_with("group.funding/") || key.starts_with("group.dividend/")) && !group_keys.contains(key)) invalid("Parent group posting lacks an actual bilateral transfer");
    }
    for (const auto& r:w.competition->companies) if ((!r.parent_company.empty())!=completed.contains(r.definition_id) || (!r.parent_company.empty() && r.parent_company!=w.company_id)) invalid("Subsidiary control has no completed acquisition");
    if (w.simulation.management && w.simulation.management->governance) for (const auto& m:w.simulation.management->governance->motions)
        if (m.kind==BoardMotionKind::acquisition && !ids.contains(m.id)) invalid("Acquisition motion bypassed bilateral negotiation");
    if (w.simulation.accounts) for (const auto& posting:w.simulation.accounts->postings) if (posting.key.starts_with("merger.cash/") || posting.key.starts_with("merger.equity/")) {
        const auto prefix=posting.key.starts_with("merger.cash/")?12U:14U;
        if (!ids.contains(posting.key.substr(prefix))) invalid("Merger consideration has no negotiated transfer");
    }
}
}
