#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class F> void refuses(F f) {
    try {f();} catch(const std::invalid_argument&) {return;}
    throw std::runtime_error("Invalid founding terms were accepted");
}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto r=game.submit({game.current_state().world.simulation.revision,std::move(action)});
    if(!r.accepted()) throw std::runtime_error(r.explanation);
}
const core::GovernanceState& board(const runtime::Runtime& game) {return *game.current_state().world.simulation.management->governance;}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_FOUNDING_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.definitions.at("scenario.century_frontier").data);
        const auto original=runtime::begin_scenario(program,"scenario.century_frontier","US-CA");
        require(scenario.backgrounds.size()==4 && scenario.management->governance->charters.size()==3,"Four backgrounds and three amendable charters must be authored");
        runtime::ScenarioOptions options;options.charter="charter.independent_lab";
        for(const auto& background:scenario.backgrounds) {
            options.background=background.id;
            const auto checkpoint=runtime::begin_scenario(program,"scenario.century_frontier","US-CA",{},options);
            const auto& s=checkpoint.world.simulation;
            require(s.cash==core::checked_unit_add(original.world.simulation.cash,background.cash_adjustment),"Background must change the actual contributed cash");
            require(core::opening_staff_count(*s.capacity)==core::checked_add(scenario.team_people,background.staff_adjustment),"Background must change actual available founding staff");
            require(s.random==original.world.simulation.random && checkpoint.world.competition==original.world.competition,"Founder choices cannot consume world RNG or grant packages to rivals");
            require(s.business_offers->catalog.size()==scenario.business_offers.size()+background.contacts.size(),"Contacts must be real ordinary business offers");
            require(persistence::decode_save(program,persistence::encode_save(program,checkpoint)).checkpoint==checkpoint,"Founding package must roundtrip exactly");
            auto forged=checkpoint;forged.world.background.clear();
            refuses([&]{runtime::validate_checkpoint(program,forged);});
        }
        options.background="background.product_builder";
        auto checkpoint=runtime::begin_scenario(program,"scenario.century_frontier","US-CA",{},options);
        checkpoint.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime game{program,checkpoint};
        require(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Founding morning must prepare");
        apply(game,core::ProposePoolExpansion{core::BasisPoints{1000}});
        apply(game,core::VoteBoardMotion{board(game).motions.back().id,true});
        require(board(game).motions.back().status==core::BoardMotionStatus::rejected,"Independent charter must make nonplayer seats oppose excess dilution");
        const auto rejected=board(game).motions.back();
        apply(game,core::ProposeCharter{"charter.customer_partnership"});
        const auto pending=game.current_state();
        const auto refused=game.submit({pending.world.simulation.revision,core::ProposeCharter{"charter.shared_research"}});
        require(!refused.accepted() && game.current_state()==pending,"A second concurrent amendment must leave the pending motion untouched");
        const auto restored=persistence::decode_save(program,persistence::encode_save(program,pending));
        require(restored.checkpoint==pending,"Pending charter review must reload exactly");
        apply(game,core::VoteBoardMotion{board(game).motions.back().id,true});
        require(board(game).charters.size()==2 && core::active_charter(game.current_state().world.simulation)->id=="charter.customer_partnership","Board approval must amend the actual charter");
        require(board(game).motions.front()==rejected && board(game).motions.front().charter_cursor==1,"Amendment must preserve earlier votes under their actual charter");
        const auto& bills=game.current_state().world.simulation.accounts->recurring;
        const auto old=std::find_if(bills.begin(),bills.end(),[](const auto& r){return r.id=="charter/employees/1";});
        const auto current=std::find_if(bills.begin(),bills.end(),[](const auto& r){return r.id=="charter/employees/2";});
        require(old!=bills.end() && old->ends_before==game.current_state().world.simulation.date && current!=bills.end() && !current->ends_before && current->monthly_amount==core::Cents{250000},"Amendment must end the old employee program and fund the new monthly commitment");
        apply(game,core::ProposePoolExpansion{core::BasisPoints{1000}});
        apply(game,core::VoteBoardMotion{board(game).motions.back().id,true});
        require(board(game).motions.back().status==core::BoardMotionStatus::approved,"New charter must change subsequent board voting");
        auto forged=game.current_state();forged.world.simulation.management->governance->motions.front().charter_cursor=2;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=game.current_state();forged.world.simulation.management->governance->charters.back().motion.clear();
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        const auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
        require(loaded.checkpoint==game.current_state(),"Executed charter and prior ballots must reload exactly");
        runtime::Runtime resumed{program,loaded.checkpoint};
        for(auto* live:{&game,&resumed}) apply(*live,core::AdvanceDay{});
        require(game.current_state()==resumed.current_state(),"The next funded day must agree after reopening");
        options.background="background.missing";refuses([&]{(void)runtime::begin_scenario(program,"scenario.century_frontier","US-CA",{},options);});
        options.background="background.researcher";options.charter="charter.missing";refuses([&]{(void)runtime::begin_scenario(program,"scenario.century_frontier","US-CA",{},options);});
        std::cout<<"Founding packages, charter ballots, monthly programs and reload continuation passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
