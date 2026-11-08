#include "campaign_worker.hpp"
#include "../runtime/integration_fixture.hpp"
#include <pgp/persistence/sha256.hpp>
#include <QCoreApplication>
#include <chrono>
#include <iostream>

using namespace pgp;
namespace {
void check(bool value,const char* message){if(!value) throw std::runtime_error(message);}
void write(const std::filesystem::path& path,std::string_view bytes){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<bytes;if(!out) throw std::runtime_error("Fixture write failed");}
struct Observe {
    bridge::CampaignSnapshotPtr snapshot;bool accepted=false;QString review;
    explicit Observe(bridge::CampaignWorker& worker) {
        QObject::connect(&worker,&bridge::CampaignWorker::snapshotReady,&worker,[this](auto value){snapshot=std::move(value);});
        QObject::connect(&worker,&bridge::CampaignWorker::operationCompleted,&worker,[this](auto,bool value,auto){accepted=value;});
        QObject::connect(&worker,&bridge::CampaignWorker::originalContentReviewReady,&worker,[this](auto value){review=value;});
    }
};
}
int main(int argc,char** argv) {
    QCoreApplication app{argc,argv};
    const auto root=std::filesystem::path{PGP_RECOVERY_SCRATCH}/("recovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto original=integration_fixture::program(PGP_RECOVERY_SOURCE,false);
        auto newer_collection=original.collection;newer_collection.content_version="0.0.2-dev";
        const auto registry=content::foundation_registry();const auto installed=runtime::compile_collection(newer_collection,registry);
        auto initial=runtime::begin_scenario(original,"scenario.foundation","US-CA");initial.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime live{original,initial};check(live.prepare_opening(initial.world.simulation.revision).accepted(),"Opening");
        const auto paths=persistence::local_paths(root);persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
        current.commit(original,live.checkpoint());
        check(live.submit({live.checkpoint().world.simulation.revision,core::AdvanceDay{}}).accepted(),"Recorded day");current.commit(original,live.checkpoint(),true);
        const auto exact=live.checkpoint();const auto save=persistence::read_bounded(current.path());
        const auto journal_path=current.path().parent_path()/"current-days.json";const auto journal=persistence::read_bounded(journal_path);
        const auto packet=current.content().packet(persistence::sha256(original.canonical_content_bytes));
        {
            bridge::CampaignWorker worker{installed,initial,paths,{},nullptr,true};Observe observed{worker};worker.start();
            check(observed.snapshot->current_campaign && observed.snapshot->checkpoint==exact && observed.snapshot->content->canonical_content_bytes==original.canonical_content_bytes,"Startup adopts original source and exact continuing state");
        }
        std::filesystem::remove(packet);write(root/"original.json",original.canonical_content_bytes);
        {
            bridge::CampaignWorker worker{installed,initial,paths,{},nullptr,true};Observe observed{worker};worker.start();
            check(observed.snapshot->storage_blocked && !observed.snapshot->current_campaign && !observed.snapshot->missing_content_hash.empty(),"Missing source has an explicit recovery state");
            worker.reviewOriginalContent(1,{});check(!observed.accepted,"Installed different source must fail exact validation");
            worker.reviewOriginalContent(2,root/"original.json");check(observed.accepted && !observed.review.isEmpty(),"Matching source produces review");
            check(!std::filesystem::exists(packet) && save==persistence::read_bounded(current.path()) && journal==persistence::read_bounded(journal_path),"Review makes no writes");
            worker.cancelOriginalContent();worker.confirmOriginalContent(3);check(!observed.accepted,"Cancelled review cannot restore");
            worker.reviewOriginalContent(4,root/"original.json");write(journal_path,journal+" ");worker.confirmOriginalContent(5);
            check(!observed.accepted && !std::filesystem::exists(packet),"Stale review must not replace original content");write(journal_path,journal);
            worker.reviewOriginalContent(6,root/"original.json");worker.confirmOriginalContent(7);
            check(observed.accepted && observed.snapshot->current_campaign && !observed.snapshot->storage_blocked && observed.snapshot->missing_content_hash.empty() && observed.snapshot->checkpoint==exact,"Confirmed restoration adopts exact saved state and clears block");
            check(save==persistence::read_bounded(current.path()) && journal==persistence::read_bounded(journal_path),"Source restoration leaves current save and journal in place");
            worker.submitCommand(8,{exact.world.simulation.revision,core::ReserveFunds{"Recovered plan",core::Cents{10},true}});
            check(observed.accepted && !observed.snapshot->storage_blocked,"Recovered company accepts and secures actual commands");
            check(persistence::load_file(original,current.path()).checkpoint==observed.snapshot->checkpoint,"Next command still saves with original source");
            worker.saveTo(9,root/"manual.pgpsave");check(observed.accepted,"Export recovered company");
        }
        // Manual recovery validates a point-in-time save before preserving an existing continuing folder.
        const auto manual_root=root/"manual-root";const auto manual_paths=persistence::local_paths(manual_root);
        std::filesystem::create_directories(manual_paths.saves_directory/"continuing");
        write(manual_paths.saves_directory/"continuing/current.pgpsave","damaged prior company");
        {
            bridge::CampaignWorker worker{installed,initial,manual_paths,{},nullptr,true};Observe observed{worker};worker.start();
            worker.loadFrom(10,root/"manual.pgpsave");check(!observed.accepted && !observed.snapshot->missing_content_hash.empty(),"Manual import can request original source");
            worker.reviewOriginalContent(11,root/"original.json");check(observed.accepted,"Manual source review");worker.confirmOriginalContent(12);
            check(observed.accepted && observed.snapshot->current_campaign && observed.snapshot->checkpoint==persistence::load_file(original,root/"manual.pgpsave").checkpoint,"Manual recovery retains the exact imported point");
            bool prior_preserved=false;
            for(const auto& entry:std::filesystem::directory_iterator(manual_paths.saves_directory))
                if(entry.path().filename().string().starts_with("continuing-preserved-")) prior_preserved=persistence::read_bounded(entry.path()/"current.pgpsave")=="damaged prior company";
            check(prior_preserved,"Manual recovery retains every byte of the prior continuing folder");
        }
        std::filesystem::remove_all(root);std::cout<<"Automatic original rules, read-only review, stale/cancel refusal and manual recovery passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
