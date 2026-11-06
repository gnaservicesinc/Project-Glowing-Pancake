#include <pgp/studio/catalog.hpp>
#include <pgp/studio/world_forms.hpp>
#include <pgp/studio/assets.hpp>
#include <pgp/studio/reusable_flow.hpp>
#include <pgp/studio/flow_graph.hpp>
#include <pgp/studio/snapshot.hpp>
#include <pgp/studio/library.hpp>
#include <pgp/persistence/storage.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
using namespace pgp;namespace fs=std::filesystem;
int checks=0;
void check(bool yes,const char* why) {++checks;if(!yes) throw std::runtime_error(why);}
void put(const fs::path& p,std::string_view text) {fs::create_directories(p.parent_path());std::ofstream out{p};out<<text;}
std::string read(const fs::path& p) {return persistence::read_bounded(p,content::maximum_document_bytes);}
struct Scratch {
    fs::path path=fs::path{PGP_CATALOG_SCRATCH}/("catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        fs::create_directories(path/".git");put(path/"CMakeLists.txt","# test\n");put(path/"src/content/collection.cpp","// test\n");
        auto versions=nlohmann::json::parse(read(PGP_CATALOG_VERSIONS));
        auto runtime=nlohmann::json::parse(read(PGP_CATALOG_SOURCE));
        versions["components"]["content"]="0.0.0-dev";runtime["content_version"]="0.0.0-dev";
        runtime.erase("asset_records"); // This fixture imports and reviews its own file.
        for(auto& definition:runtime.at("definitions")) definition["revision"]=1;
        put(path/"project-versions.json",versions.dump());put(path/"data/runtime/first-playable.json",runtime.dump());
    }
    ~Scratch(){std::error_code error;fs::remove_all(path,error);}
};
template<class Action> void refused(studio::DraftDocument& d,Action action) {
    const auto before=d.program().canonical_content_bytes;const auto revision=d.edit_revision();bool failed=false;
    try {action();}catch(const std::exception&){failed=true;}
    check(failed && before==d.program().canonical_content_bytes && revision==d.edit_revision(),"Rejected catalog edit changed document state or history");
}
void scoped_sector_references(const runtime::CompiledProgram& original) {
    auto collection=original.collection;
    auto& flow=std::get<content::FlowDefinition>(collection.definitions.at("society.monthly_briefing").data);
    auto node=flow.nodes.extract(flow.entry_node);node.key()="science";
    flow.entry_node="science";flow.nodes.insert(std::move(node));
    const auto program=runtime::compile_collection(std::move(collection),original.registry);
    const auto index=studio::catalog_index(program);
    const auto messages=studio::content_uses(index,"text","society.monthly_briefing",false);
    check(std::any_of(messages.begin(),messages.end(),[](const auto& ref){return ref.path.ends_with("/nodes/science/text_key");}),
        "A flow node named science was mistaken for a society sector reference");
    const auto& society=std::get<core::SocietyCatalog>(program.collection.definitions.at("society.global").data);
    const auto sectors=studio::content_uses(index,"segment",society.science.front(),false);
    check(std::any_of(sectors.begin(),sectors.end(),[](const auto& ref){return ref.owner=="society.global";}),
        "An actual society sector lost its typed customer reference");
}
void index_and_refactor(studio::DraftDocument& d) {
    auto next=d.collection();
    content::FlowDefinition literal{};literal.scope=content::Scope::company;
    literal.trigger={content::ConditionKind::clause,content::FactClause{"company.cash_cents",content::Comparison::greater_equal,std::int64_t{0},content::Unit::constant_2014_cents},{}};
    literal.lifecycle.mode=content::LifecycleMode::once;literal.lifecycle.occurrence_scope=content::Scope::company;literal.entry_node="op";
    content::Node effect{};effect.kind=content::NodeKind::effect;effect.next="capacity_project_id";
    effect.effects.push_back({"record_decision","self",content::Scope::company,{{"value",std::string{"data.licensed_documents"}}}});
    content::Node end{};end.kind=content::NodeKind::end;
    literal.nodes.emplace("op",std::move(effect));literal.nodes.emplace("capacity_project_id",std::move(end));
    next.definitions.emplace("catalog.literal",content::Definition{"catalog.literal",1,std::move(literal)});
    (void)d.replace(next);
    const auto index=studio::catalog_index(d);
    check(index.entries.size()>100 && index.references.size()>100,"Catalog index omits nested simulation records");
    const auto uses=studio::content_uses(index,"dataset","data.licensed_documents");
    check(uses.size()>=5 && std::any_of(uses.begin(),uses.end(),[](const auto& ref){return ref.path.ends_with("/data_id");}),"Dataset impact search lost rival recipe inputs");
    const auto plan=studio::RefactorPlan::rename(d,"dataset","data.licensed_documents","data.authored_documents");
    check(plan.changes().size()>5 && std::any_of(plan.changes().begin(),plan.changes().end(),[](const auto& c){return !c.declaration;}),"Reference review omits declarations or typed uses");
    const auto before=d.program().canonical_content_bytes;
    check(plan.apply(d),"Reviewed dataset refactor did not apply");
    const auto after=studio::catalog_index(d);
    check(studio::content_uses(after,"dataset","data.licensed_documents").empty(),"Dataset refactor left a typed reference dangling");
    const auto& marker=std::get<content::FlowDefinition>(d.collection().find("catalog.literal")->data).nodes.at("op").effects.front();
    check(std::get<std::string>(marker.arguments.at("value"))=="data.licensed_documents","Refactoring rewrote descriptive/decision text as an identifier");
    check(studio::preview_world(d,"scenario.first_playable","US-CA").find("Exact save reload:")!=std::string::npos,"Renamed data/rival references failed real opening and exact reload");
    check(d.undo() && d.program().canonical_content_bytes==before,"One refactor undo did not restore the full collection");
    refused(d,[&]{(void)plan.apply(d);});
    refused(d,[&]{(void)studio::RefactorPlan::rename(d,"dataset","data.licensed_documents","data.consented_scans");});
    refused(d,[&]{(void)studio::RefactorPlan::rename(d,"state","US-WA","state.renamed");});
    const auto research=studio::RefactorPlan::rename(d,"research","research.efficient_serving","research.authored_serving");
    check(research.apply(d) && !d.collection().find("research.efficient_serving") && d.collection().find("research.authored_serving"),"Research refactor lost its definition identity");
    check(studio::content_uses(studio::catalog_index(d),"research","research.efficient_serving").empty(),"Research prerequisites, recipes, executives or hardware retained stale uses");
    check(d.undo(),"Research refactor did not enter shared undo");
    check(studio::duplicate_catalog_definition(d,"catalog.literal","catalog.copy"),"Catalog definition duplication failed");
    refused(d,[&]{(void)studio::remove_catalog_entry(d,"/definitions/research.efficient_serving/id");});
    check(studio::remove_catalog_entry(d,"/definitions/catalog.copy/id") && !d.collection().find("catalog.copy"),"Unreferenced definition could not be safely removed");
    const auto old_text=d.collection().messages.begin()->first;
    check(studio::edit_message(d,old_text,"fr","Texte français") &&
        studio::edit_message(d,old_text,"fr-CA","Texte canadien"),"Locale authoring failed");
    check(*d.collection().message(old_text,"fr-CA")=="Texte canadien" &&
        *d.collection().message(old_text,"fr-FR")=="Texte français" &&
        *d.collection().message(old_text,"de-DE")==d.collection().messages.at(old_text),"Locale fallback did not follow exact locale, language and English order");
    refused(d,[&]{(void)studio::edit_message(d,"missing.english","fr","Manquant");});
    const auto text=studio::RefactorPlan::rename(d,"text",old_text,"catalog.text");
    check(text.apply(d) && d.collection().messages.contains("catalog.text"),"Localized message key refactor failed");
    check(studio::content_uses(studio::catalog_index(d),"text",old_text).empty(),"Message refactor left a presentation/choice reference behind");
    check(*d.collection().message("catalog.text","fr-CA")=="Texte canadien" && !d.collection().message(old_text,"fr"),"Renaming a message dropped a translation");
    const auto research_path="/definitions/research.efficient_serving";
    const auto form=studio::WorldForm::open_catalog(d,"scenario.first_playable",research_path);
    check(form.apply(d,{{"cash_cents","123456"}}),"Standalone research is inaccessible through ordinary catalog forms");
    auto prereq=studio::WorldForm::append_catalog(d,"scenario.first_playable",std::string(research_path)+"/prerequisites/any_of",0);
    check(prereq.apply(d,{{"","research.sequence_modeling"}}),"Typed research prerequisites could not be added");
    auto next_status=d.collection();
    next_status.definitions.at("research.efficient_serving").deprecated=true;
    check(d.replace(next_status),"Deprecation status did not enter the canonical document");
    constexpr auto scenario="scenario.world_competition";
    for (const auto& [family,id,field,value]:{
        std::tuple{"recipe","recipe.deduplicated","name","Authored compact recipe"},
        std::tuple{"dataset","data.consented_scans","rights","Authored consented commercial terms"},
        std::tuple{"hardware","rack.reference","name","Authored reference hardware"},
        std::tuple{"product","product.document_specialist","price_cents_per_task","121"},
        std::tuple{"executive","executive.research","priorities","Authored publication priorities"}}) {
        const auto records=studio::catalog_index(d);
        const auto entry=std::find_if(records.entries.begin(),records.entries.end(),[&](const auto& row) {
            return row.owner==scenario && row.family==family && row.id==id;
        });
        check(entry!=records.entries.end(),"A model, data, hardware, product or people catalog record was absent");
        const auto path=entry->path.substr(0,entry->path.rfind('/'));
        check(studio::WorldForm::open_catalog(d,scenario,path).apply(d,{{field,value}}),"An ordinary catalog record could not be edited through its owning schema");
    }
    check(studio::preview_world(d,scenario,"US-WA").find("Exact save reload:")!=std::string::npos,"Catalog field edits did not reach a valid opening");
    d.save("catalog-authored.json");
}
using Json=nlohmann::json;
void reusable_group(const fs::path& root) {
    const Json once{{"mode","once"},{"max_occurrences_per_scope",1}};
    const Json trigger{{"fact","company.cash_cents"},{"op","gte"},{"value",0}};
    const auto definition=[&](const char* id,const char* entry,Json nodes) {
        return Json{{"id",id},{"revision",1},{"kind","flow"},{"scope","company"},{"trigger",trigger},
            {"lifecycle",once},{"entry_node",entry},{"nodes",std::move(nodes)}};
    };
    const Json end{{"kind","end"}};
    const auto body=definition("flow.body","wait",{{"wait",{{"kind","delay"},{"duration_days",1},{"next","assign"}}},
        {"assign",{{"kind","effect"},{"next","end"},{"effects",Json::array({{
            {"op","set_variable"},{"target","self"},{"variable","story.output"},{"value",{{"fact","story.input"}}}
        }})}}},{"end",end}});
    const auto caller=definition("flow.caller","end",{{"end",end}});
    Json source{{"content_schema",1},{"content_version","0.7.0-dev.1"},{"currency_basis","constant_2014_cents"},
        {"definitions",Json::array({body,caller})},{"variables",Json::array()}};
    for (const auto* name:{"story.input","story.output","story.amount","story.result","story.inner_input","story.inner_output","story.outer_input","story.outer_output"})
        source["variables"].push_back({{"id",name},{"scope","company"},{"type","integer"},{"unit","count"},
            {"minimum",0},{"maximum",100},{"default",std::string_view(name)=="story.amount"?43:0},{"help","Bound group test value"}});
    source["variables"].push_back({{"id","story.days"},{"scope","company"},{"type","integer"},{"unit","days"},
        {"minimum",0},{"maximum",100},{"default",0},{"help","Incompatible unit"}});
    put(root/"data/drafts/group-source.json",source.dump());
    studio::DraftDocument d{root,"data/drafts/group-source.json"};
    check(studio::create_flow_group(d,"flow.body","group.inner"),"Flow body could not become a reusable group");
    check(studio::FlowForm::open(d,"group.inner","group").apply(d,"add_input","story.input"),"Group input port did not use the shared typed form");
    check(studio::FlowForm::open(d,"group.inner","group").apply(d,"add_output","story.output"),"Group output port did not use the shared typed form");
    refused(d,[&]{(void)studio::insert_group_call(d,"flow.caller","end","group.inner",{{"story.input","story.days"},{"story.output","story.result"}});});
    refused(d,[&]{(void)studio::insert_group_call(d,"flow.caller","end","group.inner",{{"story.input","story.amount"}});});
    refused(d,[&]{(void)studio::insert_group_call(d,"group.inner","end","group.inner",{{"story.input","story.inner_input"},{"story.output","story.inner_output"}});});
    check(studio::create_flow_group(d,"flow.caller","group.outer"),"Nested group could not be created");
    check(studio::FlowForm::open(d,"group.outer","group").apply(d,"add_input","story.outer_input"),"Outer input was unavailable");
    check(studio::FlowForm::open(d,"group.outer","group").apply(d,"add_output","story.outer_output"),"Outer output was unavailable");
    refused(d,[&]{(void)studio::insert_group_call(d,"group.outer","end","group.inner",
        {{"story.input","story.inner_input"},{"story.output","story.outer_input"}});});
    check(studio::insert_group_call(d,"group.outer","end","group.inner",
        {{"story.input","story.outer_input"},{"story.output","story.outer_output"}}),"Nested typed forwarding failed");
    check(studio::insert_group_call(d,"flow.caller","end","group.outer",
        {{"story.outer_input","story.amount"},{"story.outer_output","story.result"}}),"Caller could not bind both ports");
    const auto hash=persistence::sha256(d.program().canonical_content_bytes);
    const auto graph=studio::FlowGraph::open(d,"group.inner");
    const auto layout=graph.automatic_layout();
    (void)studio::FlowGraph::open(d,"group.inner",layout);
    check(persistence::sha256(d.program().canonical_content_bytes)==hash,"Group layout changed deterministic content identity");
    const auto uses=studio::content_uses(studio::catalog_index(d),"flow","group.inner",true);
    check(uses.size()==2 && std::any_of(uses.begin(),uses.end(),[](const auto& r){return r.owner=="flow.caller";}),"Transitive group impact omitted an outer caller");
    runtime::Checkpoint start;start.world=runtime::foundation_world(core::Cents{100});
    start.world.enabled_flows=std::vector<std::string>{"flow.caller"};
    runtime::Runtime engine{d.program(),start};
    check(engine.prepare_opening(0).accepted(),"Group caller failed to run");
    const auto waiting=engine.checkpoint();
    check(waiting.runtime.activations.size()==1 && waiting.runtime.activations.front().frames.size()==2 &&
        waiting.runtime.activations.front().status==runtime::ActivationStatus::delayed && waiting.world.variables.empty(),
        "Grouped wait escaped its saved caller stack or assigned before its due date");
    const auto restored=persistence::decode_save(d.program(),persistence::encode_save(d.program(),waiting)).checkpoint;
    check(restored==waiting,"Pending nested group did not reload exactly");
    runtime::Runtime resumed{d.program(),restored};
    check(engine.submit({waiting.world.simulation.revision,core::AdvanceDay{}}).accepted() &&
        resumed.submit({waiting.world.simulation.revision,core::AdvanceDay{}}).accepted(),"Saved nested group failed to resume");
    const auto done=engine.checkpoint();
    check(done==resumed.checkpoint() && done.world.variables.size()==1 &&
        done.world.variables.front().variable_id=="story.result" && done.world.variables.front().value==content::Scalar{std::int64_t{43}},
        "Group input/output mutated the template variables or resumed to the wrong caller target");
    check(done.runtime.receipts.size()==1 && done.runtime.receipts.front().bound_variable=="story.result" &&
        done.runtime.receipts.front().group_calls.size()==2 &&
        persistence::decode_save(d.program(),persistence::encode_save(d.program(),done)).checkpoint==done,
        "Group output receipt did not roundtrip with actual binding evidence");
    auto forged=done;
    forged.runtime.receipts.front().bound_variable="story.output";
    refused(d,[&]{runtime::validate_checkpoint(d.program(),forged);});
}
void assets(Scratch& scratch,studio::DraftDocument& draft) {
    const auto file=scratch.path/"local-map.json";
    const auto license=scratch.path/"assets/source-license.txt";
    put(file,"{\"shape\":[[0,0],[1,1]]}\n");
    put(license,"Original geometry fixture for testing the asset authoring pipeline.\n");
    studio::AssetRegister registry{scratch.path};
    studio::AssetRecord r;
    r.id="map.author_fixture";r.kind="data";r.title="Original map fixture";r.creator="Project fixture";
    r.retrieved_on="2026-11-05";r.license_id="GPL-3.0-only";r.license_evidence_path="assets/source-license.txt";
    r.purpose="Studio verification";r.credit_text="Original map fixture";r.credit_group="Maps";r.review_status="pending";
    registry.save(r,file);
    check(registry.records().size()==1 && registry.inspect(r.id).find(r.credit_text)!=std::string::npos &&
        registry.records().front().sha256==persistence::sha256(read(file)),"Local import, hash and offline credits did not agree");
    refused(draft,[&]{(void)registry.seal(r.id);});
    auto revised=registry.records().front();revised.review_status="reviewed";
    registry.save(revised);
    const auto sealed=registry.seal(r.id);
    auto candidate=draft.collection();candidate.asset_records[r.id]=sealed.fingerprint;
    check(draft.replace(candidate),"Asset pin did not enter undo and canonical content");
    draft.save();
    const auto target="data/runtime/first-playable.json";
    const auto original=read(scratch.path/target);
    const studio::SnapshotRequest request{"data/drafts/catalog-authored.json",target,
        "0.1.0-dev.1","Catalog and provenance review","reject-prior-content",{}};
    const auto report=studio::review_snapshot(scratch.path,request);
    check(report.runtime_changed && report.report_json.find(sealed.evidence_hash)!=std::string::npos,
        "Snapshot review omitted the pinned file and evidence");
    put(license,"Externally changed evidence.\n");
    refused(draft,[&]{(void)studio::finalize_snapshot(scratch.path,request,report.plan_id);});
    check(read(scratch.path/target)==original,"Refused asset review changed the runtime collection");
    put(license,"Original geometry fixture for testing the asset authoring pipeline.\n");
    const auto finalized=studio::finalize_snapshot(scratch.path,request,report.plan_id);
    const auto manifest=Json::parse(read(scratch.path/"data/runtime/content-manifest.json"));
    check(finalized.content_version==request.content_version && studio::check_snapshot(scratch.path).collection_hash==finalized.collection_hash &&
        manifest.at("assets").size()==1 && manifest.at("assets")[0].at("license_evidence_hash")==sealed.evidence_hash,
        "Finalized snapshot omitted exact asset and credit identity");
    studio::AssetRegister fresh{scratch.path};
    auto current=read(scratch.path/"assets/manifest.json");put(scratch.path/"assets/manifest.json",current+"\n");
    refused(draft,[&]{fresh.save(revised);});
    check(read(scratch.path/"assets/manifest.json")==current+"\n","External asset manifest edit was overwritten");
}
}
int main() {
    try {Scratch s;studio::DraftDocument d{s.path,"data/runtime/first-playable.json"};scoped_sector_references(d.program());index_and_refactor(d);
        reusable_group(s.path);
        assets(s,d);
        studio::DraftDocument reopened{s.path,"data/drafts/catalog-authored.json"};
        check(reopened.program().canonical_content_bytes==d.program().canonical_content_bytes,"Catalog edits failed exact draft reopen");
        const auto library=studio::inspect_checkout(s.path);
        const auto draft=std::find_if(library.documents.begin(),library.documents.end(),[](const auto& doc) {
            return doc.relative_path=="data/drafts/catalog-authored.json";
        });
        check(draft!=library.documents.end() && std::any_of(draft->definitions.begin(),draft->definitions.end(),[](const auto& row) {
            return row.id=="research.efficient_serving" && row.state=="Deprecated";
        }) && std::any_of(draft->definitions.begin(),draft->definitions.end(),[](const auto& row) {
            return row.id=="catalog.literal" && row.state=="Disabled";
        }),"Disabled and deprecated definitions were lost from the reopened library");
        std::cout<<checks<<" catalog index, typed reference, refactor and persistence checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
