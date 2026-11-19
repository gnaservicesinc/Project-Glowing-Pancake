#include "world_controller.hpp"
#include "society_controller.hpp"
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
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
        if(!definition.founding->partners.empty()) selected.partner=definition.founding->partners.front().id;
        for(const auto& chapter:definition.founding->scenes) selected.choices.emplace(chapter.id,chapter.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selected;options.seed=19972015;options.identity=runtime::CampaignIdentity{"Devices test","Alex Vale","they/them"};
        runtime::Runtime live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",std::nullopt,options)};
        check(live.prepare_opening(live.current_state().world.simulation.revision).accepted(),"Opening preparation failed");
        const auto submit=[&](core::CommandAction a) {const auto result=live.submit({live.current_state().world.simulation.revision,std::move(a)});check(result.accepted(),result.explanation.c_str());};
        const auto snapshot=[&] {return std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,live.current_state(),{},true,false});};
        bridge::WorldController world{{}};world.update(snapshot(),program,true);
        bridge::SocietyController society;society.update(snapshot(),true);
        const auto regions=society.mapRegions()->rowCount();
        society.regions()->setFilterText(QStringLiteral("No matching region"));
        check(regions>0 && society.mapRegions()->rowCount()==regions && society.regions()->rowCount()==0,"World-map briefings must not inherit the World affairs filter");
        society.mapRegions()->setSelectedId(society.mapRegions()->idAt(regions-1));
        check(society.regions()->rowCount()==0 && society.regions()->selectedId().isEmpty(),"Map selection must not replace another app's filtered context");
        check(world.referenceCity("US-MI")=="Detroit" && world.referenceCity("state.us_nj")=="Newark" && world.referenceCity("US-TN")=="Memphis" && world.referenceCity("US-HI")=="Honolulu", "City context must use the complete offline 2010 baseline, including aliases and CDPs");
        check(world.devices().size()==3,"Shared office must include three owned working device kinds");
        check(world.navigate("workspace","hq","phone","lab","research") && world.navigation().value("device")=="laptop","Phone app routing must choose an actually compatible device");
        const auto hq_desktop_ids=[&](const core::SimulationState& st) {
            std::vector<std::uint32_t> ids;
            if(st.infrastructure && st.infrastructure->workplace && st.infrastructure->workplace->headquarters) {
                const auto& eq=st.infrastructure->catalog.workplace->headquarters->equipment;
                for(const auto& item:st.infrastructure->workplace->headquarters->items) {
                    const auto it=std::ranges::find(eq,item.offer,&core::HQEquipmentOffer::id);
                    if(it!=eq.end() && core::hq_desk_device(it->kind)) ids.push_back(item.id);
                }
            }
            return ids;
        };
        const auto power_desktops=[&](auto& exec,bool on) {
            const auto ids=hq_desktop_ids(live.current_state().world.simulation);
            if(!ids.empty()) {for(const auto id:ids) exec(core::ManageHeadquarters{core::PowerHQEquipment{id,on}});}
            else exec(core::SetDevicePower{3,on});
        };
        submit(core::SetDevicePower{2,false});world.update(snapshot(),program,false);
        check(world.navigation().value("device")=="desktop","Closing the laptop must move the app to an owned working workstation");
        power_desktops(submit,false);world.update(snapshot(),program,false);
        check(world.navigation().value("app")=="devices" && !world.appAvailable("lab","phone"),"No compatible device must return to equipment without an app-access bypass");
        check(world.navigate("workspace","hq","phone","inbox","") && world.navigation().value("device")=="phone","Phone must retain company decision access");
        submit(core::SetDevicePower{1,false});world.update(snapshot(),program,false);
        check(world.devices().empty() && world.navigation().value("app")=="devices","All powered-off devices must leave equipment controls available");
        submit(core::SetDevicePower{1,true});
        const auto before=live.current_state();submit(core::BuyPersonalDevice{"tablet"});
        const auto tablet_id=live.current_state().world.simulation.infrastructure->workplace->devices->records.back().id;
        check(live.current_state().world.simulation.cash==core::checked_unit_subtract(before.world.simulation.cash,core::Cents{65000}),"Tablet purchase must spend the quoted cash");
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",true).empty(),"Undelivered tablet cannot run apps");
        const auto exact=live.current_state();check(persistence::decode_save(program,persistence::encode_save(program,exact)).checkpoint==exact,"Ownership and delivery must reload exactly");
        for(int day=0;day<2;++day)submit(core::AdvanceDay{});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",false)=="tablet","Delivered portable tablet must work away from headquarters");
        submit(core::ServicePersonalDevice{tablet_id});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","lab",true).empty(),"Paid service must take the tablet offline");
        const auto servicing=live.current_state();check(persistence::decode_save(program,persistence::encode_save(program,servicing)).checkpoint==servicing,"Service and its real expense must reload exactly");
        auto forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records.back().included=true;refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records.back().offer="laptop";refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=servicing;forged.world.simulation.infrastructure->workplace->devices->records.back().services[0].ready_on=forged.world.simulation.date;refuses([&]{runtime::validate_checkpoint(program,forged);});
        const auto refusal=live.submit({live.current_state().world.simulation.revision,core::RetirePersonalDevice{1}});
        check(!refusal.accepted() && live.current_state()==servicing,"Last usable portable device cannot be retired during another device's service");
        for(int day=0;day<2;++day)submit(core::AdvanceDay{});
        submit(core::RetirePersonalDevice{1});
        check(core::resolve_app_device(live.current_state().world.simulation,"phone","inbox",false)=="tablet","Retired phone must route decisions to the working tablet");
        check(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Retirement history must reload exactly");
        const auto opening_day=live.current_state().world.simulation.infrastructure->workplace->starts_on;
        while(live.current_state().world.simulation.date<core::advance_days(opening_day,core::Days{14})) submit(core::AdvanceDay{});
        check(!live.submit({live.current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(),"An actual prepared decision must stop time");
        submit(core::SetDevicePower{tablet_id,false});submit(core::SetDevicePower{tablet_id,true});
        check(core::resolve_app_device(live.current_state().world.simulation,"tablet","inbox",false)=="tablet","Required content decisions must still allow power recovery");
        core::Simulation travel{live.current_state().world.simulation};
        const auto trip=[&](core::CommandAction action) {const auto result=travel.submit({travel.checkpoint().revision,std::move(action)});check(result.accepted(),result.explanation.c_str());};
        power_desktops(trip,true);
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
        world.buyDevice("tablet",QStringLiteral("previous/")+reviewed);check(orders==0,"A stale equipment quote must not issue a purchase");
        world.buyDevice("tablet",reviewed);check(orders==1,"A current reviewed quote must issue the typed purchase");
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{2,away,{},true,false}),program,true);
        world.buyDevice("tablet",reviewed);check(orders==1,"A confirmation from another campaign generation must refuse even when revisions match");
        world.buyDevice("tablet",world.deviceOffers().front().toMap().value("revision").toString());check(orders==2,"A freshly reviewed quote must work after campaign replacement");
        QTemporaryDir drafts{QDir::currentPath()+"/campaign-drafts-XXXXXX"};check(drafts.isValid(),"Create owned draft test directory");
        bridge::WorldController restored{drafts.path().toStdString()};
        const auto ui_snapshot=[](const auto& checkpoint,std::uint64_t generation) {return std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{generation,checkpoint,{},true,true});};
        const auto legacy=live.current_state();
        restored.update(ui_snapshot(legacy,1),program,true);restored.setDraft("lab/note","Legacy unfinished field");
        auto first=legacy;first.world.identity->instance_id="0123456789abcdef0123456789abcdef";
        restored.update(ui_snapshot(first,2),program,true);
        check(restored.draft("lab/note","").isEmpty(),"A new company cannot inherit an older company's same-name/seed draft");
        restored.setDraft("lab/note","First new company");check(restored.navigate("map","state","","",""),"Store original navigation");
        auto second=first;second.world.identity->instance_id="fedcba9876543210fedcba9876543210";
        restored.update(ui_snapshot(second,3),program,true);
        check(restored.draft("lab/note","").isEmpty() && restored.navigation().value("place")=="workspace","Identical names and seed must still have independent drafts and navigation");
        restored.setDraft("lab/note","Second new company");
        const auto saved=persistence::encode_save(program,first);const auto reloaded=persistence::decode_save(program,saved).checkpoint;
        check(reloaded==first && first.world.simulation==legacy.world.simulation,"The local instance must persist without consuming simulation RNG or changing company work");
        restored.update(ui_snapshot(reloaded,4),program,true);
        check(restored.draft("lab/note","")=="First new company" && restored.navigation().value("place")=="map","Exact reload must recover that run's own fields and place");
        restored.update(ui_snapshot(legacy,5),program,true);
        check(restored.draft("lab/note","")=="Legacy unfinished field" && persistence::encode_save(program,legacy).find("instance_id")==std::string::npos,"Legacy saves and existing drafts must retain their original shape and location");
        auto invalid_identity=*first.world.identity;invalid_identity.instance_id="../other";refuses([&]{runtime::validate_campaign_identity(invalid_identity);});
        // Read-only presentation fixture: source metadata is projected without
        // claiming that this later story's gameplay trigger has occurred.
        auto cast_checkpoint=live.current_state();runtime::Presentation contact;
        contact.id=700;contact.day=cast_checkpoint.world.simulation.date;contact.kind=runtime::PresentationKind::dialogue;
        contact.flow_id="story.cedar_delivery_review";contact.node_id="choice";cast_checkpoint.runtime.presentations.push_back(contact);
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{3,cast_checkpoint,{},true,false}),program,true);
        const auto cast=world.decisionParticipants("content",700);const auto source_cast=std::get<content::FlowDefinition>(program.collection.find(contact.flow_id)->data).nodes.at(contact.node_id).participants;
        if(cast.size()!=2 || cast.back().toMap().value("name")!="Lio Ward") {
            std::cerr<<"Cast refs "<<source_cast.size()<<" rows "<<cast.size()<<'\n';
            for(const auto& item:cast) {const auto row=item.toMap();std::cerr<<row.value("id").toString().toStdString()<<" / "<<row.value("name").toString().toStdString()<<" / "<<row.value("portrait").toString().toStdString()<<'\n';}
        }
        check(source_cast.size()==2 && cast.size()==2 && cast[0].toMap().value("id")=="founder" && cast[1].toMap().value("name")=="Lio Ward" && cast[1].toMap().value("portrait").toString().startsWith("image://portraits/1/"),"Decisions must resolve explicit source contacts and layered vectors alongside the actual founder");
        const auto candidate=world.person("executive.research",cast_checkpoint.world.simulation.date);
        check(candidate.value("name")=="Mara Hart" && !candidate.value("portrait").toString().isEmpty(),"Company participants must reuse their saved hiring profile");
        check(world.decisionParticipants("content",99999).isEmpty(),"Unknown presentations cannot invent a cast");
        check(cast_checkpoint.world.simulation==live.current_state().world.simulation,"Inspecting participants must not mutate company resources or RNG");
        auto rules=*definition.infrastructure->workplace->devices;refuses([&]{rules.offers[0].delivery=core::Days{-1};core::validate_device_rules(rules);});

        // F-08B: Physical device workspaces, multi-unit identity, suspend/close, unavailability, pop-out & geometry clamping
        {
            runtime::Runtime ws_live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",std::nullopt,options)};
            check(ws_live.prepare_opening(ws_live.current_state().world.simulation.revision).accepted(),"F-08B opening prep");
            const auto ws_submit=[&](core::CommandAction a) {const auto r=ws_live.submit({ws_live.current_state().world.simulation.revision,std::move(a)});check(r.accepted(),r.explanation.c_str());};
            // Buy a second laptop so two distinct physical laptops exist simultaneously.
            ws_submit(core::BuyPersonalDevice{"laptop"});
            for(int d=0;d<3;++d) ws_submit(core::AdvanceDay{});
            auto ws_cp=ws_live.current_state();
            ws_cp.world.identity->instance_id="abcdef0123456789abcdef0123456789";
            const auto before_sim=ws_cp.world.simulation;

            QTemporaryDir ws_dir{QDir::currentPath()+"/device-workspaces-XXXXXX"};check(ws_dir.isValid(),"Workspace dir");
            bridge::WorldController wc{ws_dir.path().toStdString()};
            wc.update(ui_snapshot(ws_cp,10),program,true);

            const auto phys=wc.physicalDevices();
            QString laptop1, laptop2;
            for(const auto& row:phys) {
                const auto m=row.toMap();
                if(m.value("kind")=="laptop") {
                    if(laptop1.isEmpty()) laptop1=m.value("deviceId").toString();
                    else laptop2=m.value("deviceId").toString();
                }
            }
            check(!laptop1.isEmpty() && !laptop2.isEmpty() && laptop1!=laptop2,"Two owned laptops must have distinct physical device IDs");
            check(wc.physicalDevice(laptop1).value("name").toString()!=wc.physicalDevice(laptop2).value("name").toString(),"Multiple laptops must have unambiguous names");
            check(wc.physicalDevice(laptop1).value("owner")=="protagonist" && wc.physicalDevice(laptop2).value("owner")=="company","Founding portable device is protagonist-owned; purchased device is company-owned");

            // Independent app stacks & unfinished drafts on Laptop #1 vs Laptop #2
            check(wc.openDeviceApp(laptop1,"lab","research"),"Open Research on Laptop #1");
            check(wc.openDeviceApp(laptop1,"people",""),"Open People on Laptop #1");
            wc.setDeviceAppState(laptop1,"lab","selectedTopic","research.embeddings");
            check(wc.openDeviceApp(laptop2,"office","accounts"),"Open Accounts on Laptop #2");
            wc.setDeviceAppState(laptop2,"office","expenseFilter","payroll");
            check(wc.deviceWorkspace(laptop1).value("openApps").toStringList()==QStringList({"lab","people"}),"Laptop #1 holds its own app stack");
            check(wc.deviceWorkspace(laptop2).value("openApps").toStringList()==QStringList({"office"}),"Laptop #2 holds an independent app stack");

            // Suspend keeps open apps & drafts; explicit close removes app & clears its session
            wc.suspendDeviceApp(laptop1);
            check(wc.deviceWorkspace(laptop1).value("activeApp").toString().isEmpty(),"Suspend clears active app");
            check(wc.deviceWorkspace(laptop1).value("openApps").toStringList().size()==2,"Suspend keeps open apps");
            check(wc.deviceAppState(laptop1,"lab").value("selectedTopic")=="research.embeddings","Suspend preserves drafts");
            wc.closeDeviceApp(laptop1,"people");
            check(wc.deviceWorkspace(laptop1).value("openApps").toStringList()==QStringList({"lab"}),"Explicit close removes only that app");
            check(wc.switchDeviceApp(laptop1,"lab") && wc.deviceWorkspace(laptop1).value("activeApp")=="lab","Switch restores preserved Research app");

            // Pop-out, detached research tree, and offscreen geometry clamping
            check(wc.setDevicePoppedOut(laptop2,true),"Pop out Laptop #2");
            check(wc.poppedOutDevices().size()==1 && wc.poppedOutDevices().front().toMap().value("deviceId")==laptop2,"Popped-out list reports Laptop #2");
            check(wc.openDetachedResearchTree(laptop1) && wc.detachedResearchTree().value("open").toBool() && wc.detachedResearchTree().value("deviceId")==laptop1,"Detached research tree links to Laptop #1");
            wc.saveWindowGeometry("device:"+laptop2,99999,99999,900,600);
            const auto clamped=wc.clampedWindowGeometry("device:"+laptop2,900,600);
            check(clamped.value("x").toInt()>=0 && clamped.value("x").toInt()<5000 && clamped.value("y").toInt()>=0 && clamped.value("y").toInt()<5000,"Offscreen detached window geometry must clamp safely");

            // Reload fresh WorldController from same directory: exact workspace restoration, zero sim/RNG mutation
            bridge::WorldController wc2{ws_dir.path().toStdString()};
            wc2.update(ui_snapshot(ws_cp,11),program,true);
            check(wc2.deviceWorkspace(laptop1).value("activeApp")=="lab","Reloaded Laptop #1 activeApp");
            check(wc2.deviceAppState(laptop1,"lab").value("selectedTopic")=="research.embeddings","Reloaded Laptop #1 draft");
            check(wc2.deviceAppState(laptop2,"office").value("expenseFilter")=="payroll","Reloaded Laptop #2 draft");
            check(wc2.poppedOutDevices().size()==1,"Reloaded pop-out state");
            check(wc2.detachedResearchTree().value("open").toBool(),"Reloaded detached research tree state");
            check(ws_cp.world.simulation==before_sim,"Workspace operations must never mutate simulation state or RNG");

            // Powering off Laptop #1 preserves its workspace & reports honest unavailability while Laptop #2 stays active
            const auto laptop1_num=wc2.physicalDevice(laptop1).value("numericId").toULongLong();
            ws_submit(core::SetDevicePower{laptop1_num,false});
            auto off_cp=ws_live.current_state();off_cp.world.identity->instance_id=ws_cp.world.identity->instance_id;
            wc2.update(ui_snapshot(off_cp,12),program,false);
            check(!wc2.physicalDevice(laptop1).value("available").toBool() && !wc2.physicalDevice(laptop1).value("unavailabilityReason").toString().isEmpty(),"Powered-off Laptop #1 must report unavailability");
            check(wc2.physicalDevice(laptop2).value("available").toBool(),"Laptop #2 stays available independently");
            check(wc2.deviceAppState(laptop1,"lab").value("selectedTopic")=="research.embeddings","Powering off Laptop #1 preserves its drafts");
            check(!wc2.alternateDevicesForApp("lab",laptop1).isEmpty(),"Alternate eligible device offered without silent aliasing");
            ws_submit(core::SetDevicePower{laptop1_num,true});
            auto on_cp=ws_live.current_state();on_cp.world.identity->instance_id=ws_cp.world.identity->instance_id;
            wc2.update(ui_snapshot(on_cp,13),program,false);
            check(wc2.physicalDevice(laptop1).value("available").toBool() && wc2.deviceAppState(laptop1,"lab").value("selectedTopic")=="research.embeddings","Restoring power resumes preserved workspace");
        }
        std::cout<<"Owned device delivery, power, app routing, portable access, service, retirement, physical workspaces, pop-out, rollback and exact persistence passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
