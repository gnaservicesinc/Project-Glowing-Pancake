#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace {
using namespace pgp;
void check(bool okay,const char* why) {if(!okay) throw std::runtime_error(why);}
}
int main() {
    try {
        using namespace pgp;
        auto json=nlohmann::json::parse(persistence::read_bounded(PGP_HQ_SOURCE,content::maximum_document_bytes));
        for(auto& d:json["definitions"]) if(d["id"]=="scenario.founder_campaign") {
            auto& c=d["infrastructure"]["workplace"]["headquarters"]["contractors"][0];
            c["delay_bp"]=10000;c["overrun_bp"]=10000;c["maximum_delay_days"]=2;c["maximum_overrun_bp"]=1000;
        }
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(json.dump(),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selected;selected.family="rich";selected.route="bootstrap";selected.office="home";
        for(const auto& scene:scenario.founding->scenes) selected.choices.emplace(scene.id,scene.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selected;options.seed=719;
        options.identity=runtime::CampaignIdentity{"Physical HQ","Alex Vale","they/them"};
        auto inherited_options=options;
        inherited_options.founding->route="hired_ceo";inherited_options.founding->office="shared";inherited_options.founding->leg="political";
        inherited_options.founding->researcher="none";
        const auto inherited=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},inherited_options);
        const auto& office=inherited.world.simulation;
        check(core::furnished_hq_desks(office)==4 && core::assigned_hq_desks(office)==4 && office.capacity->sources.size()==4,"The CEO inherits furnishings for the three actual employees and their own desk.");
        check(core::capacity_report(*office.capacity).effective==core::MilliCUPerDay{16000},"Inherited output must be the sum of the four actual modest computers.");
        check(persistence::decode_save(program,persistence::encode_save(program,inherited)).checkpoint==inherited,"The inherited office and roster must reload exactly.");
        auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        check(core::current_headquarters(initial.world.simulation).rooms.size()==6,"Wealthy families must provide six actual free furnishing rooms.");
        for(const auto& [family,count]:std::vector<std::pair<std::string,std::size_t>>{{"poor",3},{"middle",4}}) {
            auto family_options=options;family_options.founding->family=family;family_options.founding->route="investment";family_options.founding->backers={"commercial"};
            const auto home=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},family_options);
            check(core::current_headquarters(home.world.simulation).rooms.size()==count,"Family background must select the actual authored home capacity.");
            check(persistence::decode_save(program,persistence::encode_save(program,home)).checkpoint==home,"Every family home's rooms must reload exactly.");
        }
        initial.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime game{program,initial};
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto headquarters=[&]() -> const auto& {return *state().infrastructure->workplace->headquarters;};
        const auto apply=[&](core::CommandAction action) {const auto result=game.submit({state().revision,std::move(action)});if(!result.accepted()) throw std::runtime_error(result.explanation);};
        const auto action=[&](core::HeadquartersAction a) {apply(core::ManageHeadquarters{std::move(a)});};
        const auto refuse=[&](core::HeadquartersAction a) {const auto before=game.current_state();const auto result=game.submit({state().revision,core::ManageHeadquarters{std::move(a)}});check(!result.accepted() && before==game.current_state(),"Refused HQ action must leave money, placement and RNG unchanged.");};
        check(game.prepare_opening(state().revision).accepted(),"Fresh company must prepare.");
        check(headquarters().items.empty() && core::headquarters_seats(state())==0 && state().capacity->sources.empty(),"The garage must start empty, without desks or compute.");
        const auto opening_cash=state().cash;
        action(core::BuyHQEquipment{"desk"});action(core::BuyHQEquipment{"desktop"});
        check(state().cash==core::checked_unit_subtract(opening_cash,core::Cents{175000}),"Desk and computer need actual separate supplier payments.");
        refuse(core::PlaceHQEquipment{1,0,0,0,false});
        for(int n=0;n<3;++n) apply(core::AdvanceDay{});
        check(state().capacity->sources.size()==1 && state().capacity->sources.front().effective==core::MilliCUPerDay{0},"Delivered but unplaced machines must supply no compute.");
        action(core::PlaceHQEquipment{1,0,0,0,false});action(core::PlaceHQEquipment{2,1,0,0,false});
        check(core::hq_computer_available(state()) && state().capacity->sources.front().effective==core::MilliCUPerDay{4000},"A placed powered desktop must supply its actual small capacity.");
        action(core::AssignHQDesk{1,"founder"});
        refuse(core::AssignHQDesk{1,"unknown applicant"});
        action(core::PowerHQEquipment{2,false});check(!core::hq_computer_available(state()) && state().capacity->sources.front().effective.value()==0,"Power off must remove real machine output.");
        action(core::PowerHQEquipment{2,true});apply(core::AdvanceDay{});
        check(headquarters().energy_wh>0 && headquarters().electricity_cost>core::Cents{0},"Powered HQ equipment must have metered electricity costs.");
        action(core::BuyHQEquipment{"desk"});apply(core::AdvanceDay{});
        refuse(core::PlaceHQEquipment{3,0,1,1,false});
        const auto quote=core::preview_hq_lease(state(),{"austin","studio"});
        check(quote.monthly==core::Cents{224000},"Realtor rent must follow the actual city and office area.");
        action(core::LeaseHQ{"austin","studio"});
        check(game.current_state().world.state_id=="state.us_tx" && core::current_headquarters(state()).width==10 && state().capacity->sources.front().effective.value()==0,"Moving must change geography and store equipment until it is placed again.");
        {
            runtime::Runtime recovery{program,game.current_state()};
            const auto quote=core::preview_hq_home(state());const auto cash=state().cash;
            const auto returned=recovery.submit({state().revision,core::ManageHeadquarters{core::ReturnHomeHQ{}}});
            check(returned.accepted(),"A leased company must be able to return to the family home.");
            const auto& home=recovery.current_state().world.simulation;
            check(recovery.current_state().world.state_id=="state.us_ca" && core::current_headquarters(home).rooms.size()==6,"Returning home must restore the original California estate's actual rooms.");
            check(home.cash==core::checked_unit_subtract(core::checked_unit_add(cash,quote.refund),quote.exit_fee),"Home relocation must settle the lease refund and exit fee exactly.");
            check(std::ranges::any_of(home.accounts->recurring,[&](const auto& bill){return bill.id=="hq.rent/2" && bill.ends_before==home.date;}),"Returning home must end the commercial rent obligation.");
            check(std::ranges::all_of(home.infrastructure->workplace->headquarters->items,[](const auto& item){return item.premises==0;}),"Home return keeps equipment in storage until placement.");
            const auto before=recovery.current_state();
            check(!recovery.submit({home.revision,core::ManageHeadquarters{core::ReturnHomeHQ{}}}).accepted() && recovery.current_state()==before,"Repeated home return must refuse without duplicate refunds.");
            check(persistence::decode_save(program,persistence::encode_save(program,before)).checkpoint==before,"Home recovery, paid exit and stopped rent must reload exactly.");
        }
        action(core::PlaceHQEquipment{1,0,0,0,false});action(core::PlaceHQEquipment{2,1,0,0,false});action(core::PlaceHQEquipment{3,0,3,0,false});
        const auto before_preview=game.current_state();
        check(core::hq_placement_blocker(state(),{1,0,3,0,false}).size()>0,"Placement preview must detect another desk.");
        check(core::hq_placement_blocker(state(),{1,0,6,0,true}).empty(),"Rotation preview must find free floor.");
        check(core::hq_placement_blocker(state(),{1,0,2147483647,0,false}).size()>0,"Oversized coordinates must be refused safely.");
        check(game.current_state()==before_preview,"Hover previews must not change money, RNG or placements.");
        action(core::BuyHQEquipment{"thin_client"}); // 4
        action(core::BuyHQEquipment{"rack"}); // 5
        action(core::BuyHQEquipment{"laptop"}); // 6
        action(core::BuyHQEquipment{"research_laptop"}); // 7
        action(core::BuyHQEquipment{"plant"}); // 8
        for(int n=0;n<7;++n) apply(core::AdvanceDay{});
        action(core::PlaceHQEquipment{4,3,0,0,false});
        action(core::AssignHQDesk{3,"founder"});
        check(!core::hq_workstation_ready(state(),"founder"),"A thin client without a powered rack is not a working computer.");
        action(core::PlaceHQEquipment{5,0,7,0,false});
        check(core::hq_workstation_ready(state(),"founder"),"A thin client must become usable when its rack is available.");
        action(core::PowerHQEquipment{5,false});
        check(!core::hq_workstation_ready(state(),"founder"),"Turning off the rack must disable dependent clients.");
        action(core::AssignHQLaptop{6,"founder"});
        action(core::AssignHQDesk{3,""});
        check(core::hq_workstation_ready(state(),"founder") && !core::assigned_hq_desk(state(),"founder"),"An issued local laptop must support working from home without a fixed desk.");
        action(core::AssignHQLaptop{7,"founder"});
        check(headquarters().items[5].person.empty() && !core::hq_workstation_ready(state(),"founder"),"One employee gets one laptop; a research laptop requires its remote rack.");
        action(core::PowerHQEquipment{5,true});
        check(core::hq_workstation_ready(state(),"founder"),"Research laptop must use the powered remote rack.");
        const auto rack_source=std::ranges::find(state().capacity->sources,std::string{"hq.equipment/5"},&core::CapacitySource::id);
        check(rack_source!=state().capacity->sources.end() && rack_source->effective==core::MilliCUPerDay{40000},"Clients must not duplicate rack capacity.");
        action(core::PlaceHQEquipment{8,0,8,0,false});
        refuse(core::PlaceHQEquipment{8,0,0,0,false});
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Portable assignments and remote dependencies must reload exactly.");
        action(core::BuyHQLand{"seattle","compact"});
        const std::vector<core::HQRoom> layout{{0,0,3,4},{3,0,3,4}};
        const auto random_before=headquarters().stream;
        for(const auto& c:scenario.infrastructure->workplace->headquarters->contractors) (void)core::preview_hq_build(state(),{1,c.id,6,4,layout});
        check(headquarters().stream==random_before,"Comparing contractor bids must not consume future random outcomes.");
        refuse(core::BuildHQ{1,"budget",7,4,{{0,0,7,4}}});
        action(core::BuildHQ{1,"budget",6,4,layout});
        const auto contract=headquarters().construction.front();
        check(contract.actual>contract.quote && contract.finish_on>contract.promised_on,"Forced risk fixture must create actual overrun and delay.");
        while(state().date<contract.promised_on) apply(core::AdvanceDay{});
        check(headquarters().construction.front().delay_announced && !headquarters().construction.front().completed_on,"A delay must be disclosed at the promised date, without granting a finished building.");
        const auto saved=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
        check(saved.checkpoint==game.current_state(),"Pending construction, paid contingency, furnishings and RNG must reload exactly within the current build.");
        while(state().date<contract.finish_on) apply(core::AdvanceDay{});
        check(headquarters().construction.front().completed_on.has_value(),"The contractor must finish after the actual delay.");
        action(core::MoveToOwnedHQ{1});
        check(game.current_state().world.state_id=="state.us_wa" && core::current_headquarters(state()).rent==core::Cents{0},"Owned premises must be in their purchased city and have no rent.");
        bool land_tax=false,building_tax=false,old_rent_ended=false;
        for(const auto& bill:state().accounts->recurring) {if(bill.id=="hq.land.tax/1") land_tax=true;if(bill.id=="hq.build.tax/1") building_tax=true;if(bill.id=="hq.rent/2" && bill.ends_before==state().date) old_rent_ended=true;}
        check(land_tax && building_tax && old_rent_ended,"Land/building taxes must continue while the old office rent ends.");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Relocation and property ownership must reload exactly.");
        action(core::ReturnHomeHQ{});
        check(core::current_headquarters(state()).rooms.size()==6 && headquarters().items[6].person=="founder","Returning from owned premises must retain the family's rooms and issued laptop.");
        check(std::ranges::any_of(state().accounts->recurring,[](const auto& bill){return bill.id=="hq.build.tax/1" && !bill.ends_before;}),"Moving home must not erase tax on a building the company still owns.");
        action(core::PlaceHQEquipment{5,0,8,0,false});
        check(core::hq_workstation_ready(state(),"founder"),"The returning team can reinstall its rack and resume laptop work.");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Reinstalled home equipment and retained property taxes must reload exactly.");
        std::cout<<"Empty garage, paid delivery, placement/collision, assignments, physical compute/electricity, city leases, bids, construction risks, ownership taxes and fresh reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
