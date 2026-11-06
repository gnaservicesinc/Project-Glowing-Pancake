#include <pgp/core/simulation.hpp>
#include "civic_internal.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
constexpr OrdinalDay first_election_epoch{0};
constexpr std::uint32_t state_assembly_seats = 100;
constexpr int first_election_year = 2016;
using Preferences = std::array<std::uint32_t, civic_blocs>;

OrdinalDay ballot(int year) {
    // The epoch is Wednesday. Monday is index zero.
    const auto first = to_ordinal({year, 11, 1});
    const auto weekday = (first.value() - first_election_epoch.value() + 2) % 7;
    const auto first_monday = (7 - weekday) % 7;
    return advance_days(first, Days{first_monday + 1});
}

std::uint32_t plurality(const Preferences& weights) {
    return static_cast<std::uint32_t>(std::max_element(weights.begin(), weights.end()) - weights.begin());
}

CivicSeats allocate_seats(const CivicCatalog& catalog, std::uint32_t count, const Preferences& weights) {
    CivicSeats result{};
    if (count == 0) return result;
    std::array<WeightedQuota, civic_blocs> quotas;
    for (std::size_t i = 0; i < quotas.size(); ++i) quotas[i] = {catalog.blocs[i].id, weights[i]};
    for (const auto& row : apportion_units(count, quotas)) {
        const auto bloc = std::lower_bound(catalog.blocs.begin(), catalog.blocs.end(), row.id,
            [](const auto& candidate, std::string_view id) { return candidate.id < id; });
        result[static_cast<std::size_t>(bloc - catalog.blocs.begin())] = static_cast<std::uint32_t>(row.units);
    }
    return result;
}

CivicInstitutions opening_bodies(const CivicCatalog& catalog) {
    CivicInstitutions bodies;
    bodies.president = catalog.opening_president;
    for (const auto& state : catalog.constituencies) {
        const auto seats = allocate_seats(catalog, state.house_seats, state.preferences);
        for (std::size_t bloc = 0; bloc < civic_blocs; ++bloc) bodies.house[bloc] += seats[bloc];
        const auto winner = plurality(state.preferences);
        for (std::size_t index = 0; index < state.senate_classes.size(); ++index)
            if (state.senate_classes[index]) ++bodies.senate_classes[index][winner];
        bodies.assemblies.push_back(allocate_seats(catalog, state_assembly_seats, state.preferences));
        bodies.governors.push_back(winner);
    }
    return bodies;
}

BasisPointDelta incumbent_performance(const CivicState& civic, OrdinalDay day) {
    std::int32_t score = 0;
    for (const auto& docket : civic.dockets) {
        if (civic_policy(civic.catalog, docket.policy_id).jurisdiction != "US") continue;
        for (const auto& vote : docket.decisions)
            if (vote.stage == CivicStage::executive && vote.day < day && day.value() - vote.day.value() <= 730)
                score += vote.passed ? 100 : -100;
    }
    return BasisPointDelta{std::clamp(score, -600, 600)};
}

