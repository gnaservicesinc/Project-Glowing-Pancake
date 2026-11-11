#include <pgp/studio/snapshot.hpp>
#include "snapshot_internal.hpp"
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <map>
namespace {
using Json=nlohmann::json;
void usage() {
    std::cout<<"pgp-content-tool review --checkout ROOT --source data/drafts/NAME.json --version VERSION --summary TEXT [--target data/runtime/first-playable.json] [--game-version VERSION] [--plan .work/NAME.json]\n"
        <<"pgp-content-tool finalize --checkout ROOT --plan .work/NAME.json\npgp-content-tool check --checkout ROOT\npgp-content-tool recover --checkout ROOT\n"
        <<"Review is read-only except an explicitly selected local plan. Finalization updates reviewed internal content, versions and recovery metadata; it never commits, tags or publishes.\n";
}
}
int main(int argc,char** argv) {
    using namespace pgp;namespace fs=std::filesystem;
    if (argc<2 || std::string_view{argv[1]}=="--help") {usage();return argc<2?2:0;}
    try {
        const std::string command=argv[1];std::map<std::string,std::string> options;
        for (int i=2;i<argc;i+=2) {
            const std::string name=argv[i];
            if (!name.starts_with("--") || i+1>=argc || !options.emplace(name,argv[i+1]).second) throw std::invalid_argument("Use unique named options with values.");
        }
        const auto take=[&](const char* name,bool required=true) {
            const auto it=options.find(name);if (it==options.end()) {if (required) throw std::invalid_argument(std::string{"Missing option "}+name);return std::string{};}
            auto value=it->second;options.erase(it);return value;
        };
        const auto root=fs::absolute(fs::path{take("--checkout")});
        const auto ensure_empty=[&]{if (!options.empty()) throw std::invalid_argument("Unsupported option: "+options.begin()->first);};
        if (command=="recover") {ensure_empty();std::cout<<studio::recover_snapshot(root)<<'\n';return 0;}
        if (command=="check") {
            ensure_empty();const auto r=studio::check_snapshot(root);
            std::cout<<Json{{"generation",r.generation},{"content_version",r.content_version},{"collection_hash",r.collection_hash},
                {"required_game_version",r.required_game_version},{"game_version_requirement_resolved",r.game_version_requirement_resolved},{"release_ready",false}}.dump(2)<<'\n';return 0;
        }
        const auto plan_path=[&](std::string value) {
            const auto path=fs::path{value};
            if (path.has_root_path() || !path.generic_string().starts_with(".work/") || path.extension()!=".json") throw std::invalid_argument("Review plans use a new .work/NAME.json file in the selected checkout.");
            for (const auto& part:path) if (part=="." || part==".." || part.empty()) throw std::invalid_argument("Plan paths cannot traverse checkout parents.");
            auto current=root;for (const auto& part:path) {current/=part;if (fs::is_symlink(fs::symlink_status(current))) throw std::invalid_argument("Plan paths cannot follow symbolic links.");}
            return root/path;
        };
        if (command=="review") {
            studio::SnapshotRequest request;request.source=take("--source");request.content_version=take("--version",false);request.summary=take("--summary",false);
            if (const auto target=take("--target",false);!target.empty()) request.target=target;
            if (const auto game=take("--game-version",false);!game.empty()) request.game_version=game;
            const auto output=take("--plan",false);ensure_empty();const auto review=studio::review_snapshot(root,request);
            if (!output.empty()) persistence::atomic_write(plan_path(output),review.report_json,[](std::string_view){throw std::invalid_argument("Select a new review-plan filename.");},4U*1024U*1024U,false);
            std::cout<<review.report_json;return 0;
        }
        if (command=="finalize") {
            const auto selected=plan_path(take("--plan"));ensure_empty();
            // The public finalizer rebuilds the plan with the actual validator,
            // then compares its digest; editable plan text grants no write paths.
            // Review documents include before/after definition bodies. Their
            // event budget is larger than the compact state/history metadata.
            const auto plan=studio::detail::snapshot_parse(persistence::read_bounded(selected,4U*1024U*1024U),
                4U*1024U*1024U,800000);const auto& row=plan.at("request");
            studio::SnapshotRequest request;request.source=row.at("source").get<std::string>();request.target=row.at("target").get<std::string>();
            request.content_version=row.at("content_version").get<std::string>();request.summary=row.at("summary").get<std::string>();request.save_policy=row.at("save_policy").get<std::string>();
            if (!row.at("game_version").is_null()) request.game_version=row.at("game_version").get<std::string>();
            const auto result=studio::finalize_snapshot(root,request,plan.at("plan_id").get<std::string>());
            std::cout<<Json{{"generation",result.generation},{"content_version",result.content_version},{"collection_hash",result.collection_hash},
                {"required_game_version",result.required_game_version},{"game_version_requirement_resolved",result.game_version_requirement_resolved},{"release_ready",false}}.dump(2)<<'\n';return 0;
        }
        throw std::invalid_argument("Choose review, finalize, check or recover.");
    } catch (const persistence::PersistenceError& e) {std::cerr<<e.code()<<": "<<e.what()<<'\n';return 2;}
    catch (const std::exception& e) {std::cerr<<"Content snapshot: "<<e.what()<<'\n';return 2;}
}
