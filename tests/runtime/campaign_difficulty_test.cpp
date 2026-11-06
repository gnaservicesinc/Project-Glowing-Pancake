#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class F> void refuses(F f) {
    try {f();} catch(const std::invalid_argument&) {return;}
    throw std::runtime_error("Invalid difficulty terms were accepted");
}
void apply(runtime::Runtime& game,core::CommandAction command) {
    const auto r=game.submit({game.current_state().world.simulation.revision,std::move(command)});
    if(!r.accepted()) throw std::runtime_error(r.explanation);
}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        auto collection=content::load_collection(persistence::read_bounded(PGP_DIFFICULTY_SOURCE,content::maximum_document_bytes),registry);
        const auto full=runtime::compile_collection(collection,registry);
        const auto original=runtime::begin_scenario(full,"scenario.world_competition","US-CA");
        const auto old=nlohmann::json::parse(persistence::encode_save(full,original));
        require(!old["checkpoint"]["world"].contains("difficulty") && !old["checkpoint"]["world"]["competition"]["setup"].contains("review_interval_days"),"Default callers must retain the legacy optional save shape");

        // A four-day fixture isolates real strategy scheduling without a campaign sweep.
        std::erase_if(collection.definitions,[](const auto& entry) {
            return std::holds_alternative<content::FlowDefinition>(entry.second.data) ||
                (std::holds_alternative<content::ScenarioDefinition>(entry.second.data) && entry.first!="scenario.world_competition");
        });
        auto& authored=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.world_competition").data);
        authored.flows.clear();authored.competition->domestic=1;authored.competition->international=0;authored.competition->heavyweights=0;
        const auto program=runtime::compile_collection(collection,registry);
        require(authored.difficulties.size()==3,"All three difficulties must be authored");
        std::optional<runtime::RivalObservation> observation;
        for(const auto& d:authored.difficulties) {
            runtime::ScenarioOptions options;options.difficulty=d.id;options.background="background.community_organizer";options.charter="charter.customer_partnership";
            auto checkpoint=runtime::begin_scenario(program,"scenario.world_competition","US-CA",{},options);
            const auto& world=checkpoint.world;const auto& s=world.simulation;
            const auto capital=core::mul_div_floor(static_cast<std::uint64_t>(authored.cash.value()-10000000),d.starting_cash_percent,100).quotient;
            require(s.cash==core::Cents{capital},"Difficulty and background must change actual opening contributed capital in the advertised order");
            require(s.lab->catalog==original.world.simulation.lab->catalog && s.random==original.world.simulation.random,"Difficulty cannot change research/outcome rules or opening world streams");
            require(world.competition->setup->review_interval==d.rival_review_interval && world.competition->setup->estimate_error==authored.competition->estimate_error,"Difficulty must change cadence while preserving bounded rival information");
            const auto& recipe=*std::find_if(s.lab->catalog.recipes.begin(),s.lab->catalog.recipes.end(),[](const auto& r){return r.id=="recipe.document_baseline";});
            core::TrainingRequest request{"preview",recipe.id,"",{s.lab->catalog.data.front().id},"lease.initial",core::MilliCUPerDay{1000},1,recipe.minimum_work};
            const auto before=checkpoint;
            const auto baseline=core::preview_training(*s.lab,request);
            const auto preview=runtime::preview_campaign_training(program,world,request);
            for(std::size_t axis=0;axis<core::lab_domain_count;++axis)
                require(preview.lower[axis]==core::BasisPoints{std::max(INT64_C(0),baseline.lower[axis].value()-d.forecast_padding.value())} &&
                    preview.upper[axis]==core::BasisPoints{std::min(INT64_C(10000),baseline.upper[axis].value()+d.forecast_padding.value())},"Player forecast padding must widen the existing estimate without narrowing it");
            require(checkpoint==before && preview.minimum_days==baseline.minimum_days,"Reading a difficulty forecast cannot consume RNG or change work/time");
            runtime::Runtime game{program,checkpoint};
            require(game.prepare_opening(s.revision).accepted(),"Difficulty opening must prepare");
            const auto offer=game.current_state().world.simulation.management->catalog.financing.front();
            const auto cash=game.current_state().world.simulation.cash;
            apply(game,core::TakeFinancing{offer.id});
            require(game.current_state().world.simulation.cash==core::checked_unit_add(cash,offer.proceeds) &&
                offer.proceeds==core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(authored.management->financing.front().proceeds.value()),d.financing_percent,100).quotient},"The accepted financing must pay exactly the difficulty-adjusted terms");
            const auto& first=game.current_state().world.competition->companies.front().planning->decisions.front().observation;
            if(observation) require(first==*observation,"Difficulty cannot give rivals different opening public information");else observation=first;
            for(int day=0;day<4;++day) apply(game,core::AdvanceDay{});
            const auto& rival=game.current_state().world.competition->companies.front();
            require(rival.planning->decisions.size()==(d.rival_review_interval==core::Days{3}?2U:1U),"Rivals must use the advertised strategic review cadence");
            const auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
            require(loaded.checkpoint==game.current_state(),"Difficulty, adjusted finance and rival continuation must reload exactly");
            runtime::Runtime resumed{program,loaded.checkpoint};
            apply(game,core::AdvanceDay{});apply(resumed,core::AdvanceDay{});
            require(game.current_state()==resumed.current_state(),"The next difficulty day must remain exact after reopening");
            auto forged=checkpoint;forged.world.difficulty="difficulty.missing";refuses([&]{runtime::validate_checkpoint(program,forged);});
            forged=checkpoint;forged.world.competition->setup->review_interval=core::Days{4};refuses([&]{runtime::validate_checkpoint(program,forged);});
            forged=checkpoint;forged.world.simulation.management->catalog.financing.front().proceeds=core::Cents{1};refuses([&]{runtime::validate_checkpoint(program,forged);});
        }
        runtime::ScenarioOptions hard;hard.difficulty="difficulty.demanding";
        auto introduction=runtime::begin_scenario(full,"scenario.first_playable","US-CA",{},hard);
        introduction.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime scripted{full,introduction};
        require(scripted.prepare_opening(scripted.current_state().world.simulation.revision).accepted(),"Scripted introduction must prepare");
        for(int day=0;day<4;++day) apply(scripted,core::AdvanceDay{});
        require(!scripted.current_state().world.competition->companies.front().attempted,"Scripted rival cannot spend before its advertised adjusted date");
        apply(scripted,core::AdvanceDay{});
        require(scripted.current_state().world.competition->companies.front().attempted,"Scripted rival must react on the actual difficulty-adjusted date");
        require(persistence::decode_save(full,persistence::encode_save(full,scripted.current_state())).checkpoint==scripted.current_state(),"Scripted difficulty must retain real dated training evidence");
        std::cout<<"Difficulty capital, paid financing, honest forecasts, rival cadence and save continuation passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