void elect(CivicState& civic, OrdinalDay day, BasisPoints adoption) {
    if (civic.elections.size() >= civic_election_limit)
        throw std::invalid_argument("The election archive reached its simulation bound.");
    const auto& catalog = civic.catalog;
    const auto year = from_ordinal(day).year;
    const auto cycle = static_cast<unsigned>((year - first_election_year) / 2);
    const auto senate_class = cycle % 3;
    const bool presidential = cycle % 2 == 0;
    auto bodies = civic_institutions(civic, day);
    const auto incumbent = bodies.president;
    const auto performance = incumbent_performance(civic, day);
    const auto start = civic.elections.empty() ? civic.politics_origin : civic.elections.back().stream_after;
    SplitMix64 stream{start};
    CivicSeats electors{};
    bodies.house.fill(0);
    bodies.senate_classes[senate_class].fill(0);
    for (std::size_t i = 0; i < catalog.constituencies.size(); ++i) {
        const auto& state = catalog.constituencies[i];
        const auto exposure = (adoption.value() - catalog.adoption_reference.value()) * state.sector_exposure.value() / 10000;
        Preferences weights{};
        for (std::size_t bloc = 0; bloc < weights.size(); ++bloc) {
            const auto& coalition = catalog.blocs[bloc];
            const auto radius = catalog.election_uncertainty.value();
            const auto sample = stream.bounded(static_cast<std::uint64_t>(2 * radius + 1));
            const auto noise = static_cast<std::int64_t>(sample) - radius;
            const auto opinion = coalition.positions[static_cast<std::size_t>(state.priority)] / 16;
            const auto record = bloc == incumbent ? exposure + performance.value() : 0;
            weights[bloc] = static_cast<std::uint32_t>(std::max<std::int64_t>(1,
                static_cast<std::int64_t>(state.preferences[bloc]) + opinion + coalition.campaign_effort.value() + record + noise));
        }
        const auto seats = allocate_seats(catalog, state.house_seats, weights);
        for (std::size_t bloc = 0; bloc < seats.size(); ++bloc) bodies.house[bloc] += seats[bloc];
        const auto winner = plurality(weights);
        bodies.assemblies[i] = allocate_seats(catalog, state_assembly_seats, weights);
        if (state.senate_classes[senate_class]) ++bodies.senate_classes[senate_class][winner];
        if (state.governor_cycle == cycle % 2) bodies.governors[i] = winner;
        if (presidential) electors[winner] += state.house_seats == 0 ? 3U : state.house_seats + 2U;
    }
    if (presidential) bodies.president = plurality(electors);
    civic.elections.push_back({day, adoption, performance, start, stream.state(), electors, std::move(bodies)});
}

std::optional<OrdinalDay> next_agenda(const CivicPolicy& policy, OrdinalDay from) {
    if (policy.first_agenda >= from) return policy.first_agenda;
    if (policy.repeat_years == 0) return std::nullopt;
    const auto first = from_ordinal(policy.first_agenda);
    const auto year = from_ordinal(from).year;
    const auto repeat = static_cast<int>(policy.repeat_years);
    auto candidate = first.year + (year - first.year) / repeat * repeat;
    if (to_ordinal({candidate, first.month, std::min(first.day, days_in_month(candidate, first.month))}) < from)
        candidate += repeat;
    if (candidate > 9980) return std::nullopt;
    return to_ordinal({candidate, first.month, std::min(first.day, days_in_month(candidate, first.month))});
}

std::int64_t support(const CivicCatalog& catalog, const CivicPolicy& policy, const CivicDocket& docket, std::size_t bloc) {
    const auto influence = std::accumulate(docket.submissions.begin(), docket.submissions.end(), std::int64_t{0},
        [](auto total, const auto& row) { return total + (row.support ? row.influence.value() : -row.influence.value()); });
    return catalog.blocs[bloc].positions[static_cast<std::size_t>(policy.issue)] + policy.case_strength.value() + influence;
}

CivicDecision vote(const CivicState& civic, const CivicPolicy& policy, const CivicDocket& docket, CivicStage gate, OrdinalDay day) {
    const auto bodies = civic_institutions(civic, day);
    const bool federal = policy.jurisdiction == "US";
    const auto state = static_cast<std::size_t>(std::lower_bound(civic.catalog.constituencies.begin(), civic.catalog.constituencies.end(),
        policy.jurisdiction, [](const auto& row, std::string_view id) { return row.id < id; }) - civic.catalog.constituencies.begin());
    if (gate == CivicStage::executive) {
        const auto bloc = federal ? bodies.president : bodies.governors.at(state);
        const bool approved = support(civic.catalog, policy, docket, bloc) > 0;
        return {gate, day, approved ? 1U : 0U, 1, approved};
    }
    if (gate == CivicStage::review) {
        const auto approved = std::all_of(docket.decisions.begin(), docket.decisions.end(), [&](const auto& decision) {
            return decision.stage == CivicStage::executive || decision.stage == CivicStage::committee ||
                static_cast<std::int64_t>(decision.yes) * 10000 >= static_cast<std::int64_t>(decision.seats) * policy.review_floor.value();
        });
        return {gate, day, approved ? 1U : 0U, 1, approved};
    }
    auto seats = federal ? (gate == CivicStage::senate ? civic_senate(bodies) : bodies.house) : bodies.assemblies.at(state);
    if (gate == CivicStage::committee) seats = allocate_seats(civic.catalog, 15, seats);
    std::uint32_t yes = 0;
    for (std::size_t bloc = 0; bloc < seats.size(); ++bloc) {
        const auto share = std::clamp<std::int64_t>(5000 + support(civic.catalog, policy, docket, bloc) / 2, 0, 10000);
        yes += static_cast<std::uint32_t>(mul_div_round_half_up(seats[bloc], static_cast<std::uint64_t>(share), 10000));
    }
    const auto total = std::accumulate(seats.begin(), seats.end(), 0U);
    return {gate, day, yes, total, yes > total / 2};
}

