#include <pgp/core/corporate_tax.hpp>
#include <pgp/core/accounts.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void add(Cents& target,Cents amount) { target=checked_unit_add(target,amount); }
int quarter(OrdinalDay day) {
    const auto date=from_ordinal(day);
    return (date.year-1)*4+static_cast<int>((date.month-1)/3);
}
OrdinalDay end_of_quarter(int index) {
    const auto year=index/4+1;
    const auto month=static_cast<unsigned>(index%4+1)*3;
    return to_ordinal({year,month,days_in_month(year,month)});
}
OrdinalDay period_start(const CorporateTaxes& state) {
    return state.assessments.empty()?state.starts_on:advance_days(state.assessments.back().through,Days{1});
}
Cents portion(Cents amount,BasisPoints rate) {
    return Cents{mul_div_floor(static_cast<std::uint64_t>(amount.value()),static_cast<std::uint64_t>(rate.value()),10000).quotient};
}
bool tax_posting(const Posting& posting) {
    return posting.debit==Account::expenses && posting.credit==Account::payables &&
           posting.document_id.starts_with(tax_bill_prefix);
}
void include(TaxBase& base,const Posting& posting) {
    if (!posting.key.starts_with("group.dividend/")) {
        if (posting.credit==Account::revenue) add(base.income,posting.amount);
        if (posting.debit==Account::revenue) add(base.income,Cents{-posting.amount.value()});
    }
    if (posting.debit==Account::expenses && !tax_posting(posting)) add(base.deductions,posting.amount);
    if (posting.credit==Account::expenses) add(base.deductions,Cents{-posting.amount.value()});
    if (posting.debit==Account::expenses && posting.credit==Account::cash && posting.key.starts_with("lab/research/"))
        add(base.research_fees,posting.amount);
}
const TaxAssessment& assess(CorporateTaxes& state,OrdinalDay first,OrdinalDay through,const TaxBase& base) {
    const auto& policy=state.policy;
    if (state.assessments.size()>=tax_return_limit) invalid("Corporate tax history exceeds its supported bound");
    TaxAssessment result;
    result.first=first;
    result.through=through;
    result.due=advance_days(through,policy.payment_delay);
    result.base=base;
    const auto index=quarter(through);
    std::erase_if(state.losses,[&](const auto& row) {
        if (index-quarter(row.originated_on)<=static_cast<int>(policy.loss_carry_quarters)) return false;
        add(result.loss_expired,row.remaining);
        return true;
    });
    const auto earnings=checked_unit_subtract(base.income,base.deductions);
    if (earnings.value()<0) {
        const auto room=checked_unit_subtract(policy.loss_carry_cap,tax_loss_balance(state));
        result.loss_added=earnings.value()<=-room.value()?room:Cents{-earnings.value()};
        if (result.loss_added.value()>0) state.losses.push_back({through,result.loss_added});
    } else {
        auto allowed=portion(earnings,policy.loss_offset);
        for (auto& loss:state.losses) {
            if (allowed.value()==0) break;
            const auto applied=std::min(loss.remaining,allowed);
            loss.remaining=checked_unit_subtract(loss.remaining,applied);
            allowed=checked_unit_subtract(allowed,applied);
            add(result.loss_used,applied);
        }
        std::erase_if(state.losses,[](const auto& row) { return row.remaining==Cents{0}; });
        result.taxable=checked_unit_subtract(earnings,result.loss_used);
        result.gross_tax=portion(result.taxable,policy.rate);
        if (state.assessments.size()<policy.research_credit_quarters)
            result.credit=std::min({result.gross_tax,policy.research_credit_cap,portion(base.research_fees,policy.research_credit)});
        result.bill=checked_unit_subtract(result.gross_tax,result.credit);
    }
    state.assessments.push_back(result);
    return state.assessments.back();
}
}

