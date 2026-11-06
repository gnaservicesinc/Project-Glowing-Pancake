#include <pgp/core/simulation.hpp>
#include "civic_internal.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* detail) { throw std::invalid_argument(detail); }

struct SubmissionRef {
    std::uint64_t docket;
    std::size_t index;
    const CivicSubmission* submission;
};

void verify_history(const SimulationState& simulation, const CivicState& saved) {
    auto replay = opening_civic(saved.catalog, saved.starts_on, saved.politics_origin);
    std::map<std::string_view, const Posting*, std::less<>> fees;
    for (const auto& posting : simulation.accounts->postings)
        if (posting.key.starts_with("civic.advocacy/") && !fees.emplace(posting.key, &posting).second)
            invalid("The civic disclosure ledger contains a duplicate fee.");
    std::vector<SubmissionRef> submissions;
    for (std::size_t index = 0; index < saved.dockets.size(); ++index) {
        const auto& docket = saved.dockets[index];
        if (docket.id != index + 1 || docket.submissions.size() > civic_advocacy_limit + 1 || docket.decisions.size() > 5)
            invalid("A saved docket exceeded its identity, vote or disclosure bound.");
        for (std::size_t i = 0; i < docket.submissions.size(); ++i)
            submissions.push_back({docket.id, i, &docket.submissions[i]});
    }
    std::sort(submissions.begin(), submissions.end(), [](const auto& left, const auto& right) {
        return std::tie(left.submission->day, left.docket, left.index) < std::tie(right.submission->day, right.docket, right.index);
    });
    auto next_submission = submissions.begin();
    auto next_election = saved.elections.begin();
    const auto process_until = [&](OrdinalDay day) {
        while (replay.next_boundary && *replay.next_boundary <= day) {
            const auto boundary = *replay.next_boundary;
            auto adoption = BasisPoints{0};
            if (next_civic_election(boundary) == boundary) {
                if (next_election == saved.elections.end() || next_election->day != boundary)
                    invalid("A scheduled election is missing from the saved calendar.");
                adoption = next_election->adoption;
                ++next_election;
            }
            civic_detail::calendar_day(replay, boundary, adoption);
        }
    };
    while (next_submission != submissions.end()) {
        const auto [id, index, row] = *next_submission++;
        if (!saved.prepared_on || row->day > *saved.prepared_on || row->day < saved.starts_on)
            invalid("A public submission is outside the played calendar.");
        process_until(row->day);
        if (id > replay.dockets.size()) invalid("A public submission precedes its docket.");
        auto& docket = replay.dockets[static_cast<std::size_t>(id - 1)];
        const auto& policy = civic_policy(replay.catalog, docket.policy_id);
        if (docket.submissions.size() != index || !civic_detail::commenting(docket, row->day))
            invalid("A public submission missed its eligible hearing window.");
        if (row->evidence.empty()) {
            const auto terms = civic_advocacy_terms(replay, docket, row->day);
            if (!terms || row->fee != terms->fee || row->influence != terms->influence)
                invalid("Saved advocacy differs from its disclosed terms and diminishing returns.");
            const auto key = civic_detail::advocacy_key(id, civic_advocacy_limit - terms->remaining + 1);
            const auto found = fees.find(key);
            if (found == fees.end() || found->second->day != row->day || found->second->amount != row->fee ||
                found->second->debit != Account::expenses || found->second->credit != Account::cash)
                invalid("A civic disclosure has no matching expense and cash posting.");
            fees.erase(found);
        } else if (row->fee.value() != 0 || row->influence != policy.evidence_shift || policy.evidence_shift.value() == 0 ||
            !civic_detail::has_evidence(simulation, policy, row->evidence, row->day) ||
            std::any_of(docket.submissions.begin(), docket.submissions.end(), [](const auto& prior) { return !prior.evidence.empty(); }))
            invalid("Saved civic evidence lacks a distinct eligible supporting record.");
        docket.submissions.push_back(*row);
    }
    if (saved.prepared_on) process_until(*saved.prepared_on);
    replay.prepared_on = saved.prepared_on;
    if (!fees.empty() || next_election != saved.elections.end() || replay != saved)
        invalid("The saved ballots, institutions or policy dates differ from their recorded calendar and inputs.");
}
} // namespace

void validate_civic(const SimulationState& simulation, bool deep) {
    if (!simulation.civic) return;
    const auto& civic = *simulation.civic;
    const auto phase = simulation.phase;
    const auto date = simulation.date;
    const auto prior = OrdinalDay{checked_subtract(date.value(), 1)};
    if (!simulation.accounts || civic.starts_on != simulation.accounts->starts_on ||
        civic.elections.size() > civic_election_limit || civic.dockets.size() > civic_docket_limit ||
        (civic.prepared_on && (*civic.prepared_on < civic.starts_on || *civic.prepared_on > date ||
            (civic.next_boundary && *civic.next_boundary <= *civic.prepared_on))) ||
        ((phase == DayPhase::work_ready || phase == DayPhase::awaiting_choice) && civic.prepared_on != date) ||
        (phase == DayPhase::finished && civic.prepared_on != prior) ||
        (phase == DayPhase::closed && civic.prepared_on != date && civic.prepared_on != prior) ||
        (!civic.prepared_on && (date != civic.starts_on || phase != DayPhase::opening)) ||
        simulation.random[civic_detail::politics_stream] != (civic.elections.empty() ? civic.politics_origin : civic.elections.back().stream_after))
        invalid("The civic calendar or politics stream differs from the saved morning.");
    if (!deep) return;
    verify_history(simulation, civic);
    validate_civic_deployments(civic, simulation);
    if (simulation.infrastructure) for (const auto& site : simulation.infrastructure->sites)
        if (!civic_site_block(&civic, site.state_id, site.grid_watts, site.ordered_on).empty())
            invalid("Saved site exceeds the permit in force when construction was authorized.");
}
} // namespace pgp::core
