#pragma once
// Shared focused fixture. Technologies, field work and independent protocols use real commands.
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include "../content/slice_fixture.hpp"
#include <pgp/runtime/competition.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;
using namespace core;
using Json = nlohmann::json;
unsigned checks = 0;
constexpr std::string_view frontier_id = "scenario.century_frontier";
constexpr std::string_view plateau_id = "scenario.century_plateau";

void check(bool value, const char* reason) {
    ++checks;
    if (!value) throw std::runtime_error(reason);
}
const SimulationState& state(const runtime::Runtime& game) { return game.current_state().world.simulation; }
const FrontierState& future(const runtime::Runtime& game) { return *state(game).frontier; }
const CapacityProject& work(const runtime::Runtime& game, std::string_view id) {
    const auto& records = state(game).capacity->projects;
    const auto found = std::ranges::find(records,id,&CapacityProject::id);
    if (found==records.end()) throw std::runtime_error("Missing project "+std::string(id));
    return *found;
}
void apply(runtime::Runtime& game, CommandAction action) {
    const auto result = game.submit({state(game).revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(iso_date(state(game).date)+" · "+result.explanation);
}
void refuse(runtime::Runtime& game, CommandAction action, std::string_view reason = {}) {
    const auto before = game.checkpoint();
    const auto outcome = game.submit({state(game).revision,std::move(action)});
    check(!outcome.accepted() && before==game.checkpoint() && outcome.explanation.find(reason)!=std::string::npos,
        "A refused frontier action changed the campaign or gave the wrong reason.");
}
void advance(runtime::Runtime& game, OrdinalDay target) {
    while (state(game).date<target) {
        const auto& s = state(game);
        if (const auto warning=liquidity_report(*s.liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        if (const auto* alert=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{alert->id});
        if (required_management_notice(state(game))) {
            const auto& rows = state(game).management->notices;
            const auto alert = std::ranges::find_if(rows,[](const auto& n){return n.required && !n.acknowledged;});
            apply(game,AcknowledgeManagementNotice{alert->id});
        }
        apply(game,AdvanceDay{});
    }
}
void finish(runtime::Runtime& game, std::string project) {
    while (work(game,project).status==ProjectStatus::active)
        advance(game,advance_days(state(game).date,Days{1}));
    check(work(game,project).status==ProjectStatus::completed,"Required funded work failed to finish.");
}
runtime::Runtime begin(const runtime::CompiledProgram& program,std::string_view id) {
    runtime::Runtime game{program,runtime::begin_scenario(program,id,"US-WA")};
    check(game.prepare_opening(state(game).revision).accepted(),"The authored frontier morning failed to prepare.");
    return game;
}
runtime::Runtime reload(const runtime::Runtime& game) {
    const auto& p = game.program();
    const auto before = game.checkpoint();
    auto loaded = persistence::decode_save(p,persistence::encode_save(p,before));
    check(loaded.checkpoint==before,"The frontier campaign changed across exact reload.");
    return {p,std::move(loaded.checkpoint)};
}
runtime::CompiledProgram compile(Json source) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source.dump(),registry),registry);
}
runtime::CompiledProgram late_fixture(Json source,std::string_view path="path.accelerating",bool rivals=false,bool scored=false) {
    const auto full = compile(source);
    const auto& scenario = std::get<content::ScenarioDefinition>(full.collection.find(frontier_id)->data);
    const auto& catalog = *content::frontier_catalog(full.collection,scenario);
    std::uint64_t cooperative_seed = 0;
    while (opening_frontier(catalog,cooperative_seed).path!=path) ++cooperative_seed;
    // The fixture changes rival launch dates. Keep only the scenarios using
    // those altered definitions so another campaign cannot supply the same ID.
    auto& definitions=source["definitions"];
    definitions.erase(std::remove_if(definitions.begin(),definitions.end(),[](const auto& row) {
        return row["kind"]=="scenario" && row["id"]!=frontier_id && row["id"]!=plateau_id;
    }),definitions.end());
    if(scored) {
        const auto original=std::ranges::find_if(source["definitions"],[](const auto& row){return row["id"]=="society.global";});
        auto society=*original;society["id"]="society.frontier_fixture";
        society["catalog"]["forum"]["first_agenda_day"]=to_ordinal({2027,1,1}).value();
        source["definitions"].push_back(std::move(society));
    }
    // Start near the frontier so the focused test spends its days on the actual
    // authored technologies and protocols rather than replaying thirteen years.
    for (auto& row : source["definitions"]) if (row["id"]==frontier_id || row["id"]==plateau_id) {
        row["starts_on"] = "2027-01-01";
        // This late opening skips the original 2014 founder contact windows.
        row.erase("backgrounds");
        row["flows"] = {"frontier.conditional_era","frontier.agi_notice","frontier.asi_notice"};
        row["management"]["setbacks"] = Json::array();
        for (const auto key : {"civic","resilience"}) row.erase(key);
        if(scored) {
            row["society"]="society.frontier_fixture";
            row["objectives"]={{"primary",row["id"]==frontier_id?"asi":"survival"},{"secondary",Json::array()},
                {"receivables_realizable_bp",8000},{"physical_assets_realizable_bp",6000},{"profit_multiple",3}};
            for(auto& rival:row["rivals"]) rival["start_after_days"]=rivals?0:1500;
            if(rivals && row["id"]==frontier_id) {
                auto peer=row["rivals"].front();
                peer["id"]="company.century.parallel";peer["name"]="Parallel Research Cooperative";
                peer["planning"]["slot"]=2;peer["planning"]["leader"]="Morgan Ives";
                row["rivals"].push_back(std::move(peer));row["competition"]["domestic"]=2;
            }
        } else {
            for (const auto key : {"authority","society","objectives"}) row.erase(key);
            if (rivals) for (auto& rival:row["rivals"]) rival["start_after_days"]=0;
            else {row.erase("rivals");row.erase("competition");}
        }
        auto& offers = row["business_offers"];
        offers.erase(std::remove_if(offers.begin(),offers.end(),[](const auto& offer){return offer["id"]!="offer.independent_lab";}),offers.end());
        if (row.contains("rivals")) {
            auto& roster = row["rivals"];
            roster.erase(std::remove_if(roster.begin(),roster.end(),[](const auto& company) {
                return company["id"]!="company.century.frontier" && company["id"]!="company.century.parallel";
            }),roster.end());
            roster.front()["planning"]["slot"]=1;
            row["competition"]["domestic"]=roster.size();
            row["competition"]["international"]=0;
            row["competition"]["heavyweights"]=0;
        }
        if (row["id"]==frontier_id) row["seed"] = cooperative_seed;
    }
    pgp_test::prune_unused_flows(source);
    return compile(std::move(source));
}
void research(runtime::Runtime& game,std::string_view topic) {
    apply(game,StartResearch{std::string(topic),{"lease.initial",MilliCUPerDay{140000},4}});
    finish(game,state(game).lab->research.back().project_id);
}
void prepare_system(runtime::Runtime& game, bool mesh,bool register_system=true) {
    for (const auto topic : {"research.reproducible_tooling","research.held_out_evaluation","research.sequence_modeling",
            "research.attention_systems","research.multimodal_systems","research.workflow_verification","research.integrated_science"})
        research(game,topic);
    std::string source = "lease.initial";
    if (mesh) {
        research(game,"research.frontier_fabric");
        const BuildOwnedSite build{"science","Frontier fabrication lab","US-WA",160000,CoolingMode::air};
        const auto quote = preview_site_build(state(game),build);
        apply(game,build);
        apply(game,OrderOwnedHardware{"science.fabric","science","rack.frontier_mesh"});
        apply(game,HireSiteOperators{"science.operators","science",2});
        const auto opening = std::max(quote.built_on,quote.grid_on);
        apply(game,ScheduleSiteAppointment{"science.inspection","science",opening,AppointmentMode::remote});
        advance(game,advance_days(opening,Days{15}));
        source = "owned/science.fabric";
        check(std::ranges::any_of(state(game).capacity->sources,[&](const auto& r){return r.id==source && r.effective.value()>0;}),
            "Advanced hardware did not deliver commissioned physical compute.");
    }
    apply(game,BuyData{"data.frontier_corpora"});
    const auto recipe = mesh?"recipe.frontier_mesh":"recipe.frontier_general";
    apply(game,StartTraining{{"model.frontier",recipe,"",{"data.frontier_corpora"},source,
        MilliCUPerDay{mesh?750000:240000},4,MilliCU{mesh?30000000:36000000}}});
    finish(game,state(game).lab->models.back().project_id);
    apply(game,StartEvaluation{"evidence.frontier","model.frontier","evaluation.held_out",{"lease.initial",MilliCUPerDay{30000},1}});
    finish(game,state(game).lab->evaluations.back().project_id);
    apply(game,DeployModel{"product.document_classification","model.frontier","evidence.frontier",source,false,"region.us"});
    if (!register_system) return;
    apply(game,RegisterFrontierSystem{"Fictional integrated research service",{"product.document_classification"}});
    refuse(game,RegisterFrontierSystem{"Same deployed version",{"product.document_classification"}});
}
FrontierTrialQuote commission(runtime::Runtime& game, FrontierTrialKind kind, std::string_view protocol, std::string source="lease.initial") {
    const StartFrontierTrial request{1,kind,std::string(protocol),{std::move(source),MilliCUPerDay{100000},2}};
    const auto before = state(game).cash;
    const auto quote = quote_frontier_trial(state(game),request,Cents{0});
    apply(game,request);
    check(state(game).cash==checked_unit_subtract(before,quote.fee),"The frontier quote did not match the actual paid work.");
    return quote;
}
void panels(runtime::Runtime& game) {
    const auto panel = future(game).catalog.panels;
    const auto first = commission(game,FrontierTrialKind::assessment,panel[0].id);
    refuse(game,StartFrontierTrial{1,FrontierTrialKind::assessment,panel[1].id,{"lease.initial",MilliCUPerDay{100000},2}},"current trial");
    game = reload(game);
    advance(game,OrdinalDay{first.earliest_result.value()-1});
    check(!future(game).trials.front().result && !frontier_qualification(future(game),1).agi_on,"A paid trial certified early.");
    finish(game,future(game).trials.front().project);
    check(future(game).trials.front().result && future(game).trials.front().result->valid &&
        future(game).trials.front().result->field_tasks>0,"The independent panel lacks its real deployed work.");
    refuse(game,StartFrontierTrial{1,FrontierTrialKind::assessment,panel[0].id,{"lease.initial",MilliCUPerDay{100000},2}},"already has");
    (void)commission(game,FrontierTrialKind::assessment,panel[1].id);
    finish(game,future(game).trials.back().project);
    refuse(game,StartFrontierTrial{1,FrontierTrialKind::assessment,panel[2].id,{"lease.initial",MilliCUPerDay{100000},2}},"not yet eligible");
    const auto exact=advance_days(first.earliest_result,frontier_assessment_span);
    advance(game,OrdinalDay{exact.value()-12});
    (void)commission(game,FrontierTrialKind::assessment,panel[2].id);
    finish(game,future(game).trials.back().project);
    check(state(game).date==exact,"The third panel failed the exact 90-day boundary.");
    check(frontier_qualification(future(game),1).agi_on==state(game).date &&
        !frontier_qualification(future(game),1).asi_on,"Three independent windows did not establish AGI separately from replication.");
}
}
