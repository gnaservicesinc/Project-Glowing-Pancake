#include <pgp/core/company_finance.hpp>
#include <pgp/core/allocation.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace pgp::core {
namespace {
void add(Cents& total,Cents amount) { total=checked_unit_add(total,amount); }

Cents posted_to(const Posting& row,Account account) {
    if (row.debit==account) return row.amount;
    if (row.credit==account) return Cents{-row.amount.value()};
    return Cents{0};
}
CashMovement& cash_category(CompanyPeriod& period,const Posting& row,Account offset) {
    if (offset==Account::assets || (row.debit==Account::cash && row.key.starts_with("group.dividend/")))
        return period.investing;
    if (offset==Account::debt || offset==Account::equity) return period.financing;
    return period.operations;
}
bool directly_attributed_service_cost(std::string_view id) {
    return id.starts_with("recurring/support/") || id.starts_with("market.penalty/") || id.starts_with("market.cancel/");
}
OrdinalDay month_horizon(OrdinalDay first,unsigned months) {
    const auto civil=from_ordinal(first);
    const auto offset=civil.month-1+months;
    const auto year=civil.year+static_cast<int>(offset/12);
    const auto month=offset%12+1;
    const auto boundary=to_ordinal({year,month,std::min(civil.day,days_in_month(year,month))});
    return OrdinalDay{checked_subtract(boundary.value(),1)};
}
struct ContractSample {
    std::uint64_t tasks=0, days=0;
};
struct ProductSample {
    const MarketProduct* product=nullptr;
    const CapacityProject* serving=nullptr;
    std::vector<WeightedQuota> contracts;
    std::uint64_t observed=0, extra_capacity=0;
};
struct SalesSample {
    std::size_t days=0;
    std::map<std::string_view,ProductSample> products;
    std::map<std::string, std::uint64_t, std::less<>> contract_delivery;
};
SalesSample recent_sales(const SimulationState& state) {
    SalesSample sample;
    if (!state.market || !state.capacity) return sample;
    const auto& market=*state.market;
    const auto& capacity=*state.capacity;
    for (const auto& product:market.products) if (product.available) {
        const auto project=std::find_if(capacity.projects.begin(),capacity.projects.end(),[&](const auto& row) {
            return row.id==product.capacity_project_id && row.status==ProjectStatus::active;
        });
        if (project!=capacity.projects.end())
            sample.products.emplace(product.id,ProductSample{&product,&*project,{}});
    }
    for (const auto& contract:market.contracts) if (contract.status==ContractStatus::active) {
        const auto product=sample.products.find(contract.product_id);
        if (product!=sample.products.end()) product->second.contracts.push_back({contract.id,contract.tasks_per_day});
    }
    std::map<std::string_view,ContractSample> fulfilled;
    sample.days=std::min(finance_sample_days,market.days.size());
    for (auto i=market.days.size()-sample.days;i<market.days.size();++i) {
        const auto& day=market.days[i];
        for (const auto& sale:day.discretionary) {
            const auto product=sample.products.find(sale.product_id);
            if (product!=sample.products.end()) product->second.observed+=sale.tasks;
        }
        for (const auto& work:day.contracts) if (work.promised_tasks>0) {
            auto& contract=fulfilled[work.contract_id];
            contract.tasks+=work.delivered_tasks;
            ++contract.days;
        }
    }
    std::vector<ServingRequest> requests;
    for (const auto& [id, product]:sample.products) {
        (void)id;
        std::uint64_t promised=0;
        for (const auto& claim:product.contracts) promised+=claim.weight;
        const auto protected_tasks=std::min(promised,product.serving->tasks_per_day);
        requests.push_back({product.serving->id,protected_tasks,product.serving->tasks_per_day-protected_tasks});
    }
    const auto plan=preview_capacity_day(capacity,state.date,requests,false);
    for (auto& [id,product]:sample.products) {
        (void)id;
        const auto work=std::find_if(plan.projects.begin(),plan.projects.end(),[&](const auto& row) {
            return row.project_id==product.serving->id;
        });
        if (work==plan.projects.end()) continue;
        product.extra_capacity=work->useful_tasks-work->protected_tasks;
        for (const auto& allotment:apportion_units(work->protected_tasks,product.contracts)) {
            const auto measured=fulfilled.find(allotment.id);
            const auto assumed=measured==fulfilled.end()?allotment.units:measured->second.tasks/measured->second.days;
            sample.contract_delivery.emplace(allotment.id,std::min(allotment.units,assumed));
        }
    }
    return sample;
}
void add_contract_payments(std::vector<ProjectedCashDay>& path,const MarketState& market,
                           OrdinalDay first,const SalesSample& sample) {
    const auto last=advance_days(first,Days{path.size()-1});
    for (const auto& contract:market.contracts) {
        auto unearned=contract.unearned_advance();
        if (contract.status==ContractStatus::pending && contract.start_by>=first && contract.start_by<=last) {
            auto& point=path[static_cast<std::size_t>(contract.start_by.value()-first.value())];
            add(point.bills_due,checked_unit_add(contract.missed_start_penalty,unearned));
        }
        if (contract.status!=ContractStatus::active) continue;
        const auto observed=sample.contract_delivery.find(contract.id);
        const auto tasks=observed==sample.contract_delivery.end()?0:observed->second;
        for (std::size_t i=0;i<path.size();++i) {
            const auto day=advance_days(first,Days{i});
            if (day>=contract.ends_before) break;
            const auto work=contract_delivery(contract,day,tasks);
            const auto charge=work.customer_charge();
            const auto prepaid=std::min(unearned,charge);
            unearned=checked_unit_subtract(unearned,prepaid);
            add(path[i].receipts,checked_unit_subtract(charge,prepaid));
            add(path[i].bills_due,work.penalty);
            if (day.value()+1==contract.ends_before.value()) add(path[i].bills_due,unearned);
        }
    }
}
}

