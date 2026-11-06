#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/session/milestone.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <numeric>

namespace {
using namespace pgp;
using namespace core;
using Json = nlohmann::json;
int checks = 0;
void check(bool value, const char* explanation) {
    ++checks;
    if (!value) throw std::runtime_error(explanation);
}
const SimulationState& state(const runtime::Runtime& game) { return game.current_state().world.simulation; }
const CivicDocket& docket(const runtime::Runtime& game, std::string_view policy) {
    const auto& rows = state(game).civic->dockets;
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.policy_id == policy; });
    if (found == rows.end()) throw std::runtime_error("Missing civic test docket " + std::string(policy));
    return *found;
}
void apply(runtime::Runtime& game, CommandAction action) {
    const auto result = game.submit({state(game).revision, std::move(action)});
    if (!result.accepted()) throw std::runtime_error(iso_date(state(game).date) + " · " + result.explanation);
}
void rejected(runtime::Runtime& game, CommandAction action, std::string_view reason) {
    const auto original = game.checkpoint();
    const auto result = game.submit({state(game).revision, std::move(action)});
    check(!result.accepted() && result.explanation.find(reason) != std::string::npos && game.checkpoint() == original,
        "A civic refusal had the wrong reason or changed the campaign.");
}
void opening_choices(runtime::Runtime& game) {
    for (;;) {
        const auto& cards = game.current_state().runtime.presentations;
        const auto found = std::find_if(cards.begin(), cards.end(), [](const auto& card) { return !card.resolved; });
        if (found == cards.end()) break;
        const auto result = game.choose(state(game).revision, found->id, "");
        if (!result.accepted()) throw std::runtime_error(result.explanation);
    }
}
void advance(runtime::Runtime& game, OrdinalDay to) {
    while (state(game).date < to) {
        opening_choices(game);
        if (const auto* alert = required_site_alert(state(game))) apply(game, AcknowledgeSiteAlert{alert->id});
        if (const auto warning = liquidity_report(*state(game).liquidity).pending_warning) apply(game, AcknowledgeLiquidityWarning{*warning});
        if (required_management_notice(state(game))) {
            const auto& notices = state(game).management->notices;
            const auto pending = std::find_if(notices.begin(), notices.end(), [](const auto& row) { return row.required && !row.acknowledged; });
            if (pending == notices.end()) throw std::runtime_error("The fixture has a required management event without an acknowledgment.");
            apply(game, AcknowledgeManagementNotice{pending->id});
            continue;
        }
        apply(game, AdvanceDay{});
    }
}
runtime::Runtime restore(const runtime::Runtime& game) {
    const auto bytes = persistence::encode_save(game.program(), game.checkpoint());
    auto loaded = persistence::decode_save(game.program(), bytes);
    check(loaded.checkpoint == game.checkpoint(), "The civic checkpoint changed on exact reload.");
    return {game.program(), std::move(loaded.checkpoint)};
}
runtime::CompiledProgram compile(Json document) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(document.dump(), registry), registry);
}
Json source() { return Json::parse(persistence::read_bounded(PGP_CIVIC_SOURCE, content::maximum_document_bytes)); }
void forged_refusal(const runtime::Runtime& game, auto change) {
    auto document = Json::parse(persistence::encode_save(game.program(), game.checkpoint()));
    auto& civic = document["checkpoint"]["world"]["simulation"]["civic"];
    change(civic);
    document.erase("sha256");
    document["sha256"] = persistence::sha256(document.dump());
    bool refused = false;
    try { (void)persistence::decode_save(game.program(), document.dump()); }
    catch (const persistence::PersistenceError&) { refused = true; }
    check(refused, "A correctly checksummed forged civic history was accepted.");
}