void validate_corporate_tax_policy(const CorporateTaxPolicy& policy) {
    constexpr Cents money_bound{1'000'000'000'000LL};
    const bool carry=policy.loss_carry_quarters>0;
    const bool research=policy.research_credit_quarters>0;
    if (policy.rate.value()<=0 || policy.rate.value()>4000 ||
        policy.payment_delay.value()<=0 || policy.payment_delay.value()>90 ||
        policy.loss_carry_quarters>=tax_loss_limit || policy.loss_carry_cap.value()<0 || policy.loss_carry_cap>money_bound ||
        carry!=(policy.loss_carry_cap.value()>0) || carry!=(policy.loss_offset.value()>0) ||
        policy.research_credit_quarters>40 || policy.research_credit_cap.value()<0 || policy.research_credit_cap>money_bound ||
        research!=(policy.research_credit_cap.value()>0) || research!=(policy.research_credit.value()>0))
        invalid("Corporate tax rate, payment delay or bounded relief is invalid");
}
Cents tax_loss_balance(const CorporateTaxes& state) {
    Cents result{0};
    for (const auto& row:state.losses) add(result,row.remaining);
    return result;
}
std::optional<OrdinalDay> last_research_credit_day(const CorporateTaxes& state) {
    if (state.policy.research_credit_quarters==0) return std::nullopt;
    return end_of_quarter(quarter(state.starts_on)+static_cast<int>(state.policy.research_credit_quarters)-1);
}
std::string tax_bill_id(OrdinalDay through) {
    return std::string(tax_bill_prefix)+iso_date(through);
}
TaxBase open_tax_base(const AccountsState& ledger) {
    TaxBase base;
    if (!ledger.taxes) return base;
    const auto first=period_start(*ledger.taxes);
    const auto& postings=ledger.postings;
    auto row=std::lower_bound(postings.begin(),postings.end(),first,[](const auto& entry,OrdinalDay day) { return entry.day<day; });
    for (;row!=postings.end();++row) include(base,*row);
    return base;
}
const TaxAssessment* close_corporate_tax(AccountsState& ledger,OrdinalDay day,const TaxBase& carried_base) {
    if (!ledger.taxes || day!=end_of_quarter(quarter(day))) return nullptr;
    auto base=open_tax_base(ledger);
    add(base.income,carried_base.income);
    add(base.deductions,carried_base.deductions);
    add(base.research_fees,carried_base.research_fees);
    return &assess(*ledger.taxes,period_start(*ledger.taxes),day,base);
}
void validate_corporate_taxes(const AccountsState& ledger) {
    if (!ledger.taxes) return;
    const auto& state=*ledger.taxes;
    validate_corporate_tax_policy(state.policy);
    if (state.starts_on!=ledger.starts_on || state.assessments.size()>tax_return_limit || state.losses.size()>tax_loss_limit)
        invalid("Corporate tax history exceeds its period or bounds");
    CorporateTaxes replay{state.policy,state.starts_on};
    auto row=ledger.postings.begin();
    for (const auto& saved:state.assessments) {
        const auto first=period_start(replay);
        const auto through=end_of_quarter(quarter(first));
        if (!ledger.closed_through || through>*ledger.closed_through || saved.first!=first || saved.through!=through)
            invalid("Corporate tax assessments must cover consecutive settled quarters");
        TaxBase base;
        const Posting* recognition=nullptr;
        for (;row!=ledger.postings.end() && row->day<=through;++row) {
            include(base,*row);
            if (tax_posting(*row)) {
                if (recognition) invalid("Multiple corporate tax charges in one quarter");
                recognition=&*row;
            }
        }
        const auto& expected=assess(replay,first,through,base);
        if (saved!=expected) invalid("Corporate tax return differs from booked income, deductions or eligible relief");
        if (saved.bill==Cents{0}) {
            if (recognition) invalid("A zero corporate tax assessment has a ledger charge");
            continue;
        }
        const auto id=tax_bill_id(through);
        const auto bill=ledger.invoices.find(id);
        if (!recognition || recognition->day!=through || recognition->document_id!=id ||
            recognition->key!="invoice/"+id || recognition->amount!=saved.bill ||
            !bill || ledger.invoices[*bill].kind!=InvoiceKind::payable || ledger.invoices[*bill].amount!=saved.bill ||
            ledger.invoices[*bill].due!=saved.due)
            invalid("Corporate tax assessment lacks its dated payable");
    }
    if ((ledger.closed_through && end_of_quarter(quarter(period_start(replay)))<=*ledger.closed_through) ||
        replay.losses!=state.losses || std::any_of(row,ledger.postings.end(),tax_posting))
        invalid("Corporate tax state has a missing assessment, unsupported charge or incorrect loss balance");
}
} // namespace pgp::core