void queue_agency(const CivicState& civic, const CivicPolicy& policy, CivicDocket& docket, OrdinalDay day) {
    auto start = day;
    for (const auto& other : civic.dockets)
        if (other.stage == CivicStage::agency && civic_policy(civic.catalog, other.policy_id).jurisdiction == policy.jurisdiction)
            start = std::max(start, *other.next_on);
    docket.stage = CivicStage::agency;
    docket.next_on = advance_days(start, policy.implementation);
}

void advance_docket(CivicState& civic, CivicDocket& docket, OrdinalDay day) {
    const auto& policy = civic_policy(civic.catalog, docket.policy_id);
    if (docket.stage == CivicStage::agency) {
        docket.stage = CivicStage::effective;
        docket.effective_from = day;
        civic.last_effective_on = day;
        docket.expires_before = advance_days(day, policy.term);
        docket.next_on = docket.expires_before;
        return;
    }
    if (docket.stage == CivicStage::effective) {
        docket.stage = CivicStage::expired;
        docket.next_on.reset();
        return;
    }
    const auto gate = docket.stage == CivicStage::agenda ? CivicStage::committee : docket.stage;
    const auto result = vote(civic, policy, docket, gate, day);
    docket.decisions.push_back(result);
    if (!result.passed) {
        docket.stage = CivicStage::rejected;
        docket.next_on.reset();
        return;
    }
    docket.next_on = advance_days(day, policy.vote_spacing);
    switch (gate) {
    case CivicStage::committee: docket.stage = CivicStage::house; break;
    case CivicStage::house: docket.stage = policy.jurisdiction == "US" ? CivicStage::senate : CivicStage::executive; break;
    case CivicStage::senate: docket.stage = CivicStage::executive; break;
    case CivicStage::executive:
        if (policy.review.value() > 0) {
            docket.stage = CivicStage::review;
            docket.next_on = advance_days(day, policy.review);
        } else queue_agency(civic, policy, docket, day);
        break;
    case CivicStage::review: queue_agency(civic, policy, docket, day); break;
    default: throw std::invalid_argument("A civic docket has an invalid calendar stage.");
    }
}
} // namespace

std::optional<OrdinalDay> next_civic_election(OrdinalDay from) {
    auto year = std::max(first_election_year, from_ordinal(from).year);
    if (year % 2 != 0) ++year;
    if (year < 9999 && ballot(year) < from) year += 2;
    return year < 9999 ? std::optional{ballot(year)} : std::nullopt;
}

OrdinalDay civic_seating(OrdinalDay election, bool executive) {
    return to_ordinal({from_ordinal(election).year + 1, 1, executive ? 20U : 3U});
}

CivicInstitutions civic_institutions(const CivicState& civic, OrdinalDay day) {
    auto current = std::upper_bound(civic.elections.begin(), civic.elections.end(), day,
        [](OrdinalDay value, const auto& row) { return value < row.day; });
    if (current == civic.elections.begin()) return opening_bodies(civic.catalog);
    --current;
    const auto& elected = current->result;
    if (day >= civic_seating(current->day, true)) return elected;
    auto bodies = current == civic.elections.begin() ? opening_bodies(civic.catalog) : (current - 1)->result;
    if (day >= civic_seating(current->day, false)) {
        bodies.house = elected.house;
        bodies.senate_classes = elected.senate_classes;
        bodies.assemblies = elected.assemblies;
    }
    return bodies;
}

CivicSeats civic_senate(const CivicInstitutions& bodies) {
    CivicSeats seats{};
    for (const auto& cohort : bodies.senate_classes)
        for (std::size_t i = 0; i < seats.size(); ++i) seats[i] += cohort[i];
    return seats;
}