void play_dockets_and_election(const runtime::CompiledProgram& program) {
    runtime::Runtime game{program, runtime::begin_scenario(program, "scenario.first_playable", "US-CA")};
    check(game.prepare_opening(state(game).revision).accepted() && state(game).civic->dockets.size() == 2,
        "The authored opening dockets did not prepare.");
    const auto& opening = *state(game).civic;
    const auto bodies = civic_institutions(opening, state(game).date);
    const auto senate = civic_senate(bodies);
    check(std::accumulate(bodies.house.begin(), bodies.house.end(), 0U) == 435 &&
        std::accumulate(senate.begin(), senate.end(), 0U) == 100 &&
        opening.catalog.constituencies.size() == 51, "Opening institutions fail the nationwide seat totals.");
    const auto ca = docket(game, "policy.ca_connection").id;
    const auto federal = docket(game, "policy.agent_assurance").id;
    const auto fee = civic_policy(opening.catalog, "policy.agent_assurance").advocacy_fee;
    const auto before = state(game).cash;
    const auto rng = state(game).random;
    const auto due = session::plan_milestone(game.checkpoint(), "civic").deadline;
    check(due == docket(game, "policy.agent_assurance").next_on, "Civic fast-forward omits the public committee boundary.");
    apply(game, ReserveFunds{"Protect opening cash", before, true});
    rejected(game, FundPolicyAdvocacy{federal, true}, "unreserved");
    apply(game, CancelReservation{1});
    for (std::size_t i = 1; i <= 3; ++i) {
        const auto& civic = *state(game).civic;
        const auto quote = civic_advocacy_terms(civic, docket(game, "policy.agent_assurance"), state(game).date);
        check(quote && quote->influence.value() == civic_policy(civic.catalog, "policy.agent_assurance").advocacy_shift.value() / static_cast<std::int64_t>(i),
            "The paid influence quote did not diminish.");
        apply(game, FundPolicyAdvocacy{federal, true});
    }
    rejected(game, FundPolicyAdvocacy{federal, false}, "exhausted");
    rejected(game, SubmitPolicyEvidence{federal, "forged-report", true}, "eligible");
    check(state(game).cash == Cents{before.value() - 3 * fee.value()} && state(game).random == rng,
        "Public advocacy spending did not settle exactly or affected an election stream.");
    apply(game, BuildOwnedSite{"old_ca", "Existing California reservation", "US-CA", 160000, CoolingMode::air});
    while (docket(game, "policy.ca_connection").stage != CivicStage::agency) {
        const auto& current = docket(game, "policy.ca_connection");
        if (!current.next_on) throw std::runtime_error("The California fixture did not clear its published votes.");
        advance(game, *current.next_on);
    }
    const auto effective = *docket(game, "policy.ca_connection").next_on;
    const auto blocked = BuildOwnedSite{"new_ca", "New California reservation", "US-CA", 160000, CoolingMode::air};
    advance(game, OrdinalDay{effective.value() - 1});
    check(civic_site_block(&*state(game).civic, "US-CA", 160000, state(game).date).empty(), "Agency queue enforced a rule early.");
    auto resumed = restore(game);
    advance(game, effective);
    advance(resumed, effective);
    check(resumed.checkpoint() == game.checkpoint(), "Agency implementation duplicated or changed after a pending reload.");
    check(civic_rule_effective_today(*state(game).civic, effective) && state(game).infrastructure->sites.size() == 1,
        "The actual effective date or grandfathered site reservation changed.");
    rejected(game, blocked, "150 kW");
    check(civic_site_block(&*state(game).civic, "US-WA", 160000, effective).empty() &&
        civic_site_block(&*state(game).civic, "US-CA", 150000, effective).empty(),
        "The state rule escaped its territory or excluded its exact threshold.");
    const auto sites = civic_evidence_options(state(game), docket(game, "policy.ca_connection"));
    check(sites.empty(), "An effective rule kept accepting committee evidence.");
    rejected(game, FundPolicyAdvocacy{ca, false}, "window");
    forged_refusal(game, [](Json& civic) { civic["dockets"][1]["effective_from"] = 0; });

    while (docket(game, "policy.agent_assurance").stage != CivicStage::effective) {
        const auto& current = docket(game, "policy.agent_assurance");
        if (!current.next_on) throw std::runtime_error("The federal fixture did not clear its institutions and procedural review.");
        advance(game, *current.next_on);
    }
    const auto& law = docket(game, "policy.agent_assurance");
    check(law.decisions.size() == 5 && law.decisions[1].stage == CivicStage::house && law.decisions[2].stage == CivicStage::senate &&
        law.decisions[3].stage == CivicStage::executive && law.decisions[4].stage == CivicStage::review,
        "Federal policy bypassed a chamber, executive decision or procedural review.");
    const DeployModel launch{"product.workflow_agent", "model.missing", "report.missing", "lease.initial", true, "region.us"};
    rejected(game, launch, "5800 bp");
    auto foreign = launch;
    foreign.region_id = "region.europe";
    check(civic_launch_block(&*state(game).civic, state(game), foreign, state(game).date).empty(),
        "A US launch gate followed a different launch territory.");
    check(civic_site_block(&*state(game).civic, "US-CA", 160000, *docket(game, "policy.ca_connection").expires_before).empty(),
        "A state rule survived its exclusive expiration.");
    (void)restore(game);

    // Keep a long fixture solvent without substituting a new ruleset.
    apply(game, DrawDebt{{"fixture.civic-runway", Cents{500'000'000}, BasisPoints{0}, state(game).date,
        to_ordinal({2016, 12, 20}), 0, std::nullopt}, "Election boundary fixture funding"});
    apply(game, BuyData{"data.licensed_documents"});
    apply(game, StartResearch{"research.held_out_evaluation", {"lease.initial", MilliCUPerDay{20000}, 2}});
    apply(game, StartTraining{{"model.civic", "recipe.document_baseline", "", {"data.licensed_documents"},
        "lease.initial", MilliCUPerDay{60000}, 3, MilliCU{3600000}}});
    advance(game, advance_days(state(game).date, Days{60}));
    apply(game, StartEvaluation{"evidence.civic", "model.civic", "evaluation.held_out", {"lease.initial", MilliCUPerDay{20000}, 1}});
    advance(game, advance_days(state(game).date, Days{4}));
    apply(game, DeployModel{"product.documents", "model.civic", "evidence.civic", "lease.initial", false, "region.us"});
    advance(game, to_ordinal({2015, 4, 7}));
    const auto audit = docket(game, "policy.licence_review").id;
    const auto evidence = civic_evidence_options(state(game), docket(game, "policy.licence_review"));
    check(std::find(evidence.begin(), evidence.end(), "evidence.civic") != evidence.end(),
        "A relevant completed independent evaluation was absent from the docket.");
    apply(game, SubmitPolicyEvidence{audit, "evidence.civic", false});
    rejected(game, SubmitPolicyEvidence{audit, "evidence.civic", true}, "eligible");
    const auto ballot = to_ordinal({2016, 11, 8});
    check(next_civic_election(OrdinalDay{0}) == ballot, "The first US ballot missed the November 2016 cycle.");
    advance(game, OrdinalDay{ballot.value() - 1});
    const auto& audited = docket(game, "policy.licence_review");
    check(audited.stage == CivicStage::rejected && audited.decisions.back().stage == CivicStage::review &&
        !audited.effective_from && audited.submissions.front().evidence == "evidence.civic",
        "A procedural review failed to retain the evidence or stop the unimplemented rule.");
    auto ballot_reload = restore(game);
    advance(game, ballot);
    advance(ballot_reload, ballot);
    check(game.checkpoint() == ballot_reload.checkpoint() && state(game).civic->elections.size() == 1,
        "Reloading the night before the election rerolled or duplicated it.");
    const auto& election = state(game).civic->elections.front();
    check(std::accumulate(election.electors.begin(), election.electors.end(), 0U) == 538 &&
        civic_institutions(*state(game).civic, ballot) == bodies && election.stream_before != election.stream_after,
        "The presidential ballot lost electors, failed to draw, or seated its winners early.");
    check(election.adoption.value() > 0 && election.adoption == civic_adoption(*state(game).market, state(game).market->adoptions, ballot),
        "The ballot did not use the actual prior domestic service review.");
    check(civic_seating(ballot, false) == to_ordinal({2017, 1, 3}) && civic_seating(ballot, true) == to_ordinal({2017, 1, 20}) &&
        civic_institutions(*state(game).civic, civic_seating(ballot, true)) == election.result,
        "The elected terms did not seat on their own boundaries.");
    check(std::count_if(game.current_state().runtime.presentations.begin(), game.current_state().runtime.presentations.end(),
        [](const auto& card) { return card.flow_id == "civic.election_briefing"; }) == 1,
        "The shared flow runtime did not present the election exactly once.");
    forged_refusal(game, [](Json& civic) { civic["elections"][0]["stream_after"] = 1; });
    forged_refusal(game, [](Json& civic) { civic["elections"][0]["result"]["house"][0] = 435; });
    (void)restore(game);
}

