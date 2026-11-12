#include <pgp/studio/snapshot.hpp>
#include <pgp/studio/draft.hpp>
#include <pgp/studio/library.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/persistence/storage.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;namespace fs=std::filesystem;using Json=nlohmann::json;
int checks=0;
void check(bool yes,const char* why) {++checks;if (!yes) throw std::runtime_error(why);}
void put(const fs::path& path,std::string_view bytes) {fs::create_directories(path.parent_path());std::ofstream out{path,std::ios::binary};out<<bytes;out.close();if (!out) throw std::runtime_error("Cannot write snapshot fixture");}
std::string read(const fs::path& path) {return persistence::read_bounded(path,4U*1024U*1024U);}
struct Scratch {
    fs::path root=fs::path{PGP_SNAPSHOT_SCRATCH}/("snapshot-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    explicit Scratch(bool small_fixture=false) {
        fs::create_directories(root/".git");put(root/"CMakeLists.txt","# fixture\n");put(root/"src/content/collection.cpp","// fixture\n");
        put(root/"project-versions.json",read(PGP_VERSION_SOURCE));put(root/"data/runtime/first-playable.json",read(small_fixture ? PGP_DIRECT_SAVE_SOURCE : PGP_SNAPSHOT_SOURCE));
        // Tests always begin from an unfinalized fixture, even once the owning
        // checkout's content component has its first reviewed development version.
        auto versions=Json::parse(read(root/"project-versions.json"));versions["components"]["content"]="0.0.0-dev";put(root/"project-versions.json",versions.dump(2));
        auto doc=Json::parse(read(root/"data/runtime/first-playable.json"));doc["content_version"]="0.0.0-dev";
        doc.erase("asset_records"); // The synthetic checkout has no imported assets.
        for (auto& definition:doc.at("definitions")) definition["revision"]=1;
        put(root/"data/runtime/first-playable.json",doc.dump(2));
    }
    ~Scratch(){std::error_code error;fs::remove_all(root,error);}
};
studio::SnapshotRequest initial() {return {"data/runtime/first-playable.json","data/runtime/first-playable.json","0.1.0-dev.1","Reviewed internal slice.","reject-prior-content",{}};}
template<class Action> void refused(Action action,std::string_view code) {
    try {action();}catch(const persistence::PersistenceError& e){check(e.code()==code,e.what());return;}throw std::runtime_error("Expected snapshot refusal "+std::string(code));
}
std::map<std::string,std::string> tracked(const fs::path& root) {
    std::map<std::string,std::string> result;
    for (const auto& e:fs::recursive_directory_iterator(root)) if (fs::is_regular_file(e.symlink_status())) {
        const auto path=e.path().lexically_relative(root).generic_string();if (path.starts_with(".pgp-editor/") || path.starts_with(".git/")) continue;result[path]=read(e.path());
    }
    return result;
}
Json state(const fs::path& root) {return Json::parse(read(root/"authoring/content-state.json"));}
void first_snapshot(Scratch& scratch) {
    const auto request=initial();const auto before=tracked(scratch.root);const auto review=studio::review_snapshot(scratch.root,request);
    check(tracked(scratch.root)==before,"Read-only review changed tracked content or versions");
    const auto report=Json::parse(review.report_json);
    const auto source=Json::parse(read(scratch.root/"data/runtime/first-playable.json"));std::size_t saves=0;
    for (const auto& definition:source.at("definitions")) if (definition.at("kind")=="scenario") saves+=definition.at("headquarters_choices").size();
    check(review.runtime_changed && report["validation"]["exact_prepared_save_roundtrips"]==saves && report["changes"].size()==source.at("definitions").size(),"First review omitted shared-runtime/save evidence or baseline definitions");
    const auto result=studio::finalize_snapshot(scratch.root,request,review.plan_id);
    check(result.generation==1 && result.content_version=="0.1.0-dev.1" && !result.game_version_requirement_resolved,"Internal snapshot claimed game-version/release readiness");
    const auto root=Json::parse(read(scratch.root/"project-versions.json"));
    check(root["components"]["game"]=="0.0.0-dev" && root["components"]["editor"]=="0.0.0-dev" && root["components"]["content"]==result.content_version,"Content finalization silently bumped another product");
    check(studio::check_snapshot(scratch.root).collection_hash==result.collection_hash && !fs::exists(scratch.root/".pgp-editor/snapshot-transaction"),"Committed snapshot failed consistency or left a pending journal");
    const auto library=studio::inspect_checkout(scratch.root);
    check(library.documents.size()==1 && library.documents.front().validation==studio::Validation::validated,"Runtime metadata was mistaken for an editable content collection");
}
void changes_and_versions() {
    Scratch scratch;first_snapshot(scratch);
    studio::DraftDocument draft{scratch.root,"data/runtime/first-playable.json"};auto collection=draft.collection();
    auto& flow=std::get<content::FlowDefinition>(collection.definitions.at("slice.evaluation_finding").data);
    const auto text_key=flow.nodes.at(flow.entry_node).text_key;
    check(!text_key.empty(),"Expected a presentation entry in the slice fixture");
    collection.messages[text_key]+=" Reviewed wording.";draft.replace(collection);draft.save("wording.json");
    studio::SnapshotRequest request{"data/drafts/wording.json","data/runtime/first-playable.json","0.1.1-dev.1","Clarify the subgroup explanation.","reject-prior-content",{}};
    const auto preview=studio::review_snapshot(scratch.root,request);const auto report=Json::parse(preview.report_json);
    check(report["changes"].size()==1 && report["changes"][0]["id"]=="slice.evaluation_finding" && report["changes"][0]["revision"]==2,"Visible wording did not increment exactly its referencing definition once");
    const auto registry=content::foundation_registry();
    const auto old_program=runtime::compile_collection(content::load_collection(read(scratch.root/request.target),registry),registry);
    runtime::Runtime old_game{old_program,runtime::begin_scenario(old_program,"scenario.first_playable","US-CA")};
    check(old_game.prepare_opening(old_game.current_state().world.simulation.revision).accepted(),"Old-content opening fixture failed");
    const auto old_save=persistence::encode_save(old_program,old_game.checkpoint());
    const auto draft_bytes=read(scratch.root/request.source);
    (void)studio::finalize_snapshot(scratch.root,request,preview.plan_id);
    check(read(scratch.root/request.source)==draft_bytes,"Finalization overwrote or adopted the separately saved draft");
    const auto next_program=runtime::compile_collection(content::load_collection(read(scratch.root/request.target),registry),registry);
    refused([&]{(void)persistence::decode_save(next_program,old_save);},"content_identity");
    auto runtime=Json::parse(read(scratch.root/request.target));
    for (const auto& row:runtime["definitions"]) check(row["revision"]==(row["id"]=="slice.evaluation_finding"?2:1),"Unchanged definition revision was bumped");
    const auto history=Json::parse(read(scratch.root/"authoring/content-history.json"));
    check(history["batches"].back()["minimum_game_change"]=="patch" && history["batches"].back()["changed_text_keys"].size()==1,"Text patch lost its release requirement or text identity");
    const auto reviewed_changes=Json::parse(preview.report_json).at("changes");
    const auto& recorded=history["batches"].back().at("changed_definitions");
    check(recorded.size()==reviewed_changes.size() && !recorded.front().contains("before") &&
        recorded.front().at("before_hash")==persistence::sha256(reviewed_changes.front().at("before").dump()) &&
        recorded.front().at("after_hash")==persistence::sha256(reviewed_changes.front().at("after").dump()) &&
        history["batches"].back().at("previous_snapshot")==history["batches"].front().at("snapshot"),
        "Snapshot history duplicated definition bodies or lost its exact immutable revision links");
    // One reviewed batch touches three definitions but increments each only once.
    studio::DraftDocument multiple{scratch.root,request.target};auto batch=multiple.collection();std::vector<std::string> touched;
    for (auto& [id,definition]:batch.definitions) if (auto* f=std::get_if<content::FlowDefinition>(&definition.data);f && touched.size()<3) {
        f->tags.push_back("batch_review");touched.push_back(id);
    }
    multiple.replace(batch);multiple.save("multiple.json");
    auto many=request;many.source="data/drafts/multiple.json";many.content_version="0.2.0-dev.1";many.summary="Review three related flow changes.";
    const auto grouped=studio::review_snapshot(scratch.root,many);check(Json::parse(grouped.report_json)["changes"].size()==3,"A multi-definition batch did not review all selected semantic changes");
    (void)studio::finalize_snapshot(scratch.root,many,grouped.plan_id);
    const auto revised=Json::parse(read(scratch.root/many.target));
    for (const auto& row:revised["definitions"]) if (std::find(touched.begin(),touched.end(),row["id"].get<std::string>())!=touched.end()) {
        const auto base=std::find_if(runtime["definitions"].begin(),runtime["definitions"].end(),[&](const auto& prior){return prior["id"]==row["id"];});
        check(row["revision"].get<unsigned>()==(*base)["revision"].get<unsigned>()+1,"A batch incremented a definition more than once");
    }
    runtime=revised;
    const auto stable=tracked(scratch.root);auto noop=initial();noop.content_version="99.0.0";noop.summary="No-op";
    const auto no=studio::review_snapshot(scratch.root,noop);
    check(!no.runtime_changed,"No-op review created a runtime change");(void)studio::finalize_snapshot(scratch.root,noop,no.plan_id);
    check(tracked(scratch.root)==stable,"No-op finalization changed bytes/history/versions");
    const auto prior_write=state(scratch.root).at("write_sequence");
    put(scratch.root/request.target,runtime.dump());
    auto formatting=studio::review_snapshot(scratch.root,noop);check(!formatting.runtime_changed,"Pure formatting changed runtime identity");
    (void)studio::finalize_snapshot(scratch.root,noop,formatting.plan_id);
    check(studio::check_snapshot(scratch.root).generation==3 && state(scratch.root).at("write_sequence")!=prior_write,"Formatting created another content generation or reused a recovery commit marker");
    auto game=noop;game.game_version="0.1.0-dev.1";const auto g=studio::review_snapshot(scratch.root,game);
    const auto resolved=studio::finalize_snapshot(scratch.root,game,g.plan_id);
    check(resolved.generation==3 && resolved.game_version_requirement_resolved && studio::check_snapshot(scratch.root).game_version_requirement_resolved,"Explicit game-version update did not satisfy the recorded pending requirement");
    refused([&]{(void)studio::review_snapshot(scratch.root,request);},"version");
}
void direct_save() {
    Scratch scratch{true};
    first_snapshot(scratch);
    const auto source=initial().target;
    studio::DraftDocument document{scratch.root,source}, stale{scratch.root,source};
    auto collection=document.collection();
    const auto& flow=std::get<content::FlowDefinition>(collection.definitions.at("flow.news.first_public_breakthrough").data);
    const auto key=flow.nodes.at(flow.entry_node).text_key;
    collection.messages[key]="A new instruction from the event editor.";
    check(document.replace(collection) && stale.replace(collection),"The authored text must be editable.");
    document.save_reviewed("Update the briefing from Content Studio.");
    check(!document.dirty() && document.relative_file()==source &&
        Json::parse(read(scratch.root/source)).at("messages").at(key)==collection.messages.at(key),
        "Save must replace the project's runtime source and reopen the written revision.");
    check(!fs::exists(scratch.root/"data/drafts") && studio::check_snapshot(scratch.root).generation==2,
        "Saving the project must produce a consistent revision without a draft file.");
    const auto saved=read(scratch.root/source);
    refused([&]{stale.save_reviewed("Attempt a stale edit.");},"conflict");
    check(read(scratch.root/source)==saved,"A stale editor must preserve the newer project source.");
}
void conflicts_and_faults() {
    for (const auto phase:{studio::SnapshotWritePhase::staged,studio::SnapshotWritePhase::replaced,studio::SnapshotWritePhase::committed}) {
        Scratch scratch;const auto before=tracked(scratch.root);const auto request=initial();const auto review=studio::review_snapshot(scratch.root,request);
        bool interrupted=false;try {(void)studio::finalize_snapshot(scratch.root,request,review.plan_id,[&](auto current,std::size_t index){if (current==phase && (current!=studio::SnapshotWritePhase::replaced || index==2)) throw std::runtime_error("simulated interruption");});}catch(const std::runtime_error&){interrupted=true;}
        check(interrupted && fs::exists(scratch.root/".pgp-editor/snapshot-transaction/journal.json"),"Injected write interruption lacked a durable recovery journal");
        refused([&]{(void)studio::review_snapshot(scratch.root,request);},"pending_snapshot");
        (void)studio::recover_snapshot(scratch.root);
        if (phase==studio::SnapshotWritePhase::committed) check(studio::check_snapshot(scratch.root).generation==1,"Recovery lost a committed generation");
        else check(tracked(scratch.root)==before,"Recovery failed to restore the entire prior generation");
        check(!fs::exists(scratch.root/".pgp-editor/snapshot-transaction"),"Recovered snapshot kept its pending lock/journal");
    }
    // Formatting-only reconciliation still gets a distinct durable marker.
    Scratch formatting;first_snapshot(formatting);auto raw=Json::parse(read(formatting.root/initial().target));put(formatting.root/initial().target,raw.dump());
    const auto before_formatting=tracked(formatting.root);const auto format_plan=studio::review_snapshot(formatting.root,initial());
    try {(void)studio::finalize_snapshot(formatting.root,initial(),format_plan.plan_id,[](auto phase,std::size_t){if (phase==studio::SnapshotWritePhase::replaced) throw std::runtime_error("interrupt formatting");});}catch(const std::runtime_error&){}
    (void)studio::recover_snapshot(formatting.root);check(tracked(formatting.root)==before_formatting,"Formatting recovery mistook an unchanged generation for a committed write");
    Scratch scratch;first_snapshot(scratch);
    auto request=initial();const auto preview=studio::review_snapshot(scratch.root,request);
    put(scratch.root/"project-versions.json",read(scratch.root/"project-versions.json")+"\n");
    refused([&]{(void)studio::finalize_snapshot(scratch.root,request,preview.plan_id);},"conflict");
    check(!fs::exists(scratch.root/".pgp-editor/snapshot-transaction"),"Stale review started a transaction before refusing");
    // A third-party edit during a partial generation is never overwritten by rollback.
    Scratch changed;const auto r=initial();const auto plan=studio::review_snapshot(changed.root,r);const auto original=read(changed.root/"project-versions.json");
    try {(void)studio::finalize_snapshot(changed.root,r,plan.plan_id,[&](auto phase,std::size_t){if (phase==studio::SnapshotWritePhase::replaced) {auto other=Json::parse(original);other["components"]["editor"]="9.0.0-dev";put(changed.root/"project-versions.json",other.dump(2));throw std::runtime_error("stop");}});}catch(const std::runtime_error&){}
    refused([&]{(void)studio::recover_snapshot(changed.root);},"conflict");
    check(Json::parse(read(changed.root/"project-versions.json"))["components"]["editor"]=="9.0.0-dev","Recovery discarded a concurrent version edit");
    put(changed.root/"project-versions.json",original);(void)studio::recover_snapshot(changed.root);
    check(read(changed.root/r.target).find("0.0.0-dev")!=std::string::npos,"Retry recovery failed after the conflicting version was preserved and resolved");
}
void malformed() {
    Scratch scratch;auto request=initial();
    for (const auto& version:{"01.0.0","1.0.0-01","1.0.0-","1.2","2147483648.0.0","0.0.0-dev"}) {
        request.content_version=version;refused([&]{(void)studio::review_snapshot(scratch.root,request);},"version");
    }
    request=initial();request.source="../outside.json";refused([&]{(void)studio::review_snapshot(scratch.root,request);},"path");
    request=initial();request.save_policy="compatible-with-every-save";refused([&]{(void)studio::review_snapshot(scratch.root,request);},"compatibility");
    request=initial();put(scratch.root/"data/drafts/linked.json","{}");fs::remove(scratch.root/"data/drafts/linked.json");fs::create_symlink(scratch.root/request.target,scratch.root/"data/drafts/linked.json");request.source="data/drafts/linked.json";
    bool blocked=false;try {(void)studio::review_snapshot(scratch.root,request);}catch(const std::exception&){blocked=true;}check(blocked,"Snapshot followed a linked draft outside its ordinary checkout path");
}
}
int main(int argc,char** argv){try {if (argc==2 && std::string_view{argv[1]}=="--direct-save") direct_save();
    else {changes_and_versions();conflicts_and_faults();malformed();}std::cout<<checks<<" snapshot checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
