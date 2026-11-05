#include <pgp/core/accounts.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void text_valid(std::string_view text, std::size_t maximum = 256) {
    if (text.empty() || text.size() > maximum ||
        std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        invalid("Accounts text must be nonempty, bounded and contain no control characters");
}
void user_key_valid(std::string_view key) {
    text_valid(key, 512);
    for (const auto prefix : {"opening", "debt/", "invoice/", "accrue/", "settle/"})
        if (key.starts_with(prefix)) invalid("This transaction key namespace belongs to an owning accounts system");
}
std::size_t index(Account account) {
    const auto value = static_cast<std::size_t>(account);
    if (value > static_cast<std::size_t>(Account::expenses)) invalid("Unknown ledger account");
    return value;
}
bool debit_normal(Account account) {
    return account == Account::cash || account == Account::receivables ||
           account == Account::assets || account == Account::expenses;
}
OrdinalDay current_day(const AccountsState& state) {
    return state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
}
void require_day(const AccountsState& state, OrdinalDay day) {
    (void)from_ordinal(day);
    if (day != current_day(state)) invalid("Accounts mutation must name the current unclosed day");
}
Cents balance(const AccountsState& state, Account account) {
    Cents amount{0};
    for (const auto& posting : state.postings) {
        if (posting.debit == account)
            amount = checked_unit_add(amount, debit_normal(account) ? posting.amount : Cents{-posting.amount.value()});
        if (posting.credit == account)
            amount = checked_unit_add(amount, debit_normal(account) ? Cents{-posting.amount.value()} : posting.amount);
    }
    return amount;
}
void append(AccountsState& state, OrdinalDay day, Account debit, Account credit,
            Cents amount, std::string cause, std::string key, std::string document_id = {}) {
    if (amount.value() <= 0 || debit == credit) invalid("Posting needs a positive amount and distinct accounts");
    if (state.postings.size() >= accounts_posting_limit || state.next_posting_id == std::numeric_limits<std::uint64_t>::max())
        invalid("The bounded ledger is full; no required posting was discarded");
    if (std::any_of(state.postings.begin(), state.postings.end(), [&](const auto& row) { return row.key == key; }))
        invalid("This transaction key has already been committed");
    state.postings.push_back({state.next_posting_id++, day, debit, credit, amount,
                              std::move(cause), state.owner, std::move(key), std::move(document_id)});
}
template<class Operation> void mutate(AccountsState& state, Operation operation) {
    validate_accounts(state);
    auto candidate = state;
    operation(candidate);
    validate_accounts(candidate);
    state = std::move(candidate);
}
Invoice& invoice_by_id(AccountsState& state, std::string_view id) {
    const auto found = std::find_if(state.invoices.begin(), state.invoices.end(),
        [id](const auto& row) { return row.id == id; });
    if (found == state.invoices.end()) invalid("Invoice does not exist");
    return *found;
}
Debt& debt_by_id(AccountsState& state, std::string_view id) {
    const auto found = std::find_if(state.debts.begin(), state.debts.end(),
        [id](const auto& row) { return row.id == id; });
    if (found == state.debts.end()) invalid("Debt does not exist");
    return *found;
}
Cents outstanding(const Invoice& invoice) { return checked_unit_subtract(invoice.amount, invoice.settled); }
Cents available_cash(const AccountsState& state, Cents protected_cash) {
    if (protected_cash.value() < 0) invalid("Protected cash cannot be negative");
    const auto result = checked_unit_subtract(balance(state, Account::cash), protected_cash);
    if (result.value() < 0) invalid("Cash reservations exceed ledger cash");
    return result;
}
void settle(AccountsState& state, OrdinalDay day, Invoice& invoice, Cents amount,
            Cents protected_cash, std::string key) {
    if (amount.value() <= 0 || amount > outstanding(invoice)) invalid("Invoice settlement exceeds its outstanding amount");
    if (invoice.kind == InvoiceKind::payable) {
        if (amount > available_cash(state, protected_cash)) invalid("Invoice payment would consume protected or unavailable cash");
        append(state, day, Account::payables, Account::cash, amount, invoice.cause, std::move(key), invoice.id);
    } else {
        append(state, day, Account::cash, Account::receivables, amount, invoice.cause, std::move(key), invoice.id);
    }
    invoice.settled = checked_unit_add(invoice.settled, amount);
}
OrdinalDay month_start(OrdinalDay day) {
    const auto date = from_ordinal(day);
    return to_ordinal({date.year, date.month, 1});
}
OrdinalDay month_end(OrdinalDay day) {
    const auto date = from_ordinal(day);
    return to_ordinal({date.year, date.month, days_in_month(date.year, date.month)});
}
void append_trusted(AccountsState& state, OrdinalDay day, Account debit, Account credit,
                    Cents amount, std::string cause, std::string key, std::string document_id = {}) {
    if (amount.value() <= 0 || debit == credit) invalid("Posting needs a positive amount and distinct accounts");
    if (state.postings.size() >= accounts_posting_limit || state.next_posting_id == std::numeric_limits<std::uint64_t>::max())
        invalid("The bounded ledger is full; no required posting was discarded");
    state.postings.push_back({state.next_posting_id++, day, debit, credit, amount,
                              std::move(cause), state.owner, std::move(key), std::move(document_id)});
}
void accrue(AccountsState& state, OrdinalDay day, std::string id, Cents amount, std::string cause) {
    if (amount == Cents{0}) return;
    auto found = std::find_if(state.invoices.begin(), state.invoices.end(),
        [&](const auto& invoice) { return invoice.id == id; });
    if (found == state.invoices.end()) {
        if (state.invoices.size() >= accounts_document_limit) invalid("The bounded invoice register is full");
        state.invoices.push_back({id, InvoiceKind::payable, month_end(day), Cents{0}, Cents{0}, cause});
        found = std::prev(state.invoices.end());
    } else if (found->kind != InvoiceKind::payable || found->due != month_end(day) || found->cause != cause)
        invalid("An accrued obligation conflicts with an existing invoice");
    found->amount = checked_unit_add(found->amount, amount);
    append_trusted(state, day, Account::expenses, Account::payables, amount, std::move(cause),
                   "accrue/" + id + "/" + iso_date(day), id);
}
void close_day(AccountsState& state, OrdinalDay day, Cents protected_cash) {
    require_day(state, day);
    auto current_available = available_cash(state, protected_cash);
    const auto civil = from_ordinal(day);
    const auto denominator = days_in_month(civil.year, civil.month);
    const auto first = month_start(day);
    for (const auto& obligation : state.recurring) {
        if (day < obligation.starts_on || (obligation.ends_before && day >= *obligation.ends_before)) continue;
        const auto accrued_start = std::max(first, obligation.starts_on);
        const auto elapsed = static_cast<std::uint64_t>(day.value() - accrued_start.value());
        const auto before = mul_div_floor(static_cast<std::uint64_t>(obligation.monthly_amount.value()), elapsed, denominator).quotient;
        const auto after = mul_div_floor(static_cast<std::uint64_t>(obligation.monthly_amount.value()), elapsed + 1, denominator).quotient;
        accrue(state, day, "recurring/" + obligation.id + "/" + iso_date(first),
               Cents{after - before}, obligation.cause);
    }
    for (auto& debt : state.debts) {
        if (day < debt.drawn_on) continue;
        if (!debt.interest_month || *debt.interest_month != first) {
            debt.interest_month = first;
            debt.interest_remainder = 0;
        }
        const auto interest_denominator = static_cast<std::uint64_t>(denominator) * 10000;
        const auto portion = mul_div_floor(static_cast<std::uint64_t>(debt.principal.value()),
            static_cast<std::uint64_t>(debt.monthly_interest.value()), interest_denominator);
        const auto residues = portion.remainder + debt.interest_remainder;
        const auto interest = Cents{portion.quotient + residues / interest_denominator};
        debt.interest_remainder = residues % interest_denominator;
        accrue(state, day, "interest/" + debt.id + "/" + iso_date(first), interest, "Debt interest: " + debt.id);
    }
    // Collection precedes payment. A due receivable represents an agreed,
    // collectible amount; uncertain/late customers will supply explicit terms.
    std::vector<std::size_t> due;
    for (std::size_t i = 0; i < state.invoices.size(); ++i)
        if (state.invoices[i].due <= day && outstanding(state.invoices[i]).value() > 0) due.push_back(i);
    std::sort(due.begin(), due.end(), [&](auto left, auto right) {
        const auto& a = state.invoices[left]; const auto& b = state.invoices[right];
        const auto kind_a = a.kind == InvoiceKind::receivable ? 0 : 1;
        const auto kind_b = b.kind == InvoiceKind::receivable ? 0 : 1;
        return std::tie(kind_a, a.due, a.id) < std::tie(kind_b, b.due, b.id);
    });
    for (const auto i : due) {
        auto& invoice = state.invoices[i];
        auto amount = outstanding(invoice);
        if (invoice.kind == InvoiceKind::payable) amount = std::min(amount, current_available);
        if (amount.value() > 0) {
            if (invoice.kind == InvoiceKind::payable) {
                append_trusted(state, day, Account::payables, Account::cash, amount, invoice.cause,
                               "settle/" + invoice.id + "/" + iso_date(day), invoice.id);
                current_available = checked_unit_subtract(current_available, amount);
            } else {
                append_trusted(state, day, Account::cash, Account::receivables, amount, invoice.cause,
                               "settle/" + invoice.id + "/" + iso_date(day), invoice.id);
                current_available = checked_unit_add(current_available, amount);
            }
            invoice.settled = checked_unit_add(invoice.settled, amount);
        }
    }
    state.closed_through = day;
}
} // namespace

