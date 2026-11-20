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
        auto& exchange = std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.world_competition").data);
        const auto& founder=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.founder_campaign").data);
        check(std::ranges::all_of(founder.lab->recipes,[](const auto& recipe){return recipe.sample_budget>0;}),
            "Founder recipes have authored sampling budgets");
        auto& scenario=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.first_playable").data);
        scenario.flows.clear();
        auto& recipe=*std::ranges::find(scenario.lab->recipes,"recipe.document_baseline",&core::TrainingRecipe::id);
        recipe.sample_budget=800001;
        exchange.flows.clear();
        exchange.lab->acquisition = founder.lab->acquisition;
        const auto program=runtime::compile_collection(collection,registry);
        {
            runtime::Runtime market{program,runtime::begin_scenario(program,"scenario.world_competition","US-CA")};
            check(market.prepare_opening(state(market).revision).accepted(),"Open paired company labs");
            const auto& initial = *state(market).lab;
            const core::LabAllocation resources{"lease.initial",core::MilliCUPerDay{10000},1};
            const std::string id = "collected.mix.1";
            const std::vector<std::string> inputs{"collected.source_original","collected.source_commission"};
            auto terms = core::collection_terms(initial,id,inputs,"Original examples");
            const auto free = core::collection_terms(initial,id,{"collected.source_social"},"Posts");
            check(free.dataset.price.value()==0 && free.review_work.value()>0 && !free.dataset.commercial_use,
                  "A public scrape takes work and retains use restrictions");
            const auto mixed = core::collection_terms(initial,id,{inputs.front(),"collected.source_social"},"Combined");
            check(!mixed.dataset.commercial_use && !mixed.dataset.open_weight_release,"Every contributing source must permit a use");
            apply(market,core::StartDataAcquisition{id,resources,inputs,"Original examples"});
            auto nextDay = [&] {
                const auto notices = state(market).management->notices;
                for (const auto& row : notices) if (row.required && !row.acknowledged) apply(market,core::AcknowledgeManagementNotice{row.id});
                apply(market,core::AdvanceDay{});
            };
            for (unsigned day=0; day<48 && !state(market).lab->acquisitions.front().output; ++day) nextDay();
            const auto& lab = *state(market).lab;
            check(core::owns_dataset_ip(lab,id) && core::lab_data(lab,id).quality == terms.dataset.quality,
                  "Settlement must preserve the weighted source result and IP assignment");
            const auto& world = market.current_state().world;
            const auto buyer = std::ranges::find_if(world.competition->companies,[](const auto& company) {
                return company.planning && company.planning->entered_on && company.simulation.accounts;
            })->definition_id;
            const core::LicenseCompanyDataset deal{world.company_id,buyer,id};
            const auto fee = runtime::dataset_license_quote(world,program,deal);
            const auto cash = state(market).cash;
            apply(market,deal);
            check(state(market).cash==core::checked_unit_add(cash,fee),"Seller must receive the quoted payment");
            refuse(market,deal);
            refuse(market,core::LicenseCompanyDataset{buyer,"company.player","trade.dataset.1"});
            const auto rival_offer = [&]() -> std::optional<core::LicenseCompanyDataset> {
                const auto& now = market.current_state().world;
                for (const auto& supplier : now.competition->companies) {
                    if (!supplier.simulation.lab) continue;
                    for (const auto& job : supplier.simulation.lab->acquisitions)
                        if (core::owns_dataset_ip(*supplier.simulation.lab,job.offer_id))
                            return core::LicenseCompanyDataset{supplier.definition_id,now.company_id,job.offer_id};
                }
                return {};
            };
            for (unsigned day=0; day<21 && !rival_offer(); ++day) nextDay();
            const auto from_rival = rival_offer();
            check(from_rival.has_value(),"A rival must collect original work and make it available to license");
            apply(market,*from_rival);
            const auto& purchase = state(market).lab->licenses.back();
            check(purchase.offer_id.starts_with("trade.dataset.") && !core::owns_dataset_ip(*state(market).lab,purchase.offer_id),
                  "A buyer may train on a rival's corpus and cannot resell it");
            check(persistence::decode_save(program,persistence::encode_save(program,market.checkpoint())).checkpoint==market.checkpoint(),
                  "Both labs, source provenance, licenses and payments must reopen identically");
        }
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
