#include <pgp/core/simulation.hpp>
#include "civic_internal.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::array<std::string_view, civic_issues> issues{"privacy", "innovation", "labor", "competition", "investment", "security", "access"};
constexpr std::array<std::string_view, 3> rules{"launch_evidence", "site_power", "final_closure"};
constexpr std::array<std::string_view, 10> stages{"agenda", "committee", "house", "senate", "executive", "review", "agency", "effective", "rejected", "expired"};

[[noreturn]] void invalid(std::string message) { throw std::invalid_argument(std::move(message)); }
bool text_valid(std::string_view value, std::size_t limit) {
    return !value.empty() && value.size() <= limit &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c != 127; });
}
bool stable_id(std::string_view value) {
    return !value.empty() && value.size() <= 96 && value.front() >= 'a' && value.front() <= 'z' &&
        std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'; });
}
template<class Rows> const auto* row_named(const Rows& rows, std::string_view id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.id == id; });
    return found == rows.end() ? nullptr : &*found;
}
CivicDocket& mutable_docket(SimulationState& state, std::uint64_t id) {
    if (!state.civic || id == 0 || id > state.civic->dockets.size()) invalid("Choose an open policy docket.");
    return state.civic->dockets[static_cast<std::size_t>(id - 1)];
}
const EvaluationEvidence* evaluated(const SimulationState& state, std::string_view id, std::size_t axis, OrdinalDay day) {
    if (!state.lab || !state.capacity) return nullptr;
    const auto* report = row_named(state.lab->evaluations, id);
    if (!report || !report->completed) return nullptr;
    const auto* plan = row_named(state.lab->catalog.evaluations, report->plan_id);
    const auto* work = row_named(state.capacity->projects, report->project_id);
    if (!plan || !plan->independent || plan->coverage.at(axis).value() == 0 || !work ||
        work->status != ProjectStatus::completed || !work->stopped_on || *work->stopped_on > day) return nullptr;
    return report;
}
} // namespace

std::string_view civic_key(CivicIssue issue) { return issues.at(static_cast<std::size_t>(issue)); }
std::string_view civic_key(CivicRule rule) { return rules.at(static_cast<std::size_t>(rule)); }
std::string_view civic_key(CivicStage stage) { return stages.at(static_cast<std::size_t>(stage)); }

