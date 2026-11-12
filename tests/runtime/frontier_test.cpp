#include "frontier_fixture.hpp"

namespace {
void frontier_route(const runtime::CompiledProgram& p,bool mesh=true) {
    auto game = begin(p,frontier_id);
    check(future_path(future(game)).id==(mesh?"path.accelerating":"path.uneven"),"The tested frontier seed did not open its intended conditions.");
    refuse(game,StartResearch{"research.integrated_science",{"lease.initial",MilliCUPerDay{100000},2}},"prerequisites");
    prepare_system(game,mesh);
    panels(game);
    std::string independent_source="owned/science.fabric";
    if (!mesh) {
        apply(game,AcceptBusinessOffer{"offer.independent_lab"});
        independent_source=state(game).capacity->sources.back().id;
    }
    const auto protocols = future(game).catalog.experiments;
    for (std::size_t i=0; i<protocols.size(); ++i) {
        const auto& experiment = protocols[i];
        research(game,experiment.research);
        (void)commission(game,FrontierTrialKind::experiment,experiment.id);
        finish(game,future(game).trials.back().project);
        if (i==2) {
            const auto prior=std::ranges::find(future(game).trials,FrontierTrialKind::replication,&FrontierTrial::kind);
            const auto exact=advance_days(prior->result->observed_on,frontier_replication_span);
            advance(game,OrdinalDay{exact.value()-24});
        }
        refuse(game,StartFrontierTrial{1,FrontierTrialKind::replication,experiment.id,{"lease.initial",MilliCUPerDay{100000},2}},"separate physical");
        (void)commission(game,FrontierTrialKind::replication,experiment.id,independent_source);
        finish(game,future(game).trials.back().project);
        check(future(game).trials.back().result && future(game).trials.back().result->valid,"An independent improvement did not replicate.");
        if (i+1<protocols.size()) check(!frontier_qualification(future(game),1).asi_on,"A partial replication sequence qualified early.");
    }
    const auto result = frontier_qualification(future(game),1);
    const auto first_replication=std::ranges::find(future(game).trials,FrontierTrialKind::replication,&FrontierTrial::kind);
    check(state(game).date==advance_days(first_replication->result->observed_on,frontier_replication_span),
        "The third replicated improvement failed the exact 180-day boundary.");
    check(result.asi_on==state(game).date && result.improvements==3 && result.conservative[10]<9000,
        "Game ASI evidence did not qualify independently of the separate control measure.");
    if(const auto& world=game.checkpoint().world;world.objectives) {
        const auto& objectives=*world.objectives;
        check(objectives.end && objectives.end->cause==runtime::CampaignEndCause::asi_race &&
            objectives.race && objectives.race->winners==std::vector<std::string>{world.company_id} &&
            runtime::objective_result(world,ObjectiveKind::asi)==runtime::ObjectiveResult::won && state(game).paused,
            "A valid early ASI win did not end the primary race and pause.");
        refuse(game,AdvanceDay{},"final accounts");
    }
    game = reload(game);
    auto bytes = Json::parse(persistence::encode_save(p,game.checkpoint()));
    bytes["checkpoint"]["world"]["simulation"]["frontier"]["trials"][0]["result"]["lower_bp"][0]=10000;
    bytes.erase("sha256");
    bytes["sha256"] = persistence::sha256(bytes.dump());
    bool rejected = false;
    try { (void)persistence::decode_save(p,bytes.dump()); }
    catch (const persistence::PersistenceError&) { rejected=true; }
    check(rejected,"A correctly checksummed forged frontier score was accepted.");
}
void rival_route(const runtime::CompiledProgram& p) {
    auto game=begin(p,frontier_id);
    const auto qualified=[&] {
        const auto& rival=game.current_state().world.competition->companies.front().simulation;
        return rival.frontier && earliest_frontier_qualification(*rival.frontier,true).has_value();
    };
    const auto limit=advance_days(state(game).date,Days{1100});
    while (!qualified() && state(game).date<limit) advance(game,advance_days(state(game).date,Days{1}));
    if (!qualified()) {
        const auto& rival=game.current_state().world.competition->companies.front();
        std::cerr<<"Rival frontier cash "<<rival.simulation.cash.value()<<" on "<<iso_date(state(game).date)<<'\n';
        for (const auto& decision:rival.planning->decisions)
            std::cerr<<iso_date(decision.day)<<' '<<decision.action<<' '<<decision.subject<<' '<<decision.explanation<<'\n';
    }
    check(qualified(),"The authored frontier rival failed to complete the same paid ASI route.");
    const auto& rival=game.current_state().world.competition->companies.front().simulation;
    const bool complete_system=std::ranges::any_of(rival.frontier->systems,[&](const auto& system) {
        const auto evidence=frontier_qualification(*rival.frontier,system.id);
        return evidence.asi_on && evidence.assessments==3 && evidence.improvements==3;
    });
    check(!earliest_frontier_qualification(future(game),true) && complete_system,
        "A rival qualification certified the player or bypassed the independent protocols.");
    if(const auto& world=game.checkpoint().world;world.objectives) {
        const auto& race=*world.objectives->race;
        check(race.winners==std::vector<std::string>{"company.century.frontier","company.century.parallel"} &&
            world.objectives->end->day==race.day && runtime::objective_result(world,ObjectiveKind::asi)==runtime::ObjectiveResult::lost,
            "A same-day independent rival qualifier was dropped or used as a player win.");
    }
    (void)reload(game);
}
void bundle_route(const runtime::CompiledProgram& p) {
    auto game=begin(p,frontier_id);
    prepare_system(game,true);
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartTraining{{"model.bundle_member","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    finish(game,state(game).lab->models.back().project_id);
    apply(game,StartEvaluation{"evidence.bundle_member","model.bundle_member","evaluation.held_out",{"lease.initial",MilliCUPerDay{30000},1}});
    finish(game,state(game).lab->evaluations.back().project_id);
    apply(game,DeployModel{"product.documents","model.bundle_member","evidence.bundle_member","lease.initial",false});
    const auto serving=state(game).deployments->records.back().project_id;
    apply(game,RegisterFrontierSystem{"Cooperating deployed services",{"product.document_classification","product.documents"}});
    const auto panel=future(game).catalog.panels;
    StartFrontierTrial combined{2,FrontierTrialKind::assessment,panel[0].id,{"lease.initial",MilliCUPerDay{100000},4}};
    const auto solo=quote_frontier_trial(state(game),{1,combined.kind,combined.protocol,{"lease.initial",MilliCUPerDay{100000},2}},Cents{0});
    const auto quote=quote_frontier_trial(state(game),combined,Cents{0});
    auto understaffed=combined;understaffed.allocation.staff=2;
    refuse(game,understaffed,"staff per deployed member");
    check(quote.fee==checked_unit_add(solo.fee,solo.fee) && quote.work>=checked_unit_add(panel[0].terms.work,panel[0].terms.work),
        "The cooperating system ignored combined physical verification costs.");
    apply(game,combined);
    finish(game,future(game).trials.back().project);
    const auto result=*future(game).trials.back().result;
    check(result.valid && result.field_tasks>0 && result.lower[0]>7500 && result.lower[7]==0,
        "A cooperating system ignored its weaker member's operational reliability.");
    combined.protocol=panel[1].id;
    apply(game,combined);
    apply(game,CancelProject{serving});
    finish(game,future(game).trials.back().project);
    check(future(game).trials.back().result && !future(game).trials.back().result->valid &&
        !frontier_qualification(future(game),2).agi_on,"A stopped cooperating member left valid evidence behind.");
    combined.protocol=panel[2].id;
    refuse(game,combined,"operating service");
    (void)reload(game);
}
void smoke_fixture(const runtime::CompiledProgram& p,const std::filesystem::path& directory) {
    if (!std::filesystem::create_directories(directory)) throw std::runtime_error("Choose a new smoke fixture directory.");
    auto game=begin(p,frontier_id);
    prepare_system(game,true,false);
    persistence::atomic_write(directory/"collection.json",p.canonical_content_bytes,
        [](std::string_view){throw std::runtime_error("The fixture content already exists.");},content::maximum_document_bytes,false);
    const auto paths=persistence::local_paths(directory);
    persistence::save_file(p,game.checkpoint(),paths.saves_directory/"frontier-start.pgpsave");
    std::cout<<"Prepared native Frontier fixture at "<<directory<<'\n';
}
void plateau_route(const runtime::CompiledProgram& p) {
    auto game = begin(p,plateau_id);
    check(!future(game).catalog.asi_route && future_path(future(game)).ceiling==BasisPoints{8800},
        "The plateau misrepresented its reachable capability.");
    refuse(game,OrderOwnedHardware{"blocked","unused","rack.frontier_mesh"},"Energy");
    prepare_system(game,false);
    panels(game);
    refuse(game,StartFrontierTrial{1,FrontierTrialKind::experiment,"experiment.energy",{"lease.initial",MilliCUPerDay{100000},2}},"Independent panels");
    (void)reload(game);
}
void automation(const runtime::CompiledProgram& p) {
    auto game=begin(p,frontier_id);
    const auto after=[&](Days interval){return advance_days(state(game).date,interval);};
    const LabAllocation lab{"lease.initial",MilliCUPerDay{40000},2};
    const FrontierBudget queue{lab,Cents{10'000'000},Cents{2'000'000},Days{1},2};
    const auto opening_cash=state(game).cash;
    apply(game,TakeFinancing{"support.methodology_grant"});
    apply(game,AuthorizeFrontierResearch{{{"research.sequence_modeling","research.reproducible_tooling"},queue,false}});
    advance(game,after(Days{1}));
    check(state(game).lab->research.empty() && future(game).research_programs[0].waiting.find("prerequisites")!=std::string::npos,
        "The queued topic bypassed its current prerequisites.");
    check(state(game).cash<=opening_cash,"A waiting research policy invented cash.");
    apply(game,CancelFrontierPolicy{1,false});
    refuse(game,CancelFrontierPolicy{1,false});
    auto funded=queue;
    funded.limit=Cents{25'000};
    apply(game,AuthorizeFrontierResearch{{{"research.held_out_evaluation","research.reproducible_tooling"},funded,false}});
    advance(game,after(Days{20}));
    check(future(game).research_programs[0].decisions.empty() && future(game).research_programs[1].decisions.size()==2 &&
        state(game).lab->research.size()==2 && std::ranges::all_of(state(game).lab->research,[](const auto& row){return row.completed && row.sponsored_fee==Cents{37'500};}),
        "Cancelling a queue left pending starts, or the authorized queue failed to use the actual Lab.");
    const FrontierBudget recurring{lab,Cents{3'600'000},Cents{2'000'000},Days{14},2};
    apply(game,AuthorizeFrontierResearch{{{"research.refresh_measurement"},recurring,true}});
    advance(game,after(Days{35}));
    check(future(game).research_programs[2].decisions.size()==2 &&
        std::ranges::count_if(state(game).lab->research,[](const auto& row){return row.research_id=="research.refresh_measurement" && row.completed;})==2,
        "Standing research did not deliver its two distinct paid experiments.");
    game=reload(game);

    apply(game,BuyData{"data.open_documents"});
    apply(game,BuyData{"data.consented_scans"});
    apply(game,StartTraining{{"model.baseline","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    finish(game,state(game).lab->models.back().project_id);
    apply(game,StartEvaluation{"evidence.baseline","model.baseline","evaluation.held_out",lab});
    finish(game,state(game).lab->evaluations.back().project_id);
    const DeployModel baseline{"product.document_classification","model.baseline","evidence.baseline","lease.initial",false,"region.us"};
    apply(game,baseline);
    advance(game,after(Days{30}));
    const auto incumbent=state(game).deployments->records.back();
    FrontierRefreshPlan refresh{baseline.service_id,"recipe.document_baseline","evaluation.held_out",{"data.open_documents","data.consented_scans"},
        {{"lease.initial",MilliCUPerDay{300000},3},Cents{2'000'000},Cents{2'000'000},Days{30},1},MilliCU{12000000},BasisPoints{100}};
    auto unlicensed=refresh;
    unlicensed.data.push_back("data.uncleared_archive");
    refuse(game,AuthorizeFrontierRefresh{unlicensed},"actual commercial license");
    apply(game,AuthorizeFrontierRefresh{refresh});
    advance(game,after(Days{1}));
    check(future(game).refresh_programs[0].runs.empty() &&
        future(game).refresh_programs[0].waiting.find("compute left after serving")!=std::string::npos &&
        work(game,incumbent.project_id).status==ProjectStatus::active,
        "Automatic training displaced the incumbent's reserved serving capacity.");
    apply(game,CancelFrontierPolicy{1,true});

    refresh.budget.allocation.daily_compute=MilliCUPerDay{200000};
    apply(game,AuthorizeFrontierRefresh{refresh});
    advance(game,after(Days{1}));
    check(future(game).refresh_programs[1].runs.size()==1 && state(game).deployments->records.size()==1,
        "Automatic training skipped the candidate or replaced its incumbent before evaluation.");
    const auto candidate=state(game).lab->models.back().project_id;
    auto stale=reload(game);
    apply(stale,baseline);
    advance(stale,advance_days(state(stale).date,Days{1}));
    check(future(stale).refresh_programs[1].runs.front().finished_on &&
        work(stale,candidate).status==ProjectStatus::cancelled && !future(stale).refresh_programs[1].runs.front().rollout_deployment,
        "A stale original deployment did not stop its candidate before rollout.");
    apply(game,CancelFrontierPolicy{2,true});
    check(work(game,candidate).status==ProjectStatus::cancelled &&
        work(game,incumbent.project_id).status==ProjectStatus::active &&
        !future(game).refresh_programs[1].runs.front().evaluated_on,
        "Cancellation failed to release candidate compute or preserved an unauthorized expense.");
    game=reload(game);
    apply(game,AuthorizeFrontierRefresh{refresh});
    advance(game,after(Days{75}));
    const auto& run=future(game).refresh_programs[2].runs.front();
    check(run.evaluated_on && run.finished_on && run.rollout_deployment==2 && state(game).deployments->records.size()==2 &&
        state(game).deployments->records.back().model_id==frontier_refresh_id(3,1) &&
        state(game).deployments->records.back().region_id==incumbent.region_id,
        "Fresh licensed data and stronger held-out evidence failed to qualify a private replacement.");
    (void)reload(game);
}
}
int main(int argc,char** argv) {
    try {
        const auto draft = Json::parse(persistence::read_bounded(PGP_FRONTIER_SOURCE,content::maximum_document_bytes));
        const auto bundled = compile(draft);
        auto opening = begin(bundled,frontier_id);
        check(state(opening).date==to_ordinal({2014,1,1}) && state(opening).end_date==to_ordinal({2114,1,1}),
            "The authored century dates differ from the design.");
        refuse(opening,BuyData{"data.frontier_corpora"},"conditional future");
        refuse(opening,OrderOwnedHardware{"early","unused","rack.frontier_mesh"},"conditional future");
        const auto p = late_fixture(draft);
        if (argc==3 && std::string_view{argv[1]}=="--smoke-fixture") {
            smoke_fixture(p,argv[2]);
            return 0;
        }
        if (argc!=1) throw std::runtime_error("Use --smoke-fixture DIRECTORY or no arguments.");
        frontier_route(late_fixture(draft,"path.accelerating",false,true));
        frontier_route(late_fixture(draft,"path.uneven"),false);
        plateau_route(p);
        bundle_route(p);
        automation(p);
        rival_route(late_fixture(draft,"path.accelerating",true,true));
        std::cout<<"Frontier evidence passed ("<<checks<<" checks).\n";
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
