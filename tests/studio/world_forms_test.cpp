#include <pgp/studio/world_forms.hpp>
#include <pgp/persistence/storage.hpp>
#include <pgp/runtime/scenario.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#if PGP_WORLD_QT
#include "world_controller.hpp"
#include <QCoreApplication>
#endif

namespace {
using namespace pgp;namespace fs=std::filesystem;
int checks=0;
constexpr std::string_view scenario_id="scenario.first_playable";
const std::string root="/definitions/scenario.first_playable";
void check(bool yes,const char* why) {++checks;if(!yes) throw std::runtime_error(why);}
void put(const fs::path& p,std::string_view v) {fs::create_directories(p.parent_path());std::ofstream out{p};out<<v;out.close();if(!out) throw std::runtime_error("Cannot write world form fixture");}
std::string read(const fs::path& p) {return persistence::read_bounded(p,content::maximum_document_bytes);}
struct Scratch {
    fs::path path=fs::path{PGP_WORLD_SCRATCH}/("world-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        fs::create_directories(path/".git");put(path/"CMakeLists.txt","# test\n");put(path/"src/content/collection.cpp","// test\n");
        put(path/"project-versions.json",read(PGP_WORLD_VERSIONS));put(path/"data/runtime/first-playable.json",read(PGP_WORLD_SOURCE));
    }
    ~Scratch() {std::error_code e;fs::remove_all(path,e);}
};
const content::ScenarioDefinition& scenario(const studio::DraftDocument& d) {
    return std::get<content::ScenarioDefinition>(d.collection().definitions.at(std::string(scenario_id)).data);
}
template<class Action> void refused(studio::DraftDocument& d,Action action) {
    const auto before=d.program().canonical_content_bytes;const auto revision=d.edit_revision();
    bool no=false;try {action();}catch(const std::exception&) {no=true;}
    check(no && before==d.program().canonical_content_bytes && revision==d.edit_revision(),"Invalid form input changed the document or undo revision");
}
void forms(Scratch& s) {
    studio::DraftDocument d{s.path,"data/runtime/first-playable.json"};
    const auto original=d.program().canonical_content_bytes;
    const auto rows=studio::world_outline(d,scenario_id);
    std::size_t presets=0;
    for(const auto& [id,definition]:d.collection().definitions) if(std::holds_alternative<content::ScenarioDefinition>(definition.data)) {
        check(!studio::world_outline(d,id).empty() && !studio::WorldForm::open(d,id,"/definitions/"+id).fields().empty(),"A bundled scenario cannot be opened through world forms");++presets;
    }
    check(presets>=5,"World forms were not exercised on all five bundled presets");
    check(rows.size()>100 && std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.key==root+"/management/scoring";}),"World outline omits objectives and implemented nested systems");
    const auto places=studio::world_states(d,scenario_id);
    check(places.size()==51 && places.front().id.starts_with("US-"),"World list omits domestic state profiles");
    auto form=studio::WorldForm::open(d,scenario_id,root);
    check(std::any_of(form.fields().begin(),form.fields().end(),[](const auto& f){return f.key=="cash_cents" && f.help.find("constant-2014 cents")!=std::string::npos;}),"Opening money lost its named unit");
    check(form.apply(d,{{"cash_cents","120001234"},{"seed","18446744073709551615"}}),"Valid opening resources did not apply");
    check(scenario(d).cash.value()==120001234 && scenario(d).seed==UINT64_MAX,"Opening resources lost exact cents or uint64 seed");
    refused(d,[&]{(void)form.apply(d,{{"cash_cents","1"}});});
    for(const auto bad:{"1.5","1e6","-1","18446744073709551616"}) refused(d,[&]{
        (void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"cash_cents",bad}});
    });
    refused(d,[&]{(void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"id","scenario.stolen"}});});
    refused(d,[&]{(void)studio::WorldForm::open(d,scenario_id,"/definitions/slice.market_shift").apply(d,{{"seed","3"}});});
    refused(d,[&]{(void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"unregistered_field","2"}});});
    check(d.undo() && d.program().canonical_content_bytes==original,"One undo did not restore the whole resources transaction");
    const auto opening_collection=d.collection();
    const auto references=root+"/lab/recipes/0/requires_research";
    while(!scenario(d).lab->recipes.front().requires_research.empty())
        (void)studio::WorldForm::open(d,scenario_id,references+"/"+std::to_string(scenario(d).lab->recipes.front().requires_research.size()-1)).remove(d);
    auto research=studio::WorldForm::append(d,scenario_id,references,0);
    check(research.apply(d,{{"","research.data_deduplication"}}) && scenario(d).lab->recipes.front().requires_research.size()==1,"An empty prerequisite list could not create a typed reference");
    check(d.undo(),"Research reference did not enter shared undo");
    check(d.replace(opening_collection) && d.program().canonical_content_bytes==original,"The reference-list fixture did not restore the complete opening");
    check(!studio::WorldForm::open(d,scenario_id,root+"/market/offers").can_append(),"The Lab could fabricate a starting deployment without model/evidence records");
    refused(d,[&]{(void)studio::WorldForm::append(d,scenario_id,root+"/market/offers",0);});
    const auto empty_financing=root+"/management/financing";
    while(!scenario(d).management->financing.empty())
        (void)studio::WorldForm::open(d,scenario_id,empty_financing+"/"+std::to_string(scenario(d).management->financing.size()-1)).remove(d);
    auto offer=studio::WorldForm::append(d,scenario_id,empty_financing,0);
    check(offer.apply(d,{{"id","finance.empty_list"},{"name","Author financing"}}),"An empty object list could not use an existing typed catalog template");
    check(scenario(d).management->financing.size()==1 && studio::preview_world(d,scenario_id,"US-CA").find("Exact save reload:")!=std::string::npos,
        "Authored financing did not reach a valid actual opening and exact reload");
    check(d.replace(opening_collection) && d.program().canonical_content_bytes==original,"The object-list fixture did not restore the complete opening");
    const auto ownership=scenario(d).ownership;const auto founder=scenario(d).management->scoring.founder;
    auto other=ownership.begin();if(other->first==founder) ++other;
    auto shares=studio::WorldForm::open(d,scenario_id,root+"/ownership_basis_points");
    const auto owner_change=std::map<std::string,std::string,std::less<>>{{founder,std::to_string(ownership.at(founder).value()-100)},{other->first,std::to_string(other->second.value()+100)}};
    refused(d,[&]{(void)shares.apply(d,owner_change);});
    const auto staged=shares.stage(d,owner_change);
    check(d.program().canonical_content_bytes==original,"Staging a cross-section edit changed the compiled draft");
    check(studio::apply_world_batch(d,scenario_id,{staged,{root+"/management/scoring",{{"opening_founder_share_bp",std::to_string(ownership.at(founder).value()-100)}}}}),"Coupled ownership/scoring batch was rejected");
    check(scenario(d).management->scoring.opening_founder_share==scenario(d).ownership.at(founder),"Batch left inconsistent founder/scoring terms");
    check(d.undo() && d.program().canonical_content_bytes==original,"Cross-section undo did not restore both ownership and scoring");
    const auto budget=scenario(d).management->default_policy_budget.value()+1234;
    check(studio::WorldForm::open(d,scenario_id,root+"/management").apply(d,{{"default_policy_budget_cents",std::to_string(budget)}}),"Management policy terms cannot be authored");
    const auto opening=runtime::begin_scenario(d.program(),scenario_id,"US-WA");
    check(opening.world.simulation.management->catalog.default_policy_budget.value()==budget,"Authored policy terms did not reach the real runtime");
    check(studio::WorldForm::open(d,scenario_id,places.front().path).apply(d,{{"grid_lead_days","63"}}),"Map-linked site profile cannot be authored");
    const auto financing=root+"/management/financing";
    auto add=studio::WorldForm::append(d,scenario_id,financing,0);
    check(add.apply(d,{{"id","finance.authored"},{"name","Author financing"},{"proceeds_cents","1234500"}}),"Typed offer template could not create a real offer");
    check(scenario(d).management->financing.back().id=="finance.authored","New financing offer lost its stable identity");
    const auto extra=financing+"/"+std::to_string(scenario(d).management->financing.size()-1);
    check(studio::WorldForm::open(d,scenario_id,extra).remove(d),"A referenced-checked list entry could not be removed");
    const auto region="/definitions/"+scenario(d).regional_markets+"/regions/0";
    check(studio::WorldForm::open(d,scenario_id,region).apply(d,{{"description","Authored regional context"}}),"Linked regional profiles are missing from world forms");
    const auto risk="/definitions/"+scenario(d).resilience+"/catalog";
    check(studio::WorldForm::open(d,scenario_id,risk).apply(d,{{"insurer_capital_cents","250000001"}}),"Linked resilience policy cannot be authored");
    const auto society="/definitions/"+scenario(d).society+"/catalog";
    check(studio::WorldForm::open(d,scenario_id,society).apply(d,{{"lag_months","3"}}),"Linked society lags cannot be authored");
    check(runtime::begin_scenario(d.program(),scenario_id,"US-WA").world.society->catalog.lag_months==3,
        "The authored welfare lag did not reach the actual opening runtime");
    refused(d,[&]{(void)studio::WorldForm::open(d,scenario_id,society).apply(d,{{"lag_months","0"}});});
    refused(d,[&]{(void)studio::WorldForm::append(d,scenario_id,society+"/regions/0/initial_components_bp",0);});
    const auto institutions="/definitions/"+scenario(d).authority+"/catalog";
    check(studio::WorldForm::open(d,scenario_id,institutions+"/offers/0").apply(d,{{"setup_cents","50000001"}}),
        "A funded institutional role cannot be authored.");
    const auto terms=runtime::begin_scenario(d.program(),scenario_id,"US-WA").world.authority->catalog;
    check(terms.offers.front().setup==core::Cents{50000001},"The authored institutional fee did not reach the game.");
    refused(d,[&]{(void)studio::WorldForm::append(d,scenario_id,institutions+"/offers",0);});
    refused(d,[&]{(void)studio::WorldForm::append(d,scenario_id,institutions+"/identity_effects_bp",0);});
    const auto preview_before=d.program().canonical_content_bytes;const auto report=studio::preview_world(d,scenario_id,"US-WA");
    check(report.find("Exact save reload:")!=std::string::npos && report.find("company.cash_cents")!=std::string::npos &&
        report.find("Enabled trigger targets and effects")!=std::string::npos,"Actual opening/fact/trigger impact preview is missing");
    check(d.program().canonical_content_bytes==preview_before,"Opening inspection changed document state or random rules");
    refused(d,[&]{(void)studio::preview_world(d,scenario_id,"US-XX");});
    check(studio::duplicate_scenario(d,scenario_id,"scenario.authored"),"Scenario template duplication failed");
    check(std::holds_alternative<content::ScenarioDefinition>(d.collection().find("scenario.authored")->data),"New scenario is not a typed runtime definition");
    refused(d,[&]{(void)studio::duplicate_scenario(d,scenario_id,"scenario.authored");});
    check(studio::WorldForm::open(d,scenario_id,root).apply(d,{{"authority",""},{"society",""},{"civic",""},{"resilience",""},
        {"regional_markets",""},{"@enabled/infrastructure","false"}}) && studio::world_states(d,scenario_id).empty(),"Coupled optional systems could not be disabled");
    check(studio::add_missing_scenario_sections(d,scenario_id,"scenario.authored") && studio::world_states(d,scenario_id).size()==51,"Missing systems could not be restored from a complete typed scenario");
    d.save("world-authored.json");studio::DraftDocument reopened{s.path,"data/drafts/world-authored.json"};
    check(reopened.program().canonical_content_bytes==d.program().canonical_content_bytes && !d.dirty(),"World draft failed exact save/reopen");
    check(read(s.path/"data/runtime/first-playable.json")==read(PGP_WORLD_SOURCE),"World editing changed finalized runtime content");
    (void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"cash_cents","120009999"}});
    put(s.path/"data/drafts/world-authored.json",read(s.path/"data/drafts/world-authored.json")+"\n");
    refused(d,[&]{d.save();});
}
void frontier_forms(Scratch& s) {
    studio::DraftDocument d{s.path,"data/runtime/first-playable.json"};
    constexpr std::string_view century="scenario.century_frontier";
    const std::string catalog="/definitions/frontier.frontier_paths/catalog";
    const auto rows=studio::world_outline(d,century);
    check(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.key==catalog;}),"Century world forms omit their linked Frontier catalog");
    const auto gate=studio::WorldForm::open(d,century,catalog+"/gates/0");
    const auto target=std::find_if(gate.fields().begin(),gate.fields().end(),[](const auto& f){return f.key=="target";});
    check(target!=gate.fields().end() && target->control==studio::FormControl::choice && target->options.size()>8,
        "A Frontier gate cannot select its typed Lab research reference");
    const auto original=d.program().canonical_content_bytes;
    check(gate.apply(d,{{"kind","data"},{"target","data.open_documents"}}) && d.undo() && d.program().canonical_content_bytes==original,
        "A Frontier gate cannot change its technology kind and typed target in one validated edit");
    const auto conditions=catalog+"/paths/0/minimum_bp";
    const auto scores=catalog+"/recipes/0/ceiling_bp";
    auto form=studio::WorldForm::open(d,century,conditions);
    check(!form.can_append() && form.fields().size()==core::future_condition_count && form.fields()[0].label=="Progress",
        "Seeded conditions lost fixed named axes");
    form=studio::WorldForm::open(d,century,scores);
    check(!form.can_append() && form.fields().size()==core::frontier_extra_count && form.fields()[0].label=="Perception" &&
        !studio::WorldForm::open(d,century,scores+"/0").can_remove(),"Frontier capability axes became extensible or unnamed");
    refused(d,[&]{(void)studio::WorldForm::append(d,century,scores,0);});
    form=studio::WorldForm::open(d,century,catalog+"/panels/0");
    const auto before=d.program().canonical_content_bytes;
    const auto margin=std::find_if(form.fields().begin(),form.fields().end(),[](const auto& f){return f.key=="margin_bp";});
    check(margin!=form.fields().end() && form.apply(d,{{"margin_bp",std::to_string(std::stoul(margin->value)+1)}}) &&
        d.program().canonical_content_bytes!=before && d.undo() && d.program().canonical_content_bytes==before,
        "Frontier protocol edits did not validate and share the ordinary undo boundary");
}
#if PGP_WORLD_QT
void controller(Scratch& s) {
    studio::DraftDocument d{s.path,"data/runtime/first-playable.json"};studio::WorldController c;
    QObject::connect(&c,&studio::WorldController::documentEdited,[&]{c.set_document(&d);});
    c.set_document(&d,scenario_id);c.selectSection(QString::fromStdString(root));
    const auto before=d.program().canonical_content_bytes;const auto stale=c.ticket();
    check(!c.apply(stale,{{"cash_cents","1.5"}}) && d.program().canonical_content_bytes==before,"Qt form accepted a decimal or partially changed the draft");
    QString washington_path;
    for(const auto& location:c.states()) if(location.toMap().value(QStringLiteral("id"))==QStringLiteral("US-WA")) washington_path=location.toMap().value(QStringLiteral("path")).toString();
    c.selectState(QStringLiteral("US-WA"));check(!washington_path.isEmpty() && c.path()==washington_path && c.selectedState()==QStringLiteral("US-WA"),"Map/list selection lost the state identity");
    check(!c.apply(stale,{{"grid_lead_days","1"}}),"A stale Qt form ticket was accepted");
    const auto& ownership=scenario(d).ownership;const auto founder=scenario(d).management->scoring.founder;
    auto other=ownership.begin();if(other->first==founder) ++other;
    c.selectSection(QString::fromStdString(root+"/ownership_basis_points"));
    check(c.stage(c.ticket(),{{QString::fromStdString(founder),QString::number(ownership.at(founder).value()-100)},{QString::fromStdString(other->first),QString::number(other->second.value()+100)}}),"Qt could not stage coupled ownership fields");
    check(c.hasStaged() && d.program().canonical_content_bytes==before && !c.applyStaged(),"Staging mutated the draft or accepted an inconsistent founder baseline");
    c.selectSection(QString::fromStdString(root+"/management/scoring"));
    check(c.stage(c.ticket(),{{"opening_founder_share_bp",QString::number(ownership.at(founder).value()-100)}}) && c.applyStaged(),"Qt staged batch could not commit the matching scoring baseline");
    check(!c.hasStaged() && scenario(d).ownership.at(founder)==scenario(d).management->scoring.opening_founder_share,"Qt batch did not clear safely after committing both sections");
    check(d.undo(),"Qt batch did not create one undo boundary");c.set_document(&d);
    c.selectSection(QString::fromStdString(root));check(c.stage(c.ticket(),{{"cash_cents","120000001"}}),"Cannot construct stale staged batch");
    (void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"team_people","9"}});c.set_document(&d);
    check(c.hasStaged() && !c.applyStaged() && c.stagedSummary().contains(QStringLiteral("120000001")),"External edit discarded staged values or accepted stale proposals");
    c.discardStaged();check(!c.hasStaged(),"Explicit staged discard failed");
    c.setUnapplied({{"cash_cents","120000099"}});
    c.selectSection(QString::fromStdString(root+"/management"));
    check(c.hasUnapplied() && c.path()==QString::fromStdString(root),"Section navigation silently discarded unapplied fields");
    (void)studio::WorldForm::open(d,scenario_id,root).apply(d,{{"team_people","10"}});c.set_document(&d);
    check(c.hasStaged() && c.stagedSummary().contains(QStringLiteral("120000099")) && !c.applyStaged(),"External edits discarded unapplied values or accepted a stale batch");
    c.discardStaged();
    c.filter(QStringLiteral("default_policy_budget"));
    check(!c.sections().isEmpty(),"Field search did not find its owning policy section");
    c.filter({});
    check(c.preview(QStringLiteral("US-CA")) && c.report().contains(QStringLiteral("Exact save reload")),"Qt opening preview did not use the save codec");
    c.selectSection(QString::fromStdString(root+"/management"));check(c.apply(c.ticket(),{{"default_policy_budget_cents","5000001"}}) && c.report().isEmpty(),"An applied edit retained a stale opening report");
}
#endif
}
int main(int argc,char** argv) {
#if PGP_WORLD_QT
    QCoreApplication app{argc,argv};
#else
    (void)argc;(void)argv;
#endif
    try {Scratch s;forms(s);frontier_forms(s);
#if PGP_WORLD_QT
        controller(s);
#endif
        std::cout<<checks<<" world authoring, atomic batch, runtime preview and persistence checks passed\n";return 0;
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
