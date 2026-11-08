#include "world_controller.hpp"
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <iostream>
#include <algorithm>
namespace {
using namespace pgp;
void check(bool okay,const char* reason) {if(!okay) throw std::runtime_error(reason);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged device state accepted");}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_DEVICES_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selected;selected.family="rich";selected.office="shared";
        for(const auto& chapter:definition.founding->scenes) selected.choices.emplace(chapter.id,chapter.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selected;options.seed=19972015;options.identity=runtime::CampaignIdentity{"Devices test","Alex Vale","they/them"};
        runtime::Runtime live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options)};
        check(live.prepare_opening(live.current_state().world.simulation.revision).accepted(),"Opening preparation failed");
        const auto submit=[&](core::CommandAction a) {const auto result=live.submit({live.current_state().world.simulation.revision,std::move(a)});check(result.accepted(),result.explanation.c_str());};
        const auto snapshot=[&] {return std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,live.current_state(),{},true,false});};
        bridge::WorldController world{{}};world.update(snapshot(),program,true);
        check(world.devices().size()==3,"Shared office must include three owned working device kinds");
        check(world.navigate("workspace","hq","phone","lab","research") && world.navigation().value("device")=="laptop","Phone app routing must choose an actually compatible device");
        submit(core::SetDevicePower{2,false});world.update(snapshot(),program,false);
        check(world.navigation().value("device")=="desktop","Closing the laptop must move the app to an owned working workstation");
        submit(core::SetDevicePower{3,false});world.update(snapshot(),program,false);
        check(world.navigation().value("app")=="devices" && !world.appAvailable("lab","phone"),"No compatible device must return to equipment without an app-access bypass");
        check(world.navigate("workspace","hq","phone","inbox","") && world.navigation().value("device")=="phone","Phone must retain company decision access");
        submit(core::SetDevicePower{1,false});world.update(snapshot(),program,false);
        check(world.devices().empty() && world.navigation().value("app")=="devices","All powered-off devices must leave equipment controls available");
        submit(core::SetDevicePower{1,true});
        const auto before=live.current_state();submit(core::BuyPersonalDevice{"tablet"});
        check(live.current_state().world.simulation.cash==core::checked_unit_subtract(before.world.simulation.cash,core::Cents{65000}),"Tablet purchase must spend the quoted cash");
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",true).empty(),"Undelivered tablet cannot run apps");
        const auto exact=live.current_state();check(persistence::decode_save(program,persistence::encode_save(program,exact)).checkpoint==exact,"Ownership and delivery must reload exactly");
        for(int day=0;day<2;++day)submit(core::AdvanceDay{});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",false)=="tablet","Delivered portable tablet must work away from headquarters");
        submit(core::ServicePersonalDevice{4});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",true).empty(),"Paid service must take the tablet offline");
        const auto servicing=live.current_state();check(persistence::decode_save(program,persistence::encode_save(program,servicing)).checkpoint==servicing,"Service and its real expense must reload exactly");
        auto forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records[3].included=true;refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records[3].offer="laptop";refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records[3].services[0].ready_on=forged.world.simulation.date;refuses([&]{runtime::validate_checkpoint(program,forged);});
        const auto refusal=live.submit({live.current_state().world.simulation.revision,core::RetirePersonalDevice{1}});
        check(!refusal.accepted() && live.current_state()==servicing,"Last usable portable device cannot be retired during another device's service");
        for(int day=0;day<2;++day)submit(core::AdvanceDay{});
        submit(core::RetirePersonalDevice{1});
        check(core::resolve_app_device(live.current_state().world.simulation,"phone","inbox",false)=="tablet","Retired phone must route decisions to the working tablet");
        check(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Retirement history must reload exactly");
        const auto opening_day=live.current_state().world.simulation.infrastructure->workplace->starts_on;
        while(live.current_state().world.simulation.date<core::advance_days(opening_day,core::Days{14})) submit(core::AdvanceDay{});
        check(!live.submit({live.current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(),"An actual prepared decision must stop time");
        submit(core::SetDevicePower{4,false});submit(core::SetDevicePower{4,true});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","inbox",false)=="tablet","Required content decisions must still allow power recovery");
        core::Simulation travel{live.current_state().world.simulation};
        const auto trip=[&](core::CommandAction action) {const auto result=travel.submit({travel.checkpoint().revision,std::move(action)});check(result.accepted(),result.explanation.c_str());};
        trip(core::SetDevicePower{3,true});
        const core::BuildOwnedSite site{"visit_site","Regional device check","US-TX",20000,core::CoolingMode::air};
        const auto site_quote=core::preview_site_build(travel.checkpoint(),site);trip(site);
        const auto remote_on=core::advance_days(std::max({site_quote.built_on,site_quote.grid_on,core::advance_days(travel.checkpoint().date,core::Days{14})}),core::Days{1});
        trip(core::ScheduleSiteAppointment{"remote","visit_site",remote_on,core::AppointmentMode::remote});
        while(travel.checkpoint().date<remote_on) trip(core::AdvanceDay{});
        auto away=live.current_state();away.world.simulation=travel.checkpoint();
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,away,{},true,false}),program,false);
        check(world.atHeadquarters() && core::resolve_app_device(travel.checkpoint(),"desktop","lab",world.atHeadquarters())=="desktop","Remote appointments must retain physical headquarters devices");
        const auto onsite_on=core::advance_days(travel.checkpoint().date,core::Days{14});
        trip(core::ScheduleSiteAppointment{"onsite","visit_site",onsite_on,core::AppointmentMode::onsite});
        while(travel.checkpoint().date<onsite_on) trip(core::AdvanceDay{});
        away.world.simulation=travel.checkpoint();
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,away,{},true,false}),program,false);
        check(!world.atHeadquarters() && core::resolve_app_device(travel.checkpoint(),"desktop","lab",world.atHeadquarters())=="tablet","Onsite travel must exclude HQ workstations and retain portable devices");
        int orders=0;QObject::connect(&world,&bridge::WorldController::actionRequested,[&](const auto&){++orders;});
        const auto reviewed=world.deviceOffers().front().toMap().value("revision").toString();
        world.buyDevice("tablet",QString::number(reviewed.toULongLong()-1));check(orders==0,"A stale equipment quote must not issue a purchase");
        world.buyDevice("tablet",reviewed);check(orders==1,"A current reviewed quote must issue the typed purchase");
        auto rules=*definition.infrastructure->workplace->devices;refuses([&]{rules.offers[0].delivery=core::Days{-1};core::validate_device_rules(rules);});
        std::cout<<"Owned device delivery, power, app routing, portable access, service, retirement, rollback and exact persistence passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