void validate_civic_catalog(const CivicCatalog& catalog) {
    if (!text_valid(catalog.assumptions, 1024) || catalog.opening_president >= civic_blocs ||
        catalog.constituencies.size() != civic_jurisdictions || catalog.policies.empty() || catalog.policies.size() > 16 ||
        catalog.election_uncertainty.value() > 1500)
        invalid("Civic terms require three blocs, all 51 jurisdictions, and a bounded election and policy calendar.");
    std::string previous;
    for (const auto& bloc : catalog.blocs) {
        if (!stable_id(bloc.id) || bloc.id <= previous || !text_valid(bloc.name, 96) || bloc.campaign_effort.value() > 500 ||
            std::any_of(bloc.positions.begin(), bloc.positions.end(), [](auto value) { return value < -10000 || value > 10000; }))
            invalid("Civic blocs need sorted unique identities, seven issue positions and bounded campaign effort.");
        previous = bloc.id;
    }
    previous.clear();
    std::uint32_t house = 0, senate = 0;
    std::array<unsigned, 3> classes{};
    for (const auto& state : catalog.constituencies) {
        const auto senators = static_cast<std::uint32_t>(std::count(state.senate_classes.begin(), state.senate_classes.end(), true));
        if (state.id.size() != 5 || !state.id.starts_with("US-") || state.id[3] < 'A' || state.id[3] > 'Z' ||
            state.id[4] < 'A' || state.id[4] > 'Z' || state.id <= previous || !text_valid(state.name, 96) ||
            state.governor_cycle > 1 || state.house_seats > 100 ||
            (state.id == "US-DC" ? state.house_seats != 0 || senators != 0 : state.house_seats == 0 || senators != 2) ||
            std::any_of(state.preferences.begin(), state.preferences.end(), [](auto value) { return value > 10000; }) ||
            std::accumulate(state.preferences.begin(), state.preferences.end(), 0U) != 10000 ||
            static_cast<std::size_t>(state.priority) >= civic_issues || state.sector_exposure.value() > 3000)
            invalid("Each constituency needs canonical state identity, seat classes and bounded fictional preferences.");
        previous = state.id;
        house += state.house_seats;
        senate += senators;
        for (std::size_t i = 0; i < classes.size(); ++i) classes[i] += state.senate_classes[i] ? 1U : 0U;
    }
    if (house != 435 || senate != 100 ||
        std::any_of(classes.begin(), classes.end(), [](auto size) { return size != 33 && size != 34; }))
        invalid("The legislature must conserve 435 House seats and three Senate classes totaling 100 seats.");
    previous.clear();
    for (const auto& policy : catalog.policies) {
        const auto year = from_ordinal(policy.first_agenda).year;
        const bool federal = policy.jurisdiction == "US";
        if (!stable_id(policy.id) || policy.id <= previous || !text_valid(policy.title, 120) ||
            !text_valid(policy.explanation, 600) || year < 2014 || year > 9980 ||
            (!federal && !row_named(catalog.constituencies, policy.jurisdiction)) ||
            static_cast<std::size_t>(policy.issue) >= civic_issues || static_cast<std::size_t>(policy.rule) >= rules.size() ||
            (policy.repeat_years != 0 && (policy.repeat_years < 4 || policy.repeat_years > 32)) ||
            policy.hearing.value() < 1 || policy.hearing.value() > 180 || policy.vote_spacing.value() < 1 || policy.vote_spacing.value() > 60 ||
            policy.implementation.value() < 1 || policy.implementation.value() > 360 || policy.term.value() < 1 || policy.term.value() > 1461 ||
            policy.review.value() > 90 || (policy.review.value() == 0) != (policy.review_floor.value() == 0) ||
            policy.advocacy_fee.value() < 1 || policy.advocacy_fee.value() > 100'000'000 ||
            policy.advocacy_shift.value() < 6 || policy.advocacy_shift.value() > 1500 || policy.evidence_shift.value() > 1800)
            invalid("A civic policy needs a valid docket, bounded delays, fees and a specific enforcement contract.");
        if (policy.rule == CivicRule::launch_evidence) {
            (void)channel_key(policy.channel);
            if (!federal || policy.channel == ProductChannel::legacy || policy.evidence_axis >= lab_domain_count ||
                policy.evidence_floor.value() == 0 || policy.permit_watts != 0)
                invalid("A national launch rule needs a named channel and independent evidence threshold.");
        } else if(policy.rule==CivicRule::final_closure) {
            if(!federal || policy.repeat_years || policy.review==Days{0} || policy.channel!=ProductChannel::legacy ||
                policy.evidence_axis>=lab_domain_count || policy.evidence_floor!=BasisPoints{0} ||
                policy.exempt_reviewed_service || policy.permit_watts)
                invalid("A final closure must be a one-time federal order with an independent review and no channel or permit restrictions.");
        } else if (federal || policy.channel != ProductChannel::legacy || policy.evidence_axis != 0 ||
            policy.evidence_floor.value() != 0 || policy.exempt_reviewed_service || policy.permit_watts < 1000 ||
            policy.permit_watts > 1'000'000'000 || policy.permit_watts % 1000 != 0)
            invalid("A state permit rule needs a whole-kilowatt limit and no service terms.");
        previous = policy.id;
    }
}

const CivicPolicy& civic_policy(const CivicCatalog& catalog, std::string_view id) {
    const auto found = std::lower_bound(catalog.policies.begin(), catalog.policies.end(), id,
        [](const auto& row, std::string_view key) { return row.id < key; });
    if (found == catalog.policies.end() || found->id != id) invalid("The docket refers to an unknown policy.");
    return *found;
}

namespace civic_detail {
bool commenting(const CivicDocket& docket, OrdinalDay day) {
    return day >= docket.opened_on && docket.next_on && day < *docket.next_on &&
        (docket.stage == CivicStage::agenda || docket.stage == CivicStage::house || docket.stage == CivicStage::senate);
}
bool has_evidence(const SimulationState& state, const CivicPolicy& policy, std::string_view evidence, OrdinalDay day) {
    if (policy.rule == CivicRule::launch_evidence || policy.rule==CivicRule::final_closure)
        return evaluated(state, evidence, policy.evidence_axis, day) != nullptr;
    if (!state.infrastructure) return false;
    const auto* site = row_named(state.infrastructure->sites, evidence);
    return site && site->state_id == policy.jurisdiction && site->built_on <= day && site->grid_on <= day &&
        (!site->closed_on || *site->closed_on >= day);
}
std::string advocacy_key(std::uint64_t docket, std::size_t number) {
    return "civic.advocacy/" + std::to_string(docket) + "/" + std::to_string(number);
}
bool applies(const CivicDocket& docket, OrdinalDay day) {
    return docket.effective_from && docket.expires_before && *docket.effective_from <= day && day < *docket.expires_before;
}
} // namespace civic_detail

std::optional<CivicAdvocacyTerms> civic_advocacy_terms(const CivicState& civic, const CivicDocket& docket, OrdinalDay day) {
    if (!civic_detail::commenting(docket, day)) return std::nullopt;
    const auto used = static_cast<std::size_t>(std::count_if(docket.submissions.begin(), docket.submissions.end(),
        [](const auto& row) { return row.evidence.empty(); }));
    if (used >= civic_advocacy_limit) return std::nullopt;
    const auto& policy = civic_policy(civic.catalog, docket.policy_id);
    return CivicAdvocacyTerms{policy.advocacy_fee, BasisPoints{policy.advocacy_shift.value() / static_cast<std::int64_t>(used + 1)},
        civic_advocacy_limit - used};
}

std::vector<std::string> civic_evidence_options(const SimulationState& state, const CivicDocket& docket) {
    std::vector<std::string> result;
    if (!state.civic || !civic_detail::commenting(docket, state.date) ||
        std::any_of(docket.submissions.begin(), docket.submissions.end(), [](const auto& row) { return !row.evidence.empty(); })) return result;
    const auto& policy = civic_policy(state.civic->catalog, docket.policy_id);
    if (policy.evidence_shift.value() == 0) return result;
    if ((policy.rule == CivicRule::launch_evidence || policy.rule==CivicRule::final_closure) && state.lab) {
        for (const auto& row : state.lab->evaluations)
            if (civic_detail::has_evidence(state, policy, row.id, state.date)) result.push_back(row.id);
    } else if (policy.rule == CivicRule::site_power && state.infrastructure) {
        for (const auto& row : state.infrastructure->sites)
            if (!row.closed_on && civic_detail::has_evidence(state, policy, row.id, state.date)) result.push_back(row.id);
    }
    return result;
}

void fund_policy_advocacy(SimulationState& state, const FundPolicyAdvocacy& request, Cents protected_cash) {
    auto& docket = mutable_docket(state, request.docket);
    const auto& rule=civic_policy(state.civic->catalog,docket.policy_id);
    if(rule.jurisdiction=="US" && rule.rule!=CivicRule::final_closure && !national_access(state)) invalid("National advocacy opens through your background, staff, earned revenue or published research. Public information and oversight responses remain available.");
    const auto terms = civic_advocacy_terms(*state.civic, docket, state.date);
    if (!terms) invalid("The disclosed advocacy window or its three submissions are exhausted.");
    if (!state.accounts || terms->fee > checked_unit_subtract(state.cash, protected_cash))
        invalid("The disclosed advocacy fee exceeds unreserved cash.");
    const auto number = civic_advocacy_limit - terms->remaining + 1;
    const auto& policy = civic_policy(state.civic->catalog, docket.policy_id);
    record_cash(*state.accounts, state.date, Cents{-terms->fee.value()}, Account::expenses,
        "Disclosed advocacy: " + policy.title, civic_detail::advocacy_key(docket.id, number));
    state.cash = accounts_report(*state.accounts).cash;
    docket.submissions.push_back({state.date, request.support, {}, terms->fee, terms->influence});
}

void submit_policy_evidence(SimulationState& state, const SubmitPolicyEvidence& request) {
    auto& docket = mutable_docket(state, request.docket);
    const auto options = civic_evidence_options(state, docket);
    if (std::find(options.begin(), options.end(), request.evidence) == options.end())
        invalid("Submit one eligible completed evaluation or connected site record while the public docket is open.");
    const auto& policy = civic_policy(state.civic->catalog, docket.policy_id);
    docket.submissions.push_back({state.date, request.support, request.evidence, Cents{0}, policy.evidence_shift});
}

std::string civic_launch_block(const CivicState* civic, const SimulationState& state, const DeployModel& request, OrdinalDay day) {
    if (!civic || !state.deployments || !state.market) return {};
    const auto service = std::find_if(state.deployments->catalog.begin(), state.deployments->catalog.end(),
        [&](const auto& row) { return row.product.id == request.service_id; });
    if (service == state.deployments->catalog.end()) return {};
    auto product = service->product;
    product.region_id = request.region_id;
    if (product.region_id.empty()) {
        if (const auto* existing = row_named(state.market->products, product.id)) product.region_id = existing->region_id;
    }
    if (!product.region_id.empty() && product.region_id != "region.us") return {};
    const auto domestic = std::any_of(service->preferences.begin(), service->preferences.end(), [&](const auto& preference) {
        const auto* segment = row_named(state.market->segments, preference.segment_id);
        return preference.preference.eligible && segment && (request.weight_audience.empty() || request.weight_audience==segment->id) &&
            (segment->region_id.empty() || segment->region_id == "region.us") && market_access(product, *segment);
    });
    if (!domestic) return {};
    for (const auto& docket : civic->dockets) {
        if (!civic_detail::applies(docket, day)) continue;
        const auto& rule = civic_policy(civic->catalog, docket.policy_id);
        if (rule.rule != CivicRule::launch_evidence || rule.channel != product.channel ||
            (rule.exempt_reviewed_service && request.human_review)) continue;
        const auto* evidence = evaluated(state, request.evidence_id, rule.evidence_axis, day);
        if (!evidence || evidence->model_id != request.model_id || evidence->lower[rule.evidence_axis] < rule.evidence_floor) {
            return rule.title + " (US, effective " + iso_date(*docket.effective_from) + ") requires independent " +
                std::string(lab_domain_names[rule.evidence_axis]) + " evidence with a lower bound of " +
                std::to_string(rule.evidence_floor.value()) + " bp for this new launch. Choose qualifying evidence or an eligible scope.";
        }
    }
    return {};
}

std::string civic_site_block(const CivicState* civic, std::string_view state, std::uint64_t watts, OrdinalDay day) {
    if (!civic) return {};
    for (const auto& docket : civic->dockets) {
        if (!civic_detail::applies(docket, day)) continue;
        const auto& rule = civic_policy(civic->catalog, docket.policy_id);
        if (rule.rule == CivicRule::site_power && rule.jurisdiction == state && watts > rule.permit_watts)
            return rule.title + " (" + rule.jurisdiction + ", effective " + iso_date(*docket.effective_from) +
                ") limits new permits to " + std::to_string(rule.permit_watts / 1000) + " kW. Reduce the reservation or choose another jurisdiction.";
    }
    return {};
}

bool civic_rule_effective_today(const CivicState& civic, OrdinalDay day) { return civic.last_effective_on == day; }

void validate_civic_deployments(const CivicState& civic, const SimulationState& state) {
    if (!state.deployments) return;
    const auto verify=[&](const DeploymentRecord& deployment) {
        const DeployModel request{deployment.service_id, deployment.model_id, deployment.evidence_id, {},
            deployment.human_review, deployment.region_id,deployment.weight_audience};
        if (const auto blocked = civic_launch_block(&civic, state, request, deployment.deployed_on); !blocked.empty())
            invalid("Saved deployment lacks the evidence required on its launch date. " + blocked);
    };
    for (const auto& deployment:state.deployments->records) verify(deployment);
    for (const auto& rollout:state.deployments->rollouts) verify(rollout.candidate);
}
} // namespace pgp::core
