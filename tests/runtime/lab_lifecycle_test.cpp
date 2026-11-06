#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
void require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void apply(runtime::Runtime& game,core::CommandAction command) {
    const auto r=game.submit({game.current_state().world.simulation.revision,std::move(command)});
    if(!r.accepted()) throw std::runtime_error(r.explanation);
}
void refuse(runtime::Runtime& game,core::CommandAction command) {
    const auto before=game.current_state();
    require(!game.submit({before.world.simulation.revision,std::move(command)}).accepted(),"Invalid lifecycle action was accepted");
    require(game.current_state()==before,"A refused lifecycle action changed resources or work");
}
const core::LabModel& model(const runtime::Runtime& game,std::string_view id) {
    const auto& models=game.current_state().world.simulation.lab->models;
    return *std::find_if(models.begin(),models.end(),[&](const auto& r){return r.id==id;});
}
core::TrainingRequest training(std::string id) {
    return {std::move(id),"recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{60000},4,core::MilliCU{3600000}};
}
template<class F> void rejects(F f) {try {f();} catch(const std::invalid_argument&) {return;}throw std::runtime_error("Forged lifecycle evidence was accepted");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        auto collection=content::load_collection(persistence::read_bounded(PGP_LIFECYCLE_SOURCE,content::maximum_document_bytes),registry);
        auto& authored=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.first_playable").data);
        authored.flows.clear();authored.management->minimum_evaluation_interval=core::Days{7};
        auto& evaluation=*std::find_if(authored.lab->evaluations.begin(),authored.lab->evaluations.end(),[](const auto& p){return p.requires_research.empty();});
        evaluation.work=core::MilliCU{40000};const auto plan_id=evaluation.id;
        auto& independent=*std::find_if(authored.lab->evaluations.begin(),authored.lab->evaluations.end(),[](const auto& p){return p.id=="evaluation.held_out";});
        independent.work=core::MilliCU{40000};
        auto& methodology=std::get<content::ResearchDefinition>(collection.definitions.at("research.held_out_evaluation").data);
        methodology.staff_days=core::Days{1};methodology.work=core::MilliCU{20000};
        const auto program=runtime::compile_collection(collection,registry);
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
        require(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Lifecycle morning did not prepare");
        apply(game,core::BuyData{"data.licensed_documents"});apply(game,core::StartTraining{training("model.source")});
        refuse(game,core::CheckpointModel{"model.source","snapshot.zero"});
        apply(game,core::AdvanceDay{});
        const auto cash=game.current_state().world.simulation.cash;
        const auto expenses=core::accounts_report(*game.current_state().world.simulation.accounts).expenses;
        const auto random=game.current_state().world.simulation.random;
        apply(game,core::CheckpointModel{"model.source","snapshot.first"});
        const auto snapshot=model(game,"snapshot.first");
        require(snapshot.consumed_work==core::MilliCU{60000} && snapshot.checkpoint_cursor==1 && snapshot.project_id.empty(),"Live checkpoint did not freeze exactly the settled work");
        require(game.current_state().world.simulation.capacity->projects.front().status==core::ProjectStatus::active && game.current_state().world.simulation.random==random,"Checkpointing must keep training active and consume no RNG");
        apply(game,core::CloneModel{"snapshot.first","snapshot.copy"});
        require(!model(game,"snapshot.copy").checkpoint_cursor,"Cloning an immutable snapshot must use its direct fixed parent");
        apply(game,core::PauseProject{"lab/train/model.source"});
        const auto paused=core::capacity_report(*game.current_state().world.simulation.capacity);
        require(paused.assigned_staff==0 && paused.reserved==core::MilliCUPerDay{0} && game.current_state().world.simulation.cash==cash,"Pause must release allocations without refunding spent cash");
        refuse(game,core::StartEvaluation{"eval.mutable","model.source",plan_id,{"lease.initial",core::MilliCUPerDay{40000},2}});
        apply(game,core::StartTraining{training("model.other")});
        refuse(game,core::ResumeProject{"lab/train/model.source"});
        apply(game,core::AdvanceDay{});
        require(model(game,"model.source").consumed_work==core::MilliCU{60000} && core::accounts_report(*game.current_state().world.simulation.accounts).expenses>expenses,"Paused work must stay fixed while actual recurring bills accrue");
        const auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
        require(loaded.checkpoint==game.current_state(),"Open pause and live checkpoint must reload exactly");
        runtime::Runtime resumed{program,loaded.checkpoint};
        for(auto* live:{&game,&resumed}) {
            refuse(*live,core::ResumeProject{"lab/train/model.source"});
            apply(*live,core::CancelProject{"lab/train/model.other"});
            apply(*live,core::ResumeProject{"lab/train/model.source"});
            apply(*live,core::StartEvaluation{"eval.snapshot","snapshot.first",plan_id,{"lease.initial",core::MilliCUPerDay{40000},2}});
            apply(*live,core::AdvanceDay{});
        }
        require(game.current_state()==resumed.current_state(),"Resumed work and snapshot evaluation diverged after reload");
        require(model(game,"model.source").consumed_work==core::MilliCU{120000} && model(game,"snapshot.first")==snapshot,"Continuing the parent must preserve immutable checkpoint work and capability");
        require(game.current_state().world.simulation.lab->evaluations.back().completed && game.current_state().world.simulation.lab->evaluations.back().model_work==core::MilliCU{60000},"Evaluation must measure the frozen version while its source continues");
        auto forged=game.current_state();auto& fake=forged.world.simulation.lab->models[1];
        fake.inherited_work=core::MilliCU{120000};fake.consumed_work=fake.inherited_work;fake.capability=core::model_capability(forged.world.simulation.lab->catalog,fake);
        rejects([&]{runtime::validate_checkpoint(program,forged);});
        forged=game.current_state();for(auto& p:forged.world.simulation.capacity->projects) if(p.id=="lab/train/model.source") p.pauses.clear();
        rejects([&]{runtime::validate_checkpoint(program,forged);});
        apply(game,core::PauseProject{"lab/train/model.source"});apply(game,core::CancelProject{"lab/train/model.source"});
        require(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Cancelling paused work must close its interval and retain spent work");
        refuse(game,core::ResumeProject{"lab/train/model.source"});
        apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
        const auto research_project=game.current_state().world.simulation.lab->research.back().project_id;
        apply(game,core::PauseProject{research_project});
        refuse(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
        apply(game,core::ResumeProject{research_project});
        require(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Paused research must retain one unfinished run across resume and reload");
        core::DelegationPolicy policy;policy.id="paused_eval";policy.target_id="snapshot.first";policy.plan_id="evaluation.held_out";
        policy.allocation={"lease.initial",core::MilliCUPerDay{40000},2};policy.interval=core::Days{7};policy.maximum_runs=2;policy.budget=core::Cents{100000000};
        apply(game,core::AddDelegationPolicy{policy});apply(game,core::AdvanceDay{});
        require(game.current_state().world.simulation.management->policies.back().runs==1,"The bounded evaluation policy did not start its first real job");
        const auto delegated=game.current_state().world.simulation.lab->evaluations.back().project_id;
        apply(game,core::PauseProject{delegated});
        for(int day=0;day<7;++day) apply(game,core::AdvanceDay{});
        require(game.current_state().world.simulation.management->policies.back().runs==1,"A paused delegated evaluation was incorrectly replaced with another paid job");
        apply(game,core::ResumeProject{delegated});apply(game,core::AdvanceDay{});
        require(game.current_state().world.simulation.management->policies.back().runs==2,"Delegation did not continue after its resumed job actually completed");
        require(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Paused policy history must reconcile to its actual evaluation jobs");
        std::cout<<"Lab pause, resource-safe resume, immutable live checkpoints and measured reload continuation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
