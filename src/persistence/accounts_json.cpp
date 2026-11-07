#include "accounts_json.hpp"
#include "history_blocks.hpp"
#include "../content/corporate_tax_codec.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <tuple>

namespace pgp::persistence::detail {
namespace {
constexpr std::array<std::string_view,10> account_names{
    "cash","receivables","assets","accumulated_depreciation",
    "payables","debt","equity","revenue","expenses","customer_advances"};
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

std::string invoice_month(core::OrdinalDay due) {
    const auto date = core::from_ordinal(due);
    return core::iso_date(core::to_ordinal({date.year, date.month, 1}));
}
std::string dated_invoice_id(std::string stem, bool month, core::OrdinalDay due) {
    stem += month ? invoice_month(due) : core::iso_date(due);
    return stem;
}
Json packed_invoice_id(const core::Invoice& invoice) {
    const std::string_view id = invoice.id;
    constexpr std::size_t date_size = 10;
    if (id.size() > date_size && id[id.size() - date_size - 1] == '/') {
        const auto stem = id.substr(0, id.size() - date_size);
        const auto suffix = id.substr(stem.size());
        if (suffix == core::iso_date(invoice.due)) return Json::array({stem, false});
        if (suffix == invoice_month(invoice.due)) return Json::array({stem, true});
    }
    return invoice.id;
}
std::pair<std::string, bool> parse_invoice_stem(const Json& value) {
    array(value, "accounts/invoice/dated_id", 2);
    if (value.size() != 2) invalid("accounts/invoice/dated_id", "Dated invoice needs a stem and calendar reference");
    auto stem = text(value.at(0), "accounts/invoice/stem", 246);
    if (!stem.ends_with('/')) invalid("accounts/invoice/stem", "Dated invoice stem must end with a slash");
    return {std::move(stem), boolean(value.at(1), "accounts/invoice/month")};
}
std::string parse_invoice_id(const Json& value, core::OrdinalDay due) {
    if (value.is_string()) return label(value);
    auto [stem, month] = parse_invoice_stem(value);
    return dated_invoice_id(std::move(stem), month, due);
}

constexpr std::array<std::string_view, 5> key_prefixes{"invoice.issue:", "invoice/", "accrue/", "settle/", "debt/"};
bool dated_key(std::size_t index) { return index == 2 || index == 3; }
std::optional<std::size_t> key_pattern(const core::Posting& posting) {
    if (posting.document_id.empty()) return {};
    for (std::size_t i = 0; i < key_prefixes.size(); ++i) {
        std::string_view key = posting.key;
        if (!key.starts_with(key_prefixes[i])) continue;
        key.remove_prefix(key_prefixes[i].size());
        if (!dated_key(i)) {
            if (key == posting.document_id) return i;
        } else if (key.size() == posting.document_id.size() + 11 && key.starts_with(posting.document_id) &&
                   key[posting.document_id.size()] == '/' && key.substr(posting.document_id.size() + 1) == core::iso_date(posting.day)) {
            return i;
        }
    }
    return {};
}
std::string restore_key(std::size_t index, std::string_view document, core::OrdinalDay posted_on) {
    if (document.empty()) invalid("accounts/posting/key_pattern", "A document key requires its source document");
    auto key = std::string(key_prefixes[index]);
    key += document;
    if (dated_key(index)) key += '/' + core::iso_date(posted_on);
    return key;
}
struct PostingPrior {
    std::int64_t amount = 0;
    std::int64_t invoice = 0;
};
struct PostingPatterns {
    explicit PostingPatterns(core::OrdinalDay start) : day(start) {}
    core::OrdinalDay day;
    std::map<std::tuple<core::Account, core::Account, std::string_view, std::optional<std::size_t>>, std::size_t> ids;
    std::vector<PostingPrior> prior;
    Json rows = Json::array();
};
Json archived_posting(const core::Posting& posting, const core::InvoiceRegister& invoices, PostingPatterns& patterns) {
    const auto key = key_pattern(posting);
    const auto [entry, inserted] = patterns.ids.try_emplace(
        std::tuple{posting.debit, posting.credit, std::string_view{posting.cause}, key}, patterns.prior.size());
    if (inserted) {
        patterns.rows.push_back(Json::array({static_cast<unsigned>(posting.debit), static_cast<unsigned>(posting.credit),
            posting.cause, key ? Json(*key) : Json(nullptr)}));
        patterns.prior.emplace_back();
    }
    auto& prior = patterns.prior[entry->second];
    Json document = posting.document_id;
    if (!posting.document_id.empty()) if (const auto invoice = invoices.find(posting.document_id)) {
        const auto index = static_cast<std::int64_t>(*invoice);
        document = index - std::exchange(prior.invoice, index);
    }
    const auto days = core::checked_subtract(posting.day.value(), patterns.day.value());
    patterns.day = posting.day;
    const auto delta = core::checked_subtract(posting.amount.value(), std::exchange(prior.amount, posting.amount.value()));
    auto row = Json::array({days, entry->second, delta, std::move(document)});
    if (!key) row.push_back(posting.key);
    return row;
}
struct RestoredPattern {
    core::Account debit;
    core::Account credit;
    std::string cause;
    std::optional<std::size_t> key;
    PostingPrior prior{};
};
std::vector<RestoredPattern> parse_posting_patterns(const Json& rows) {
    array(rows, "accounts/posting_patterns", core::accounts_posting_limit);
    const auto account = [](const Json& value) {
        return static_cast<core::Account>(u64(value, "accounts/posting/account", account_names.size() - 1));
    };
    std::vector<RestoredPattern> patterns;
    for (const auto& row : rows) {
        array(row, "accounts/posting_pattern", 4);
        if (row.size() != 4) invalid("accounts/posting_pattern", "Missing pattern columns");
        const auto& key = row.at(3);
        patterns.push_back({account(row.at(0)), account(row.at(1)), label(row.at(2)),
            key.is_null() ? std::nullopt : std::optional{static_cast<std::size_t>(
                u64(key, "accounts/posting/key_pattern", key_prefixes.size() - 1))}});
    }
    return patterns;
}
core::Posting parse_archived_posting(const Json& row, const core::AccountsState& state,
                                     core::OrdinalDay& posted_on, std::vector<RestoredPattern>& patterns) {
    array(row, "accounts/posting", 5);
    if (row.size() < 4) invalid("accounts/posting", "Archived posting has missing columns");
    const auto days = i64(row.at(0), "accounts/posting/days");
    if (days < 0) invalid("accounts/posting/days", "Posting chronology cannot move backward");
    posted_on = core::OrdinalDay{core::checked_add(posted_on.value(), days)};
    const auto index = u64(row.at(1), "accounts/posting/pattern");
    if (index >= patterns.size()) invalid("accounts/posting/pattern", "Unknown posting pattern");
    auto& pattern = patterns[static_cast<std::size_t>(index)];
    if (row.size() != (pattern.key ? 4U : 5U)) invalid("accounts/posting", "Posting key differs from its pattern");
    pattern.prior.amount = core::checked_add(pattern.prior.amount, i64(row.at(2), "accounts/posting/amount_delta"));
    if (pattern.prior.amount <= 0) invalid("accounts/posting/amount_delta", "Posting amount must be positive");
    const auto& reference = row.at(3);
    std::string document;
    if (reference.is_number_integer()) {
        auto& invoice = pattern.prior.invoice;
        invoice = core::checked_add(invoice, i64(reference, "accounts/posting/invoice"));
        if (invoice < 0 || static_cast<std::uint64_t>(invoice) >= state.invoices.size())
            invalid("accounts/posting/invoice", "Posting points beyond its invoice history");
        document = state.invoices[static_cast<std::size_t>(invoice)].id;
    } else document = text(reference, "accounts/posting/document_id", 256, true);
    auto key = pattern.key ? restore_key(*pattern.key, document, posted_on) : text(row.at(4), "accounts/posting/key", 512);
    return {state.postings.size() + 1, posted_on, pattern.debit, pattern.credit, core::Cents{pattern.prior.amount},
            pattern.cause, state.owner, std::move(key), std::move(document)};
}
core::InvoiceKind parse_invoice_kind(const Json& value) {
    return u64(value, "accounts/invoice/kind", 1) == 0 ? core::InvoiceKind::receivable : core::InvoiceKind::payable;
}
core::Cents invoice_settled(core::Cents amount, core::Cents unpaid) {
    if (unpaid.value() < 0 || unpaid > amount) invalid("accounts/invoice/unpaid", "Unpaid amount exceeds the invoice");
    return core::checked_unit_subtract(amount, unpaid);
}
struct InvoiceOffsets {
    core::OrdinalDay due{0};
    core::Cents amount{0};
};
struct InvoicePatterns {
    std::map<std::tuple<std::string, bool, core::InvoiceKind, std::string_view>, std::size_t> ids;
    std::vector<InvoiceOffsets> offsets;
    Json rows = Json::array();
};
Json archived_invoice(const core::Invoice& invoice, InvoicePatterns& patterns) {
    auto id = packed_invoice_id(invoice);
    const auto kind = invoice.kind == core::InvoiceKind::receivable ? 0 : 1;
    const auto unpaid = core::checked_unit_subtract(invoice.amount, invoice.settled).value();
    if (id.is_string()) return Json::array({std::move(id), kind, invoice.due.value(), invoice.amount.value(), unpaid, invoice.cause});
    auto key = std::tuple{id.at(0).get<std::string>(), id.at(1).get<bool>(), invoice.kind, std::string_view{invoice.cause}};
    const auto [entry, inserted] = patterns.ids.try_emplace(std::move(key), patterns.offsets.size());
    if (inserted) {
        patterns.rows.push_back(Json::array({std::move(id), kind, invoice.cause}));
        patterns.offsets.emplace_back();
    }
    auto& prior = patterns.offsets[entry->second];
    const auto due = core::checked_subtract(invoice.due.value(), std::exchange(prior.due, invoice.due).value());
    const auto amount = core::checked_unit_subtract(invoice.amount, std::exchange(prior.amount, invoice.amount));
    return Json::array({entry->second, due, amount.value(), unpaid});
}
struct RestoredInvoicePattern {
    std::string stem;
    bool month;
    core::InvoiceKind kind;
    std::string cause;
    InvoiceOffsets prior;
};
std::vector<RestoredInvoicePattern> parse_invoice_patterns(const Json& table) {
    array(table, "accounts/invoice_patterns", core::accounts_document_limit);
    std::vector<RestoredInvoicePattern> result;
    for (const auto& row : table) {
        array(row, "accounts/invoice_pattern", 3);
        if (row.size() != 3) invalid("accounts/invoice_pattern", "Missing invoice pattern columns");
        auto [stem, month] = parse_invoice_stem(row.at(0));
        result.push_back({std::move(stem), month, parse_invoice_kind(row.at(1)), label(row.at(2)), {}});
    }
    return result;
}
core::Invoice parse_archived_invoice(const Json& row, std::vector<RestoredInvoicePattern>& patterns) {
    array(row, "accounts/invoice", 6);
    if (row.size() == 4) {
        const auto index = u64(row.at(0), "accounts/invoice/pattern");
        if (index >= patterns.size()) invalid("accounts/invoice/pattern", "Unknown invoice pattern");
        auto& pattern = patterns[static_cast<std::size_t>(index)];
        auto& prior = pattern.prior;
        prior.due = core::OrdinalDay{core::checked_add(prior.due.value(), i64(row.at(1), "accounts/invoice/due_delta"))};
        prior.amount = core::checked_unit_add(prior.amount, cents(row.at(2)));
        return {dated_invoice_id(pattern.stem, pattern.month, prior.due), pattern.kind, prior.due, prior.amount,
            invoice_settled(prior.amount, cents(row.at(3))), pattern.cause};
    }
    if (row.size() != 6) invalid("accounts/invoice", "Archived invoice has missing columns");
    const auto due = day(row.at(2));
    const auto amount = cents(row.at(3));
    return {parse_invoice_id(row.at(0), due), parse_invoice_kind(row.at(1)),
            due, amount, invoice_settled(amount, cents(row.at(4))), label(row.at(5))};
}
}

Json accounts_json(const core::AccountsState& state, bool packed) {
    HistoryRows postings{packed}, invoices{packed};
    Json recurring=Json::array(), debts=Json::array(),holdings=Json::array();
    PostingPatterns patterns{state.starts_on};
    InvoicePatterns invoice_patterns;
    for (const auto& posting:state.postings) postings.push(packed ? archived_posting(posting, state.invoices, patterns) : Json{
        {"id",posting.id},{"day",posting.day.value()},
        {"debit",account_json(posting.debit)},{"credit",account_json(posting.credit)},
        {"amount_cents",posting.amount.value()},{"cause",posting.cause},
        {"owner",posting.owner},{"key",posting.key},{"document_id",posting.document_id}});
    for (const auto& invoice:state.invoices) {
        if (invoice.kind!=core::InvoiceKind::receivable && invoice.kind!=core::InvoiceKind::payable)
            invalid("accounts/invoice/kind","Unsupported invoice kind");
        if (packed) invoices.push(archived_invoice(invoice, invoice_patterns));
        else invoices.push({{"id",invoice.id},
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
    Json result{{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",optional_day(state.closed_through)},{"next_posting_id",state.next_posting_id},
        {"postings",postings.finish()},{"invoices",invoices.finish()},
        {"recurring",std::move(recurring)},{"debts",std::move(debts)},{"holdings",std::move(holdings)}};
    if (packed) {
        result["posting_patterns"] = std::move(patterns.rows);
        result["invoice_patterns"] = std::move(invoice_patterns.rows);
    }
    if (state.taxes) {
        using content::civic_codec::encode;
        const auto& tax=*state.taxes;
        result["corporate_taxes"]={{"policy",encode(tax.policy)},{"starts_on",encode(tax.starts_on)},
            {"losses",encode(tax.losses)},{"assessments",encode(tax.assessments)}};
    }
    return result;
}

core::AccountsState parse_accounts(const Json& value, bool packed) {
    fields(value,"accounts",{"owner","starts_on","closed_through","next_posting_id",
        "postings","invoices","recurring","debts","holdings"},{"posting_patterns","invoice_patterns","corporate_taxes"});
    if (value.contains("posting_patterns") != packed || (!packed && value.contains("invoice_patterns")))
        invalid("accounts/posting_patterns", "Posting patterns must match the accounts storage format");
    core::AccountsState result;
    result.owner=label(value.at("owner"));
    result.starts_on=day(value.at("starts_on"));
    result.closed_through=parse_optional_day(value.at("closed_through"));
    result.next_posting_id=u64(value.at("next_posting_id"),"accounts/next_posting_id");
    auto invoice_patterns = value.contains("invoice_patterns") ?
        parse_invoice_patterns(value.at("invoice_patterns")) : std::vector<RestoredInvoicePattern>{};
    read_history_rows(value.at("invoices"), "accounts/invoices", core::accounts_document_limit, packed, [&](const Json& invoice) {
        if (packed) {
            result.invoices.push_back(parse_archived_invoice(invoice, invoice_patterns), result.closed_through);
            return;
        }
        fields(invoice,"accounts/invoice",{"id","kind","due","amount_cents","settled_cents","cause"});
        const auto kind=text(invoice.at("kind"),"accounts/invoice/kind",16);
        if (kind!="payable" && kind!="receivable") invalid("accounts/invoice/kind","Unsupported invoice kind");
        result.invoices.push_back({label(invoice.at("id")),
            kind=="payable"?core::InvoiceKind::payable:core::InvoiceKind::receivable,
            day(invoice.at("due")),cents(invoice.at("amount_cents")),
            cents(invoice.at("settled_cents")),label(invoice.at("cause"))}, result.closed_through);
    });
    auto posting_day = result.starts_on;
    auto patterns = packed ? parse_posting_patterns(value.at("posting_patterns")) : std::vector<RestoredPattern>{};
    read_history_rows(value.at("postings"), "accounts/postings", core::accounts_posting_limit, packed, [&](const Json& posting) {
        if (packed) {
            result.postings.push_back(parse_archived_posting(posting, result, posting_day, patterns));
            return;
        }
        fields(posting,"accounts/posting",{"id","day","debit","credit","amount_cents","cause","owner","key","document_id"});
        result.postings.push_back({u64(posting.at("id"),"accounts/posting/id"),day(posting.at("day")),
            parse_account(posting.at("debit")),parse_account(posting.at("credit")),
            cents(posting.at("amount_cents")),label(posting.at("cause")),label(posting.at("owner")),
            text(posting.at("key"),"accounts/posting/key",512),
            text(posting.at("document_id"),"accounts/posting/document_id",256,true)});
    });
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
    if (value.contains("corporate_taxes")) {
        using content::civic_codec::decode;
        const auto& tax=value.at("corporate_taxes");
        fields(tax,"accounts/corporate_taxes",{"policy","starts_on","losses","assessments"});
        core::CorporateTaxes book{decode<core::CorporateTaxPolicy>(tax.at("policy")),day(tax.at("starts_on"))};
        book.losses=decode<std::vector<core::TaxLoss>>(tax.at("losses"));
        const auto& returns=tax.at("assessments");
        array(returns,"accounts/corporate_taxes/assessments",core::tax_return_limit);
        for (const auto& row:returns) book.assessments.push_back(decode<core::TaxAssessment>(row));
        result.taxes=std::move(book);
    }
    return result;
}
}
