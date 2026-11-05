#include <pgp/session/slice_run.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
namespace {
using namespace pgp;
int checks=0;
void check(bool condition,const char* why) { ++checks; if (!condition) throw std::runtime_error(why); }
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void refused(runtime::Runtime& game,core::CommandAction action) {
    const auto before=game.checkpoint();
    check(!game.submit({before.world.simulation.revision,std::move(action)}).accepted(),"Invalid deployment was accepted");
    check(game.checkpoint()==before,"Rejected deployment changed evidence, resources, accounts or RNG");
}
void days(runtime::Runtime& game,unsigned count) { while(count--) apply(game,core::AdvanceDay{}); }
void roundtrip(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    check(persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint==game.checkpoint(),"Deployment save changed profile history or qualified state");
}
void run() {
    std::ifstream file{PGP_SLICE_CONTENT_PATH}; auto doc=nlohmann::json::parse(file);
    doc["definitions"][0]["flows"]=nlohmann::json::array();doc["definitions"][0]["rivals"]=nlohmann::json::array();
    doc["definitions"][0]["management"]["setbacks"]=nlohmann::json::array(); // Isolate qualification; setback boundaries have their own integration test.
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    check(game.prepare_opening(0).accepted(),"Morning did not prepare");
    const core::LabAllocation allocation{"lease.initial",core::MilliCUPerDay{20000},1};
    apply(game,core::BuyData{"data.licensed_documents"});
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(game,core::StartTraining{{"baseline","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{60000},3,core::MilliCU{3600000}}});
    refused(game,core::DeployModel{"product.documents","baseline","missing","lease.initial",false});
    days(game,60);
    apply(game,core::StartEvaluation{"heldout","baseline","evaluation.held_out",allocation});
    apply(game,core::StartEvaluation{"headline","baseline","evaluation.headline",allocation});
    days(game,3);
    refused(game,core::DeployModel{"product.documents","baseline","headline","lease.initial",false});
    refused(game,core::DeployModel{"product.documents_tables","baseline","heldout","lease.initial",false});
    refused(game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",true});
    refused(game,core::AddMarketProduct{{"forged","forged",core::Cents{1},true}});
    const auto qualified=game.checkpoint();
    const auto truth=game.current_state().world.simulation.lab->models.front();
    const auto rng=game.current_state().world.simulation.random;
    apply(game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",false});
    apply(game,core::AcceptBusinessOffer{"offer.enterprise_service"});
    apply(game,core::DeployModel{"product.documents_tables","baseline","heldout","lease.initial",true});
    const auto& reviewed=game.current_state().world.simulation.capacity->projects.back();
    check(reviewed.tasks_per_day==500 && reviewed.staff==2 && reviewed.monthly_support_cost.value()==2000000 && reviewed.requested.value()==10000,"Review control omitted its throughput, staff, support or compute cost");
    check(game.current_state().world.simulation.lab->models.front()==truth && game.current_state().world.simulation.random==rng,"Deployment changed model truth or sampled hidden evidence");
    refused(game,core::CancelProject{"serving.documents"});
    roundtrip(program,game);days(game,5);
    const auto with_delivery=game.checkpoint();
    // A new source/model profile can replace a serving project without rewriting
    // earlier deliveries, invoices or contracts.
    apply(game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",false});
    check(game.current_state().world.simulation.market->contracts==with_delivery.world.simulation.market->contracts &&
          game.current_state().world.simulation.market->days==with_delivery.world.simulation.market->days,"Replacement rewrote contract or delivery history");
    roundtrip(program,game);days(game,2);roundtrip(program,game);
    apply(game,core::BuyData{"data.consented_scans"});
    apply(game,core::StartTraining{{"targeted","recipe.document_baseline","",{"data.licensed_documents","data.consented_scans"},"lease.initial",core::MilliCUPerDay{50000},3,core::MilliCU{3600000}}});
    days(game,72);
    apply(game,core::StartEvaluation{"targeted_test","targeted","evaluation.held_out",allocation});days(game,3);
    refused(game,core::DeployModel{"product.documents_tables","targeted","heldout","lease.initial",false});
    auto oversized=game;
    apply(oversized,core::StartTraining{{"busy","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{50000},3,core::MilliCU{3600000}}});
    refused(oversized,core::DeployModel{"product.documents_tables","targeted","targeted_test","lease.initial",false});
    apply(game,core::DeployModel{"product.documents_tables","targeted","targeted_test","lease.initial",false});
    const auto& records=game.current_state().world.simulation.deployments->records;
    check(records.size()==4 && !records.back().human_review,"Targeted model did not replace the review-controlled broader service");
    days(game,3);roundtrip(program,game);
    auto tampered=game.checkpoint();tampered.world.simulation.market->capacity_bindings.front().project_id=records.back().project_id;
    bool rejected=false;try { runtime::validate_checkpoint(program,tampered); } catch(const std::exception&) { rejected=true; }
    check(rejected,"Forged historical capacity binding survived full validation");
    apply(game,core::CloneModel{"targeted","clone"});
    apply(game,core::StartEvaluation{"clone_test","clone","evaluation.held_out",allocation});days(game,3);
    apply(game,core::DeployModel{"product.documents_tables","clone","clone_test","lease.initial",false});
    roundtrip(program,game);
    // Even otherwise usable evidence cannot grant commercial rights.
    auto uncleared=doc;
    uncleared["definitions"][0]["lab"]["data"][1]["commercial_use"]=false;
    const auto restricted_program=runtime::compile_collection(content::load_collection(uncleared.dump(),registry),registry);
    auto restricted=qualified;restricted.world.simulation.lab->catalog.data[1].commercial_use=false;
    runtime::Runtime restricted_game{restricted_program,std::move(restricted)};
    refused(restricted_game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",false});
}
}
int main() { try {run();std::cout<<checks<<" deployment checks passed\n";return 0;} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
