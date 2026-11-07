#include "../content/slice_fixture.hpp"
#include <pgp/session/slice_run.hpp>
#include <pgp/core/company_finance.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
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
void finish_work(runtime::Runtime& game,std::string_view id,unsigned limit) {
    for(unsigned elapsed=0;elapsed<=limit;++elapsed) {
        const auto& projects=game.current_state().world.simulation.capacity->projects;
        const auto work=std::ranges::find(projects,id,&core::CapacityProject::id);
        if(work!=projects.end() && work->status==core::ProjectStatus::completed) return;
        if(elapsed<limit) apply(game,core::AdvanceDay{});
    }
    throw std::runtime_error("Deployment fixture failed to complete work: "+std::string(id));
}
void roundtrip(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    check(persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint==game.checkpoint(),"Deployment save changed profile history or qualified state");
}
void service_rollout(const runtime::CompiledProgram& program,runtime::Runtime game,const core::LabAllocation& allocation) {
    const auto state=[&]() -> const core::SimulationState& { return game.current_state().world.simulation; };
    apply(game,core::CloneModel{"baseline","candidate"});
    apply(game,core::StartEvaluation{"candidate_test","candidate","evaluation.held_out",allocation});
    days(game,3);
    const core::DeployModel candidate{"product.documents","candidate","candidate_test","lease.initial",false};
    const core::StageServiceRollout stage{candidate,core::BasisPoints{2000},core::Days{2}};
    const auto before=game.checkpoint();
    const auto incumbent=state().market->products.front().capacity_project_id;
    refused(game,core::StageServiceRollout{candidate,core::BasisPoints{5001},core::Days{2}});
    apply(game,stage);
    const auto trial=state().market->trials.front();
    const auto trial_project=[&]() -> const core::CapacityProject& {
        return *std::ranges::find(state().capacity->projects,trial.project_id,&core::CapacityProject::id);
    };
    const auto trial_support=[&]() {
        return std::ranges::find(state().accounts->recurring,"support/"+trial.project_id,&core::RecurringObligation::id);
    };
    check(state().market->capacity_bindings==before.world.simulation.market->capacity_bindings &&
          state().market->contracts==before.world.simulation.market->contracts && trial_project().tasks_per_day==400 &&
          trial_project().staff==1 && trial_project().requested.value()==8000 &&
          trial_support()!=state().accounts->recurring.end() && trial_support()->ends_before==trial.ends_before,
          "Staging replaced the incumbent, changed a contract or omitted the candidate resources");
    refused(game,stage);
    refused(game,candidate);
    refused(game,core::CancelProject{trial.project_id});
    refused(game,core::CancelProject{incumbent});
    refused(game,core::FinishServiceRollout{"product.documents",true});
    auto discarded=game;
    apply(discarded,core::FinishServiceRollout{"product.documents",false});
    check(discarded.current_state().world.simulation.market->capacity_bindings==state().market->capacity_bindings &&
          discarded.current_state().world.simulation.market->trials.front().stopped_on==state().date,
          "Discarding a trial changed production or kept the extra reservation");
    roundtrip(program,discarded);
    auto blocked_market=*state().market;
    auto blocked_accounts=*state().accounts;
    auto blocked_capacity=*state().capacity;
    core::MarketInfluences limits;
    for (const auto& segment:blocked_market.segments)
        limits.push_back({candidate.service_id,segment.id,core::BasisPoints{0},true,false});
    core::close_market_day(blocked_market,blocked_accounts,blocked_capacity,state().date,true,nullptr,nullptr,{},limits);
    check(std::ranges::none_of(blocked_market.days.back().discretionary,[](const auto& row){return row.trial_tasks>0;}) &&
          blocked_market.days.back().contracts.front().delivered_tasks==1000,
          "A candidate-specific eligibility rule blocked the incumbent or let the trial serve");
    days(game,1);
    const auto& market=state().market->days.back();
    const auto& capacity=state().capacity->days.back();
    std::uint64_t trial_tasks=0;
    for (const auto& delivery:market.discretionary) if (delivery.product_id==candidate.service_id) {
        check(delivery.trial_tasks<=delivery.tasks/5 && delivery.charged.value()==
              static_cast<std::int64_t>(delivery.tasks)*state().market->products.front().price_per_task.value(),
              "Staged traffic exceeded its cap or duplicated customer charges");
        trial_tasks+=delivery.trial_tasks;
    }
    const auto staged=std::ranges::find(capacity.projects,trial.project_id,&core::ProjectDayWork::project_id);
    const auto production=std::ranges::find(capacity.projects,incumbent,&core::ProjectDayWork::project_id);
    check(trial_tasks>0 && staged!=capacity.projects.end() && staged->useful_tasks==trial_tasks &&
          staged->protected_tasks==0 && production!=capacity.projects.end() &&
          production->protected_tasks==1000 && market.contracts.front().delivered_tasks==1000,
          "Trial work did not settle on its own compute or displaced signed work");
    roundtrip(program,game);
    auto forged=game.checkpoint();
    forged.world.simulation.market->trials.front().share=core::BasisPoints{1};
    bool rejected=false;
    try { runtime::validate_checkpoint(program,forged); } catch (const std::exception&) { rejected=true; }
    check(rejected,"Tampered traffic share survived checkpoint validation");
    days(game,1);
    const auto at_expiry=game.checkpoint();
    const auto support=trial_support();
    check(trial_project().status==core::ProjectStatus::cancelled && trial_project().stopped_on==state().date &&
          support!=state().accounts->recurring.end() && support->ends_before==state().date &&
          core::pending_rollout(*state().deployments,candidate.service_id),
          "Expiry kept trial commitments or lost the decision");
    apply(game,core::FinishServiceRollout{"product.documents",true});
    check(state().deployments->records.back().model_id=="candidate" && state().deployments->rollouts.front().promoted_release>0 &&
          state().market->contracts==at_expiry.world.simulation.market->contracts &&
          state().market->days==at_expiry.world.simulation.market->days,
          "Promotion changed past work or signed terms");
    apply(game,core::RollbackService{"product.documents"});
    check(state().deployments->records.back().model_id=="baseline" && state().deployments->records.back().restored_release==1 &&
          state().deployments->records.back().project_id!=incumbent && state().market->days==at_expiry.world.simulation.market->days,
          "Rollback failed to restore the prior qualification as a new production binding");
    days(game,1);
    roundtrip(program,game);
}
void service_support(const runtime::CompiledProgram& program,runtime::Runtime game) {
    using namespace core;
    constexpr std::string_view id="product.documents_tables";
    const auto state=[&]() -> const SimulationState& {return game.current_state().world.simulation;};
    const auto product=[&]() -> const MarketProduct& {return *std::ranges::find(state().market->products,id,&MarketProduct::id);};
    const auto work=[&]() -> const CapacityProject& {
        return *std::ranges::find(state().capacity->projects,product().capacity_project_id,&CapacityProject::id);
    };
    const auto edges=[&]() {
        const auto plan=market_opportunity(*state().market,*state().capacity,state().date);
        std::vector<AllocationEdge> rows;
        for (const auto& segment:plan.segments) for (const auto& edge:segment.edges)
            if (edge.product_id==id) rows.push_back(edge);
        return rows;
    };
    const auto started=state().date;
    const auto retires=advance_days(started,Days{36});
    MarketContract signed_terms;
    signed_terms.id="support.customer";
    signed_terms.segment_id=state().market->contracts.front().segment_id;
    signed_terms.product_id=id;
    signed_terms.tasks_per_day=100;
    signed_terms.monthly_fee=Cents{28000};
    signed_terms.accepted_on=started;
    signed_terms.start_by=advance_days(started,Days{1});
    signed_terms.ends_before=retires;
    signed_terms.cause="Existing integration support";
    apply(game,AcceptContract{signed_terms});
    apply(game,StartContract{signed_terms.id});
    const auto before=edges();
    const CommitServiceCompatibility promise{std::string(id),Days{32}};
    const AnnounceServiceRetirement notice{std::string(id),Days{36}};
    const DeployModel compatible{std::string(id),"targeted","targeted_test","lease.initial",false};
    const DeployModel breaks_throughput{std::string(id),"targeted","targeted_test","lease.initial",true};
    refused(game,AnnounceServiceRetirement{std::string(id),Days{35}});
    auto uncommitted=game;
    apply(uncommitted,breaks_throughput); // The same paid review is qualified without the 2,000-task commitment.
    apply(game,promise);
    const auto after=edges();
    check(!before.empty() && before.size()==after.size() && after.front().weight>before.front().weight &&
          after.front().task_limit==before.front().task_limit,"Published support did not help established integrations within the same allocation limits");
    refused(game,promise);
    refused(game,CancelProject{work().id});
    refused(game,breaks_throughput);
    refused(game,StageServiceRollout{breaks_throughput});
    refused(game,RollbackService{std::string(id)});
    const auto prior=work().id;
    apply(game,compatible);
    check(work().id!=prior && state().market->compatibility.front().project_id==prior,"A compatible replacement changed the published baseline");
    apply(game,notice);
    const auto capped=edges();
    check(product().retirement->retires_on==retires && capped.front().task_limit<after.front().task_limit,
          "Published retirement left full new-customer adoption available");
    auto longer=signed_terms;
    longer.id="support.too_long";
    longer.ends_before=advance_days(retires,Days{1});
    refused(game,AcceptContract{longer});
    refused(game,notice);
    roundtrip(program,game);
    const auto actual=forecast_company_finances(state());
    auto without_date=state();
    std::ranges::find(without_date.market->products,id,&MarketProduct::id)->retirement.reset();
    check(actual.cases[1].cash.year.cash<forecast_company_finances(without_date).cases[1].cash.year.cash,
          "Cash scenario still credited discretionary sales beyond the published date");
    auto forged=game.checkpoint();
    forged.world.simulation.market->compatibility.front().project_id="serving.documents";
    bool rejected=false;
    try { runtime::validate_checkpoint(program,forged); } catch (const std::exception&) { rejected=true; }
    check(rejected,"A compatibility commitment could be reassigned to another service in a save");
    days(game,32);
    check(!market_compatibility(*state().market,id,state().date),"The compatibility term failed to expire on its exclusive boundary");
    apply(game,breaks_throughput);
    const auto end_term=std::ranges::find(state().accounts->recurring,"support/"+work().id,&RecurringObligation::id);
    check(end_term!=state().accounts->recurring.end() && end_term->ends_before==retires,"A replacement lost the published support billing end date");
    refused(game,StageServiceRollout{compatible,BasisPoints{2000},Days{5}});
    apply(game,StageServiceRollout{compatible,BasisPoints{2000},Days{4}});
    days(game,4);
    const auto& last=state().market->days.back();
    const auto fulfilled=std::ranges::find(last.contracts,signed_terms.id,&ContractDayResult::contract_id);
    check(state().date==retires && !product().available && work().status==ProjectStatus::cancelled && work().stopped_on==retires &&
          fulfilled!=last.contracts.end() && fulfilled->delivered_tasks==100 &&
          state().deployments->rollouts.back().resolved_on==retires,"Retirement failed to close the trial and preserve final contracted work");
    refused(game,compatible);
    refused(game,RollbackService{std::string(id)});
    days(game,1);
    check(std::ranges::none_of(state().market->days.back().discretionary,[&](const auto& sale){return sale.product_id==id;}),
          "A retired service still billed new work");
    roundtrip(program,game);
}
void run() {
    std::ifstream file{PGP_SLICE_CONTENT_PATH}; auto doc=nlohmann::json::parse(file);
    pgp_test::scenario_doc(doc)["flows"]=nlohmann::json::array();pgp_test::scenario_doc(doc)["rivals"]=nlohmann::json::array();
    pgp_test::scenario_doc(doc)["management"]["setbacks"]=nlohmann::json::array(); // Isolate qualification; setback boundaries have their own integration test.
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    check(game.prepare_opening(0).accepted(),"Morning did not prepare");
    const core::LabAllocation allocation{"lease.initial",core::MilliCUPerDay{20000},1};
    apply(game,core::BuyData{"data.licensed_documents"});
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(game,core::StartTraining{{"baseline","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{60000},3,core::MilliCU{3600000}}});
    refused(game,core::DeployModel{"product.documents","baseline","missing","lease.initial",false});
    refused(game,core::AcceptBusinessOffer{"offer.enterprise_service"});
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
    const auto before=core::accounts_report(*game.current_state().world.simulation.accounts);
    apply(game,core::AcceptBusinessOffer{"offer.enterprise_service"});
    const auto prepaid=core::accounts_report(*game.current_state().world.simulation.accounts);
    check(prepaid.cash==core::checked_unit_add(before.cash,core::Cents{2000000}) &&
          prepaid.customer_advances==core::Cents{2000000} && prepaid.revenue==before.revenue,
          "The bundled enterprise customer did not pay the disclosed refundable advance");
    apply(game,core::DeployModel{"product.documents_tables","baseline","heldout","lease.initial",true});
    const auto& reviewed=game.current_state().world.simulation.capacity->projects.back();
    check(reviewed.tasks_per_day==500 && reviewed.staff==2 && reviewed.monthly_support_cost.value()==2000000 && reviewed.requested.value()==10000,"Review control omitted its throughput, staff, support or compute cost");
    check(game.current_state().world.simulation.lab->models.front()==truth && game.current_state().world.simulation.random==rng,"Deployment changed model truth or sampled hidden evidence");
    refused(game,core::CancelProject{"serving.documents"});
    roundtrip(program,game);days(game,5);
    const auto with_delivery=game.checkpoint();
    service_rollout(program,game,allocation);
    // A new source/model profile can replace a serving project without rewriting
    // earlier deliveries, invoices or contracts.
    apply(game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",false});
    check(game.current_state().world.simulation.market->contracts==with_delivery.world.simulation.market->contracts &&
          game.current_state().world.simulation.market->days==with_delivery.world.simulation.market->days,"Replacement rewrote contract or delivery history");
    roundtrip(program,game);days(game,2);roundtrip(program,game);
    apply(game,core::BuyData{"data.consented_scans"});
    apply(game,core::StartTraining{{"targeted","recipe.document_baseline","",{"data.licensed_documents","data.consented_scans"},"lease.initial",core::MilliCUPerDay{50000},3,core::MilliCU{3600000}}});
    finish_work(game,"lab/train/targeted",100);
    apply(game,core::StartEvaluation{"targeted_test","targeted","evaluation.held_out",allocation});
    finish_work(game,"lab/evaluate/targeted_test",6);
    refused(game,core::DeployModel{"product.documents_tables","targeted","heldout","lease.initial",false});
    auto oversized=game;
    apply(oversized,core::StartTraining{{"busy","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{50000},3,core::MilliCU{3600000}}});
    refused(oversized,core::DeployModel{"product.documents_tables","targeted","targeted_test","lease.initial",false});
    apply(game,core::DeployModel{"product.documents_tables","targeted","targeted_test","lease.initial",false});
    const auto& records=game.current_state().world.simulation.deployments->records;
    check(records.size()==4 && !records.back().human_review,"Targeted model did not replace the review-controlled broader service");
    service_support(program,game);
    days(game,3);roundtrip(program,game);
    auto tampered=game.checkpoint();tampered.world.simulation.market->capacity_bindings.front().project_id=records.back().project_id;
    bool rejected=false;try { runtime::validate_checkpoint(program,tampered); } catch(const std::exception&) { rejected=true; }
    check(rejected,"Forged historical capacity binding survived full validation");
    apply(game,core::CloneModel{"targeted","clone"});
    apply(game,core::StartEvaluation{"clone_test","clone","evaluation.held_out",allocation});
    finish_work(game,"lab/evaluate/clone_test",6);
    apply(game,core::DeployModel{"product.documents_tables","clone","clone_test","lease.initial",false});
    roundtrip(program,game);
    // Even otherwise usable evidence cannot grant commercial rights.
    auto uncleared=doc;
    auto& data=pgp_test::scenario_doc(uncleared)["lab"]["data"];
    const auto offer=std::ranges::find_if(data,[](const auto& row) { return row["id"]=="data.licensed_documents"; });
    check(offer!=data.end(),"The commercial-rights fixture lost its training dataset");
    (*offer)["commercial_use"]=false;
    const auto restricted_program=runtime::compile_collection(content::load_collection(uncleared.dump(),registry),registry);
    auto restricted=qualified;
    auto& catalog=restricted.world.simulation.lab->catalog.data;
    const auto license=std::ranges::find(catalog,"data.licensed_documents",&core::DataOffer::id);
    check(license!=catalog.end(),"The qualified model lost its dataset license");
    license->commercial_use=false;
    runtime::Runtime restricted_game{restricted_program,std::move(restricted)};
    refused(restricted_game,core::DeployModel{"product.documents","baseline","heldout","lease.initial",false});
}
}
int main() { try {run();std::cout<<checks<<" deployment checks passed\n";return 0;} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
