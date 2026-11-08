#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
namespace {
using namespace pgp;
void check(bool okay,const char* why) {if(!okay)throw std::runtime_error(why);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged operations appointment accepted");}
void submit(runtime::Runtime& live,core::CommandAction a) {const auto r=live.submit({live.current_state().world.simulation.revision,std::move(a)});check(r.accepted(),r.explanation.c_str());}
void clear_choices(runtime::Runtime& live) {
    for(int tries=0;tries<8;++tries) {
        const auto& choices=live.current_state().runtime.presentations;const auto it=std::ranges::find_if(choices,[](const auto& p){return !p.resolved && !p.choices.empty();});if(it==choices.end())return;
        const auto choice=std::ranges::find(it->choices,std::string{"decline"},&runtime::PresentedChoice::id);
        if(choice==it->choices.end())throw std::runtime_error("Unexpected required decision in focused operations fixture: "+it->flow_id);
        const auto result=live.choose(live.current_state().world.simulation.revision,it->id,choice->id);check(result.accepted(),result.explanation.c_str());
    }throw std::runtime_error("Operations fixture decision allowance exhausted");
}
void day(runtime::Runtime& live) {clear_choices(live);submit(live,core::AdvanceDay{});clear_choices(live);}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& live) {check(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Manager, management work and physical branch state must reload exactly");}
core::MilliCUPerDay owned_output(const core::SimulationState& s) {core::MilliCUPerDay total{0};for(const auto& source:s.capacity->sources)if(source.kind==core::CapacitySourceKind::owned)total=core::checked_unit_add(total,source.effective);return total;}
}
int main() {
    try {
        const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_DEPARTMENTS_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selection;selection.family="rich";selection.office="leased";
        for(const auto& scene:definition.founding->scenes)selection.choices.emplace(scene.id,scene.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selection;options.seed=19972015;options.identity=runtime::CampaignIdentity{"Operations test","Alex Vale","they/them"};
        runtime::Runtime live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options)};
        const auto ready=live.prepare_opening(live.current_state().world.simulation.revision);check(ready.accepted(),ready.explanation.c_str());
        const auto applicants=live.current_state().world.simulation.management->people->applicants;const auto first=applicants[0].profile.id,second=applicants[1].profile.id;
        submit(live,core::HireEmployee{first});submit(live,core::HireEmployee{second});
        const auto onboarding=live.current_state();auto refused=live.submit({onboarding.world.simulation.revision,core::AssignOperationsManager{first,"lease.initial"}});
        check(!refused.accepted() && live.current_state()==onboarding,"An unready HQ employee cannot reserve manager resources");
        submit(live,core::CreateFacilityBlueprint{{"operations_branch","Operations branch","rack.reference",20000,1,false}});
        const core::BuildFromBlueprint build{"operations_branch","managed_site","Managed branch","US-CA"};const auto quote=core::preview_blueprint_build(live.current_state().world.simulation,build);submit(live,build);
        const auto inspect=core::advance_days(std::max({quote.built_on,quote.grid_on,quote.delivery_on,quote.operators_on}),core::Days{1});
        check(inspect.value()-live.current_state().world.simulation.date.value()<90,"Focused construction fixture must stay within its short boundary");
        submit(live,core::ScheduleSiteAppointment{"operations_inspection","managed_site",inspect,core::AppointmentMode::remote});
        while(live.current_state().world.simulation.date<inspect)day(live);
        check(owned_output(live.current_state().world.simulation)>core::MilliCUPerDay{0},"Only a real commissioned branch can receive coordination benefits");
        check(core::operations_availability(live.current_state().world.simulation,inspect)==core::BasisPoints{8500},"The authored local team must provide its explained unmanaged availability");
        const auto before=live.current_state();const auto assigned=core::capacity_report(*before.world.simulation.capacity).assigned_staff;
        submit(live,core::AssignOperationsManager{first,"lease.initial"});
        const auto& appointed=live.current_state().world.simulation;
        check(core::capacity_report(*appointed.capacity).assigned_staff==assigned+1 && appointed.cash==before.world.simulation.cash && appointed.random==before.world.simulation.random,"Assignment must reserve an actual worker without a new salary, cash mutation or random draw");
        check(core::operations_availability(appointed,appointed.date)==core::BasisPoints{8500},"A title alone cannot grant an immediate availability bonus");exact(program,live);
        const auto reserved=live.current_state();refused=live.submit({appointed.revision,core::CancelProject{"hq.operations/1"}});
        check(!refused.accepted() && live.current_state()==reserved,"Generic cancellation cannot bypass the manager assignment history");
        refused=live.submit({appointed.revision,core::AssignOperationsManager{second,"not_owned"}});
        check(!refused.accepted() && live.current_state()==reserved,"Failed reassignment must preserve the existing manager and work reservation");
        day(live);const auto active=live.current_state();const auto& managed=active.world.simulation;
        check(core::operations_availability(managed,managed.date)>core::BasisPoints{8500},"Completed management work must improve real branch availability");
        check(managed.lab->research==before.world.simulation.lab->research && managed.lab->models==before.world.simulation.lab->models && managed.lab->evaluations==before.world.simulation.lab->evaluations,"Management work cannot fabricate research, models or measured evidence");
        core::Simulation uncoordinated{managed};auto result=uncoordinated.submit({uncoordinated.checkpoint().revision,core::AssignOperationsManager{"",""}});check(result.accepted(),result.explanation.c_str());
        check(owned_output(managed)>owned_output(uncoordinated.checkpoint()),"Coordination must improve actual usable owned capacity under identical hardware and power conditions");
        for(const auto& source:managed.capacity->sources)check(source.effective<=source.nominal,"Management can never exceed physical nameplate capacity");exact(program,live);
        const auto lease=std::ranges::find(managed.capacity->sources,std::string{"lease.initial"},&core::CapacitySource::id)->effective;
        submit(live,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{0}});day(live);
        check(core::operations_availability(live.current_state().world.simulation,live.current_state().world.simulation.date)==core::BasisPoints{8500},"A manager without delivered compute cannot retain a fictitious daily bonus");
        submit(live,core::SetEffectiveCapacity{"lease.initial",lease});day(live);
        submit(live,core::AssignOperationsManager{second,"lease.initial"});check(core::capacity_report(*live.current_state().world.simulation.capacity).assigned_staff==assigned+1,"Reassignment must replace one reservation instead of stacking workers");
        check(core::operations_availability(live.current_state().world.simulation,live.current_state().world.simulation.date)==core::BasisPoints{8500},"A replacement manager must deliver their own work first");exact(program,live);
        auto forged=live.current_state();auto& p=forged.world.simulation.capacity->projects;auto manager=std::ranges::find(p,std::string{"hq.operations/2"},&core::CapacityProject::id);manager->requested=core::MilliCUPerDay{200};refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=live.current_state();forged.world.simulation.infrastructure->workplace->departments->operations[1].person=applicants[2].profile.id;refuses([&]{runtime::validate_checkpoint(program,forged);});
        submit(live,core::AssignOperationsManager{"",""});check(core::capacity_report(*live.current_state().world.simulation.capacity).assigned_staff==assigned,"Releasing the manager must return their worker and compute while retaining payroll and work history");exact(program,live);
        core::Simulation busy{live.current_state().world.simulation};core::CapacityProject work;work.id="test.busy";work.source_id="lease.initial";work.interconnect=std::ranges::find(busy.checkpoint().capacity->sources,work.source_id,&core::CapacitySource::id)->interconnect;work.kind=core::ProjectKind::research;work.starts_on=busy.checkpoint().date;work.requested=core::MilliCUPerDay{1000};work.total_work=core::MilliCU{10000};work.staff=busy.checkpoint().capacity->available_staff;work.cause="Focused existing staff commitment";
        result=busy.submit({busy.checkpoint().revision,core::ReserveProject{work}});check(result.accepted(),result.explanation.c_str());const auto full=busy.checkpoint();result=busy.submit({full.revision,core::AssignOperationsManager{first,"lease.initial"}});check(!result.accepted() && busy.checkpoint()==full,"An already committed team cannot supply a free extra manager");
        core::Simulation appointment{live.current_state().world.simulation};
        result=appointment.submit({appointment.checkpoint().revision,core::AssignOperationsManager{first,"lease.initial"}});check(result.accepted(),result.explanation.c_str());
        auto short_plan=appointment.checkpoint();auto& short_term=short_plan.infrastructure->workplace->departments->operations.back();
        short_term.planned_until=core::advance_days(short_plan.date,core::Days{2});const auto planned_end=short_term.planned_until;
        auto& short_project=*std::ranges::find(short_plan.capacity->projects,short_term.project,&core::CapacityProject::id);short_project.total_work=core::integrate(short_project.requested,core::Days{2});
        core::Simulation completed{short_plan};for(int n=0;n<2;++n){result=completed.submit({completed.checkpoint().revision,core::AdvanceDay{}});check(result.accepted(),result.explanation.c_str());}
        check(!core::operations_manager_on(completed.checkpoint(),completed.checkpoint().date) && completed.checkpoint().infrastructure->workplace->departments->operations.back().ends_before==planned_end,"A completed fixed-term management assignment must release its reservation on the following morning");
        core::Simulation expired{short_plan};result=expired.submit({expired.checkpoint().revision,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{0}}});check(result.accepted(),result.explanation.c_str());
        for(int n=0;n<2;++n){result=expired.submit({expired.checkpoint().revision,core::AdvanceDay{}});check(result.accepted(),result.explanation.c_str());}
        check(!core::operations_manager_on(expired.checkpoint(),expired.checkpoint().date) && expired.checkpoint().infrastructure->workplace->departments->operations.back().ends_before==planned_end,"An undelivered management assignment must stop at its saved horizon without inventing completed work");
        std::cout<<"Real HQ manager reservations, delayed work-backed branch availability, physical limits, reassignment, rollback and exact saves passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
