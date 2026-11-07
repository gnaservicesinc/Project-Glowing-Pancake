#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#if PGP_TEST_STUDIO
#include <pgp/studio/flow_probe.hpp>
#endif

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;
using namespace pgp::core;
using Json = nlohmann::json;
constexpr std::string_view century = "scenario.century_frontier";
constexpr std::string_view recovery = "century.product_customer.cedar_recovery";
constexpr std::string_view story = "story.cedar_delivery_review";
constexpr std::string_view missed = "coverage.missed_pilot";

void check(bool okay, std::string_view reason) {
    if (!okay) throw std::runtime_error(std::string(reason));
}
runtime::CompiledProgram compile(const Json& source) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source.dump(), registry), registry);
}
const SimulationState& simulation(const runtime::Runtime& game) {
    return game.current_state().world.simulation;
}
void accepted(const runtime::RuntimeResult& result) { check(result.accepted(), result.explanation); }
void apply(runtime::Runtime& game, CommandAction action) {
    const auto result = game.submit({simulation(game).revision, std::move(action)});
    if (!result.accepted()) throw std::runtime_error(iso_date(simulation(game).date) + " · " + result.explanation);
}
const runtime::Presentation* waiting(const runtime::Runtime& game) {
    const auto& rows = game.current_state().runtime.presentations;
    const auto row = std::ranges::find_if(rows, [](const auto& presentation) {
        return presentation.kind == runtime::PresentationKind::dialogue && !presentation.resolved;
    });
    return row == rows.end() ? nullptr : &*row;
}
void choose(runtime::Runtime& game, std::string_view flow, std::string_view selection) {
    const auto* prompt = waiting(game);
    check(prompt && prompt->flow_id == flow, "Expected the authored choice before more work.");
    accepted(game.choose(simulation(game).revision, prompt->id, selection));
}
void until(runtime::Runtime& game, OrdinalDay target) {
    while (simulation(game).date < target) {
        if (const auto* prompt = waiting(game)) choose(game, prompt->flow_id, "decline");
        if (const auto* alert = required_site_alert(simulation(game))) apply(game, AcknowledgeSiteAlert{alert->id});
        if (required_management_notice(simulation(game))) {
            const auto& notices = simulation(game).management->notices;
            const auto row = std::ranges::find_if(notices, [](const auto& n) { return n.required && !n.acknowledged; });
            apply(game, AcknowledgeManagementNotice{row->id});
        }
        apply(game, AdvanceDay{});
    }
}
runtime::Runtime reload(const runtime::Runtime& game) {
    const auto& program = game.program();
    const auto bytes = persistence::encode_save(program, game.current_state());
    auto loaded = persistence::decode_save(program, bytes);
    check(loaded.checkpoint == game.current_state(), "A campaign or story wait changed on reload.");
    return {program, std::move(loaded.checkpoint)};
}
bool has_flow(const runtime::Runtime& game, std::string_view id) {
    return std::ranges::any_of(game.current_state().runtime.activations, [&](const auto& row) {
        return row.definition_id == id;
    });
}
bool tagged(const content::FlowDefinition& flow, std::string_view tag) {
    return std::ranges::find(flow.tags, tag) != flow.tags.end();
}
void coverage(const runtime::CompiledProgram& program) {
    constexpr std::array paths{"specialist", "open_ecosystem", "frontier_lab",
                               "public_service", "infrastructure_provider", "institution"};
    for (const auto id : {century, std::string_view{"scenario.century_plateau"}}) {
        const auto& scenario = std::get<content::ScenarioDefinition>(program.collection.find(id)->data);
        check(scenario.flows.size() >= 120 && scenario.research.size() >= 60 && scenario.lab->data.size() >= 24 &&
              scenario.infrastructure->hardware.size() >= 18 && scenario.competition->domestic == 6 &&
              scenario.competition->international == 6 && scenario.competition->heavyweights == 3,
              "A century catalog lost a path, roster or content family.");
        runtime::Runtime opening{program, runtime::begin_scenario(program, id, "US-WA")};
        accepted(opening.prepare_opening(simulation(opening).revision));
        const auto& companies = opening.current_state().world.competition->companies;
        check(companies.size() == 15 && std::ranges::count_if(companies, [](const auto& company) {
            return company.planning->entered_on.has_value();
        }) == 6, "The roster did not stage six opening and nine later companies.");
        std::size_t opportunity = 0;
        for (const auto& flow_id : scenario.flows) {
            const auto& flow = std::get<content::FlowDefinition>(program.collection.find(flow_id)->data);
            if (tagged(flow, "opportunity") || tagged(flow, "recovery")) ++opportunity;
        }
        check(opportunity * 3 >= scenario.flows.size(), "Too few opportunity and recovery flows.");
        for (const auto* path : paths) {
            for (const auto* stage : {"early", "operational", "late"}) {
                check(std::ranges::any_of(scenario.flows, [&](const auto& flow_id) {
                    const auto& flow = std::get<content::FlowDefinition>(program.collection.find(flow_id)->data);
                    return tagged(flow, path) && tagged(flow, stage) &&
                        (std::string_view(stage) != "early" || tagged(flow, "opportunity"));
                }), "A strategic path is missing a useful opening or later review.");
            }
        }
    }
}
runtime::Runtime prepared(const runtime::CompiledProgram& program, bool stories) {
    auto opening = runtime::begin_scenario(program, century, "US-WA");
    if (!stories) {
        std::erase_if(*opening.world.enabled_flows, [&](const auto& id) {
            return tagged(std::get<content::FlowDefinition>(program.collection.find(id)->data), "story");
        });
    }
    runtime::Runtime game{program, std::move(opening)};
    accepted(game.prepare_opening(simulation(game).revision));
    apply(game, BuyData{"data.open_documents"});
    apply(game, StartResearch{"research.held_out_evaluation", {"lease.initial", MilliCUPerDay{20000}, 2}});
    apply(game, StartResearch{"research.reproducible_tooling", {"lease.initial", MilliCUPerDay{10000}, 1}});
    apply(game, StartTraining{{"coverage.model", "recipe.document_baseline", "", {"data.open_documents"},
                              "lease.initial", MilliCUPerDay{60000}, 3, MilliCU{3600000}}});
    until(game, advance_days(simulation(game).date, Days{60}));
    apply(game, StartEvaluation{"coverage.evidence", "coverage.model", "evaluation.held_out",
                               {"lease.initial", MilliCUPerDay{20000}, 1}});
    until(game, advance_days(simulation(game).date, Days{4}));
    apply(game, DeployModel{"product.documents", "coverage.model", "coverage.evidence",
                           "lease.initial", false, "region.us"});
    const auto day = simulation(game).date;
    MarketContract contract;
    contract.id = missed;
    contract.segment_id = "segment.enterprise";
    contract.product_id = "product.documents";
    contract.tasks_per_day = 100;
    contract.monthly_fee = Cents{10000};
    contract.missed_start_penalty = Cents{50000};
    contract.accepted_on = day;
    contract.start_by = day;
    contract.ends_before = advance_days(day, Days{10});
    contract.cause = "Coverage fixture: actual unsigned start confirmation.";
    apply(game, AcceptContract{contract});
    apply(game, AdvanceDay{});
    return game;
}
void check_recovered(const runtime::Runtime& game) {
    const auto& s = simulation(game);
    const auto& contracts = s.market->contracts;
    const auto row = std::ranges::find(contracts, "pilot/offer.regional_recovery", &MarketContract::id);
    check(row != contracts.end() && row->status == ContractStatus::active && row->tasks_per_day == 300,
          "The core pilot was not signed under its disclosed narrow terms.");
    check(std::ranges::any_of(s.market->days.back().contracts, [](const auto& settled) {
        return settled.contract_id == "pilot/offer.regional_recovery" && settled.delivered_tasks == 300;
    }), "The recovery pilot did not settle its promised work.");
    check(has_flow(game, "century.research_data.first_license") && has_flow(game, "slice.research_milestone") &&
          has_flow(game, "slice.pilot_result"), "Ordinary campaign activity depends on optional stories.");
}
void recovery_walkthrough(Json source) {
    // Start with the same terms and roster at the beginning of the recovery
    // offer's era. Give opening-only offers a one-day fixture window.
    auto society = *std::ranges::find_if(source["definitions"], [](const auto& row) {
        return row["id"] == "society.century";
    });
    society["id"] = "society.coverage_fixture";
    society["catalog"]["forum"]["first_agenda_day"] = to_ordinal({2017,1,1}).value();
    source["definitions"].push_back(std::move(society));
    for (auto& row : source["definitions"]) if (row["id"] == century) {
        row["starts_on"] = "2017-01-01";
        row["society"] = "society.coverage_fixture";
        row["management"]["setbacks"] = Json::array();
        // The fixture starts three years after the original founder contacts.
        row.erase("backgrounds");
        for (auto& offer : row["business_offers"]) {
            if (offer["available_from"] < "2017-01-01") offer["available_from"] = "2017-01-01";
            if (offer["available_before"] <= "2017-01-01") offer["available_before"] = "2017-01-02";
        }
    }
    const auto program = compile(source);
    auto baseline = prepared(program, false);
    const auto& checkpoint = baseline.current_state();
    const auto& world = checkpoint.world;
    const runtime::ScopeBinding company{content::Scope::company, world.company_id};
    const auto fact = [&](std::string_view id) {
        return std::get<bool>(runtime::read_fact(world, *program.registry.fact(id), company));
    };
    check(fact("company.supplier_offer_eligible") && !fact("company.small_supplier_eligible"),
          "An unrelated eligible lease reopened the expired supplier terms.");
    check(!has_flow(baseline, story), "The optional story opened despite being disabled.");
    baseline = reload(baseline);
    const auto cash = simulation(baseline).cash;
    auto refused = baseline;
    choose(refused, recovery, "decline");
    check(simulation(refused).cash == cash && simulation(refused).business_offers->accepted.empty(),
          "Declining the narrower pilot committed terms or moved cash.");
    choose(baseline, recovery, "accept");
    check(simulation(baseline).cash == cash, "Signing a recovery pilot granted unearned cash.");
    until(baseline, advance_days(simulation(baseline).date, Days{8}));
    check_recovered(baseline);

    auto enriched = prepared(program, true);
    choose(enriched, recovery, "accept");
    const auto activation = std::ranges::find(enriched.current_state().runtime.activations, story, &runtime::Activation::definition_id);
    check(activation != enriched.current_state().runtime.activations.end() && activation->committed_source &&
          activation->committed_source->subject.id == missed, "The story lost its actual missed contract receipt.");
    const auto notice = std::ranges::find_if(enriched.current_state().runtime.activations, [&](const auto& row) {
        return row.definition_id == "slice.pilot_result" && row.committed_source && row.committed_source->subject.id == missed;
    });
    check(notice != enriched.current_state().runtime.activations.end() &&
          notice->committed_source->settled_on == activation->committed_source->settled_on,
          "The story and baseline report did not share the same settlement.");
    enriched = reload(enriched);
#if PGP_TEST_STUDIO
    studio::FlowProbe probe{program, enriched.current_state()};
    check(probe.inspect(story, runtime::FlowPredicate::trigger).trace.result == true,
          "Studio could not explain the eligibility of the authored follow-up.");
#endif
    auto declined = enriched;
    choose(declined, story, "decline");
    choose(enriched, story, "accept");
    check(enriched.current_state().world.decisions == std::vector<std::string>{"cedar.ledger_review"},
          "The follow-up failed to record its chosen reminder.");
    enriched = reload(enriched);
    until(declined, simulation(baseline).date);
    until(enriched, simulation(baseline).date);
    check(simulation(declined) == simulation(enriched), "The optional reminder changed the underlying contract or accounts.");
    check(std::ranges::count(enriched.current_state().runtime.presentations,
          "story.cedar_delivery_review.followup", &runtime::Presentation::text_key) == 1,
          "The dated follow-up was lost or repeated after its saved wait.");
    check(std::ranges::count(declined.current_state().runtime.presentations,
          "story.cedar_delivery_review.followup", &runtime::Presentation::text_key) == 0,
          "Declining the story still produced its delayed follow-up.");
    check_recovered(enriched);
    enriched = reload(enriched);

    apply(enriched, PublishResearch{"research.reproducible_tooling"});
    apply(enriched, AdvanceDay{});
    enriched = reload(enriched);
    const auto before_lease = simulation(enriched).cash;
    choose(enriched, "story.morrow_cooperative", "accept");
    const auto& leases = simulation(enriched).business_offers->accepted;
    const auto lease = std::ranges::find(leases, "offer.cooperative_burst", &AcceptedBusinessOffer::offer_id);
    check(lease != leases.end() && simulation(enriched).cash == checked_unit_subtract(before_lease, Cents{120000}),
          "The cooperative story did not charge its disclosed setup and sign the bounded lease.");
    const auto expiry = lease->expires_before;
    enriched = reload(enriched);
    until(enriched, expiry);
    const auto& sources = simulation(enriched).capacity->sources;
    const auto source_row = std::ranges::find_if(sources, [](const auto& row) {
        return row.id.find("cooperative_burst") != std::string::npos;
    });
    check(source_row != sources.end() && source_row->effective.value() == 0 &&
          std::ranges::count(enriched.current_state().runtime.presentations, "story.morrow_cooperative.followup",
                            &runtime::Presentation::text_key) == 1,
          "The paid story lease or its follow-up did not expire at the real boundary.");
}

