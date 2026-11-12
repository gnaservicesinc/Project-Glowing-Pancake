#include "snapshot_internal.hpp"
#include "snapshot_version.hpp"
#include "checkout.hpp"
#include <pgp/studio/assets.hpp>
#include <pgp/core/build_identity.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

namespace pgp::studio::detail {
namespace {
namespace fs=std::filesystem;
using Json=SnapshotJson;
constexpr auto history_path=snapshot_history_path;
constexpr auto state_path="authoring/content-state.json";
constexpr auto runtime_manifest="data/runtime/content-manifest.json";
std::string required_read(PreparedSnapshot& p,const fs::path& root,const fs::path& path,std::size_t limit=4U*1024U*1024U) {
    const auto bytes=snapshot_read(root,path,limit);p.observed.emplace(path,bytes);
    if (!bytes) snapshot_refuse("missing_file","Required snapshot source is absent: "+path.generic_string());return *bytes;
}
std::optional<std::string> observe(PreparedSnapshot& p,const fs::path& root,const fs::path& path) {
    auto bytes=snapshot_read(root,path);p.observed.emplace(path,bytes);return bytes;
}
std::string pretty(const Json& j) {return j.dump(2)+"\n";}
std::string hash(const Json& j) {return persistence::sha256(j.dump());}
Json definitions(const Json& source) {
    Json result=Json::object();for (const auto& row:source.at("definitions")) result[row.at("id").get<std::string>()]=row;return result;
}
void text_refs(const Json& node,std::set<std::string>& keys) {
    if (node.is_object()) for (const auto& [key,value]:node.items()) {
        if ((key=="text_key" || key=="title_key" || key=="label_key") && value.is_string()) keys.insert(value.get<std::string>());
        else text_refs(value,keys);
    } else if (node.is_array()) for (const auto& child:node) text_refs(child,keys);
}
Json signature(Json definition,const Json& source) {
    definition.erase("revision");std::set<std::string> refs;text_refs(definition,refs);
    Json messages=Json::object();const auto& all=source.value("messages",Json::object());
    for (const auto& key:refs) if (all.contains(key)) messages[key]=all.at(key);
    Json result{{"definition",std::move(definition)},{"messages",std::move(messages)},{"variables",source.value("variables",Json::array())}};
    Json translations=Json::object();
    if (source.contains("translations")) for (const auto& [locale, entries]:source.at("translations").items())
        for (const auto& key:refs) if (entries.contains(key)) translations[locale][key]=entries.at(key);
    if (!translations.empty()) result["translations"]=std::move(translations);
    return result;
}
Json changed_text(const Json& old,const Json& next) {
    const auto a=old.value("messages",Json::object()),b=next.value("messages",Json::object());
    std::set<std::string> keys;for (const auto& [k,v]:a.items()){(void)v;keys.insert(k);}for (const auto& [k,v]:b.items()){(void)v;keys.insert(k);}
    Json changes=Json::array();for (const auto& key:keys) if (!a.contains(key) || !b.contains(key) || a.at(key)!=b.at(key)) changes.push_back(key);return changes;
}
Json validate_assets(PreparedSnapshot& plan,const fs::path& root,const content::Collection& collection) {
    Json result=Json::array();
    if (collection.asset_records.empty()) return result;
    AssetRegister registry{root};
    plan.asset_observations["assets/manifest.json"]={registry.manifest_hash(),content::maximum_document_bytes};
    for (const auto& [id,expected]:collection.asset_records) {
        const auto seal=registry.seal(id);
        if (seal.fingerprint!=expected) snapshot_refuse("asset_conflict","The pinned asset record changed. Review and pin the current record: "+id);
        const auto& r=seal.record;
        plan.asset_observations[r.path]={r.sha256,16U*1024U*1024U};
        plan.asset_observations[r.license_evidence_path]={seal.evidence_hash,content::maximum_document_bytes};
        result.push_back({{"id",id},{"path",r.path},{"sha256",r.sha256},{"record_hash",expected},
            {"kind",r.kind},{"credit_text",r.credit_text},{"license_id",r.license_id},{"license_evidence_path",r.license_evidence_path},
            {"license_evidence_hash",seal.evidence_hash}});
    }
    return result;
}
Json validate_runtime(const runtime::CompiledProgram& program) {
    std::size_t scenarios=0;
    std::vector<runtime::Checkpoint> openings;
    for (const auto& [id,definition]:program.collection.definitions) {
        const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);if (!scenario) continue;
        if (++scenarios>16) snapshot_refuse("budget","Internal snapshot validation supports at most sixteen scenarios.");
        for (const auto& hq:scenario->headquarters_choices) {
            if (openings.size()>=64) snapshot_refuse("budget","Snapshot opening fixtures exceed their bound.");
            runtime::Runtime engine{program,runtime::begin_scenario(program,id,hq)};
            const auto result=engine.prepare_opening(engine.current_state().world.simulation.revision);
            if (!result.accepted()) snapshot_refuse("validation","A real scenario opening failed: "+id+": "+result.explanation);
            openings.push_back(engine.checkpoint());
        }
    }
    if (scenarios==0) snapshot_refuse("validation","Reviewed slice content must include an executable scenario.");
    persistence::validate_save_roundtrips(program,openings);
    Json lints=Json::array();for (const auto& lint:program.lints) lints.push_back({{"code",lint.code},{"path",lint.path},{"explanation",lint.explanation}});
    return {{"compiler","shared-game-runtime"},{"scenarios",scenarios},{"prepared_headquarters",openings.size()},{"exact_prepared_save_roundtrips",openings.size()},{"lints",std::move(lints)}};
}
Json result_json(const SnapshotResult& r) {
    return {{"generation",r.generation},{"content_version",r.content_version},{"collection_hash",r.collection_hash},
        {"required_game_version",r.required_game_version},{"game_version_requirement_resolved",r.game_version_requirement_resolved},{"release_ready",false}};
}
Json read_state(const fs::path& root) {
    const auto bytes=snapshot_read(root,state_path);if (!bytes) snapshot_refuse("missing_snapshot","No finalized internal content snapshot exists.");
    auto j=snapshot_parse(*bytes);snapshot_fields(j,{"format","generation","target","content_version","collection_hash","runtime_manifest_hash","history_hash","snapshot","snapshot_hash","required_game_version","save_policy","write_sequence"});
    if (j.at("format")!=1 || !j.at("generation").is_number_unsigned() || j.at("generation")==0 || j.at("generation")>snapshot_history_generations || !j.at("write_sequence").is_number_unsigned() || j.at("write_sequence")==0 || j.at("save_policy")!="reject-prior-content") snapshot_refuse("snapshot","Unsupported snapshot marker or generation.");
    return j;
}
void validate_owned_artifacts(PreparedSnapshot& plan,const fs::path& root,const Json& state,Json& history,Json& baseline) {
    const auto history_bytes=required_read(plan,root,history_path);history=snapshot_parse(history_bytes,snapshot_history_bytes,snapshot_history_tokens);
    snapshot_fields(history,{"format","batches"});
    if (history.at("format")!=1 || !history.at("batches").is_array() || history.at("batches").empty() || history.at("batches").size()>snapshot_history_generations ||
        state.at("history_hash")!=persistence::sha256(history_bytes)) snapshot_refuse("snapshot","Content history differs from its committed snapshot marker.");
    const auto& batch=history.at("batches").back();
    if (batch.at("generation")!=state.at("generation") || batch.at("content_version")!=state.at("content_version") || batch.at("collection_hash")!=state.at("collection_hash")) snapshot_refuse("snapshot","Content history and the committed generation disagree.");
    const auto saved=fs::path{state.at("snapshot").get<std::string>()};
    if (!saved.generic_string().starts_with("authoring/snapshots/") || saved.extension()!=".json") snapshot_refuse("path","Committed baseline is not an owned snapshot path.");
    const auto snapshot_bytes=required_read(plan,root,saved,content::maximum_document_bytes);
    if (state.at("snapshot_hash")!=persistence::sha256(snapshot_bytes)) snapshot_refuse("snapshot","The immutable finalized baseline was changed externally.");
    const auto program=compile_source(snapshot_bytes,content::foundation_registry());baseline=snapshot_parse(program.canonical_content_bytes);
    if (state.at("collection_hash")!=persistence::sha256(program.canonical_content_bytes) || baseline.at("content_version")!=state.at("content_version")) snapshot_refuse("snapshot","Finalized baseline identity is inconsistent.");
    const auto manifest_bytes=required_read(plan,root,runtime_manifest);const auto manifest=snapshot_parse(manifest_bytes);
    if (state.at("runtime_manifest_hash")!=persistence::sha256(manifest_bytes) || manifest.at("collection_hash")!=state.at("collection_hash") || manifest.at("generation")!=state.at("generation")) snapshot_refuse("snapshot","Runtime manifest differs from its committed generation.");
}
}
PreparedSnapshot prepare_snapshot(const fs::path& root,const SnapshotRequest& request,std::optional<EditedSnapshot> edited) {
    if (checkout_schema(root)!=content::content_schema) snapshot_refuse("unsupported_schema","The selected checkout is read-only for this validator.");
    if (fs::exists(checked_path(root,".pgp-editor/snapshot-transaction"))) snapshot_refuse("pending_snapshot","Recover the existing snapshot transaction before review/finalization.");
    const auto allowed=[](const fs::path& path){const auto s=path.generic_string();return path.extension()==".json" && (s.starts_with("data/drafts/") || s.starts_with("data/runtime/")) && s!="data/runtime/content-manifest.json";};
    if (!allowed(request.source) || !request.target.generic_string().starts_with("data/runtime/") || !allowed(request.target)) snapshot_refuse("path","Choose a local collection draft and a reviewed runtime collection target.");
    if (request.save_policy!="reject-prior-content") snapshot_refuse("compatibility","This internal writer cannot claim a migration. Use the actual exact-content save refusal policy.");
    PreparedSnapshot plan;
    const auto versions_bytes=required_read(plan,root,"project-versions.json",65536);
    if (edited && (request.source!=request.target || persistence::sha256(versions_bytes)!=edited->expected_versions))
        snapshot_refuse("conflict","The project changed since it was opened. Reopen the project before saving.");
    auto versions=snapshot_parse(versions_bytes);snapshot_manifest(versions);
    const auto versions_before=versions.at("components");
    const auto& identity=core::build_identity();
    if (versions.at("compatibility")!=Json{{"content_schema",identity.content_schema},{"simulation_rules",identity.simulation_rules},{"save_format",identity.save_format}})
        snapshot_refuse("compatibility","The checkout compatibility contract differs from the installed compiler/runtime.");
    const auto source=required_read(plan,root,request.source,content::maximum_document_bytes);
    if (edited && persistence::sha256(source)!=edited->expected_source)
        snapshot_refuse("conflict","The content changed on disk. Reopen the project before saving.");
    const auto current=request.source==request.target ? source : required_read(plan,root,request.target,content::maximum_document_bytes);
    const auto source_program=compile_source(edited ? edited->collection : std::string_view{source},content::foundation_registry());
    auto candidate=snapshot_parse(source_program.canonical_content_bytes);
    auto current_program=compile_source(current,content::foundation_registry());
    const auto state_bytes=observe(plan,root,state_path);
    Json old=Json::object(),history{{"format",1},{"batches",Json::array()}},prior_state;
    std::uint64_t generation=0,write_sequence=0;std::string previous_requirement;
    if (state_bytes) {
        prior_state=read_state(root);
        if (prior_state.at("target")!=request.target.generic_string()) snapshot_refuse("scope","This internal writer finalizes one slice collection per checkout.");
        validate_owned_artifacts(plan,root,prior_state,history,old);
        generation=prior_state.at("generation").get<std::uint64_t>();write_sequence=prior_state.at("write_sequence").get<std::uint64_t>();previous_requirement=prior_state.at("required_game_version").get<std::string>();
        if (versions.at("components").at("content")!=prior_state.at("content_version")) snapshot_refuse("version","Content version was changed outside the shared snapshot writer.");
    } else {
        if (observe(plan,root,history_path) || observe(plan,root,runtime_manifest)) snapshot_refuse("snapshot","Existing history/manifest has no commit marker; preserve it and inspect the incomplete batch.");
        old=snapshot_parse(current_program.canonical_content_bytes);
    }
    const auto current_version=versions.at("components").at("content").get<std::string>();
    if (candidate.at("content_version")!=current_version || current_program.collection.content_version!=current_version) snapshot_refuse("version","Reopen/rebase the draft against the current content version; do not edit version fields manually.");
    const auto before=definitions(old),proposed=definitions(candidate);
    Json changes=Json::array();bool feature=!state_bytes.has_value() || old.value("variables",Json::array())!=candidate.value("variables",Json::array());
    for (const auto& [id,row]:proposed.items()) {
        if (before.contains(id) && row.at("revision")!=before.at(id).at("revision")) snapshot_refuse("revision","Draft edits cannot pre-increment definition revisions: "+id);
        if (!before.contains(id) && row.at("revision")!=1) snapshot_refuse("revision","New definitions start at revision one.");
        const bool changed=!state_bytes || !before.contains(id) || signature(row,candidate)!=signature(before.at(id),old);
        if (!changed) continue;
        auto next_revision=row.at("revision").get<std::uint64_t>();
        if (state_bytes && before.contains(id)) {if (next_revision==UINT32_MAX) snapshot_refuse("revision","Definition revision is exhausted.");++next_revision;}
        const bool added=!state_bytes || !before.contains(id);
        Json old_body=added?Json(nullptr):before.at(id);if (!old_body.is_null()) old_body.erase("revision");
        auto body=row;body.erase("revision");
        if (added || old_body!=body) feature=true;
        changes.push_back({{"id",id},{"kind",row.at("kind")},{"change",added?"introduced":"changed"},
            {"old_revision",added?Json(nullptr):row.at("revision")},{"revision",next_revision},{"before",added?Json(nullptr):before.at(id)},{"after",row}});
    }
    for (const auto& [id,row]:before.items()) if (!proposed.contains(id)) {
        feature=true;changes.push_back({{"id",id},{"kind",row.at("kind")},{"change","removed"},{"old_revision",row.at("revision")},{"revision",nullptr},{"before",row},{"after",nullptr}});
    }
    for (auto& change:changes) if (!change.at("after").is_null()) change["after"]["revision"]=change.at("revision");
    const auto texts=changed_text(old,candidate);
    auto old_semantics=old,next_semantics=candidate;old_semantics.erase("content_version");next_semantics.erase("content_version");
    for (auto& row:old_semantics["definitions"]) row.erase("revision");for (auto& row:next_semantics["definitions"]) row.erase("revision");
    const bool runtime_changed=!state_bytes || old_semantics!=next_semantics;
    auto proposed_version=current_version;
    if (runtime_changed) {
        if (request.summary.empty() || request.summary.size()>2048 || request.summary.find('\0')!=std::string::npos) snapshot_refuse("summary","Provide a short explicit reviewed change summary.");
        const auto requested_version=edited ? snapshot_game_bump(current_version,feature) : request.content_version;
        (void)snapshot_version(requested_version);
        if (snapshot_compare(requested_version,current_version)<=0) snapshot_refuse("version","Finalized content needs a strictly newer Semantic Version.");
        proposed_version=requested_version;
        if (generation>=snapshot_history_generations) snapshot_refuse("budget","Internal content history reached its bounded batch count.");
        ++generation;
    }
    candidate["content_version"]=proposed_version;
    if (runtime_changed) for (auto& row:candidate["definitions"]) {
        for (const auto& change:changes) if (change.at("id")==row.at("id") && !change.at("revision").is_null()) row["revision"]=change.at("revision");
    }
    auto final_program=compile_source(candidate.dump(),content::foundation_registry());
    candidate=snapshot_parse(final_program.canonical_content_bytes);
    const auto assets=validate_assets(plan,root,final_program.collection);
    const auto validation=validate_runtime(final_program);
    auto required=runtime_changed?snapshot_game_bump(versions.at("components").at("game").get<std::string>(),feature):previous_requirement;
    if (!previous_requirement.empty() && (required.empty() || snapshot_compare(previous_requirement,required)>0)) required=previous_requirement;
    if (request.game_version) {
        (void)snapshot_version(*request.game_version);
        if (snapshot_compare(*request.game_version,versions.at("components").at("game").get<std::string>())<=0 || snapshot_compare(*request.game_version,required)<0)
            snapshot_refuse("version","Proposed game version must be newer and satisfy every outstanding content requirement.");
        versions["components"]["game"]=*request.game_version;
    }
    versions["components"]["content"]=proposed_version;
    plan.result={generation,proposed_version,persistence::sha256(final_program.canonical_content_bytes),required,
        required.empty() || snapshot_compare(versions.at("components").at("game").get<std::string>(),required)>=0};
    if (write_sequence==UINT64_MAX) snapshot_refuse("budget","Snapshot write sequence is exhausted.");
    const auto add_write=[&](fs::path path,std::string bytes) {
        auto prior=observe(plan,root,path);if (prior && *prior==bytes) return;plan.writes.push_back({std::move(path),std::move(prior),std::move(bytes)});
    };
    Json actual_files=Json::array();
    if (runtime_changed) {
        std::ostringstream filename;filename<<"authoring/snapshots/"<<std::setw(6)<<std::setfill('0')<<generation<<".collection.json";
        const fs::path snapshot_path{filename.str()};
        if (observe(plan,root,snapshot_path)) snapshot_refuse("conflict","A proposed immutable snapshot already exists.");
        const auto snapshot_bytes=pretty(candidate);
        Json definition_manifest=Json::array(),scenario_roots=Json::object();
        for (const auto& row:candidate.at("definitions")) {
            const auto id=row.at("id").get<std::string>();
            definition_manifest.push_back({{"id",id},{"kind",row.at("kind")},{"revision",row.at("revision")},{"semantic_hash",hash(signature(row,candidate))}});
            if (row.at("kind")=="scenario") scenario_roots[id]=row.at("flows");
        }
        Json manifest{{"format",1},{"generation",generation},{"content_version",proposed_version},{"collection",request.target.generic_string()},
            {"collection_hash",plan.result.collection_hash},{"source_hash",persistence::sha256(snapshot_bytes)},{"compatibility",versions.at("compatibility")},
            {"definitions",std::move(definition_manifest)},{"scenario_flow_roots",std::move(scenario_roots)},
            {"messages_hash",hash(candidate.value("messages",Json::object()))},{"assets",assets},{"save_policy",request.save_policy}};
        // Immutable collections retain the full bodies. Keep their hashes and
        // revision links here so successive scenario edits cannot multiply
        // entire catalogues inside the bounded history metadata.
        Json recorded_changes=Json::array();
        for (const auto& change:changes) {
            auto recorded=change;
            recorded["before_hash"]=change.at("before").is_null()?Json(nullptr):Json(hash(change.at("before")));
            recorded["after_hash"]=change.at("after").is_null()?Json(nullptr):Json(hash(change.at("after")));
            recorded.erase("before");recorded.erase("after");
            recorded_changes.push_back(std::move(recorded));
        }
        history["batches"].push_back({{"generation",generation},{"content_version",proposed_version},{"previous_content_version",state_bytes?Json(current_version):Json(nullptr)},
            {"collection_hash",plan.result.collection_hash},{"target",request.target.generic_string()},{"snapshot",snapshot_path.generic_string()},
            {"previous_snapshot",state_bytes?prior_state.at("snapshot"):Json(nullptr)},
            {"summary",request.summary},{"changed_definitions",std::move(recorded_changes)},{"changed_text_keys",texts},{"compatibility",versions.at("compatibility")},
            {"minimum_game_change",feature?"minor":"patch"},{"required_game_version",required},{"game_version_at_finalization",versions.at("components").at("game")},
            {"save_policy",request.save_policy},{"validation",validation},{"purpose","internal-playtest"}});
        const auto history_bytes=pretty(history),manifest_bytes=pretty(manifest);
        Json state{{"format",1},{"generation",generation},{"write_sequence",write_sequence+1},{"target",request.target.generic_string()},{"content_version",proposed_version},
            {"collection_hash",plan.result.collection_hash},{"runtime_manifest_hash",persistence::sha256(manifest_bytes)},{"history_hash",persistence::sha256(history_bytes)},
            {"snapshot",snapshot_path.generic_string()},{"snapshot_hash",persistence::sha256(snapshot_bytes)},{"required_game_version",required},{"save_policy",request.save_policy}};
        add_write(request.target,snapshot_bytes);add_write(snapshot_path,snapshot_bytes);add_write(runtime_manifest,manifest_bytes);
        add_write(history_path,history_bytes);add_write("project-versions.json",pretty(versions));add_write(state_path,pretty(state));
    } else {
        // Canonical formatting can be restored without creating a content
        // revision/history generation. Metadata, layout and draft files remain separate.
        add_write(request.target,pretty(candidate));
        if (request.game_version) add_write("project-versions.json",pretty(versions));
        // The marker remains last even for a formatting-only reconciliation.
        if (!plan.writes.empty()) {const auto bytes=required_read(plan,root,state_path);auto marker=snapshot_parse(bytes);marker["write_sequence"]=write_sequence+1;plan.writes.push_back({state_path,bytes,pretty(marker)});}
    }
    for (const auto& file:plan.writes) {
        if (file.path==fs::path{history_path}) (void)snapshot_parse(file.after,snapshot_history_bytes,snapshot_history_tokens);
        else (void)snapshot_parse(file.after);
        actual_files.push_back({{"path",file.path.generic_string()},
        {"before_hash",file.before?Json(persistence::sha256(*file.before)):Json(nullptr)},{"after_hash",persistence::sha256(file.after)}}); }
    Json observed=Json::object();for (const auto& [path,bytes]:plan.observed) observed[path.generic_string()]=bytes?Json(persistence::sha256(*bytes)):Json(nullptr);
    for (const auto& [path,identity]:plan.asset_observations) observed[path.generic_string()]=identity.first;
    Json text_changes=Json::array();const auto old_messages=old.value("messages",Json::object()),new_messages=candidate.value("messages",Json::object());
    for (const auto& key:texts) {
        const auto name=key.get<std::string>();
        text_changes.push_back({{"key",name},{"before",old_messages.contains(name)?old_messages.at(name):Json(nullptr)},{"after",new_messages.contains(name)?new_messages.at(name):Json(nullptr)}});
    }
    Json collection_changes=Json::array();
    if (old.value("variables",Json::array())!=candidate.value("variables",Json::array()))
        collection_changes.push_back({{"path","variables"},{"before",old.value("variables",Json::array())},{"after",candidate.value("variables",Json::array())}});
    if (old.value("translations",Json::object())!=candidate.value("translations",Json::object()))
        collection_changes.push_back({{"path","translations"},{"before",old.value("translations",Json::object())},{"after",candidate.value("translations",Json::object())}});
    if (old.value("asset_records",Json::object())!=candidate.value("asset_records",Json::object()))
        collection_changes.push_back({{"path","asset_records"},{"before",old.value("asset_records",Json::object())},{"after",assets}});
    Json report{{"format",1},{"runtime_changed",runtime_changed},{"result",result_json(plan.result)},{"changes",changes},{"changed_text_keys",texts},
        {"text_changes",std::move(text_changes)},{"collection_changes",std::move(collection_changes)},
        {"versions_before",versions_before},{"versions_after",versions.at("components")},
        {"validation",validation},{"writes",std::move(actual_files)},{"observed",std::move(observed)},
        {"request",{{"source",request.source.generic_string()},{"target",request.target.generic_string()},{"content_version",request.content_version},{"summary",request.summary},{"save_policy",request.save_policy},{"game_version",request.game_version?Json(*request.game_version):Json(nullptr)}}}};
    plan.review.plan_id=hash(report);report["plan_id"]=plan.review.plan_id;plan.review.report_json=pretty(report);plan.review.runtime_changed=runtime_changed;
    return plan;
}
}
namespace pgp::studio {
SnapshotReview review_snapshot(const std::filesystem::path& selected,const SnapshotRequest& request) {
    return detail::prepare_snapshot(detail::checkout_root(selected),request).review;
}
SnapshotResult finalize_snapshot(const std::filesystem::path& selected,const SnapshotRequest& request,std::string_view id,const SnapshotWriteObserver& observer) {
    const auto root=detail::checkout_root(selected);auto plan=detail::prepare_snapshot(root,request);
    if (id!=plan.review.plan_id) detail::snapshot_refuse("conflict","The reviewed plan changed. Review current source/version/validation differences before finalizing.");
    detail::write_snapshot_batch(root,plan,observer);return plan.result;
}
SnapshotResult check_snapshot(const std::filesystem::path& selected) {
    using namespace detail;const auto root=checkout_root(selected);
    if (std::filesystem::exists(checked_path(root,".pgp-editor/snapshot-transaction"))) snapshot_refuse("pending_snapshot","Recover the pending snapshot transaction before validation/build.");
    if (checkout_schema(root)!=content::content_schema) snapshot_refuse("unsupported_schema","Unsupported checkout schema.");
    const auto state=read_state(root);PreparedSnapshot plan;SnapshotJson history,baseline;
    validate_owned_artifacts(plan,root,state,history,baseline);
    const auto versions=snapshot_parse(required_read(plan,root,"project-versions.json",65536));snapshot_manifest(versions);
    const auto target=std::filesystem::path{state.at("target").get<std::string>()};
    if (!target.generic_string().starts_with("data/runtime/") || target=="data/runtime/content-manifest.json") snapshot_refuse("path","Snapshot target is outside reviewed runtime collections.");
    const auto current=compile_source(required_read(plan,root,target,content::maximum_document_bytes),content::foundation_registry());
    if (state.at("collection_hash")!=persistence::sha256(current.canonical_content_bytes) || versions.at("components").at("content")!=state.at("content_version")) snapshot_refuse("pending_content","Runtime source has unfinalized semantic/version changes.");
    if (versions.at("compatibility")!=history.at("batches").back().at("compatibility")) snapshot_refuse("compatibility","Live compatibility versions differ from the finalized evidence.");
    const auto assets=validate_assets(plan,root,current.collection);
    if (!assets.empty()) {
        const auto manifest=snapshot_parse(required_read(plan,root,runtime_manifest));
        if (manifest.at("assets")!=assets) snapshot_refuse("asset_conflict","The runtime manifest differs from the pinned asset records.");
    }
    const auto required=state.at("required_game_version").get<std::string>();
    return {state.at("generation").get<std::uint64_t>(),state.at("content_version").get<std::string>(),state.at("collection_hash").get<std::string>(),required,
        snapshot_compare(versions.at("components").at("game").get<std::string>(),required)>=0};
}
}
