#include "campaign_worker.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        using namespace pgp;namespace fs=std::filesystem;
        const auto check=[](bool okay,const char* why){if(!okay) throw std::runtime_error(why);};
        check(bridge::CampaignController::staticMetaObject.indexOfMethod("resetLocalData()")>=0,"Release playtest builds must include the reset API.");
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_RESET_SOURCE,content::maximum_document_bytes),registry),registry);
        QTemporaryDir scratch;
        const auto root=fs::path{scratch.path().toStdString()};
        const auto paths=persistence::local_paths(root/"game");
        fs::create_directories(paths.saves_directory/"continuing");
        std::ofstream{paths.saves_directory/"continuing/current.pgpsave"}<<"incompatible development save";
        std::ofstream{paths.settings_file}<<"test settings";
        fs::create_directories(root/"game/world-navigation");
        std::ofstream{root/"game/world-navigation/draft.json"}<<"test draft";
        std::ofstream{root/"game/founding-draft.json"}<<"old setup";
        fs::create_directories(root/"outside");
        std::ofstream{root/"outside/keep.txt"}<<"unrelated data";
        fs::create_directory_symlink(root/"outside",paths.saves_directory/"outside-link");
        const auto initial=runtime::begin_scenario(program,"scenario.first_playable","US-CA");
        bridge::CampaignWorker worker{program,initial,paths,{},nullptr,true};
        bridge::CampaignSnapshotPtr snapshot;bool accepted=false;
        QObject::connect(&worker,&bridge::CampaignWorker::snapshotReady,[&](auto value){snapshot=std::move(value);});
        QObject::connect(&worker,&bridge::CampaignWorker::operationCompleted,[&](qulonglong,bool value,QString){accepted=value;});
        worker.start();check(snapshot && snapshot->storage_blocked,"Fixture must reproduce an unreadable save.");
        worker.resetLocalData(1);
        check(accepted && !snapshot->storage_blocked && !snapshot->current_campaign,"Reset must recover from a broken save.");
        check(!fs::exists(paths.saves_directory/"continuing") && !fs::exists(paths.settings_file) &&
            !fs::exists(root/"game/world-navigation") && !fs::exists(root/"game/founding-draft.json"),"Reset must delete the actual local files.");
        check(fs::exists(root/"outside/keep.txt"),"Reset must not follow links into unrelated data.");
        runtime::ScenarioOptions options;
        content::FoundingSelection selected;selected.family="rich";selected.route="bootstrap";selected.office="home";
        const auto& catalog=*std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data).founding;
        for(const auto& chapter:catalog.scenes) selected.choices.emplace(chapter.id,chapter.choices.front().id);
        options.founding=selected;options.identity=runtime::CampaignIdentity{"Fresh lab","Alex Vale","they/them"};
        worker.newCampaign(2,"US-CA",true,"scenario.founder_campaign",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        check(accepted && snapshot->current_campaign && !snapshot->storage_blocked,"A fresh company must work immediately after reset.");
        std::cout<<"Development reset scope, broken-save recovery, fresh start and Release playtest availability passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
