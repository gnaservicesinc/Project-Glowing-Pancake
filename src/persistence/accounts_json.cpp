#include "accounts_json.hpp"

#include <algorithm>
#include <array>

namespace pgp::persistence::detail {
namespace {
constexpr std::array<std::string_view,9> account_names{
    "cash","receivables","assets","accumulated_depreciation",
    "payables","debt","equity","revenue","expenses"};
Json account_json(core::Account account) {
    const auto index=static_cast<std::size_t>(account);
    if (index>=account_names.size()) invalid("accounts/account","Unsupported account");
    return account_names[index];
}
core::Account parse_account(const Json& value) {
    const auto name=text(value,"accounts/account",32);
    const auto found=std::find(account_names.begin(),account_names.end(),name);
    if (found==account_names.end()) invalid("accounts/account","Unsupported account");
    return static_cast<core::Account>(found-account_names.begin());
}
Json optional_day(const std::optional<core::OrdinalDay>& value) {
    return value?Json(value->value()):Json(nullptr);
}
core::OrdinalDay day(const Json& value) { return core::OrdinalDay{i64(value,"accounts/day")}; }
core::Cents cents(const Json& value) { return core::Cents{i64(value,"accounts/cents")}; }
std::optional<core::OrdinalDay> parse_optional_day(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{day(value)};
}
std::string label(const Json& value) { return text(value,"accounts/text",256); }
}

Json accounts_json(const core::AccountsState& state) {
    Json postings=Json::array(),invoices=Json::array(),recurring=Json::array(),
        debts=Json::array(),holdings=Json::array();
    for (const auto& posting:state.postings) postings.push_back({
        {"id",posting.id},{"day",posting.day.value()},
        {"debit",account_json(posting.debit)},{"credit",account_json(posting.credit)},
        {"amount_cents",posting.amount.value()},{"cause",posting.cause},
        {"owner",posting.owner},{"key",posting.key},{"document_id",posting.document_id}});
    for (const auto& invoice:state.invoices) {
        if (invoice.kind!=core::InvoiceKind::receivable && invoice.kind!=core::InvoiceKind::payable)
            invalid("accounts/invoice/kind","Unsupported invoice kind");
        invoices.push_back({{"id",invoice.id},
            {"kind",invoice.kind==core::InvoiceKind::receivable?"receivable":"payable"},
            {"due",invoice.due.value()},{"amount_cents",invoice.amount.value()},
            {"settled_cents",invoice.settled.value()},{"cause",invoice.cause}});
    }
    for (const auto& obligation:state.recurring) recurring.push_back({
        {"id",obligation.id},{"monthly_amount_cents",obligation.monthly_amount.value()},
        {"starts_on",obligation.starts_on.value()},{"ends_before",optional_day(obligation.ends_before)},
        {"cause",obligation.cause}});
    for (const auto& debt:state.debts) debts.push_back({
        {"id",debt.id},{"principal_cents",debt.principal.value()},
        {"monthly_interest_basis_points",debt.monthly_interest.value()},
        {"drawn_on",debt.drawn_on.value()},{"due",debt.due.value()},
        {"interest_remainder",debt.interest_remainder},{"interest_month",optional_day(debt.interest_month)}});
    for (const auto& holding:state.holdings) holdings.push_back({
        {"holder",holding.holder},{"shares_basis_points",holding.shares.value()},
        {"reserved_pool",holding.reserved_pool}});
    return {{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",optional_day(state.closed_through)},{"next_posting_id",state.next_posting_id},
        {"postings",std::move(postings)},{"invoices",std::move(invoices)},
        {"recurring",std::move(recurring)},{"debts",std::move(debts)},{"holdings",std::move(holdings)}};
}

core::AccountsState parse_accounts(const Json& value) {
    fields(value,"accounts",{"owner","starts_on","closed_through","next_posting_id",
        "postings","invoices","recurring","debts","holdings"});
    core::AccountsState result;
    result.owner=label(value.at("owner"));
    result.starts_on=day(value.at("starts_on"));
    result.closed_through=parse_optional_day(value.at("closed_through"));
    result.next_posting_id=u64(value.at("next_posting_id"),"accounts/next_posting_id");
    const auto& postings=value.at("postings");array(postings,"accounts/postings",core::accounts_posting_limit);
    for (const auto& posting:postings) {
        fields(posting,"accounts/posting",{"id","day","debit","credit","amount_cents","cause","owner","key","document_id"});
        result.postings.push_back({u64(posting.at("id"),"accounts/posting/id"),day(posting.at("day")),
            parse_account(posting.at("debit")),parse_account(posting.at("credit")),
            cents(posting.at("amount_cents")),label(posting.at("cause")),label(posting.at("owner")),
            text(posting.at("key"),"accounts/posting/key",512),
            text(posting.at("document_id"),"accounts/posting/document_id",256,true)});
    }
    const auto& invoices=value.at("invoices");array(invoices,"accounts/invoices",core::accounts_document_limit);
    for (const auto& invoice:invoices) {
        fields(invoice,"accounts/invoice",{"id","kind","due","amount_cents","settled_cents","cause"});
        const auto kind=text(invoice.at("kind"),"accounts/invoice/kind",16);
        if (kind!="payable" && kind!="receivable") invalid("accounts/invoice/kind","Unsupported invoice kind");
        result.invoices.push_back({label(invoice.at("id")),
            kind=="payable"?core::InvoiceKind::payable:core::InvoiceKind::receivable,
            day(invoice.at("due")),cents(invoice.at("amount_cents")),
            cents(invoice.at("settled_cents")),label(invoice.at("cause"))}, result.closed_through);
    }
    const auto& recurring=value.at("recurring");array(recurring,"accounts/recurring",core::accounts_terms_limit);
    for (const auto& obligation:recurring) {
        fields(obligation,"accounts/recurring/obligation",{"id","monthly_amount_cents","starts_on","ends_before","cause"});
        result.recurring.push_back({label(obligation.at("id")),cents(obligation.at("monthly_amount_cents")),
            day(obligation.at("starts_on")),parse_optional_day(obligation.at("ends_before")),label(obligation.at("cause"))});
    }
    const auto& debts=value.at("debts");array(debts,"accounts/debts",core::accounts_terms_limit);
    for (const auto& debt:debts) {
        fields(debt,"accounts/debt",{"id","principal_cents","monthly_interest_basis_points",
            "drawn_on","due","interest_remainder","interest_month"});
        result.debts.push_back({label(debt.at("id")),cents(debt.at("principal_cents")),
            core::BasisPoints{i64(debt.at("monthly_interest_basis_points"),"accounts/debt/monthly_interest_basis_points")},
            day(debt.at("drawn_on")),day(debt.at("due")),
            u64(debt.at("interest_remainder"),"accounts/debt/interest_remainder"),
            parse_optional_day(debt.at("interest_month"))});
    }
    const auto& holdings=value.at("holdings");array(holdings,"accounts/holdings",core::accounts_terms_limit);
    for (const auto& holding:holdings) {
        fields(holding,"accounts/holding",{"holder","shares_basis_points","reserved_pool"});
        result.holdings.push_back({label(holding.at("holder")),
            core::BasisPoints{i64(holding.at("shares_basis_points"),"accounts/holding/shares_basis_points")},
            boolean(holding.at("reserved_pool"),"accounts/holding/reserved_pool")});
    }
    core::validate_accounts(result);
    return result;
}
}