std::optional<CompanyPeriod> recent_company_period(const SimulationState& state) {
    if (!state.accounts || !state.accounts->closed_through) return std::nullopt;
    const auto& ledger=*state.accounts;
    CompanyPeriod period;
    period.last=*ledger.closed_through;
    period.first=std::max(ledger.starts_on,OrdinalDay{checked_subtract(period.last.value(),finance_sample_days-1)});
    period.closing_cash=accounts_report(ledger).cash;
    const auto& postings=ledger.postings;
    const auto settled_end=std::upper_bound(postings.begin(),postings.end(),period.last,[](OrdinalDay day,const auto& row) {
        return day<row.day;
    });
    for (auto after=postings.end();after!=settled_end;)
        period.closing_cash=checked_unit_subtract(period.closing_cash,posted_to(*--after,Account::cash));
    auto posting=std::lower_bound(postings.begin(),postings.end(),period.first,[](const auto& row,OrdinalDay day) {
        return row.day<day;
    });
    for (;posting!=settled_end;++posting) {
        const auto cash=posted_to(*posting,Account::cash);
        add(period.revenue,Cents{-posted_to(*posting,Account::revenue).value()});
        const auto expense=posted_to(*posting,Account::expenses);
        add(period.expenses,expense);
        if (directly_attributed_service_cost(posting->document_id)) add(period.service_cost,expense);
        if (cash.value()>0) {
            add(cash_category(period,*posting,posting->credit).received,cash);
        } else if (cash.value()<0) {
            add(cash_category(period,*posting,posting->debit).paid,Cents{-cash.value()});
        }
    }
    period.opening_cash=checked_unit_subtract(period.closing_cash,
        checked_unit_add(checked_unit_add(period.operations.net(),period.investing.net()),period.financing.net()));
    if (state.market) {
        const auto& days=state.market->days;
        auto day=std::lower_bound(days.begin(),days.end(),period.first,[](const auto& row,OrdinalDay date) {
            return row.day<date;
        });
        for (;day!=days.end() && day->day<=period.last;++day) for (const auto& segment:day->segments) {
            period.delivered_tasks+=segment.contract_tasks+segment.discretionary_tasks;
            add(period.product_revenue,segment.charged);
        }
    }
    return period;
}

CompanyCashForecast forecast_company_finances(const SimulationState& state,std::span<const DatedCustomerCash> wholesale) {
    if (!state.accounts) throw std::invalid_argument("Cash scenarios require business accounts");
    const auto first=state.date;
    const auto near=month_horizon(first,3);
    const auto year=month_horizon(first,12);
    const auto days=static_cast<std::size_t>(year.value()-first.value()+1);
    const auto sample=recent_sales(state);
    std::vector<ProjectedCashDay> base(days);
    if (state.market) add_contract_payments(base,*state.market,first,sample);
    for (const auto& term:wholesale) {
        if (term.per_day.value()<0 || term.ends_before<=term.starts_on)
            throw std::invalid_argument("Wholesale cash scenario needs a dated nonnegative estimate");
        const auto begin=std::max(first,term.starts_on);
        const auto end=std::min(year,OrdinalDay{checked_subtract(term.ends_before.value(),1)});
        for (auto day=begin.value();day<=end.value();++day) add(base[static_cast<std::size_t>(day-first.value())].receipts,term.per_day);
    }
    CompanyCashForecast result;
    result.sample_days=sample.days;
    result.reserved_cash=checked_unit_subtract(state.cash,uncommitted_cash(state));
    constexpr std::array<unsigned,3> demand{50,100,150};
    for (std::size_t i=0;i<demand.size();++i) {
        auto& forecast=result.cases[i];
        forecast.demand_percent=demand[i];
        if (sample.days>0) for (const auto& [id,product]:sample.products) {
            (void)id;
            const auto requested=mul_div_floor(product.observed,demand[i],sample.days*100).quotient;
            const auto supplied=std::min(requested,product.extra_capacity);
            forecast.tasks_per_day+=supplied;
            add(forecast.discretionary_sales_per_day,
                Cents{mul_div_floor(supplied,static_cast<std::uint64_t>(product.product->price_per_task.value()),1).quotient});
        }
        auto path=base;
        for (auto& day:path) add(day.receipts,forecast.discretionary_sales_per_day);
        forecast.cash=project_company_cash_outlook(state,near,result.reserved_cash,path);
    }
    return result;
}
} // namespace pgp::core
