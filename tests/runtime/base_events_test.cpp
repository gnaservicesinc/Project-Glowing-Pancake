#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/session/slice_run.hpp>
#include <pgp/session/campaign.hpp>
#include <pgp/runtime/scenario.hpp>
#if PGP_TEST_STUDIO
#include <pgp/studio/library.hpp>
#include <pgp/studio/draft.hpp>
#include <pgp/studio/flow_probe.hpp>
#endif
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <set>

namespace {
using namespace pgp;
using Json=nlohmann::json;
unsigned checks=0;
constexpr std::array<std::string_view,8> baseline_families{
    "slice.research_milestone","slice.evaluation_finding","slice.pilot_result","slice.liquidity_warning",
    "slice.rival_notice","slice.supplier_offer","slice.market_shift","slice.recovery_opportunity"};
void check(bool value,const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
runtime::CompiledProgram compile(const Json& document) {
    const auto registry=content::foundation_registry();
    return runtime::compile_collection(content::load_collection(document.dump(),registry),registry,runtime::supported_effects());
}
std::uint64_t revision(const runtime::Runtime& engine) { return engine.current_state().world.simulation.revision; }
void accepted(const runtime::RuntimeResult& result) { if (!result.accepted()) throw std::runtime_error(result.explanation); }
void apply(runtime::Runtime& engine,core::CommandAction action) { accepted(engine.submit({revision(engine),std::move(action)})); }
void step(runtime::Runtime& engine,unsigned count=1) {
    while (count--!=0) {
        if (engine.current_state().world.simulation.management) {
            const auto notices=engine.current_state().world.simulation.management->notices;
            for (const auto& n:notices) if (n.required && !n.acknowledged) apply(engine,core::AcknowledgeManagementNotice{n.id});
        }
        apply(engine,core::AdvanceDay{});
    }
}
void refused(runtime::Runtime& engine,core::CommandAction action) {
    const auto before=engine.checkpoint(); check(!engine.submit({revision(engine),std::move(action)}).accepted(),"Invalid event command was accepted");
    check(before==engine.checkpoint(),"Rejected event command changed cash, resources, source history or RNG");
}
Json isolated(Json document,std::string_view id) {
    pgp_test::scenario_doc(document)["flows"]=Json::array({id});
    pgp_test::scenario_doc(document)["rivals"]=Json::array();
    return document;
}
runtime::Runtime begin(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    accepted(engine.prepare_opening(revision(engine))); return engine;
}
std::uint64_t waiting(const runtime::Runtime& engine) {
    for (const auto& row:engine.current_state().runtime.presentations)
        if (row.kind==runtime::PresentationKind::dialogue && !row.resolved) return row.id;
    throw std::runtime_error("Expected a required event choice");
}
std::size_t occurrences(const runtime::Checkpoint& checkpoint,std::string_view id) {
    return static_cast<std::size_t>(std::count_if(checkpoint.runtime.activations.begin(),checkpoint.runtime.activations.end(),[&](const auto& row){return row.definition_id==id;}));
}
void invalid_save(const runtime::CompiledProgram& program,Json doc) {
    doc.erase("sha256"); doc["sha256"]=persistence::sha256(doc.dump());
    bool rejected=false;
    try { (void)persistence::decode_save(program,doc.dump()); } catch (const persistence::PersistenceError&) { rejected=true; }
    check(rejected,"Rehashed inconsistent event/rival save was accepted");
}
std::uint64_t seed_for(std::uint64_t total,std::uint64_t begin_value,std::uint64_t end_value) {
    for (std::uint64_t seed=0;seed<1024;++seed) {
        core::RandomStreams streams{seed}; const auto draw=streams.stream(core::RandomStream::markets).bounded(total);
        if (draw>=begin_value && draw<end_value) return seed;
    }
    throw std::runtime_error("Cannot construct seeded branch fixture");
}
void pack_and_studio(const Json& source,const runtime::CompiledProgram& program) {
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.first_playable")->data);
    check(scenario.flows.size()>=8 && scenario.rivals.size()==3,"Playable pack lacks eight families, two startup routines or the heavyweight");
    for (const auto id:baseline_families)
        check(std::find(scenario.flows.begin(),scenario.flows.end(),id)!=scenario.flows.end(),"Playable pack lost a required baseline family");
    std::size_t opportunities=0;
    for (const auto& id:scenario.flows) {
        const auto& flow=std::get<content::FlowDefinition>(program.collection.find(id)->data);
        check(std::find(flow.tags.begin(),flow.tags.end(),"base_event")!=flow.tags.end(),"Baseline definition is not marked as a base event");
        for (const auto& [node_id,node]:flow.nodes) {
            (void)node_id;
            for (const auto& key:{node.text_key,node.title_key}) if (!key.empty())
                check(program.collection.messages.contains(key),"Playable node has no localizable English message");
            for (const auto& choice:node.choices) check(program.collection.messages.contains(choice.label_key),"Playable choice lacks a localizable label");
        }
        if (id=="slice.evaluation_finding" || id=="slice.supplier_offer" || id=="slice.recovery_opportunity") ++opportunities;
    }
    check(opportunities>=3,"Opportunity/recovery coverage fell below one third");
    for (unsigned mutation=0;mutation<7;++mutation) {
        auto doc=source;
        if (mutation==0) pgp_test::named_row(pgp_test::scenario_doc(doc)["rivals"],"company.northstar")["warning_lead_days"]=89;
        if (mutation==1) pgp_test::named_row(pgp_test::scenario_doc(doc)["business_offers"],"offer.supplier_small")["capacity_milli_CU_per_day"]=0;
        if (mutation==2) pgp_test::named_row(pgp_test::scenario_doc(doc)["rivals"],"company.cedar")["recipe_id"]="recipe.missing";
        if (mutation==3) pgp_test::named_row(pgp_test::scenario_doc(doc)["business_offers"],"offer.supplier_small")["setup_cost_cents"]=1.0;
        if (mutation==4) pgp_test::scenario_doc(doc)["service_catalog"]["segments"][0]["daily_tasks"]=1;
        for (auto& row:doc["definitions"]) {
            if (mutation==5 && row["id"]=="slice.supplier_offer") row["nodes"]["accept"]["effects"][0]["offer_id"]="offer.missing";
            if (mutation==6 && row["id"]=="slice.market_shift") row["nodes"]["increase"]["effects"][0]["segment_id"]="segment.missing";
        }
        bool rejected=false; try { (void)compile(doc); } catch (const std::exception&) { rejected=true; }
        check(rejected,"Invalid authored warning, opportunity or service terms were accepted");
    }
#if PGP_TEST_STUDIO
    const auto library=studio::inspect_checkout(PGP_CHECKOUT_PATH);
    const auto file=std::find_if(library.documents.begin(),library.documents.end(),[](const auto& row){return row.relative_path=="data/runtime/first-playable.json";});
    check(file!=library.documents.end() && file->validation==studio::Validation::validated,"Content Studio cannot browse the playable pack");
    studio::DraftDocument document{PGP_CHECKOUT_PATH,"data/runtime/first-playable.json"};
    check(document.program().canonical_content_bytes==program.canonical_content_bytes,"Studio and game disagree about canonical playable content");
    for (const auto& id:scenario.flows) {
        studio::FlowProbe probe{program,studio::ProbeFixture{"scenario.first_playable","US-CA",id,42}};
        const auto before=probe.state(); (void)probe.inspect(id,runtime::FlowPredicate::trigger);
        check(before==probe.state(),"Studio inspection changed a real scenario or RNG");
    }
#endif
}
void supplier_paths(const Json& source) {
    auto doc=isolated(source,"slice.supplier_offer"); pgp_test::scenario_doc(doc)["seed"]=seed_for(2,1,2);
    const auto program=compile(doc); auto engine=begin(program); step(engine,13);
    check(engine.current_state().runtime.presentations.empty(),"Supplier offer fired before its review boundary");
    step(engine); const auto pending=engine.checkpoint(); const auto choice=waiting(engine);
    check(pending.world.simulation.capacity->sources.size()==1 && pending.world.simulation.business_offers->accepted.empty(),
          "Opening an optional supplier offer created capacity or a commitment");
    refused(engine,core::AdvanceDay{});
    runtime::Runtime declined{program,persistence::decode_save(program,persistence::encode_save(program,pending)).checkpoint};
    accepted(declined.choose(revision(declined),choice,"decline"));
    check(declined.current_state().world.simulation.cash==pending.world.simulation.cash && declined.current_state().world.simulation.capacity->sources.size()==1,
          "Declining the supplier offer spent money or granted capacity");
    accepted(engine.choose(revision(engine),choice,"accept"));
    const auto& signed_state=engine.current_state().world.simulation;
    check(signed_state.cash.value()==pending.world.simulation.cash.value()-100000 && signed_state.capacity->sources.size()==2 &&
        signed_state.business_offers->accepted.size()==1,"Accepted supplier terms did not create one paid source and obligation");
    check(signed_state.capacity->days.back().sources.size()==1,"New supplier source rewrote old daily capacity records");
    check(runtime::read_fact(engine.current_state().world,*program.registry.fact("company.monthly_compute_bill_cents"),
        {content::Scope::company,"company.player"})==content::Scalar{std::int64_t{2200000}},"Supplier lease was omitted from the real committed compute-bill fact");
#if PGP_TEST_STUDIO
    studio::FlowProbe probe{program,pending}; const auto observed=probe.choose(choice,"accept");
    check(observed.trace.accepted() && observed.effects.size()==1 && observed.postings.size()==1 && probe.state()==engine.checkpoint(),
          "Studio impact inspection does not show the actual supplier expense and source");
    check(pending.world.simulation.business_offers->accepted.empty(),"Copied Studio inspection mutated its source fixture");
#endif
    const auto exact=persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint;
    check(exact==engine.checkpoint(),"Accepted supplier terms did not persist exactly");
    runtime::Runtime resumed{program,exact}; step(engine); step(resumed);
    check(engine.checkpoint()==resumed.checkpoint() && engine.current_state().world.simulation.capacity->days.back().sources.size()==2,
          "New lease availability has a wrong first-day source history or reload result");
    step(engine,179);
    const auto& expired=engine.current_state().world.simulation;
    const auto supply=std::find_if(expired.capacity->sources.begin(),expired.capacity->sources.end(),[](const auto& row){return row.id=="lease/offer.supplier_small";});
    check(expired.date.value()==194 && supply!=expired.capacity->sources.end() && supply->effective.value()==0,
          "Fixed-term supply did not end at its exclusive expiry");
    check(runtime::read_fact(engine.current_state().world,*program.registry.fact("company.monthly_compute_bill_cents"),
        {content::Scope::company,"company.player"})==content::Scalar{std::int64_t{1800000}},"Expired supplier bill remained in the current commitment fact");
    check(persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint==engine.checkpoint(),"Expired supplier terms lost exact history");
    auto corrupt=Json::parse(persistence::encode_save(program,engine.checkpoint()));
    corrupt["checkpoint"]["world"]["simulation"]["business_offers"]["accepted"].push_back(corrupt["checkpoint"]["world"]["simulation"]["business_offers"]["accepted"][0]);
    invalid_save(program,std::move(corrupt));
    // A valid changed-resource fixture proves acceptance revalidates while a
    // saved offer is pending; refusal still permits a clean decline.
    auto unaffordable=pending; core::Simulation changed{unaffordable.world.simulation};
    const auto reserved=changed.submit({changed.checkpoint().revision,core::ReserveFunds{"Protected funds",changed.checkpoint().cash}});
    check(reserved.accepted(),"Cannot construct the protected-cash boundary fixture"); unaffordable.world.simulation=changed.checkpoint();
    runtime::Runtime constrained{program,unaffordable}; const auto before=constrained.checkpoint();
    check(!constrained.choose(revision(constrained),choice,"accept").accepted() && constrained.checkpoint()==before,
          "Unaffordable supplier acceptance partially committed its expense/source");
    accepted(constrained.choose(revision(constrained),choice,"decline"));
    auto quiet_doc=doc; pgp_test::scenario_doc(quiet_doc)["seed"]=seed_for(2,0,1); auto quiet=begin(compile(quiet_doc)); step(quiet,14);
    check(quiet.current_state().runtime.presentations.empty() && quiet.current_state().world.simulation.business_offers->accepted.empty(),"Quiet supplier outcome was not quiet");
    auto poor_doc=doc; pgp_test::scenario_doc(poor_doc)["cash_cents"]=50000; auto poor=begin(compile(poor_doc)); step(poor,14);
    check(poor.current_state().runtime.activations.empty(),"An unaffordable supplier offer was presented");
}
void demand_paths(const Json& source) {
    for (const bool up:{false,true}) {
        auto doc=isolated(source,"slice.market_shift"); pgp_test::scenario_doc(doc)["seed"]=up?seed_for(4,2,3):seed_for(4,3,4);
        const auto program=compile(doc); auto engine=begin(program); step(engine,45);
        const auto snapshot=engine.checkpoint(); const auto& market=*snapshot.world.simulation.market;
        const auto enterprise=std::find_if(market.segments.begin(),market.segments.end(),[](const auto& row){return row.id=="segment.enterprise";});
        check(enterprise!=market.segments.end(),"The demand incident lost its enterprise customers");
        std::uint64_t other_demand=0;
        for (const auto& row:market.segments) if (row.id!=enterprise->id) other_demand+=row.daily_tasks;
        const auto settled_enterprise_demand=[&](const core::MarketState& current) {
            const auto& day=current.days.back();
            if (day.total_demand) return *day.total_demand-other_demand;
            const auto stored=std::find_if(day.segments.begin(),day.segments.end(),[&](const auto& row){return row.segment_id==enterprise->id;});
            if (stored==day.segments.end()) throw std::runtime_error("The settled day omitted its enterprise demand");
            return stored->demand;
        };
        const auto expected=up?6000U:4000U;
        check(market.demand_changes.size()==1 && settled_enterprise_demand(market)==5000 &&
            core::effective_market_demand(market,*enterprise,core::OrdinalDay{45})==expected,
            "Demand incident did not change actual future demand or rewrote settled demand");
        runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,snapshot)).checkpoint};
        for (unsigned i=0;i<30;++i) { step(engine); step(resumed); check(engine.checkpoint()==resumed.checkpoint(),"Saved demand outcome rerolled or expired differently"); }
        check(settled_enterprise_demand(*engine.current_state().world.simulation.market)==expected &&
            core::effective_market_demand(*engine.current_state().world.simulation.market,*enterprise,core::OrdinalDay{75})==5000,
              "Demand modifier did not expire once at its exclusive date");
        step(engine); check(settled_enterprise_demand(*engine.current_state().world.simulation.market)==5000,"Expired demand still changed customer work");
        const auto before=engine.checkpoint(); accepted(engine.prepare_opening(revision(engine)));
        check(engine.checkpoint()==before,"Reopening prepared event news consumed a draw or repeated a consequence");
        auto malformed=Json::parse(persistence::encode_save(program,engine.checkpoint()));
        malformed["checkpoint"]["world"]["simulation"]["market"]["demand_changes"][0]["expires_before"]=76;
        invalid_save(program,std::move(malformed));
        refused(engine,core::AddMarketDemandChange{{"rewrite","segment.enterprise",core::OrdinalDay{0},core::OrdinalDay{90},core::BasisPointDelta{1000}}});
    }
    auto quiet_doc=isolated(source,"slice.market_shift"); pgp_test::scenario_doc(quiet_doc)["seed"]=seed_for(4,0,2);
    auto quiet=begin(compile(quiet_doc)); step(quiet,45);
    check(quiet.current_state().world.simulation.market->demand_changes.empty() && quiet.current_state().runtime.presentations.empty(),"No-event world condition changed demand or published news");
}
void research_and_finance(const Json& source) {
    auto research=begin(compile(isolated(source,"slice.research_milestone")));
    for (const auto id:{"research.efficient_image_features","research.data_deduplication","research.held_out_evaluation","research.reproducible_tooling"})
        apply(research,core::StartResearch{id,{"lease.initial",core::MilliCUPerDay{10000},1}});
    step(research,6);
    check(occurrences(research.checkpoint(),"slice.research_milestone")==4,"Same-day committed research sources were lost or coalesced into free repeated research");
    check(research.current_state().world.simulation.lab->research.size()==4,"Research news granted duplicate knowledge");
    const auto research_saved=research.checkpoint(); accepted(research.prepare_opening(revision(research)));
    check(research.checkpoint()==research_saved,"Repeating a research bulletin granted or dispatched work twice");
    auto doc=isolated(source,"slice.liquidity_warning"); pgp_test::scenario_doc(doc)["cash_cents"]=100000;
    const auto program=compile(doc); auto finance=begin(program); step(finance,31);
    check(occurrences(finance.checkpoint(),"slice.liquidity_warning")==1,"Real liquidity warning did not start its finance alert");
    const auto deadline=core::liquidity_report(*finance.current_state().world.simulation.liquidity).deadline;
    const auto pending=core::liquidity_report(*finance.current_state().world.simulation.liquidity).pending_warning;
    check(pending.has_value(),"Low-cash fixture did not produce an actual required warning");
    apply(finance,core::AcknowledgeLiquidityWarning{*pending}); step(finance,2);
    check(occurrences(finance.checkpoint(),"slice.liquidity_warning")==1 &&
        core::liquidity_report(*finance.current_state().world.simulation.liquidity).deadline==deadline,
        "Unchanged workout repeated its bulletin or reset its response window");
    apply(finance,core::InvestEquity{"rescue",core::Cents{200000000},core::BasisPoints{1000},"finance.rescue"}); step(finance);
    check(finance.current_state().world.simulation.liquidity->status==core::LiquidityStatus::operating,"Real financing did not pay and resolve warned liabilities");
    check(std::any_of(finance.current_state().runtime.presentations.begin(),finance.current_state().runtime.presentations.end(),[](const auto& row) {
        return row.text_key=="slice.finance.recovered";
    }),"Natural workout recovery did not run the continuous exit");
    const auto& state=finance.current_state().world.simulation;
    apply(finance,core::IssueInvoice{{"second.setback",core::InvoiceKind::payable,state.date,core::Cents{state.cash.value()+1},core::Cents{0},"Second actual unpaid liability"}}); step(finance);
    check(occurrences(finance.checkpoint(),"slice.liquidity_warning")==2,"A new genuine setback could not re-enter the finance alert");
    check(persistence::decode_save(program,persistence::encode_save(program,finance.checkpoint())).checkpoint==finance.checkpoint(),"Re-entered finance alert lost exact warning/deadline history");
}
void recovery_and_failures(const Json& source) {
    const auto program=compile(source);
    const auto partial=session::run_slice_baseline(program,core::OrdinalDay{64});
    check(partial.stop==session::SliceStop::requested_boundary && occurrences(partial.checkpoint,"slice.evaluation_finding")==1,
          "Baseline failed to discover its real held-out subgroup limitation");
    runtime::Runtime engine{program,partial.checkpoint}; const auto choice=waiting(engine);
    check(engine.current_state().world.simulation.business_offers->accepted.size()>=1,"Opening enterprise contract was not accepted through real terms");
    const auto before=engine.checkpoint(); accepted(engine.choose(revision(engine),choice,"accept"));
    check(engine.current_state().world.simulation.cash==before.world.simulation.cash &&
        std::any_of(engine.current_state().world.simulation.market->contracts.begin(),engine.current_state().world.simulation.market->contracts.end(),[](const auto& row) {
            return row.id=="pilot/offer.recovery_pilot" && row.status==core::ContractStatus::active && row.tasks_per_day==500;
        }),"Recovery offer created free money or failed to create its real smaller pilot");
    apply(engine,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{0}}); step(engine);
    const auto& day=engine.current_state().world.simulation.market->days.back();
    check(day.contracts.size()==2 && day.contracts.front().delivered_tasks==0 && day.contracts.front().service_credit==day.contracts.front().gross_fee,
          "Pilot failure ignored real unavailable capacity or billed undelivered flat service");
    check(occurrences(engine.checkpoint(),"slice.pilot_result")>=2,"Actual pilot start/failure did not publish customer results");
    const auto saved=persistence::encode_save(program,engine.checkpoint());
    runtime::Runtime resumed{program,persistence::decode_save(program,saved).checkpoint};
    apply(engine,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{100000}});
    apply(resumed,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{100000}});
    step(engine); step(resumed); check(engine.checkpoint()==resumed.checkpoint(),"Pilot failure source result changed when later delivery recovered");
    core::MarketContract missed;
    missed.id="fixture.missed"; missed.segment_id="segment.enterprise"; missed.product_id="product.documents";
    missed.tasks_per_day=100; missed.monthly_fee=core::Cents{10000}; missed.missed_start_penalty=core::Cents{50000};
    missed.accepted_on=engine.current_state().world.simulation.date; missed.start_by=missed.accepted_on;
    missed.ends_before=core::advance_days(missed.accepted_on,core::Days{10}); missed.cause="Actual missed pilot deadline";
    apply(engine,core::AcceptContract{missed}); step(engine);
    check(std::any_of(engine.current_state().runtime.activations.begin(),engine.current_state().runtime.activations.end(),[](const auto& row) {
        return row.committed_source && row.committed_source->subject.id=="fixture.missed" && row.committed_source->result==content::Scalar{std::int64_t{0}};
    }),"A missed pilot start did not publish its actual zero-delivery consequence");
    check(persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint==engine.checkpoint(),"Missed pilot outcome did not persist");
    const auto digest=session::group_weekly_news(engine.checkpoint());
    check(!digest.empty() && std::none_of(digest.begin(),digest.end(),[](const auto& group) {
        return std::any_of(group.second.begin(),group.second.end(),[](const auto* row){return row->kind==runtime::PresentationKind::dialogue;});
    }),"Weekly digest lost news or absorbed a required decision");
}
void full_baseline_and_rivals(const Json& source,const runtime::CompiledProgram& program) {
    const auto first=session::run_slice_baseline(program,core::OrdinalDay{154});
    check(first.stop==session::SliceStop::requested_boundary,"Baseline did not reach the heavyweight mobilization boundary");
    const auto& companies=first.checkpoint.world.competition->companies;
    const auto heavy=std::find_if(companies.begin(),companies.end(),[](const auto& row){return row.definition_id=="company.northstar";});
    check(heavy!=companies.end() && heavy->warned_on==core::OrdinalDay{64} && heavy->attempted && !heavy->simulation.lab->models.empty() &&
        heavy->simulation.lab->models.front().created_on==core::OrdinalDay{154},"Heavyweight acted before its visible customer-delivery trigger and 90-day warning");
    check(std::count_if(companies.begin(),companies.end(),[](const auto& row){return row.published_on.has_value();})==2,
          "Two startup routines did not produce actual financed and completed training");
    for (const auto& rival:companies) {
        check(core::accounts_report(*rival.simulation.accounts).expenses.value()>1200000 && rival.simulation.capacity->projects.size()==1,
              "Rival created unbudgeted training or omitted real staff, data and lease expenses");
    }
    auto saved=Json::parse(persistence::encode_save(program,first.checkpoint));
    for (unsigned mutation=0;mutation<4;++mutation) {
        auto corrupt=saved;
        if (mutation==0) corrupt["checkpoint"]["world"]["competition"]["companies"][2]["warned_on"]=149;
        if (mutation==1) corrupt["checkpoint"]["world"]["competition"]["companies"][0]["simulation"]["cash_cents"]=999999999;
        if (mutation==2) { corrupt["checkpoint"]["world"].erase("competition"); corrupt["checkpoint"]["world"].erase("competition_mode"); }
        if (mutation==3) corrupt["checkpoint"]["world"]["competition"]["notices"][0]["response_due"]=1;
        invalid_save(program,std::move(corrupt));
    }
    const auto continued=session::run_slice_baseline(program,{},persistence::decode_save(program,saved.dump()).checkpoint);
    const auto uninterrupted=session::run_slice_baseline(program);
    check(continued.stop==session::SliceStop::finished && uninterrupted.stop==session::SliceStop::finished &&
        continued.checkpoint==uninterrupted.checkpoint,"Complete baseline-only three-year play differs after saved mobilization");
    const auto& final=continued.checkpoint;
    check(final.world.simulation.date==core::parse_iso_date("2017-01-01") && final.world.simulation.cash.value()>0 &&
        core::accounts_report(*final.world.simulation.accounts).debt.value()==0 &&
        core::accounts_report(*final.world.simulation.accounts).equity.value()==120000000,"Specialist baseline failed to finish solvent without financing");
    check(core::market_report(*final.world.simulation.market).delivered_tasks>1'000'000 && final.world.competition->notices.size()==4,
          "Complete baseline lacks useful customer work or actual bounded competitive outcomes");
    for (const auto id:baseline_families) if (id!="slice.liquidity_warning")
        check(occurrences(final,id)>0,"A required baseline family never fired in the seeded full run");
    check(occurrences(final,"operations.site_outcome")==0,"A campaign without owned sites published a site incident");
    const auto final_bytes=persistence::encode_save(program,final);
    check(persistence::decode_save(program,final_bytes).checkpoint==final,"Final baseline history, expiry and competitive evidence did not persist exactly");
    // An additional optional story root must not carry missing baseline mechanics.
    auto story=source;
    auto extra=pgp_test::definition(story,"slice.research_milestone"); extra["id"]="story.optional"; extra["on_committed"]="lab.research_completed"; extra["tags"]=Json::array({"story"});
    story["definitions"].push_back(extra); pgp_test::scenario_doc(story)["flows"].push_back("story.optional");
    // Duplicate baseline subscriptions are forbidden even for story overlays;
    // make this optional root a once-only presentation with its own trigger.
    story["definitions"].back().erase("on_committed"); story["definitions"].back()["lifecycle"]={{"mode","once"},{"max_occurrences_per_scope",1}};
    const auto no_story=session::run_slice_baseline(compile(story),core::OrdinalDay{154});
    check(no_story.checkpoint==first.checkpoint,"Disabling optional human story roots changed the playable baseline or consumed RNG");
    std::cout << "Three-year baseline cash " << final.world.simulation.cash.value() << "; useful tasks "
              << core::market_report(*final.world.simulation.market).delivered_tasks << "; source hash " << persistence::save_identity(program).content_hash << '\n';
}
void rival_nonfiring_and_budget(const Json& source) {
    auto doc=source; pgp_test::named_row(pgp_test::scenario_doc(doc)["rivals"],"company.cedar")["cash_cents"]=1200000;
    const auto program=compile(doc);
    auto initial=runtime::begin_scenario(program,"scenario.first_playable","US-CA"); initial.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime engine{program,std::move(initial)}; accepted(engine.prepare_opening(revision(engine))); step(engine,9);
    check(std::none_of(engine.current_state().world.competition->companies.begin(),engine.current_state().world.competition->companies.end(),[](const auto& row){return row.attempted;}),
          "A rival routine started before its authored action date");
    step(engine);
    const auto& poor=engine.current_state().world.competition->companies.front();
    check(poor.attempted && poor.simulation.lab->models.empty() && poor.simulation.lab->licenses.empty(),
          "An unaffordable rival routine created free data or training");
    check(std::any_of(engine.current_state().world.competition->notices.begin(),engine.current_state().world.competition->notices.end(),[](const auto& row){return row.kind=="budget_declined";}),
          "Bounded rival budget refusal lost its visible causal result");
    apply(engine,core::BuyData{"data.licensed_documents"});
    apply(engine,core::StartTraining{{"private_model","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{60000},4,core::MilliCU{3600000}}});
    step(engine,90);
    const auto& rivals=engine.current_state().world.competition->companies;
    const auto heavy=std::find_if(rivals.begin(),rivals.end(),[](const auto& row){return row.definition_id=="company.northstar";});
    check(heavy!=rivals.end() && !heavy->warned_on && !heavy->attempted && heavy->simulation.lab->models.empty(),
          "Heavyweight mobilized without the declared visible customer-delivery threshold");
    check(engine.current_state().world.simulation.lab->models.front().consumed_work.value()==3600000 && engine.current_state().world.simulation.market->products.empty(),
          "The private completed-model fixture accidentally exposed a customer deployment");
    check(persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint==engine.checkpoint(),"Budget-declined/closed rival failed exact save validation");
}
}
int main() {
    try {
        std::ifstream input(PGP_SLICE_CONTENT_PATH); const auto source=Json::parse(input); const auto program=compile(source);
        pack_and_studio(source,program); supplier_paths(source); demand_paths(source); research_and_finance(source);
        recovery_and_failures(source); full_baseline_and_rivals(source,program); rival_nonfiring_and_budget(source);
        std::cout << checks << " base-event, opportunity, rival, Studio and three-year checks passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
