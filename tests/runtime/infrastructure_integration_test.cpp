#include <pgp/runtime/scenario.hpp>
#include <pgp/session/milestone.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
namespace {
using namespace pgp;using namespace core;using Json=nlohmann::json;unsigned checks=0;
void check(bool value,const char* why) {++checks;if (!value) throw std::runtime_error(why);}
const SimulationState& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
void apply(runtime::Runtime& game,CommandAction action) {const auto r=game.submit({state(game).revision,std::move(action)});if (!r.accepted()) throw std::runtime_error(r.explanation);}
void refuse(runtime::Runtime& game,CommandAction action) {const auto before=game.checkpoint();const auto r=game.submit({state(game).revision,std::move(action)});check(!r.accepted() && game.checkpoint()==before,"Refused infrastructure transaction changed the world");}
void advance(runtime::Runtime& game,OrdinalDay target) {while (state(game).date<target) apply(game,AdvanceDay{});}
const CapacitySource& source(const runtime::Runtime& game,std::string_view id) {for (const auto& row:state(game).capacity->sources) if (row.id==id) return row;throw std::runtime_error("Expected owned source is absent");}
void exact(const runtime::CompiledProgram& p,const runtime::Runtime& game) {check(persistence::decode_save(p,persistence::encode_save(p,game.checkpoint())).checkpoint==game.checkpoint(),"Owned construction, metering or calendar failed exact save reload");}
void corrupt(const runtime::CompiledProgram& p,Json value) {
    value.erase("sha256");value["sha256"]=persistence::sha256(value.dump());bool rejected=false;
    try {(void)persistence::decode_save(p,value.dump());}catch(const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"A checksum-valid forged infrastructure state was accepted");
}
void complete_research(runtime::Runtime& game,std::string id) {
    apply(game,StartResearch{id,{"owned/reference",MilliCUPerDay{50000},2}});
    for (unsigned n=0;n<90 && !state(game).lab->research.back().completed;++n) apply(game,AdvanceDay{});
    check(state(game).lab->research.back().completed,"Research did not complete on the paid owned rack");
}
void run(const char* path) {
    std::ifstream input{path};const std::string bytes{std::istreambuf_iterator<char>{input},{}};
    const auto registry=content::foundation_registry();auto collection=content::load_collection(bytes,registry);
    auto& scenario=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.first_playable").data);
    scenario.flows.clear();scenario.rivals.clear();scenario.competition.reset();scenario.management->setbacks.clear();
    const auto program=runtime::compile_collection(collection,registry,runtime::supported_effects());
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-TX")};
    check(game.prepare_opening(state(game).revision).accepted() && state(game).infrastructure->catalog.states.size()==51,"Actual campaign did not load all domestic profiles");
    const auto wa=preview_site_build(state(game),{"washington","Washington workshop","US-WA",20000,CoolingMode::air});
    const auto tx=preview_site_build(state(game),{"texas","Texas workshop","US-TX",20000,CoolingMode::air});
    check(tx.price<wa.price && tx.grid_on>wa.grid_on,"Authored state price versus connection tradeoff is absent");
    apply(game,BuildOwnedSite{"washington","Washington workshop","US-WA",20000,CoolingMode::air});
    apply(game,OrderOwnedHardware{"reference","washington","rack.reference"});
    refuse(game,OrderOwnedHardware{"early_fabric","washington","rack.fabric"});
    apply(game,HireSiteOperators{"crew_washington","washington",1});
    const auto visit=std::max(wa.built_on,wa.grid_on);
    apply(game,ScheduleSiteAppointment{"remote_wa","washington",visit,AppointmentMode::remote});
    const auto milestone=session::plan_milestone(game.checkpoint(),"site");
    check(milestone.deadline==state(game).infrastructure->hardware.front().delivered_on,"Site milestone did not stop on a real delivery/hiring date");
    exact(program,game);
    const auto pending=persistence::encode_save(program,game.checkpoint());runtime::Runtime resumed{program,persistence::decode_save(program,pending).checkpoint};
    advance(game,visit);advance(resumed,visit);check(game.checkpoint()==resumed.checkpoint(),"Pending construction changed outcome on resume");
    check(source(game,"owned/reference").effective==MilliCUPerDay{100000} && state(game).infrastructure->appointments.front().inspection->operators==1,"Remote inspection did not commission actually staffed hardware");
    const auto unchanged=game.checkpoint();const auto stale=game.submit({state(game).revision-1,SetSitePowerLimit{"washington",0}});
    check(!stale.accepted() && game.checkpoint()==unchanged,"Stale owned power review committed at the wrong revision");
    complete_research(game,"research.reproducible_tooling");complete_research(game,"research.sequence_modeling");
    apply(game,BuyData{"data.open_documents"});
    const auto va=preview_site_build(state(game),{"virginia","Virginia fabric site","US-VA",64000,CoolingMode::air});
    apply(game,BuildOwnedSite{"virginia","Virginia fabric site","US-VA",64000,CoolingMode::air});
    apply(game,OrderOwnedHardware{"fabric","virginia","rack.fabric"});apply(game,HireSiteOperators{"crew_virginia","virginia",2});
    const auto va_visit=std::max(va.built_on,va.grid_on);apply(game,ScheduleSiteAppointment{"delegate_va","virginia",va_visit,AppointmentMode::delegated});advance(game,va_visit);
    const auto& f=source(game,"owned/fabric");check(f.memory_mib==98304 && f.interconnect=="fabric.v1" && f.effective==MilliCUPerDay{300000},"Delivered fabric failed its actual memory, interconnect or grid constraint");
    TrainingRequest request;request.model_id="model.wide";request.recipe_id="recipe.wide_tables";request.data_ids={"data.open_documents"};request.source_id="lease.initial";request.daily_compute=MilliCUPerDay{60000};request.staff=2;request.work=MilliCU{180000};
    refuse(game,StartTraining{request});request.source_id="owned/reference";refuse(game,StartTraining{request});request.source_id="owned/fabric";apply(game,StartTraining{request});
    apply(game,AdvanceDay{});check(state(game).infrastructure->sites.back().last_energy_wh>0 && state(game).capacity->days.back().projects.front().work.value()>0,"A compatible fabric model did no metered work");
    apply(game,SetSitePowerLimit{"virginia",0});apply(game,AdvanceDay{});
    check(state(game).infrastructure->sites.back().last_energy_wh==0 && state(game).capacity->days.back().projects.front().work.value()==0,"A powered-off owned site still supplied work or incurred electricity");
    apply(game,SetSitePowerLimit{"virginia",64000});apply(game,NoticeInitialLeaseTermination{});
    advance(game,*state(game).infrastructure->initial_lease_ends_before);
    check(source(game,"lease.initial").effective.value()==0 && source(game,"owned/fabric").effective.value()>0 && state(game).lab->models.back().consumed_work==request.work,"Owned work did not survive planned opening lease termination");
    exact(program,game);
    auto encoded=Json::parse(persistence::encode_save(program,game.checkpoint()));const auto owned=[](Json& j)->Json& {return j["checkpoint"]["world"]["simulation"]["infrastructure"];};
    auto forged=encoded;owned(forged)["catalog"]["map_sha256"]=std::string(64,'b');corrupt(program,forged);
    forged=encoded;owned(forged)["sites"][1]["meters"][0]["energy_wh"]=1;corrupt(program,forged);
    forged=encoded;owned(forged)["hardware"][1]["paid_cents"]=1;corrupt(program,forged);
    forged=encoded;owned(forged)["appointments"][1]["inspection"]["observed_energy_wh"]=1;corrupt(program,forged);
    forged=encoded;owned(forged)["notices"][0]["day"]=1;corrupt(program,forged);
    std::cout<<checks<<" infrastructure integration checks passed; "<<encoded.dump().size()<<" bytes with two metered sites\n";
}
}
int main(int argc,char** argv) {try {run(argc>1?argv[1]:PGP_INFRASTRUCTURE_SOURCE);return 0;}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