std::string_view to_string(Account account) noexcept {
    switch (account) {
    case Account::cash: return "cash";
    case Account::receivables: return "receivables";
    case Account::assets: return "assets";
    case Account::accumulated_depreciation: return "accumulated_depreciation";
    case Account::payables: return "payables";
    case Account::debt: return "debt";
    case Account::equity: return "equity";
    case Account::revenue: return "revenue";
    case Account::expenses: return "expenses";
    }
    return "invalid";
}
AccountsReport accounts_report(const AccountsState& state) {
    std::array<Cents, 9> balances{Cents{0}, Cents{0}, Cents{0}, Cents{0}, Cents{0}, Cents{0}, Cents{0}, Cents{0}, Cents{0}};
    for (const auto& posting : state.postings) {
        auto& debit = balances[index(posting.debit)];
        auto& credit = balances[index(posting.credit)];
        debit = checked_unit_add(debit, debit_normal(posting.debit) ? posting.amount : Cents{-posting.amount.value()});
        credit = checked_unit_add(credit, debit_normal(posting.credit) ? Cents{-posting.amount.value()} : posting.amount);
    }
    return {balances[0], balances[1], balances[2], balances[3], balances[4], balances[5], balances[6],
            balances[7], balances[8], checked_unit_subtract(balances[7], balances[8])};
}
void validate_accounts(const AccountsState& state) {
    text_valid(state.owner);
    (void)from_ordinal(state.starts_on);
    if (state.closed_through) {
        (void)from_ordinal(*state.closed_through);
        if (*state.closed_through < state.starts_on) invalid("Accounts closed cursor predates the opening");
    }
    if (state.postings.size() > accounts_posting_limit || state.invoices.size() > accounts_document_limit ||
        state.recurring.size() > accounts_terms_limit || state.debts.size() > accounts_terms_limit ||
        state.holdings.empty() || state.holdings.size() > accounts_terms_limit ||
        state.next_posting_id != state.postings.size() + 1)
        invalid("Accounts state exceeds its deterministic bounds or posting identity");
    struct FlatStringSet {
        std::vector<std::string_view> table;
        std::size_t mask = 0;
        void reset(std::size_t count) {
            std::size_t cap = 16;
            while (cap < count * 2 + 1) cap <<= 1;
            table.assign(cap, std::string_view{});
            mask = cap - 1;
        }
        bool insert(std::string_view key) {
            std::size_t idx = std::hash<std::string_view>{}(key) & mask;
            while (!table[idx].empty()) {
                if (table[idx] == key) return false;
                idx = (idx + 1) & mask;
            }
            table[idx] = key;
            return true;
        }
    };
    struct DocumentTotals { std::string_view id{}; Cents recognized{0}; Cents settled{0}; };
    struct FlatDocMap {
        std::vector<DocumentTotals> table;
        std::size_t mask = 0;
        std::size_t count = 0;
        void init(std::size_t expected) {
            std::size_t cap = 16;
            while (cap < expected * 2 + 1) cap <<= 1;
            table.assign(cap, DocumentTotals{});
            mask = cap - 1;
            count = 0;
        }
        DocumentTotals& operator[](std::string_view key) {
            std::size_t idx = std::hash<std::string_view>{}(key) & mask;
            while (!table[idx].id.empty()) {
                if (table[idx].id == key) return table[idx];
                idx = (idx + 1) & mask;
            }
            table[idx].id = key;
            ++count;
            return table[idx];
        }
        const DocumentTotals* find(std::string_view key) const {
            std::size_t idx = std::hash<std::string_view>{}(key) & mask;
            while (!table[idx].id.empty()) {
                if (table[idx].id == key) return &table[idx];
                idx = (idx + 1) & mask;
            }
            return nullptr;
        }
    };
    FlatStringSet keys;
    keys.reset(state.postings.size());
    std::uint64_t expected_id = 1;
    OrdinalDay previous_day = state.starts_on;
    FlatDocMap receivable_documents, payable_documents, debt_documents;
    receivable_documents.init(state.invoices.size());
    payable_documents.init(state.invoices.size());
    debt_documents.init(state.debts.size());
    const auto latest = current_day(state);
    for (const auto& posting : state.postings) {
        (void)index(posting.debit); (void)index(posting.credit);
        text_valid(posting.cause); text_valid(posting.key, 512);
        if (posting.id != expected_id++ || posting.owner != state.owner || posting.amount.value() <= 0 ||
            posting.debit == posting.credit || posting.day < previous_day || posting.day > latest ||
            !keys.insert(posting.key))
            invalid("Invalid or duplicate balanced posting identity, owner, date or amount");
        previous_day = posting.day;
        const auto subledger = [&](Account account, FlatDocMap& documents, Account recognized_offset) {
            if (posting.debit != account && posting.credit != account) return false;
            text_valid(posting.document_id);
            auto& totals = documents[posting.document_id];
            const bool recognition = debit_normal(account) ? posting.debit == account : posting.credit == account;
            if (recognition) {
                if ((debit_normal(account) ? posting.credit : posting.debit) != recognized_offset)
                    invalid("Subledger recognition has an invalid economic offset");
                totals.recognized = checked_unit_add(totals.recognized, posting.amount);
            } else {
                if ((debit_normal(account) ? posting.debit : posting.credit) != Account::cash)
                    invalid("Subledger settlement must move cash exactly once");
                totals.settled = checked_unit_add(totals.settled, posting.amount);
                if (totals.settled > totals.recognized) invalid("Subledger settlement precedes its recognized amount");
            }
            return true;
        };
        const bool linked = subledger(Account::receivables, receivable_documents, Account::revenue) ||
            subledger(Account::payables, payable_documents, Account::expenses) ||
            subledger(Account::debt, debt_documents, Account::cash);
        if (!linked && !posting.document_id.empty()) invalid("Posting links a document without a subledger account");
    }
    auto report = accounts_report(state);
    if (report.cash.value() < 0 || report.receivables.value() < 0 || report.assets.value() < 0 ||
        report.accumulated_depreciation.value() < 0 || report.payables.value() < 0 || report.debt.value() < 0 ||
        report.revenue.value() < 0 || report.expenses.value() < 0)
        invalid("Ledger contains a negative asset, liability, income or cost balance");
    keys.reset(state.invoices.size());
    Cents receivables{0}, payables{0};
    for (const auto& invoice : state.invoices) {
        text_valid(invoice.id); text_valid(invoice.cause);
        (void)from_ordinal(invoice.due);
        if (!keys.insert(invoice.id) || invoice.amount.value() <= 0 || invoice.settled.value() < 0 || invoice.settled > invoice.amount ||
            invoice.due < state.starts_on || (invoice.kind != InvoiceKind::receivable && invoice.kind != InvoiceKind::payable))
            invalid("Invoice identity, units, due date or kind is invalid");
        const auto& documents = invoice.kind == InvoiceKind::receivable ? receivable_documents : payable_documents;
        const auto* source = documents.find(invoice.id);
        if (!source || source->recognized != invoice.amount || source->settled != invoice.settled)
            invalid("Invoice amount and settlement differ from their own committed postings");
        auto& total = invoice.kind == InvoiceKind::receivable ? receivables : payables;
        total = checked_unit_add(total, outstanding(invoice));
    }
    if (receivable_documents.count + payable_documents.count != state.invoices.size())
        invalid("Ledger links an unknown invoice identity");
    if (receivables != report.receivables || payables != report.payables)
        invalid("Invoice subledgers do not reconcile with receivables and payables");
    keys.reset(state.recurring.size());
    for (const auto& obligation : state.recurring) {
        text_valid(obligation.id, 128); text_valid(obligation.cause);
        (void)from_ordinal(obligation.starts_on);
        if (!keys.insert(obligation.id) || obligation.monthly_amount.value() <= 0 || obligation.starts_on < state.starts_on ||
            (obligation.ends_before && *obligation.ends_before < obligation.starts_on))
            invalid("Recurring obligation identity, amount or term is invalid");
        if (obligation.ends_before) (void)from_ordinal(*obligation.ends_before);
    }
    keys.reset(state.debts.size());
    Cents principal{0};
    for (const auto& debt : state.debts) {
        text_valid(debt.id, 128);
        (void)from_ordinal(debt.drawn_on); (void)from_ordinal(debt.due);
        if (!keys.insert(debt.id) || debt.principal.value() < 0 || debt.drawn_on < state.starts_on ||
            debt.drawn_on > latest || debt.due < debt.drawn_on || debt.interest_remainder >= 310000)
            invalid("Debt identity, units, interest residue or term is invalid");
        if (debt.interest_month) {
            const auto date = from_ordinal(*debt.interest_month);
            if (date.day != 1 || *debt.interest_month > latest ||
                *debt.interest_month < month_start(debt.drawn_on) ||
                debt.interest_remainder >= 10000ULL * days_in_month(date.year, date.month))
                invalid("Debt interest month or fractional residue is invalid");
        } else if (debt.interest_remainder != 0) invalid("Debt interest residue has no month");
        const auto* source = debt_documents.find(debt.id);
        if (!source || checked_unit_subtract(source->recognized, source->settled) != debt.principal)
            invalid("Debt principal differs from its own committed draw and repayment postings");
        principal = checked_unit_add(principal, debt.principal);
    }
    if (debt_documents.count != state.debts.size()) invalid("Ledger links an unknown debt identity");
    if (principal != report.debt) invalid("Debt subledger does not reconcile with principal postings");
    // Reconstruct only the finite accrual history. Terms cannot be moved or a
    // fractional-interest cursor reset on reload to skip a future charge.
    struct ExpectedAccrual { OrdinalDay due; Cents amount; std::string cause; };
    std::unordered_map<std::string, ExpectedAccrual> expected_accruals;
    const auto expected_accrual = [&](std::string id, OrdinalDay day, Cents amount, std::string_view cause) {
        if (amount == Cents{0}) return;
        auto [found, fresh] = expected_accruals.try_emplace(std::move(id), ExpectedAccrual{month_end(day), Cents{0}, std::string(cause)});
        (void)fresh;
        found->second.amount = checked_unit_add(found->second.amount, amount);
    };
    if (state.closed_through) {
        for (const auto& obligation : state.recurring) {
            auto first_month = month_start(obligation.starts_on);
            while (first_month <= *state.closed_through) {
                const auto period_begin = std::max(first_month, obligation.starts_on);
                auto period_end = std::min(month_end(first_month), *state.closed_through);
                if (obligation.ends_before && period_end >= *obligation.ends_before)
                    period_end = OrdinalDay{checked_subtract(obligation.ends_before->value(), 1)};
                if (period_end >= period_begin) {
                    const auto date = from_ordinal(first_month);
                    const auto elapsed = static_cast<std::uint64_t>(period_end.value() - period_begin.value() + 1);
                    const auto amount = Cents{mul_div_floor(static_cast<std::uint64_t>(obligation.monthly_amount.value()), elapsed,
                                                          days_in_month(date.year, date.month)).quotient};
                    expected_accrual("recurring/" + obligation.id + "/" + iso_date(first_month), first_month, amount, obligation.cause);
                }
                if (month_end(first_month) >= *state.closed_through ||
                    (obligation.ends_before && month_end(first_month) >= *obligation.ends_before)) break;
                first_month = advance_days(month_end(first_month), Days{1});
            }
        }
    }
    if (!state.debts.empty()) {
        std::unordered_map<std::string_view, std::map<OrdinalDay, Cents>> principal_changes;
        for (const auto& posting : state.postings) {
            if (posting.debit != Account::debt && posting.credit != Account::debt) continue;
            auto& changes = principal_changes[posting.document_id];
            auto [found, fresh] = changes.try_emplace(posting.day, Cents{0});
            (void)fresh;
            found->second = checked_unit_add(found->second, posting.credit == Account::debt ? posting.amount : Cents{-posting.amount.value()});
        }
        for (const auto& debt : state.debts) {
            std::optional<OrdinalDay> interest_month;
            std::uint64_t remainder = 0;
            Cents daily_principal{0};
            const std::string debt_cause = "Debt interest: " + debt.id;
            if (state.closed_through && debt.drawn_on <= *state.closed_through) {
                const auto& changes = principal_changes.at(debt.id);
                for (auto day = debt.drawn_on; day <= *state.closed_through; day = advance_days(day, Days{1})) {
                    if (const auto change = changes.find(day); change != changes.end())
                        daily_principal = checked_unit_add(daily_principal, change->second);
                    const auto first = month_start(day);
                    if (!interest_month || *interest_month != first) { interest_month = first; remainder = 0; }
                    const auto date = from_ordinal(day);
                    const auto denominator = 10000ULL * days_in_month(date.year, date.month);
                    const auto portion = mul_div_floor(static_cast<std::uint64_t>(daily_principal.value()),
                        static_cast<std::uint64_t>(debt.monthly_interest.value()), denominator);
                    const auto residues = portion.remainder + remainder;
                    const auto interest = Cents{portion.quotient + residues / denominator};
                    remainder = residues % denominator;
                    expected_accrual("interest/" + debt.id + "/" + iso_date(first), day, interest, debt_cause);
                }
            }
            if (debt.interest_month != interest_month || debt.interest_remainder != remainder)
                invalid("Debt interest cursor differs from its closed-day principal history");
        }
    }
    for (const auto& invoice : state.invoices) {
        if (!invoice.id.starts_with("recurring/") && !invoice.id.starts_with("interest/")) continue;
        const auto expected = expected_accruals.find(invoice.id);
        if (expected == expected_accruals.end() || invoice.kind != InvoiceKind::payable || invoice.due != expected->second.due ||
            invoice.amount != expected->second.amount ||
            invoice.cause != expected->second.cause)
            invalid("Accrued invoice differs from its owning terms and closed-day history");
        expected_accruals.erase(expected);
    }
    if (!expected_accruals.empty()) invalid("Closed-day obligations are missing accrued invoices");
    keys.reset(state.holdings.size());
    std::int64_t shares = 0;
    unsigned pools = 0;
    for (const auto& holding : state.holdings) {
        text_valid(holding.holder);
        if (!keys.insert(holding.holder)) invalid("Duplicate cap-table holder");
        shares = checked_add(shares, holding.shares.value());
        if (holding.reserved_pool) ++pools;
    }
    if (shares != 10000 || pools != 1) invalid("The fully diluted cap table must conserve 10000 basis points and one reserved pool");
}
AccountsState opening_accounts(Cents cash, OrdinalDay starts_on, std::string owner) {
    if (cash.value() < 0) invalid("Opening cash cannot be negative");
    AccountsState state;
    state.owner = std::move(owner); state.starts_on = starts_on;
    state.holdings = {{"founder", BasisPoints{5500}, false}, {"cofounder", BasisPoints{2000}, false},
                      {"seed_investor", BasisPoints{2000}, false}, {"option_pool", BasisPoints{500}, true}};
    if (cash.value() > 0) append(state, starts_on, Account::cash, Account::equity, cash, "Opening contributed capital", "opening");
    validate_accounts(state);
    return state;
}
void record_cash(AccountsState& state, OrdinalDay day, Cents delta, Account offset, std::string cause, std::string key) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day); user_key_valid(key);
        if (delta == Cents{0} || (offset != Account::equity && offset != Account::revenue && offset != Account::expenses && offset != Account::assets))
            invalid("Direct cash posting needs a supported economic offset and nonzero delta");
        if (delta.value() > 0) append(candidate, day, Account::cash, offset, delta, std::move(cause), std::move(key));
        else append(candidate, day, offset, Account::cash, Cents{checked_subtract(0, delta.value())}, std::move(cause), std::move(key));
    });
}
void add_recurring(AccountsState& state, RecurringObligation obligation) {
    mutate(state, [&](auto& candidate) {
        if (obligation.starts_on < current_day(candidate)) invalid("A new obligation cannot accrue in the past");
        candidate.recurring.push_back(std::move(obligation));
    });
}
void end_recurring(AccountsState& state, std::string_view id, OrdinalDay ends_before) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, ends_before);
        const auto found = std::find_if(candidate.recurring.begin(), candidate.recurring.end(),
            [&](const auto& obligation) { return obligation.id == id; });
        if (found == candidate.recurring.end() || ends_before < found->starts_on ||
            (found->ends_before && *found->ends_before <= ends_before))
            invalid("Recurring service is missing or has already ended");
        found->ends_before = ends_before;
    });
}
void issue_invoice(AccountsState& state, OrdinalDay day, Invoice invoice) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        if (invoice.amount.value() <= 0 || invoice.settled != Cents{0} || invoice.due < day)
            invalid("New invoice requires an unconsumed positive amount and a current or future due date");
        if (invoice.id.starts_with("recurring/") || invoice.id.starts_with("interest/"))
            invalid("Invoice identity is reserved for an owning obligation system");
        if (invoice.kind == InvoiceKind::receivable)
            append(candidate, day, Account::receivables, Account::revenue, invoice.amount, invoice.cause, "invoice/" + invoice.id, invoice.id);
        else if (invoice.kind == InvoiceKind::payable)
            append(candidate, day, Account::expenses, Account::payables, invoice.amount, invoice.cause, "invoice/" + invoice.id, invoice.id);
        else invalid("Unknown invoice kind");
        candidate.invoices.push_back(std::move(invoice));
    });
}
void settle_invoice(AccountsState& state, OrdinalDay day, std::string_view id, Cents amount, Cents protected_cash, std::string key) {
    mutate(state, [&](auto& candidate) { require_day(candidate, day); user_key_valid(key); settle(candidate, day, invoice_by_id(candidate, id), amount, protected_cash, std::move(key)); });
}
void draw_debt(AccountsState& state, OrdinalDay day, Debt debt, std::string cause) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        if (debt.principal.value() <= 0 || debt.drawn_on != day || debt.interest_month || debt.interest_remainder != 0)
            invalid("New debt must draw positive principal today without accrued interest");
        append(candidate, day, Account::cash, Account::debt, debt.principal, std::move(cause), "debt/" + debt.id, debt.id);
        candidate.debts.push_back(std::move(debt));
    });
}
void repay_debt(AccountsState& state, OrdinalDay day, std::string_view id, Cents amount, Cents protected_cash, std::string key) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        user_key_valid(key);
        auto& debt = debt_by_id(candidate, id);
        if (amount.value() <= 0 || amount > debt.principal || amount > available_cash(candidate, protected_cash))
            invalid("Debt repayment exceeds principal or unreserved cash");
        append(candidate, day, Account::debt, Account::cash, amount, "Debt principal: " + debt.id, std::move(key), debt.id);
        debt.principal = checked_unit_subtract(debt.principal, amount);
    });
}
void invest_equity(AccountsState& state, OrdinalDay day, std::string investor, Cents proceeds, BasisPoints acquired, std::string key) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day); text_valid(investor, 128); user_key_valid(key);
        if (proceeds.value() <= 0 || acquired.value() == 0) invalid("Equity investment needs positive cash and ownership");
        auto existing = std::find_if(candidate.holdings.begin(), candidate.holdings.end(), [&](const auto& row) { return row.holder == investor; });
        if (existing != candidate.holdings.end() && existing->reserved_pool) invalid("A reserved option pool cannot act as an investor");
        struct Remainder { std::size_t at; std::uint64_t fraction; std::string holder; };
        std::vector<Remainder> remainders;
        std::int64_t allocated = 0;
        const auto retained = static_cast<std::uint64_t>(10000 - acquired.value());
        for (std::size_t i = 0; i < candidate.holdings.size(); ++i) {
            auto& holding = candidate.holdings[i];
            const auto portion = mul_div_floor(static_cast<std::uint64_t>(holding.shares.value()), retained, 10000);
            holding.shares = BasisPoints{portion.quotient};
            allocated = checked_add(allocated, holding.shares.value());
            remainders.push_back({i, portion.remainder, holding.holder});
        }
        std::sort(remainders.begin(), remainders.end(), [](const auto& a, const auto& b) {
            if (a.fraction != b.fraction) return a.fraction > b.fraction;
            return a.holder < b.holder;
        });
        const auto residue = static_cast<std::size_t>(static_cast<std::int64_t>(retained) - allocated);
        for (std::size_t i = 0; i < residue; ++i) {
            auto& shares = candidate.holdings[remainders[i].at].shares;
            shares = BasisPoints{checked_add(shares.value(), 1)};
        }
        if (existing == candidate.holdings.end()) candidate.holdings.push_back({investor, acquired, false});
        else existing->shares = BasisPoints{checked_add(existing->shares.value(), acquired.value())};
        append(candidate, day, Account::cash, Account::equity, proceeds, "Equity investment: " + investor, std::move(key));
    });
}
void close_accounts_day(AccountsState& state, OrdinalDay day, Cents protected_cash, bool validate_io) {
    if (validate_io) {
        mutate(state, [&](auto& candidate) { close_day(candidate, day, protected_cash); });
    } else {
        close_day(state, day, protected_cash);
    }
}
CashProjection project_cash(const AccountsState& state, OrdinalDay through, Cents protected_cash) {
    validate_accounts(state); (void)from_ordinal(through);
    const auto first = current_day(state);
    if (through < state.starts_on || (state.closed_through && through < *state.closed_through) ||
        through.value() - first.value() > 36524) invalid("Cash projection horizon is outside the supported campaign bound");
    auto candidate = state;
    for (auto day = first; day <= through; day = advance_days(day, Days{1})) close_day(candidate, day, protected_cash);
    validate_accounts(candidate);
    CashProjection result{through, balance(candidate, Account::cash), available_cash(candidate, protected_cash), Cents{0}, Cents{0}};
    for (const auto& invoice : candidate.invoices)
        if (invoice.kind == InvoiceKind::payable && invoice.due <= through)
            result.overdue_payables = checked_unit_add(result.overdue_payables, outstanding(invoice));
    for (const auto& debt : candidate.debts)
        if (debt.due <= through) result.due_principal = checked_unit_add(result.due_principal, debt.principal);
    return result;
}
} // namespace pgp::core
