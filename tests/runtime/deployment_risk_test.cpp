#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
using namespace core;
using Json = nlohmann::json;
unsigned checks = 0;
void check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
template<class Rows> const auto& find(const Rows& rows, std::string_view id) {
    const auto row = std::ranges::find_if(rows, [&](const auto& item) { return item.id == id; });
    if (row == rows.end()) throw std::runtime_error("Missing expected record: " + std::string(id));
    return *row;
}
const SimulationState& state(const runtime::Runtime& game) { return game.current_state().world.simulation; }
const DeploymentRiskState& risk(const runtime::Runtime& game) { return *state(game).deployments->risk; }
void apply(runtime::Runtime& game, CommandAction command) {
    const auto result = game.submit({state(game).revision, std::move(command)});
    if (!result.accepted()) throw std::runtime_error("Day " + iso_date(state(game).date) + ": " + result.explanation);
}
void refuse(runtime::Runtime& game, CommandAction command) {
    const auto before = game.checkpoint();
    check(!game.submit({state(game).revision, std::move(command)}).accepted(), "Invalid incident action was accepted");
    check(game.checkpoint() == before, "Refused incident action changed work, cash, news or randomness");
}
void acknowledge(runtime::Runtime& game) {
    while (const auto* alert = required_deployment_incident(state(game))) {
        const auto id = alert->id;
        apply(game, AcknowledgeDeploymentIncident{id});
    }
}
void advance(runtime::Runtime& game, OrdinalDay target) {
    while (state(game).date < target) {
        acknowledge(game);
        apply(game, AdvanceDay{});
    }
}
std::string exact(const runtime::CompiledProgram& program, const runtime::Runtime& game) {
    const auto saved = persistence::encode_save(program, game.checkpoint());
    check(persistence::decode_save(program, saved).checkpoint == game.checkpoint(),
          "Incident scheduling, containment, response work or news changed on reload");
    return saved;
}
void forged(const runtime::CompiledProgram& program, Json saved) {
    saved.erase("sha256"); saved["sha256"] = persistence::sha256(saved.dump());
    bool rejected = false;
    try { (void)persistence::decode_save(program, saved.dump()); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected, "Checksum-valid invented deployment incident history survived restore");
}
Json rules(unsigned probability) {
    Json cells = Json::array();
    for (const bool large : {false, true}) for (const auto autonomy : {"draft", "internal", "external"})
        for (const bool mismatch : {false, true}) for (const bool supervised : {false, true})
            for (const bool staged : {false, true})
                cells.push_back({{"large",large},{"autonomy",autonomy},{"mismatch",mismatch},
                    {"supervised",supervised},{"staged",staged},{"probability_bp",probability}});
    return {{"assumptions","Deterministic fixture probabilities, not an empirical estimate."},
        {"exposure","bounded"},{"severity","consequential"},
        {"domain","documents"},{"control_threshold_bp",8000},{"large_daily_tasks",1000},
        {"elevated_from_bp",100},{"high_from_bp",500},{"lookup",std::move(cells)},
        {"compensation_cents",200000},{"response_fee_cents",50000},
        {"response_work_milli_CU",100000},{"response_staff_days",4}};
}
runtime::CompiledProgram compile(Json document, unsigned probability = 10000) {
    auto& scenario = pgp_test::scenario_doc(document);
    scenario["flows"] = Json::array({"deployment.incident_news"});
    scenario["rivals"] = Json::array();
    scenario["management"]["setbacks"] = Json::array();
    // Pick a seed with a real pending interval before April's scheduled event.
    for (std::uint64_t seed = 1; seed < 1000; ++seed) {
        auto draw = RandomStreams{seed}.stream(RandomStream::risk);
        (void)draw.bounded(10000);
        const auto offset = draw.bounded(30);
        if (offset >= 7 && offset <= 20) { scenario["seed"] = seed; break; }
    }
    for (auto& offer : scenario["service_catalog"]["offers"])
        if (offer["id"] == "product.documents") offer["incident_risk"] = rules(probability);
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(document.dump(),registry), registry, runtime::supported_effects());
}
runtime::Runtime prepared(const runtime::CompiledProgram& program) {
    runtime::Runtime game{program, runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    check(game.prepare_opening(0).accepted(), "The incident fixture morning did not prepare");
    apply(game, BuyData{"data.licensed_documents"});
    apply(game, StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game, StartTraining{{"incident.teacher","recipe.document_baseline","",{"data.licensed_documents"},
        "lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    advance(game, OrdinalDay{60});
    apply(game, StartEvaluation{"incident.evidence","incident.teacher","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game, OrdinalDay{63});
    apply(game, DeployModel{"product.documents","incident.teacher","incident.evidence","lease.initial",false});
    apply(game, AcceptBusinessOffer{"offer.enterprise_service"});
    return game;
}
const CapacityProject& serving(const runtime::Runtime& game) {
    return find(state(game).capacity->projects, find(state(game).market->products,"product.documents").capacity_project_id);
}
std::size_t news_count(const runtime::Runtime& game) {
    return static_cast<std::size_t>(std::ranges::count(game.current_state().runtime.presentations,
        std::string{"deployment.incident_news"}, &runtime::Presentation::flow_id));
}
void lifecycle(const Json& document) {
    const auto program = compile(document);
    auto game = prepared(program);
    check(risk(game).samples.empty(), "Deployment sampling invented exposure before actual customer work");
    const auto original_project = serving(game).id;
    advance(game, OrdinalDay{89});
    const auto random_before = state(game).random;
    advance(game, OrdinalDay{90});
    check(risk(game).samples.size() == 1 && risk(game).samples.front().incident &&
          risk(game).samples.front().prior_attempts > 0 && risk(game).samples.front().project_id == original_project,
          "The monthly risk sample omitted actual prior deployment work");
    const auto event_on = risk(game).samples.front().event_on;
    check(event_on >= OrdinalDay{97} && event_on < OrdinalDay{120} && risk(game).incidents.empty() && news_count(game) == 0,
          "The sampled incident was revealed or applied before its dated opening");
    for (std::size_t stream = 0; stream < stream_count; ++stream)
        if (stream != static_cast<std::size_t>(RandomStream::risk))
            check(state(game).random[stream] == random_before[stream], "Deployment risk consumed another owning random stream");
    const auto pending = exact(program,game);
    const auto before_report = game.checkpoint();
    const auto report = deployment_risk_report(state(game), state(game).deployments->records.back());
    check(report.band == "High" && game.checkpoint() == before_report,
          "A risk-band report consumed randomness or failed to show the authored band");
    check(game.prepare_opening(state(game).revision).accepted() && game.checkpoint() == before_report,
          "Reopening a prepared morning rerolled deployment risk");
    runtime::Runtime resumed{program,persistence::decode_save(program,pending).checkpoint};
    advance(game,event_on); advance(resumed,event_on);
    check(game.checkpoint() == resumed.checkpoint(), "A future incident rerolled across save and reload");
    check(risk(game).incidents.size() == 1 && required_deployment_incident(state(game)) && state(game).paused,
          "A due incident did not hold work for review");
    const auto incident_id = risk(game).incidents.front().id;
    check(serving(game).status == ProjectStatus::active && serving(game).disruptions.size() == 1 &&
          !serving(game).disruptions.front().ends_before && serving(game).requested == MilliCUPerDay{40000},
          "Containment released reservations or cancelled the deployment");
    check(news_count(game) == 1, "The actual incident did not produce shared news on its own opening day");
    const auto& occurrence = game.current_state().runtime.activations.back();
    check(occurrence.committed_source && occurrence.committed_source->settled_on == event_on &&
          occurrence.committed_source->subject.id == original_project &&
          occurrence.committed_source->cause == deployment_incident_description(state(game),risk(game).alerts.front()),
          "Incident news lost its actual dated owning cause");
    refuse(game,AdvanceDay{});
    const auto& release=state(game).deployments->records.back();
    refuse(game,release_request(release,*state(game).capacity));
    exact(program,game);
    acknowledge(game);
    const auto incident_random = state(game).random;
    const auto held = core::capacity_report(*state(game).capacity);
    const auto closed_on = state(game).date;
    advance(game,advance_days(closed_on,Days{1}));
    const auto& contract_day = state(game).market->days.back().contracts.front();
    check(contract_day.promised_tasks > 0 && contract_day.delivered_tasks == 0 &&
          contract_day.service_credit.value() > 0 && serving(game).completed_work.value() > 0 &&
          core::capacity_report(*state(game).capacity).reserved == held.reserved,
          "Contained deployment work escaped the hold or signed customers lost their ordinary credits");
    const auto& day_work = *std::ranges::find(state(game).capacity->days.back().projects,original_project,&ProjectDayWork::project_id);
    check(day_work.work == MilliCU{0} && day_work.useful_tasks == 0,
          "Incident containment recorded useful work without serving customers");
    const RespondDeploymentIncident response{incident_id,"lease.initial",MilliCUPerDay{20000},2};
    const auto before_preview = game.checkpoint();
    const auto quote = preview_deployment_response(state(game),response);
    check(quote.cash_required == Cents{250000} && quote.compensation_due == Cents{200000} &&
          quote.fee == Cents{50000} && quote.work == MilliCU{100000} && quote.minimum_days == Days{5} &&
          game.checkpoint() == before_preview, "Incident response quote omitted real cash, work or staff time");
    refuse(game,RespondDeploymentIncident{incident_id,"missing",MilliCUPerDay{20000},2});
    const auto cash = state(game).cash;
    apply(game,response);
    check(state(game).cash == checked_unit_subtract(cash,quote.cash_required) && risk(game).responses.size() == 1,
          "Starting response did not pay its actual compensation and fee once");
    const auto response_project = risk(game).responses.front().project_id;
    const auto& work = find(state(game).capacity->projects,response_project);
    check(work.kind == ProjectKind::incident_response && work.staff == 2 && work.requested == MilliCUPerDay{20000} &&
          work.total_work == quote.work && !risk(game).incidents.front().recovered_on,
          "Paying the response bypassed its finite compute and staff work");
    refuse(game,response);
    advance(game,advance_days(state(game).date,Days{1}));
    const auto response_save = exact(program,game);
    auto retry=game;
    apply(retry,PauseProject{response_project});
    advance(retry,advance_days(state(retry).date,Days{1}));
    check(find(state(retry).capacity->projects,response_project).completed_work==MilliCU{20000},
          "Paused incident work consumed additional compute");
    exact(program,retry);
    apply(retry,CancelProject{response_project});
    const auto retry_quote=preview_deployment_response(state(retry),response);
    check(retry_quote.compensation_due==Cents{0} && retry_quote.cash_required==quote.fee &&
          !risk(retry).incidents.front().recovered_on && project_disrupted(serving(retry),state(retry).date),
          "Cancellation released containment or charged the settled claim again");
    apply(retry,response);
    exact(program,retry);
    runtime::Runtime response_reload{program,persistence::decode_save(program,response_save).checkpoint};
    const auto recovery = advance_days(risk(game).responses.front().started_on,quote.minimum_days);
    advance(game,recovery); advance(response_reload,recovery);
    check(game.checkpoint() == response_reload.checkpoint() && risk(game).incidents.front().recovered_on == recovery &&
          serving(game).disruptions.front().ends_before == recovery &&
          find(state(game).capacity->projects,response_project).completed_work == quote.work && news_count(game) == 2,
          "Paid response did not finish, release containment and report recovery on the next morning");
    check(state(game).random == incident_random && risk(game).samples.size() == 1,
          "Acknowledgement or response rerolled the incident");
    advance(game,advance_days(recovery,Days{1}));
    check(state(game).market->days.back().contracts.front().delivered_tasks > 0,
          "Completed response failed to restore actual signed delivery");
    exact(program,game);
    auto corrupt = Json::parse(pending);
    corrupt["checkpoint"]["world"]["simulation"]["deployments"]["risk"]["samples"][0]["prior_attempts"] = 0;
    forged(program,std::move(corrupt));
    corrupt = Json::parse(response_save);
    for (auto& project : corrupt["checkpoint"]["world"]["simulation"]["capacity"]["projects"])
        if (project["id"] == original_project) project["disruptions"][0]["ends_before"] = event_on.value();
    forged(program,std::move(corrupt));
    corrupt = Json::parse(response_save);
    corrupt["checkpoint"]["world"]["simulation"]["deployments"]["risk"]["incidents"][0]["recovered_on"] = event_on.value();
    forged(program,std::move(corrupt));
}
void quiet(const Json& document) {
    const auto program = compile(document,0);
    auto game = prepared(program);
    advance(game,OrdinalDay{120});
    check(risk(game).samples.size() == 2 && std::ranges::none_of(risk(game).samples,&DeploymentRiskSample::incident) &&
          risk(game).incidents.empty() && risk(game).alerts.empty() && news_count(game) == 0 && serving(game).disruptions.empty(),
          "An explicit quiet outcome created an incident, disruption or news");
    exact(program,game);
}
} // namespace
int main() {
    try {
        std::ifstream source{PGP_DEPLOYMENT_RISK_SOURCE};
        const auto document = Json::parse(source);
        lifecycle(document); quiet(document);
        std::cout << checks << " deployment incident checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