void staggered_terms(const CivicCatalog& catalog) {
    SimulationState start;
    start.date = to_ordinal({2016, 11, 7});
    start.end_date = to_ordinal({2023, 1, 22});
    start.cash = Cents{1000};
    start.accounts = opening_accounts(start.cash, start.date);
    start.civic = opening_civic(catalog, start.date, start.random[static_cast<std::size_t>(RandomStream::politics)]);
    Simulation clock{std::move(start)};
    const auto before = civic_institutions(*clock.checkpoint().civic, clock.checkpoint().date);
    while (clock.checkpoint().phase != DayPhase::finished) {
        const auto result = clock.submit_in_transaction({clock.checkpoint().revision, AdvanceDay{}});
        if (!result.accepted()) throw std::runtime_error(result.explanation);
    }
    validate_simulation(clock.checkpoint());
    const auto& rows = clock.checkpoint().civic->elections;
    check(rows.size() == 4 && from_ordinal(rows[1].day).year == 2018 && from_ordinal(rows[3].day).year == 2022,
        "The six-year election fixture skipped an even-year ballot.");
    check(rows[0].result.senate_classes[1] == before.senate_classes[1] &&
        rows[0].result.senate_classes[2] == before.senate_classes[2] &&
        rows[1].result.senate_classes[0] == rows[0].result.senate_classes[0] &&
        rows[1].result.president == rows[0].result.president &&
        rows[1].electors == CivicSeats{} && rows[3].electors == CivicSeats{},
        "The midterms replaced an unscheduled Senate class or opened an extra presidential ballot.");
}
} // namespace

int main() {
    try {
        const auto program = compile(source());
        const auto& catalog = std::get<core::CivicCatalog>(program.collection.definitions.at("civic.domestic").data);
        play_dockets_and_election(program);
        staggered_terms(catalog);
        std::cout << checks << " civic calendar, enforcement and exact reload checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