namespace civic_detail {
std::optional<OrdinalDay> boundary(const CivicState& civic, OrdinalDay from) {
    auto next = next_civic_election(from);
    const auto consider = [&](std::optional<OrdinalDay> value) {
        if (value && *value >= from && (!next || *value < *next)) next = value;
    };
    if (!civic.elections.empty()) {
        consider(civic_seating(civic.elections.back().day, false));
        consider(civic_seating(civic.elections.back().day, true));
    }
    for (const auto& policy : civic.catalog.policies) consider(next_agenda(policy, from));
    for (const auto& docket : civic.dockets) consider(docket.next_on);
    return next;
}

void calendar_day(CivicState& civic, OrdinalDay day, BasisPoints adoption) {
    if (next_civic_election(day) == day) elect(civic, day, adoption);
    for (auto& docket : civic.dockets) if (docket.next_on == day) advance_docket(civic, docket, day);
    for (const auto& policy : civic.catalog.policies) if (next_agenda(policy, day) == day) {
        if (civic.dockets.size() >= civic_docket_limit) throw std::invalid_argument("The policy docket reached its simulation bound.");
        CivicDocket docket;
        docket.id = civic.dockets.size() + 1;
        docket.policy_id = policy.id;
        docket.opened_on = day;
        docket.next_on = advance_days(day, policy.hearing);
        civic.dockets.push_back(std::move(docket));
    }
    civic.prepared_on = day;
    civic.next_boundary = boundary(civic, advance_days(day, Days{1}));
}
} // namespace civic_detail

CivicState opening_civic(CivicCatalog catalog, OrdinalDay start, std::uint64_t stream_origin) {
    validate_civic_catalog(catalog);
    CivicState civic;
    civic.catalog = std::move(catalog);
    civic.starts_on = start;
    civic.politics_origin = stream_origin;
    civic.next_boundary = civic_detail::boundary(civic, start);
    return civic;
}

template<class Rows>
static BasisPoints civic_adoption_rows(const MarketState& market, const Rows& adoptions, OrdinalDay day) {
    const auto current = from_ordinal(day);
    const auto end = OrdinalDay{to_ordinal({current.year, current.month, 1}).value() - 1};
    const auto first = std::lower_bound(adoptions.begin(), adoptions.end(), end,
        [](const auto& row, OrdinalDay date) { return row.reviewed_on < date; });
    std::uint64_t weighted = 0, tasks = 0;
    for (auto row = first; row != adoptions.end() && row->reviewed_on == end; ++row) {
        const auto segment = std::find_if(market.segments.begin(), market.segments.end(), [&](const auto& s) { return s.id == row->segment_id; });
        if (segment == market.segments.end() || (!segment->region_id.empty() && segment->region_id != "region.us")) continue;
        const auto outside = std::find_if(row->shares.begin(), row->shares.end(), [](const auto& s) { return s.product_id == "outside"; });
        if (outside == row->shares.end()) continue;
        tasks += row->observed_tasks;
        weighted += row->observed_tasks * static_cast<std::uint64_t>(10000 - outside->share.value());
    }
    return BasisPoints{tasks ? weighted / tasks : 0};
}

BasisPoints civic_adoption(const MarketState& market, std::span<const MarketAdoption> adoptions, OrdinalDay day) {
    return civic_adoption_rows(market, adoptions, day);
}
BasisPoints civic_adoption(const MarketState& market, const MarketAdoptionHistory& adoptions, OrdinalDay day) {
    return civic_adoption_rows(market, adoptions, day);
}

void prepare_civic(SimulationState& state, const std::vector<MarketAdoption>* industry_adoptions) {
    if (!state.civic || state.civic->prepared_on == state.date) return;
    auto& civic = *state.civic;
    if (civic.next_boundary && *civic.next_boundary < state.date)
        throw std::invalid_argument("The civic calendar cannot skip a due boundary.");
    if (civic.next_boundary == state.date) {
        auto adoption = BasisPoints{0};
        if (state.market && next_civic_election(state.date) == state.date) {
            if (industry_adoptions) {
                auto rows = *industry_adoptions;
                std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
                    return std::tie(left.reviewed_on, left.segment_id) < std::tie(right.reviewed_on, right.segment_id);
                });
                adoption = civic_adoption(*state.market, rows, state.date);
            } else adoption = civic_adoption(*state.market, state.market->adoptions, state.date);
        }
        civic_detail::calendar_day(civic, state.date, adoption);
        if (!civic.elections.empty()) state.random[civic_detail::politics_stream] = civic.elections.back().stream_after;
    }
    civic.prepared_on = state.date;
}
} // namespace pgp::core
