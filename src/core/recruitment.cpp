#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char *message) { throw std::invalid_argument(message); }
Cents scale(Cents amount, std::uint32_t bp) {
    return Cents{mul_div_round_half_up(static_cast<std::uint64_t>(amount.value()), bp, 10000)};
}
const PeopleRules &rules(const SimulationState &s) {
    if (!s.management || !s.management->people || !s.management->catalog.people ||
        !s.management->catalog.people->recruitment)
        invalid("This company has no negotiated recruiting office.");
    return *s.management->catalog.people;
}
Applicant &applicant(SimulationState &s, const std::string &id) {
    (void)rules(s);
    auto &rows = s.management->people->applicants;
    const auto it = std::ranges::find(rows, id, [](const auto &row) { return row.profile.id; });
    if (it == rows.end())
        invalid("This applicant is no longer available. Review the current applicants.");
    return *it;
}
std::string expected_reply(const Recruitment &n, const EmploymentOffer &offer, std::size_t round,
                           const RecruitmentRules &r) {
    if (offer.salary >= n.minimum_salary && offer.signing >= n.minimum_signing)
        return "accept";
    return round >= r.maximum_rounds ? "decline" : "counter";
}
Cents counter(Cents offer, Cents ask, Cents minimum) {
    return std::max(minimum, std::min(ask, Cents{(checked_unit_add(offer, ask).value() + 1) / 2}));
}
} // namespace
void validate_recruitment_rules(const RecruitmentRules &r) {
    if (r.response < Days{1} || r.response > Days{14} || r.validity < Days{1} || r.validity > Days{60} ||
        r.maximum_rounds < 2 || r.maximum_rounds > 6 || r.minimum_ask < 7500 || r.maximum_ask < r.minimum_ask ||
        r.maximum_ask > 12500 || r.minimum_acceptance < 5000 || r.maximum_acceptance < r.minimum_acceptance ||
        r.maximum_acceptance > 10000 || r.minimum_offer < 2500 || r.minimum_offer > r.minimum_ask ||
        r.maximum_offer < r.maximum_ask || r.maximum_offer > 30000 || r.maximum_signing_multiple < 1 ||
        r.maximum_signing_multiple > 10)
        invalid("Recruiting needs bounded response dates, negotiation rounds and compensation ranges.");
}
void initialize_recruitment(Applicant &a, const RecruitmentRules &r, SplitMix64 &random) {
    const auto strongest = std::max({a.profile.research, a.profile.engineering, a.profile.evaluation});
    a.salary = scale(a.salary, r.minimum_ask + (r.maximum_ask - r.minimum_ask) * strongest / 100);
    const auto preference = [&] {
        return r.minimum_acceptance +
               static_cast<std::uint32_t>(random.bounded(r.maximum_acceptance - r.minimum_acceptance + 1));
    };
    a.recruitment =
        Recruitment{a.salary, a.signing, scale(a.salary, preference()), scale(a.signing, preference()), {}, {}, {}, {}};
}
std::string recruitment_status(const Applicant &a, OrdinalDay day) {
    if (!a.recruitment)
        return "fixed";
    const auto &n = *a.recruitment;
    if (n.hired_on)
        return "hired";
    if (n.withdrawn_on)
        return "declined";
    if (n.offers.empty())
        return "available";
    const auto &offer = n.offers.back();
    if (day >= offer.expires_on)
        return "expired";
    if (n.agreed_on || offer.reply == "accept")
        return "agreed";
    if (offer.reply == "pending")
        return "waiting";
    return offer.reply == "counter" ? "counter" : "declined";
}
std::pair<Cents, Cents> agreed_employment(const Applicant &a) {
    if (!a.recruitment || a.recruitment->offers.empty())
        invalid("Agree compensation with this applicant before hiring.");
    const auto &n = *a.recruitment;
    const auto &offer = n.offers.back();
    if (n.agreed_on && offer.reply == "counter")
        return {offer.counter_salary, offer.counter_signing};
    if (offer.reply == "accept")
        return {offer.salary, offer.signing};
    invalid("This applicant has not accepted employment terms.");
}
void manage_recruitment(SimulationState &s, const RecruitmentAction &action) {
    const auto &people = rules(s);
    const auto &r = *people.recruitment;
    std::visit(
        [&](const auto &a) {
            auto &person = applicant(s, a.id);
            auto &n = *person.recruitment;
            const auto status = recruitment_status(person, s.date);
            using A = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<A, ProposeEmployment>) {
                if (status != "available" && status != "counter")
                    invalid("Wait for a response, or choose an applicant still discussing terms.");
                if (n.offers.size() >= r.maximum_rounds)
                    invalid("This applicant has ended compensation discussions.");
                if (a.salary < scale(people.monthly_salary, r.minimum_offer) ||
                    a.salary > scale(people.monthly_salary, r.maximum_offer) || a.signing < Cents{0} ||
                    a.signing > Cents{people.signing_cost.value() * r.maximum_signing_multiple})
                    invalid("Enter compensation within the disclosed recruiting range.");
                const auto response = advance_days(s.date, r.response), expires = advance_days(response, r.validity);
                if (expires >= s.end_date)
                    invalid("There is not enough campaign time to complete this offer.");
                n.offers.push_back({a.salary, a.signing, Cents{0}, Cents{0}, s.date, response, expires, "pending"});
            } else if constexpr (std::is_same_v<A, AcceptEmploymentCounter>) {
                if (status != "counter")
                    invalid("Review a current counteroffer before accepting it.");
                n.agreed_on = s.date;
            } else {
                if (status == "declined" || status == "expired" || status == "hired")
                    invalid("This application has already ended.");
                n.withdrawn_on = s.date;
            }
        },
        action);
}
void prepare_recruitment(SimulationState &s) {
    if (!s.management || !s.management->people || !s.management->catalog.people->recruitment)
        return;
    const auto &r = *s.management->catalog.people->recruitment;
    for (auto &a : s.management->people->applicants) {
        if (!a.recruitment || a.recruitment->withdrawn_on || a.recruitment->offers.empty())
            continue;
        auto &n = *a.recruitment;
        auto &offer = n.offers.back();
        if (offer.reply != "pending" || s.date < offer.respond_on)
            continue;
        offer.reply = expected_reply(n, offer, n.offers.size(), r);
        if (offer.reply == "counter") {
            offer.counter_salary = counter(offer.salary, n.ask_salary, n.minimum_salary);
            offer.counter_signing = counter(offer.signing, n.ask_signing, n.minimum_signing);
        }
        s.paused = true;
    }
}
void validate_recruitment(const Applicant &a, const PeopleRules &people, OrdinalDay day, bool hired) {
    if (!a.recruitment)
        return;
    if (!people.recruitment)
        invalid("Applicant negotiations need their authored rules.");
    const auto &n = *a.recruitment;
    const auto &r = *people.recruitment;
    const auto base = people.monthly_salary;
    if (n.offers.size() > r.maximum_rounds || n.ask_salary < scale(scale(base, 7500), r.minimum_ask) ||
        n.ask_salary > scale(scale(base, 15000), r.maximum_ask) || n.ask_signing != people.signing_cost ||
        n.minimum_salary < scale(n.ask_salary, r.minimum_acceptance) ||
        n.minimum_salary > scale(n.ask_salary, r.maximum_acceptance) ||
        n.minimum_signing < scale(n.ask_signing, r.minimum_acceptance) ||
        n.minimum_signing > scale(n.ask_signing, r.maximum_acceptance))
        invalid("Applicant compensation preferences changed.");
    if (hired != n.hired_on.has_value() || (n.hired_on && (*n.hired_on > day || n.withdrawn_on)) ||
        (n.withdrawn_on && (*n.withdrawn_on < a.profile.applied_on || *n.withdrawn_on > day)))
        invalid("Recruitment agreement dates are inconsistent.");
    std::size_t round = 0;
    OrdinalDay prior = a.profile.applied_on;
    std::optional<OrdinalDay> prior_expiry;
    for (const auto &offer : n.offers) {
        ++round;
        if (offer.submitted_on < prior || (prior_expiry && offer.submitted_on >= *prior_expiry) ||
            offer.submitted_on > day || offer.respond_on != advance_days(offer.submitted_on, r.response) ||
            offer.expires_on != advance_days(offer.respond_on, r.validity) ||
            offer.salary < scale(base, r.minimum_offer) || offer.salary > scale(base, r.maximum_offer) ||
            offer.signing < Cents{0} || offer.signing > Cents{people.signing_cost.value() * r.maximum_signing_multiple})
            invalid("Employment offer amounts or dates changed.");
        if (offer.reply == "pending") {
            if (round != n.offers.size() || (day >= offer.respond_on && !n.withdrawn_on))
                invalid("An applicant response was lost.");
        } else if (day < offer.respond_on || offer.reply != expected_reply(n, offer, round, r))
            invalid("The recorded applicant response differs from their actual terms.");
        if (offer.reply == "counter") {
            if (offer.counter_salary != counter(offer.salary, n.ask_salary, n.minimum_salary) ||
                offer.counter_signing != counter(offer.signing, n.ask_signing, n.minimum_signing))
                invalid("The applicant counteroffer changed.");
        } else if (offer.counter_salary != Cents{0} || offer.counter_signing != Cents{0})
            invalid("Only a counteroffer can quote revised compensation.");
        if (round < n.offers.size() && offer.reply != "counter")
            invalid("A new offer needs an actual prior counteroffer.");
        prior = offer.respond_on;
        prior_expiry = offer.expires_on;
    }
    if (n.agreed_on &&
        (n.offers.empty() || n.offers.back().reply != "counter" || *n.agreed_on < n.offers.back().respond_on ||
         *n.agreed_on >= n.offers.back().expires_on || *n.agreed_on > day))
        invalid("Accepting counteroffer terms needs an unexpired response.");
    if (hired) {
        const auto terms = agreed_employment(a);
        if (n.offers.empty() || *n.hired_on < n.agreed_on.value_or(n.offers.back().respond_on) ||
            *n.hired_on >= n.offers.back().expires_on || a.salary != terms.first || a.signing != terms.second)
            invalid("A hire must retain the agreed compensation.");
    } else if (a.salary != n.ask_salary || a.signing != n.ask_signing)
        invalid("An unsigned applicant must retain their original request.");
}
} // namespace pgp::core
