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
            auto& headquarters=d["infrastructure"]["workplace"]["headquarters"];
            headquarters["traffic"]={{"comfortable_walk",8},{"amenity_walk",10},{"crowded_at",2},{"blocked_mood",8},{"maximum_friction",4}};
            d["management"]["people"]["mood"]={{"pay",0},{"company_prestige",0},{"ceo_prestige",0},{"role",0},
                {"coworkers",0},{"amenity_cap",14},{"temperaments",nlohmann::json::object()},{"amenities",{{"plant",2}}}};
            auto& c=headquarters["contractors"][0];
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
        inherited_options.founding->route="hired_ceo";inherited_options.founding->office="shared";inherited_options.founding->advantages={"political"};
        inherited_options.founding->researcher="none";
        const auto inherited=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},inherited_options);
        const auto& office=inherited.world.simulation;
        check(core::furnished_hq_desks(office)==4 && core::assigned_hq_desks(office)==4 && office.capacity->sources.size()==4,"The CEO inherits furnishings for the three actual employees and their own desk.");
        check(core::capacity_report(*office.capacity).effective==core::MilliCUPerDay{16000},"Inherited output must be the sum of the four actual modest computers.");
        check(persistence::decode_save(program,persistence::encode_save(program,inherited)).checkpoint==inherited,"The inherited office and roster must reload exactly.");
        {
            auto setup=inherited;
            setup.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime design{program,std::move(setup)};
            const auto& staff=office.management->people->employees;
            const auto person=staff.front().terms.profile.id;
            const auto snapshot=[&]() -> const auto& {return design.current_state().world.simulation;};
            const auto submit=[&](core::CommandAction action) {
                const auto receipt=design.submit({snapshot().revision,std::move(action)});
                if(!receipt.accepted()) throw std::runtime_error(receipt.explanation);
            };
            const auto place=[&](core::HeadquartersAction action){submit(core::ManageHeadquarters{std::move(action)});};
            check(design.prepare_opening(snapshot().revision).accepted(),"Office puzzle fixture must prepare.");
            place(core::LeaseHQ{"los_angeles","compact"});
            place(core::PlaceHQEquipment{3,0,0,0,false});
            place(core::AssignHQDesk{3,person});
            place(core::BuyHQEquipment{"plant"});
            submit(core::AdvanceDay{});submit(core::AdvanceDay{});
            const auto id=snapshot().infrastructure->workplace->headquarters->items.back().id;
            place(core::PlaceHQEquipment{id,0,4,3,false});
            const auto open=core::hq_traffic(snapshot());
            const auto comfortable=core::workplace_moods(snapshot()).at(person);
            check(open.desks.size()==1 && open.access.doors.size()==1 && open.reachable_tiles==19 &&
                open.desks.front().route==std::vector{21,15,9,8} &&
                comfortable.amenities>0 && comfortable.office_walk==0,
                "The assigned desk and nearby plant should be reached by walking through the interior doorway.");
            const core::PlaceHQEquipment obstruction{id,0,2,1,false};
            const auto before=design.current_state();
            const auto blocked=core::hq_traffic(snapshot(),&obstruction);
            check(design.current_state()==before && blocked.open_tiles==open.open_tiles &&
                blocked.reachable_tiles==12 && blocked.desks.front().route.empty(),
                "A doorway obstruction should strand a room without consuming cash or time in the preview.");
            place(obstruction);
            const auto frustrated=core::workplace_moods(snapshot()).at(person);
            check(core::hq_traffic(snapshot())==blocked && frustrated.office_walk==-8 &&
                frustrated.amenities==0 && frustrated.score<comfortable.score,
                "Committed traffic and amenity access must match the preview and change the occupant's mood.");
            const auto reopened=persistence::decode_save(program,persistence::encode_save(program,design.current_state())).checkpoint;
            check(core::hq_traffic(reopened.world.simulation)==blocked,"Walking routes must reproduce after loading.");
            place(core::PlaceHQEquipment{id,0,4,3,false});
            check(core::hq_traffic(snapshot())==open && core::workplace_moods(snapshot()).at(person).score==comfortable.score,
                "Clearing a route must immediately restore its amenity and mood benefits.");
            place(core::PlaceHQEquipment{5,0,0,2,false});
            place(core::AssignHQDesk{5,staff.at(1).terms.profile.id});
            const auto shared=core::hq_traffic(snapshot());
            check(shared.crowded_tiles==std::vector{8} && shared.desks.front().congestion==1 &&
                core::workplace_moods(snapshot()).at(person).office_walk==-1,
                "Assigned routes that funnel through the same doorway must show congestion and walking friction.");
        }
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
        const std::vector<core::HQRoom> layout{{0,0,2,4},{4,0,2,4},{2,0,2,4,true}};
        const auto random_before=headquarters().stream;
        for(const auto& c:scenario.infrastructure->workplace->headquarters->contractors) (void)core::preview_hq_build(state(),{1,c.id,6,4,layout});
        check(headquarters().stream==random_before,"Comparing contractor bids must not consume future random outcomes.");
        refuse(core::BuildHQ{1,"budget",7,4,{{0,0,7,4}}});
        refuse(core::BuildHQ{1,"budget",6,4,{{0,0,2,4,true},{2,0,2,4},{4,0,2,4}}});
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
        refuse(core::PlaceHQEquipment{3,0,2,0,false});
        action(core::PlaceHQEquipment{3,0,0,2,false});
        const auto walk=core::hq_traffic(state());
        check(walk.access.entry==21 && walk.access.doors.size()==2 && walk.desks.front().route==std::vector{21,15,9,8,7},
            "A central hallway must lead to two real door openings and stay clear of furniture.");
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
        {
            const auto base_puzzle=core::hq_infrastructure_puzzle(state());
            check(base_puzzle.active_racks<=1 && base_puzzle.mood_bonus==0,
                  "Baseline single-or-zero uncrowded rack without overload must not grant artificial puzzle bonuses.");
            std::string rack_offer;
            for(const auto& offer:state().infrastructure->catalog.workplace->headquarters->equipment)
                if(offer.kind=="rack") {rack_offer=offer.id;break;}
            check(!rack_offer.empty(),"HQ catalog must define a rack equipment offer.");
            action(core::BuyHQEquipment{rack_offer});
            const auto r1_id=state().infrastructure->workplace->headquarters->items.back().id;
            action(core::BuyHQEquipment{rack_offer});
            const auto r2_id=state().infrastructure->workplace->headquarters->items.back().id;
            for(int wait=0;wait<14;++wait) {
                bool delivered=true;
                for(const auto& item:state().infrastructure->workplace->headquarters->items)
                    if((item.id==r1_id || item.id==r2_id) && item.delivered_on>state().date) delivered=false;
                if(delivered) break;
                apply(core::AdvanceDay{});
            }
            const auto place_next_open=[&](std::uint32_t item_id) {
                const auto& prem=core::current_headquarters(state());
                for(int y=0;y<prem.depth;++y) for(int x=0;x<prem.width;++x) {
                    const core::PlaceHQEquipment p{item_id,0,x,y,false};
                    if(core::hq_placement_blocker(state(),p).empty()) {
                        const auto pv=core::hq_infrastructure_puzzle(state(),&p);
                        action(p);
                        const auto after=core::hq_infrastructure_puzzle(state());
                        check(pv.score==after.score && pv.active_racks==after.active_racks &&
                              pv.total_watts==after.total_watts && pv.hall_isolated_racks==after.hall_isolated_racks &&
                              pv.thermal_crowding==after.thermal_crowding && pv.utility_proximity==after.utility_proximity &&
                              pv.power_margin==after.power_margin && pv.redundancy_bonus==after.redundancy_bonus &&
                              pv.capability_bonus==after.capability_bonus && pv.reliability_bonus==after.reliability_bonus &&
                              pv.mood_bonus==after.mood_bonus && pv.summary==after.summary,
                              "hq_infrastructure_puzzle preview and committed placement state must match identically.");
                        return true;
                    }
                }
                return false;
            };
            check(place_next_open(r1_id) && place_next_open(r2_id),
                  "Starter/owned HQ must place both test racks to evaluate multi-rack puzzle scoring.");
            const auto multi=core::hq_infrastructure_puzzle(state());
            check(multi.active_racks>=2 && multi.total_watts>0 && multi.score>=0 && multi.score<=100,
                  "Multi-rack HQ infrastructure puzzle must track racks, watts and bounded score.");
            if(state().infrastructure->workplace && !state().infrastructure->catalog.hardware.empty() &&
               !state().infrastructure->catalog.states.empty()) {
                auto bp_sim=state();
                const auto& icat=bp_sim.infrastructure->catalog;
                bool tested_bp=false;
                for(const auto& hw:icat.hardware) {
                    if(!hw.requires_research.empty() || !core::frontier_technology_block(bp_sim,core::FrontierTechnology::hardware,hw.id,bp_sim.date).empty()) continue;
                    const auto* chosen_state=&icat.states.front();
                    bool found_state=false;
                    for(const auto& st:icat.states) {
                        if(st.connectivity_class>=hw.connectivity_class) {chosen_state=&st;found_state=true;break;}
                    }
                    if(!found_state) continue;
                    const auto max_kw_watts=(icat.maximum_site_watts/1000ULL)*1000ULL;
                    const auto min_kw_watts=((icat.minimum_site_watts+999ULL)/1000ULL)*1000ULL;
                    const auto target_watts=std::clamp(((hw.active_it_watts*2ULL+999ULL)/1000ULL)*1000ULL,min_kw_watts,max_kw_watts);
                    const auto ops=std::clamp<std::uint32_t>(hw.operators,1U,icat.maximum_operators_per_site);
                    bp_sim.infrastructure->workplace->blueprints.clear();
                    bp_sim.infrastructure->workplace->blueprints.push_back(core::FacilityBlueprint{
                        "bp.puzzle","Puzzle Pod",hw.id,target_watts,ops,false});
                    try {
                        const auto bq=core::preview_blueprint_build(bp_sim,core::BuildFromBlueprint{"bp.puzzle","bp.test","Puzzle Site",chosen_state->id});
                        check(bq.network_score>=0 && bq.network_score<=100 && bq.cooling_score>=0 && bq.cooling_score<=100 &&
                              bq.redundancy_score>=0 && bq.redundancy_score<=100 && bq.puzzle_score>=0 && bq.puzzle_score<=100,
                              "BlueprintQuote must expose bounded 0..100 network, cooling, redundancy and puzzle scores.");
                        tested_bp=true;
                        break;
                    } catch(const std::invalid_argument&) {
                    }
                }
                check(tested_bp || !bp_sim.infrastructure->workplace->blueprints.empty(),
                      "BlueprintQuote puzzle check should run cleanly when a compatible state/hardware pair exists.");
            }
        }
        std::cout<<"Empty garage, paid delivery, placement/collision, assignments, physical compute/electricity, city leases, bids, construction risks, ownership taxes and fresh reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
