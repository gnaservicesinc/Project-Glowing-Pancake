#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
using namespace pgp;
void check(bool okay,const char* why) {if(!okay)throw std::runtime_error(why);}
const auto& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted())throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,core::CommandAction action) {
    const auto before=game.current_state();
    check(!game.submit({state(game).revision,std::move(action)}).accepted() && game.current_state()==before,
        "Invalid mixture must refuse atomically");
}
}

int main() {
    try {
        using namespace pgp;
        const auto registry=content::foundation_registry();
        auto collection=content::load_collection(persistence::read_bounded(PGP_DATA_MIX_SOURCE,content::maximum_document_bytes),registry);
        const auto& founder=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.founder_campaign").data);
        check(std::ranges::all_of(founder.lab->recipes,[](const auto& recipe){return recipe.sample_budget>0;}),
            "Founder recipes have authored sampling budgets");
        auto& scenario=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.first_playable").data);
        scenario.flows.clear();
        auto& recipe=*std::ranges::find(scenario.lab->recipes,"recipe.document_baseline",&core::TrainingRecipe::id);
        recipe.sample_budget=800001;
        const auto program=runtime::compile_collection(collection,registry);
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
        check(game.prepare_opening(state(game).revision).accepted(),"Prepare opening");

        const std::string documents="data.licensed_documents", scans="data.consented_scans";
        core::TrainingRequest document_mix{"model.docs",recipe.id,"",{documents,scans},"lease.initial",core::MilliCUPerDay{60000},4,core::MilliCU{3600000}};
        document_mix.data_weights={{documents,3},{scans,1}};
        auto scan_mix=document_mix;
        scan_mix.model_id="model.scans";
        scan_mix.data_weights={{documents,1},{scans,3}};
        const auto before=game.current_state();
        const auto doc_samples=core::preview_data_mix(*state(game).lab,document_mix);
        const auto scan_samples=core::preview_data_mix(*state(game).lab,scan_mix);
        check(game.current_state()==before,"Sample preview is read-only");
        const auto& doc_slice=*std::ranges::find(doc_samples.sources,documents,&core::DataMixSlice::id);
        const auto& scan_slice=*std::ranges::find(scan_samples.sources,scans,&core::DataMixSlice::id);
        check(doc_slice.planned==600001 && scan_slice.planned==600001 && scan_slice.retained==500000,
            "Whole sample slots round deterministically and shortages do not create duplicate data");
        check(scan_samples.retained==700000,"Unfilled slots are counted");
        const auto allocations=std::accumulate(scan_samples.sources.begin(),scan_samples.sources.end(),UINT64_C(0),
            [](std::uint64_t sum,const auto& row){return sum+row.planned;});
        check(allocations==scan_samples.budget,"All planned slots reconcile to the budget");
        check(doc_samples.effective_units[0]>scan_samples.effective_units[0] &&
            scan_samples.effective_units[1]>doc_samples.effective_units[1],"Different emphasis changes the useful domain coverage");
        refuse(game,core::StartTraining{document_mix});
        apply(game,core::BuyData{documents});
        apply(game,core::BuyData{scans});
        auto invalid=document_mix;
        invalid.data_weights[documents]=0;
        refuse(game,core::StartTraining{invalid});
        invalid=document_mix;
        invalid.data_weights.erase(scans);
        refuse(game,core::StartTraining{invalid});

        const auto evaluate_mix=[&](const core::TrainingRequest& request) {
            const auto forecast=core::preview_training(*state(game).lab,request);
            apply(game,core::StartTraining{request});
            for(int day=0;day<60;++day)apply(game,core::AdvanceDay{});
            const auto& model=state(game).lab->models.back();
            check(model.consumed_work==request.work && model.data_weights==request.data_weights,"Training commits the previewed mix and real work");
            for(std::size_t i=0;i<core::introductory_lab_domains;++i)
                check(forecast.lower[i]<=model.capability[i] && model.capability[i]<=forecast.upper[i],"Outcome lies inside the matching training forecast");
            return model.capability;
        };
        const auto doc_outcome=evaluate_mix(document_mix);
        const auto scan_outcome=evaluate_mix(scan_mix);
        check(doc_outcome[0]>scan_outcome[0] && scan_outcome[1]>doc_outcome[1],"Identical compute with different mixes changes actual capability");

        apply(game,core::CloneModel{"model.scans","checkpoint.scans"});
        auto continuation=scan_mix;
        continuation.model_id="model.scans.followup";
        continuation.parent_id="checkpoint.scans";
        refuse(game,core::StartTraining{continuation});
        continuation.data_weights.clear();
        continuation.data_ids.clear();
        check(core::preview_data_mix(*state(game).lab,continuation).retained==scan_samples.retained,"Checkpoint preview inherits its saved balance");
        apply(game,core::StartTraining{continuation});
        check(state(game).lab->models.back().data_weights==scan_mix.data_weights,"Checkpoint training retains original weights");
        const auto reloaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint;
        check(reloaded==game.current_state(),"Training and checkpoint balances save exactly");
        auto forged=reloaded;
        forged.world.simulation.lab->models.back().data_weights=document_mix.data_weights;
        bool rejected=false;
        try{runtime::validate_checkpoint(program,forged);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Checkpoint validation rejects a changed lineage mixture");
        std::cout<<"Dataset allocation, shortages, rights, domain tradeoffs, training outcomes and checkpoint replay passed.\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