void seeded_incidents(Json source) {
    struct Incident {
        std::string_view id, segment;
        std::int64_t rise, fall;
    };
    constexpr std::array incidents{
        Incident{"century.product_customer.cedar_quarter", "segment.canada.enterprise", 1200, -800},
        Incident{"century.product_customer.district_rollout", "segment.europe.public_services", 1500, -1000},
        Incident{"century.product_customer.harbormark_cycle", "segment.asia_pacific.industrial", 1000, -1200}
    };
    for (auto& row : source["definitions"]) for (const auto& event : incidents) if (row["id"] == event.id) {
        // Move the first eligible review to the opening of this isolated
        // fixture. Keep the real scope, source, effects, graph and expiry.
        row["trigger"]["all_of"][0]["value"] = 0;
        row["lifecycle"]["anchor"]["offset"]["value"] = 0;
    }
    for (const auto branch : {0, 1, 2}) {
        const auto expected_branch = [&](std::uint64_t seed) {
            RandomStreams streams{seed};
            const auto draw = streams.stream(RandomStream::markets).bounded(10);
            return draw < 6 ? 0 : draw < 8 ? 1 : 2;
        };
        std::uint64_t seed = 0;
        while (expected_branch(seed) != branch) ++seed;
        for (auto& row : source["definitions"]) if (row["id"] == century) row["seed"] = seed;
        const auto program = compile(source);
        for (const auto& event : incidents) {
            auto initial = runtime::begin_scenario(program, century, "US-WA");
            initial.world.enabled_flows = std::vector{std::string(event.id)};
            runtime::Runtime game{program, std::move(initial)};
            accepted(game.prepare_opening(simulation(game).revision));
            const auto& changes = simulation(game).market->demand_changes;
            if (branch == 0) {
                check(changes.empty() && game.current_state().runtime.presentations.empty(),
                      "A quiet demand draw changed the market or published an incident.");
                continue;
            }
            check(changes.size() == 1 && changes.front().segment_id == event.segment &&
                  changes.front().delta.value() == (branch == 1 ? event.rise : event.fall),
                  "An incident changed the wrong region, magnitude or direction.");
            const auto expiry = changes.front().expires_before;
            game = reload(game);
            until(game, expiry);
            const auto& market = *simulation(game).market;
            const auto segment = std::ranges::find(market.segments, event.segment, &MarketSegment::id);
            check(segment != market.segments.end() && effective_market_demand(market, *segment, expiry) == segment->daily_tasks &&
                  game.current_state().runtime.presentations.size() == 2,
                  "A saved demand change did not expire once at its disclosed boundary.");
        }
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("Usage: pgp-campaign-content-test [collection]");
        std::ifstream file{argc == 2 ? argv[1] : PGP_SLICE_CONTENT_PATH};
        const auto source = Json::parse(file);
        coverage(compile(source));
        recovery_walkthrough(source);
        seeded_incidents(source);
        std::cout << "Six paths and both century catalogs verified. Real pilot recovered with and without stories. "
                     "Paid story lease, seeded demand and saved expiry verified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
