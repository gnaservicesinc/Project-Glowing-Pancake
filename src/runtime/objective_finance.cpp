#include "objectives_internal.hpp"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tuple>

namespace pgp::runtime::objective_detail {
using namespace core;
namespace {
template<class T> const T& named(const std::vector<T>& values,std::string_view id) {
    const auto it=std::find_if(values.begin(),values.end(),[&](const auto& value){return value.id==id;});
    if(it==values.end()) throw std::invalid_argument("Objective accounting lost an owned document.");
    return *it;
}
bool market_invoice(std::string_view id) {
    return id.starts_with("market.sale/") || id.starts_with("market.contract/");
}
Cents fraction(Cents value,std::uint64_t numerator,std::uint64_t denominator) {
    if(value.value()<0) throw std::invalid_argument("Terminal assets and ownership require nonnegative bases.");
    return Cents{mul_div_floor(static_cast<std::uint64_t>(value.value()),numerator,denominator).quotient};
}
std::uint64_t shares(const std::vector<EquityHolding>& holdings,std::string_view id) {
    for(const auto& row:holdings) if(row.holder==id) return static_cast<std::uint64_t>(row.shares.value());
    return 0;
}
std::vector<EquityHolding> holdings_at_deadline(const WorldState& world,const SimulationState& institution,OrdinalDay before) {
    if(institution.management->governance) return corporate_holdings_before(institution,before);
    const auto& owner=institution.accounts->owner;
    if(controlled_on(world,owner,OrdinalDay{checked_subtract(before.value(),1)}))
        return {{"parent/"+world.company_id,BasisPoints{10000},false}};
    // Planned rivals without corporate motions have only the common opening
    // cap table and their journaled fixed financing acceptances.
    auto holdings=opening_accounts(Cents{0},institution.accounts->starts_on,owner).holdings;
    for(const auto& acceptance:institution.management->financing) if(acceptance.day<before) {
        const auto& offer=named(institution.management->catalog.financing,acceptance.offer_id);
        if(offer.term==Days{0}) dilute_holdings(holdings,"investor/"+offer.id,offer.acquired);
    }
    return holdings;
}
std::size_t cutoff(const AccountsState& accounts,OrdinalDay before) {
    return static_cast<std::size_t>(std::lower_bound(accounts.postings.begin(),accounts.postings.end(),before,
        [](const Posting& p,OrdinalDay day){return p.day<day;})-accounts.postings.begin());
}
struct FinancialInputs {
    Cents cash{0},receivables{0},physical{0},payables{0},debt{0},profit{0},personal_cash{0};
};
void add(Cents& amount,Cents delta) {amount=checked_unit_add(amount,delta);}
Cents balance(const Posting& posting,Account account) {
    return posting.debit==account?posting.amount:posting.credit==account?Cents{-posting.amount.value()}:Cents{0};
}
// These are the two actual legs of fulfilled wholesale receipts. Compare the
// dated consolidation perimeter so pre-acquisition outside sales stay outside.
using JournalKey=std::pair<std::string,std::uint64_t>;
struct WholesaleAudit {
    std::set<JournalKey> external_sales,internal_legs;
};
WholesaleAudit wholesale_audit(const WorldState& world,OrdinalDay first,OrdinalDay before) {
    WholesaleAudit audit;
    for(const auto& contract:world.competition->wholesale) {
        for(std::size_t i=0;i<contract.days.size();++i) {
            const auto day=advance_days(contract.starts_on,Days{i});
            if(day<first || day>=before) continue;
            const auto& receipt=contract.days[i];
            if(!receipt.seller_posting) continue;
            if(controlled_on(world,contract.buyer_id,day)) {
                audit.internal_legs.emplace(world.company_id,receipt.seller_posting);
                audit.internal_legs.emplace(contract.buyer_id,receipt.buyer_posting);
            } else audit.external_sales.emplace(world.company_id,receipt.seller_posting);
        }
    }
    return audit;
}
FinancialInputs inputs(const SimulationState& institution,OrdinalDay before,OrdinalDay earnings_from,const WholesaleAudit& wholesale) {
    FinancialInputs result;
    const auto& accounts=*institution.accounts;
    result.personal_cash=succession_catalog(institution).personal_cash;
    for(const auto& p:accounts.postings | std::views::take(static_cast<std::ptrdiff_t>(cutoff(accounts,before)))) {
        add(result.cash,balance(p,Account::cash));
        add(result.receivables,balance(p,Account::receivables));
        add(result.payables,Cents{-balance(p,Account::payables).value()});
        // The liquidation calculation also owes any unearned advances.
        add(result.payables,Cents{-balance(p,Account::customer_advances).value()});
        add(result.debt,Cents{-balance(p,Account::debt).value()});
        // Site/hardware cost less recorded depreciation and disposals.
        // Investments in subsidiaries and internal funding are not physical
        // property; the underlying subsidiary assets are counted separately.
        if(p.key.starts_with("owned.")) {
            add(result.physical,balance(p,Account::assets));
            add(result.physical,balance(p,Account::accumulated_depreciation));
        }
        if(p.debit==Account::payables && p.credit==Account::cash && p.document_id.starts_with("recurring/leadership/founder."))
            add(result.personal_cash,p.amount);
        if(p.day<earnings_from) continue;
        const JournalKey key{accounts.owner,p.id};
        if(wholesale.internal_legs.contains(key)) continue;
        if((p.debit==Account::receivables && market_invoice(p.document_id)) ||
           (p.debit==Account::cash && wholesale.external_sales.contains(key)))
            add(result.profit,Cents{-balance(p,Account::revenue).value()});
        if(!p.document_id.starts_with("interest/") && !p.key.starts_with("owned.disposal/"))
            add(result.profit,Cents{-balance(p,Account::expenses).value()});
    }
    return result;
}
void capitalize(LegacyValuation& row,const ObjectiveCharter& rules) {
    auto total=checked_unit_subtract(checked_unit_subtract(row.realized_assets,row.payables),row.debt);
    if(row.audited_profit.value()>0)
        add(total,fraction(row.audited_profit,rules.profit_multiple,1));
    row.company_equity=Cents{std::max<std::int64_t>(0,total.value())};
}
}

LiquidityExposure recorded_exposure(const AccountsState& accounts,std::size_t cursor,OrdinalDay through) {
    if(cursor>accounts.postings.size()) throw std::invalid_argument("The postmortem points beyond its source ledger.");
    struct Obligation {LiquidityCause cause;};
    std::map<std::string,Obligation,std::less<>> bills,debts;
    for(const auto& i:accounts.invoices) if(i.kind==InvoiceKind::payable && i.due<=through)
        bills.emplace(i.id,Obligation{{LiquidityCauseKind::payable,i.id,i.due,Cents{0},i.cause}});
    for(const auto& d:accounts.debts) if(d.due<=through)
        debts.emplace(d.id,Obligation{{LiquidityCauseKind::matured_debt,d.id,d.due,Cents{0},{}}});
    for(const auto& p:accounts.postings | std::views::take(static_cast<std::ptrdiff_t>(cursor))) {
        if(const auto it=bills.find(p.document_id);it!=bills.end()) add(it->second.cause.unpaid,Cents{-balance(p,Account::payables).value()});
        if(const auto it=debts.find(p.document_id);it!=debts.end()) {
            add(it->second.cause.unpaid,Cents{-balance(p,Account::debt).value()});
            if(p.credit==Account::debt) it->second.cause.cause=p.cause;
        }
    }
    LiquidityExposure result;
    const auto report=[&](const auto& records,Cents& sum) {
        for(const auto& [id,row]:records) {
            (void)id;
            const auto& a=row.cause;
            if(a.unpaid.value()<=0) continue;
            add(sum,a.unpaid);
            if(!result.oldest || std::tie(a.due,a.kind,a.reference)<std::tie(result.oldest->due,result.oldest->kind,result.oldest->reference))
                result.oldest=a;
        }
    };
    report(bills,result.unpaid_payables);report(debts,result.matured_principal);
    return result;
}

std::vector<LegacyValuation> terminal_profit(const WorldState& world,const ObjectiveCharter& rules) {
    const auto deadline=world.objectives->deadline;
    const auto first=objective_window_start(deadline,ObjectiveKind::profit);
    const auto audit=wholesale_audit(world,first,deadline);
    const auto last=OrdinalDay{checked_subtract(deadline.value(),1)};
    std::vector<LegacyValuation> result;
    std::map<std::string,std::vector<EquityHolding>,std::less<>> ownership;
    std::map<std::string,std::string,std::less<>> founders;
    for(const auto* institution:institutions(world)) {
        if(institution->accounts->starts_on>=deadline) continue;
        const auto& id=institution->accounts->owner;
        auto earnings_from=first;
        for(const auto& deal:world.competition->acquisitions)
            if(deal.target==id && deal.completed_on && *deal.completed_on<deadline) earnings_from=std::max(first,*deal.completed_on);
        const auto values=inputs(*institution,deadline,earnings_from,audit);
        LegacyValuation row;row.institution=id;
        row.realized_assets=checked_unit_add(checked_unit_add(values.cash,
            fraction(values.receivables,static_cast<std::uint64_t>(rules.receivables_realizable.value()),10000)),
            fraction(values.physical,static_cast<std::uint64_t>(rules.physical_assets_realizable.value()),10000));
        row.payables=values.payables;row.debt=values.debt;row.audited_profit=values.profit;
        row.personal_cash=values.personal_cash;row.personal_liabilities=succession_catalog(*institution).personal_liabilities;
        const auto personal=personal_valuation(*institution,deadline);
        row.personal_cash=checked_unit_subtract(row.personal_cash,personal.spent);
        row.personal_assets=personal.assets;
        row.personal_liabilities=checked_unit_add(row.personal_liabilities,personal.liabilities);
        ownership.emplace(id,holdings_at_deadline(world,*institution,deadline));
        founders.emplace(id,institution->succession->founder_holder);
        result.push_back(std::move(row));
    }
    auto& group=result.front();
    for(auto& row:result) {
        if(&row!=&group && controlled_on(world,row.institution,last)) {
            add(group.realized_assets,row.realized_assets);add(group.payables,row.payables);
            add(group.debt,row.debt);add(group.audited_profit,row.audited_profit);
        }
    }
    for(auto& row:result) capitalize(row,rules);
    const auto& parent_holdings=ownership.at(world.company_id);
    for(auto& row:result) {
        const auto& holder=founders.at(row.institution);
        row.founder_equity=fraction(row.company_equity,shares(ownership.at(row.institution),holder),10000);
        for(const auto& deal:world.competition->acquisitions) if(deal.target==row.institution && deal.completed_on && *deal.completed_on<deadline) {
            for(const auto& seller:deal.settlements) if(seller.holder==holder) {
                add(row.personal_cash,seller.cash);
                if(seller.shares.value()>0) {
                    const auto pool=shares(parent_holdings,"merger.sellers/"+deal.target);
                    add(row.founder_equity,fraction(group.company_equity,pool*static_cast<std::uint64_t>(seller.shares.value()),
                        10000U*static_cast<std::uint64_t>(deal.shares.value())));
                }
            }
        }
        row.legacy_wealth=checked_unit_subtract(checked_unit_add(checked_unit_add(row.founder_equity,row.personal_cash),row.personal_assets),row.personal_liabilities);
    }
    return result;
}

SurvivalEvidence terminal_survival(const WorldState& world) {
    const auto deadline=world.objectives->deadline;
    const auto first=objective_window_start(deadline,ObjectiveKind::survival);
    const auto last=OrdinalDay{checked_subtract(deadline.value(),1)};
    const auto& state=world.simulation;
    SurvivalEvidence result;
    const auto& workouts=state.liquidity->workouts;
    result.operating=std::none_of(workouts.begin(),workouts.end(),[&](const auto& row){return row.closed_on && *row.closed_on<=deadline;});
    const auto exposure=recorded_exposure(*state.accounts,cutoff(*state.accounts,deadline),last);
    result.serviced_liabilities=exposure.unpaid_payables==Cents{0} && exposure.matured_principal==Cents{0};
    if(!result.serviced_liabilities) for(const auto& workout:workouts) {
        if(workout.warned_on>=deadline || workout.deadline<=deadline || !workout.acknowledged ||
           (workout.resolved_on && *workout.resolved_on<deadline)) continue;
        result.serviced_liabilities=std::any_of(state.receipts.begin(),state.receipts.end(),[&](const auto& r){
            return r.cause=="acknowledge_liquidity_warning" && r.day>=workout.warned_on && r.day<deadline;});
    }
    const auto add_tasks=[&](std::uint64_t tasks) {
        result.paid_tasks=static_cast<std::uint64_t>(checked_add(static_cast<std::int64_t>(result.paid_tasks),static_cast<std::int64_t>(tasks)));
    };
    for(const auto* institution:institutions(world)) {
        const auto& id=institution->accounts->owner;
        if(!controlled_on(world,id,last) || institution->accounts->starts_on>=deadline) continue;
        if(institution->capacity && std::none_of(institution->liquidity->workouts.begin(),institution->liquidity->workouts.end(),
            [&](const auto& row){return row.closed_on && *row.closed_on<=deadline;}) &&
            available_staff_on(*institution->capacity,last)>0) result.active_team=true;
        std::set<std::string_view,std::less<>> paid;
        const auto& ledger=institution->accounts->postings;
        auto p=std::lower_bound(ledger.begin(),ledger.end(),first,[](const auto& row,OrdinalDay date){return row.day<date;});
        for(;p!=ledger.end() && p->day<deadline;++p)
            if((p->debit==Account::cash || p->debit==Account::customer_advances) &&
               p->credit==Account::receivables && market_invoice(p->document_id)) paid.insert(p->document_id);
        if(institution->market) {
            const auto& market=*institution->market;
            auto day=std::lower_bound(market.days.begin(),market.days.end(),first,[](const auto& row,OrdinalDay date){return row.day<date;});
            for(;day!=market.days.end() && day->day<deadline;++day) if(controlled_on(world,id,day->day)) {
                const auto date=iso_date(day->day);
                const bool combined=paid.contains("market.sale/combined/"+date);
                for(const auto& row:day->discretionary) if(row.charged.value()>0 &&
                    (combined || paid.contains("market.sale/"+row.segment_id+"/"+row.product_id+"/"+date))) add_tasks(row.tasks);
                for(const auto& row:day->contracts) if(row.delivered_tasks &&
                    paid.contains("market.contract/"+row.contract_id+"/"+date)) add_tasks(row.delivered_tasks);
            }
        }
        if(institution->lab && institution->capacity) {
            std::set<std::string_view,std::less<>> funded;
            for(const auto& run:institution->lab->research) funded.insert(run.project_id);
            const auto& days=institution->capacity->days;
            auto day=std::lower_bound(days.begin(),days.end(),first,[](const auto& row,OrdinalDay date){return row.day<date;});
            for(;day!=days.end() && day->day<deadline;++day) if(controlled_on(world,id,day->day))
                for(const auto& work:day->projects) if(work.assigned_staff && funded.contains(work.project_id))
                    result.funded_research=checked_unit_add(result.funded_research,work.work);
        }
    }
    for(const auto& contract:world.competition->wholesale) for(std::size_t i=0;i<contract.days.size();++i) {
        const auto date=advance_days(contract.starts_on,Days{i});
        if(date>=first && date<deadline && !controlled_on(world,contract.buyer_id,date) && contract.days[i].seller_posting)
            result.paid_wholesale=checked_unit_add(result.paid_wholesale,contract.days[i].used);
    }
    return result;
}
bool survival_met(const SurvivalEvidence& evidence) noexcept {
    return evidence.operating && evidence.serviced_liabilities && evidence.active_team &&
        (evidence.paid_tasks || evidence.paid_wholesale.value()>0 || evidence.funded_research.value()>0);
}
} // namespace pgp::runtime::objective_detail
