#include "life_report.hpp"
#include "objectives_controller.hpp"
#include "../../src/bridge/founding_fixture.hpp"
#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

namespace {
void check(bool okay,const char* message) {if(!okay) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        using namespace pgp;
        auto document=nlohmann::json::parse(persistence::read_bounded(PGP_LIFE_SOURCE,content::maximum_document_bytes));
        const auto registry=content::foundation_registry();
        const auto installed=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        const auto& founder=std::get<content::ScenarioDefinition>(installed.collection.find("scenario.founder_campaign")->data);
        check(founder.ends_before==core::to_ordinal({2097,1,1}) && founder.objectives->retrospective,
            "The founder campaign must end at age 100 with a retrospective.");
        QTemporaryDir temporary;
        bridge::FoundingController form{installed.collection,std::filesystem::path{temporary.path().toStdString()}/"founding.json"};
        form.setField("family","rich");
        form.setField("route","bootstrap");
        bridge::complete_formative_fixture(form);
        const auto selection=form.selection();
        runtime::ScenarioOptions options;
        options.founding=selection;
        options.identity=runtime::CampaignIdentity{"Cedar Works","Alex Vale","they/them"};
        auto fresh=runtime::begin_scenario(installed,"scenario.founder_campaign","US-CA",{},options);
        const auto untouched=fresh;
        const auto live=bridge::lifeReport(fresh,installed);
        check(!live.value("obituary").toBool() && live.value("headline").toString().contains("life in progress") && fresh==untouched,
            "A live report must preserve the simulation and describe the founder as alive.");

        auto& scenario=pgp_test::definition(document,"scenario.century_frontier");
        scenario["starts_on"]="2113-12-29";
        scenario.erase("backgrounds");
        scenario.erase("resilience");
        scenario.erase("frontier");
        scenario["flows"]=nlohmann::json::array();
        scenario["business_offers"]=nlohmann::json::array();
        scenario["management"]["setbacks"]=nlohmann::json::array();
        scenario["management"]["succession"]["founder_birth_year"]=2075;
        scenario["objectives"]["retrospective"]=true;
        auto society=pgp_test::definition(document,"society.century");
        society["id"]="society.fixture_life";
        society["catalog"]["forum"]["first_agenda_day"]=core::to_ordinal({2113,12,29}).value();
        scenario["society"]="society.fixture_life";
        scenario["competition"]["domestic"]=1;
        scenario["competition"]["international"]=0;
        scenario["competition"]["heavyweights"]=0;
        auto& rivals=scenario["rivals"];
        rivals.erase(std::remove_if(rivals.begin(),rivals.end(),[](const auto& row){
            return row["id"]!="company.century.frontier";
        }),rivals.end());
        rivals.front()["start_after_days"]=0;
        rivals.front()["planning"]["slot"]=1;
        document["definitions"].push_back(std::move(society));
        auto& definitions=document["definitions"];
        definitions.erase(std::remove_if(definitions.begin(),definitions.end(),[](const auto& row){
            return row["kind"]=="scenario" && row["id"]!="scenario.century_frontier";
        }),definitions.end());
        pgp_test::prune_unused_flows(document);
        const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.century_frontier","US-WA",
            core::ObjectiveSelection{core::ObjectiveKind::utopia,{}})};
        auto result=game.prepare_opening(game.current_state().world.simulation.revision);
        check(result.accepted(),"The retrospective fixture must prepare.");
        const auto act=[&](core::CommandAction action) {
            const auto receipt=game.submit({game.current_state().world.simulation.revision,std::move(action)});
            if(!receipt.accepted()) throw std::runtime_error(receipt.explanation);
        };
        for(int day=0;day<3;++day) {
            const auto& state=game.current_state().world.simulation;
            if(const auto warning=core::liquidity_report(*state.liquidity).pending_warning) act(core::AcknowledgeLiquidityWarning{*warning});
            while(const auto* notice=core::required_succession_notice(game.current_state().world.simulation))
                act(core::AcknowledgeSuccession{notice->id});
            act(core::AdvanceDay{});
        }
        const auto& ended=game.current_state();
        check(ended.world.objectives->end && ended.world.objectives->end->cause==runtime::CampaignEndCause::deadline &&
            !runtime::required_objective_review(ended.world) && !runtime::objective_postmortem_candidate(ended.world),
            "World conditions must reach the retrospective without a failed-objective review.");
        for(const auto kind:core::objective_kinds)
            check(runtime::objective_result(ended.world,kind)==runtime::ObjectiveResult::unavailable,
                "The retrospective must never award a player victory or defeat.");
        check(persistence::decode_save(program,persistence::encode_save(program,ended)).checkpoint==ended,
            "The completed record must reload exactly.");
        auto snapshot=std::make_shared<bridge::CampaignSnapshot>();
        snapshot->checkpoint=ended;
        bridge::ObjectivesController controller;
        controller.update(snapshot,program,true);
        check(controller.retrospective() && controller.rows().empty() && !controller.editable() &&
            !controller.canContinue() && controller.breachText().isEmpty() && !controller.report().isEmpty(),
            "The public controller must expose a newspaper instead of score choices and results.");

        // Presentation checks use an explicitly constructed terminal biography.
        auto obituary=fresh;
        auto& end=obituary.world.objectives->end.emplace();
        end.day=founder.ends_before;
        end.cause=runtime::CampaignEndCause::deadline;
        const auto newspaper=bridge::lifeReport(obituary,installed);
        check(newspaper.value("obituary").toBool() && newspaper.value("headline").toString()=="Alex Vale dies at 100",
            "The obituary must use the founder's actual birth date and name.");
        end.day=core::to_ordinal({2016,1,1});
        end.cause=runtime::CampaignEndCause::insolvency;
        check(!bridge::lifeReport(obituary,installed).value("obituary").toBool(),
            "A company closure must never fabricate the founder's death.");
        end.cause=runtime::CampaignEndCause::retirement;
        const auto voluntary=bridge::lifeReport(obituary,installed);
        check(!voluntary.value("obituary").toBool() && voluntary.value("headline").toString()=="Alex Vale looks back at 19",
            "Voluntarily ending after resignation must report the current age without an obituary.");
        std::cout<<"Retrospective deadline, absence of scores/reviews, real terminal save and dated newspaper checks passed.\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
