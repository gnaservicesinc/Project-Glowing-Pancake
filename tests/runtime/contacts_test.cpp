#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <iostream>
#include <algorithm>
namespace {
using namespace pgp;
void check(bool yes,const char* why) {if(!yes)throw std::runtime_error(why);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged contact or office history accepted");}
runtime::Runtime start(const runtime::CompiledProgram& program,bool connected) {
    const auto& d=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection selection;selection.family=connected?"rich":"middle";selection.advantages={"genius"};selection.office="home";
    for(const auto& scene:d.founding->scenes) selection.choices.emplace(scene.id,scene.choices.front().id);
    runtime::ScenarioOptions options;options.founding=selection;options.identity=runtime::CampaignIdentity{"Contacts test","Alex Vale","they/them"};options.seed=19972015;
    runtime::Runtime live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options)};
    const auto ready=live.prepare_opening(live.current_state().world.simulation.revision);check(ready.accepted(),ready.explanation.c_str());return live;
}
void submit(runtime::Runtime& live,core::CommandAction a) {const auto result=live.submit({live.current_state().world.simulation.revision,std::move(a)});check(result.accepted(),result.explanation.c_str());}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& live) {check(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Office obligations, contact history and proof must reload exactly");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();const auto source=content::load_collection(persistence::read_bounded(PGP_CONTACTS_SOURCE,content::maximum_document_bytes),registry);const auto program=runtime::compile_collection(source,registry);
        auto plain=start(program,false);const auto before=plain.current_state();
        check(!core::national_access(before.world.simulation) && !core::world_access(before.world.simulation),"An ordinary new founder must start without national or world contacts");
        auto refusal=plain.submit({before.world.simulation.revision,core::RequestInstitutionalMandate{"unavailable","region.us",{},{},{},0,0}});
        check(!refusal.accepted() && refusal.explanation.find("National contacts")!=std::string::npos && plain.current_state()==before,"National initiation must refuse atomically before spending or authority changes");
        refusal=plain.submit({before.world.simulation.revision,core::EndorseForumStandard{1,"region.europe","report",true}});
        check(!refusal.accepted() && refusal.explanation.find("World-level contacts")!=std::string::npos && plain.current_state()==before,"World actions must require the first actual overseas opening");
        auto rich=start(program,true);check(core::national_access(rich.current_state().world.simulation) && !core::world_access(rich.current_state().world.simulation),"Connected background unlocks national contacts independently of world contacts");
        const auto cash=rich.current_state().world.simulation.cash;const auto capacity=rich.current_state().world.simulation.capacity;
        submit(rich,core::OpenLiaisonOffice{"liaison.canada"});
        const auto ordered=rich.current_state();const auto& s=ordered.world.simulation;
        check(s.cash==core::checked_unit_subtract(cash,core::Cents{3000000}) && s.capacity==capacity && !core::world_access(s),"An overseas setup must spend cash, add no compute or HQ workers and wait for opening");exact(program,rich);
        refusal=rich.submit({s.revision,core::OpenLiaisonOffice{"liaison.canada"}});check(!refusal.accepted() && rich.current_state()==ordered,"Duplicate regional office commitments must roll back");
        auto withdrawn=rich;submit(withdrawn,core::WithdrawOperations{});
        for(int day=0;day<14;++day)submit(withdrawn,core::AdvanceDay{});
        check(!core::world_access(withdrawn.current_state().world.simulation),"Operations withdrawn before opening cannot establish world contacts");
        auto unearned=withdrawn.current_state();unearned.world.simulation.infrastructure->workplace->access->first_overseas_opened=unearned.world.simulation.date;
        refuses([&]{runtime::validate_checkpoint(program,unearned);});exact(program,withdrawn);
        auto cancelled=rich;submit(cancelled,core::CloseLiaisonOffice{1});check(!core::world_access(cancelled.current_state().world.simulation),"Cancelling an unopened office must not unlock world actors");
        submit(cancelled,core::OpenLiaisonOffice{"liaison.canada"});exact(program,cancelled);
        for(int day=0;day<14;++day)submit(rich,core::AdvanceDay{});
        const auto opened=rich.current_state();check(core::world_access(opened.world.simulation) && core::liaison_office_open(opened.world.simulation,opened.world.simulation.infrastructure->workplace->access->offices[0],opened.world.simulation.date),"Only the real opening date grants world contacts and operating local headcount");exact(program,rich);
        const auto choice=std::ranges::find_if(rich.current_state().runtime.presentations,[](const auto& p){return !p.resolved && !p.choices.empty();});
        if(choice!=rich.current_state().runtime.presentations.end()) {const auto id=choice->id;const auto decline=std::ranges::find(choice->choices,std::string{"decline"},&runtime::PresentedChoice::id);check(decline!=choice->choices.end(),"Expected an optional supplier decision");const auto result=rich.choose(rich.current_state().world.simulation.revision,id,decline->id);check(result.accepted(),result.explanation.c_str());}
        submit(rich,core::CloseLiaisonOffice{1});check(core::world_access(rich.current_state().world.simulation),"Established world contacts must survive later office closure");exact(program,rich);
        auto forged=opened;forged.world.simulation.infrastructure->workplace->access->first_overseas_opened=forged.world.simulation.infrastructure->workplace->starts_on;refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=ordered;forged.world.simulation.infrastructure->workplace->access->connected_background=false;refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=ordered;auto& recurring=forged.world.simulation.accounts->recurring;auto rent=std::ranges::find(recurring,std::string{"office.liaison/1"},&core::RecurringObligation::id);rent->monthly_amount=core::Cents{1};refuses([&]{runtime::validate_checkpoint(program,forged);});
        auto tiny_source=source;auto& tiny_rules=*std::get<content::ScenarioDefinition>(tiny_source.definitions.at("scenario.founder_campaign").data).infrastructure->workplace->access;tiny_rules.productive_staff=2;
        const auto tiny_program=runtime::compile_collection(tiny_source,registry);auto staffed=start(tiny_program,false);
        const auto applicant=staffed.current_state().world.simulation.management->people->applicants.front().profile.id;submit(staffed,core::HireEmployee{applicant});
        for(int day=0;day<7;++day)submit(staffed,core::AdvanceDay{});
        check(staffed.current_state().world.simulation.infrastructure->workplace->access->national->cause=="staff","The hand-auditable staff threshold must unlock only after real onboarding");exact(tiny_program,staffed);
        submit(staffed,core::WithdrawOperations{});check(core::national_access(staffed.current_state().world.simulation),"National staffing history must survive later departures, including the same day");exact(tiny_program,staffed);
        auto earned=start(program,false);const auto due=core::advance_days(earned.current_state().world.simulation.date,core::Days{30});
        submit(earned,core::IssueInvoice{{"qualified_revenue",core::InvoiceKind::receivable,due,core::Cents{25000000},core::Cents{0},"Focused earned-revenue fixture"}});
        check(earned.current_state().world.simulation.infrastructure->workplace->access->national->cause=="revenue","Recognized revenue, rather than cash investment, must qualify national scale");exact(program,earned);
        forged=earned.current_state();forged.world.simulation.infrastructure->workplace->access->national->postings=0;refuses([&]{runtime::validate_checkpoint(program,forged);});
        std::cout<<"Background and earned national access, paid overseas openings, cancellation, permanent contact history, refusal, payroll and exact saves passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
