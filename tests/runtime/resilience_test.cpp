#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;
using namespace core;
using Json=nlohmann::json;
std::size_t checks=0;
void check(bool value,const char* why) {++checks;if (!value) throw std::runtime_error(why);}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& item){return item.id==id;});
    if (row==rows.end()) throw std::runtime_error("Missing expected record "+std::string(id));
    return *row;
}
const SimulationState& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
void apply(runtime::Runtime& game,CommandAction command) {
    const auto kind=command.index();
    const auto result=game.submit({state(game).revision,std::move(command)});
    if (!result.accepted()) throw std::runtime_error("Day "+std::to_string(state(game).date.value())+
        ", command "+std::to_string(kind)+": "+result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction command) {
    const auto before=game.checkpoint();
    check(!game.submit({state(game).revision,std::move(command)}).accepted() && game.checkpoint()==before,
        "A refused resilience action partially changed cash, work or randomness");
}
void advance(runtime::Runtime& game,OrdinalDay target) {
    while (state(game).date<target) {
        if (const auto* alert=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{alert->id});
        if (const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        apply(game,AdvanceDay{});
    }
}
Json read(const char* path) {std::ifstream file{path};return Json::parse(file);}
runtime::CompiledProgram compile(Json doc,SiteHazard kind) {
    for (auto& row:doc["definitions"]) {
        if (row["kind"]=="scenario" && row["id"]=="scenario.first_playable") {
            row["flows"]=Json::array({"operations.site_outcome"});
            row["rivals"]=Json::array();
            row["management"]["setbacks"]=Json::array();
        }
        if (row["kind"]=="site_resilience") for (auto& exposure:row["catalog"]["exposures"]) {
            for (auto& series:exposure["monthly_probability_bp"]) for (auto& value:series) value=0;
            if (exposure["state"]=="US-WA") exposure["monthly_probability_bp"][static_cast<std::size_t>(kind)][5]=9999;
        }
        if (row["kind"]=="site_resilience") row["catalog"]["insurer_capital_cents"]=7000000;
    }
    const auto registry=content::foundation_registry();
    return runtime::compile_collection(content::load_collection(doc.dump(),registry),registry,runtime::supported_effects());
}
runtime::Runtime prepared(const runtime::CompiledProgram& program) {
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-WA")};
    check(game.prepare_opening(0).accepted(),"The real campaign did not prepare");
    for (const auto& [id,region]:std::array<std::pair<std::string,std::string>,2>{{{"primary","US-WA"},{"backup","US-TX"}}}) {
        const BuildOwnedSite build{id,id,region,20000,CoolingMode::air};
        const auto quote=preview_site_build(state(game),build);
        apply(game,build);apply(game,OrderOwnedHardware{id,id,"rack.reference"});
        apply(game,HireSiteOperators{id,id,1});
        apply(game,ScheduleSiteAppointment{id,id,std::max(quote.built_on,quote.grid_on),AppointmentMode::remote});
    }
    apply(game,BuyData{"data.licensed_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartTraining{{"model.service","recipe.document_baseline","",{"data.licensed_documents"},
        "lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    advance(game,OrdinalDay{80});
    apply(game,StartEvaluation{"evidence.service","model.service","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{84});
    apply(game,DeployModel{"product.documents","model.service","evidence.service","owned/primary",false});
    return game;
}
std::string exact(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    const auto encoded=persistence::encode_save(program,game.checkpoint());
    check(persistence::decode_save(program,encoded).checkpoint==game.checkpoint(),"Site risks, protected cash or replica work failed exact reload");
    return encoded;
}
void forged(const runtime::CompiledProgram& program,Json doc) {
    doc.erase("sha256");doc["sha256"]=persistence::sha256(doc.dump());
    bool rejected=false;
    try {(void)persistence::decode_save(program,doc.dump());} catch (const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"A checksum-valid invented incident, policy claim or free replica survived restore");
}
const SiteIncident& incident(const runtime::Runtime& game) {return state(game).resilience->incidents.front();}
MilliCUPerDay effective(const runtime::Runtime& game,std::string_view source) {return find(state(game).capacity->sources,source).effective;}
void physical_and_financial(const Json& source) {
    const auto program=compile(source,SiteHazard::fire);
    auto ordinary=prepared(program),insured=ordinary,protected_site=ordinary,replicated=ordinary;
    apply(insured,InsureSite{"primary","insurance.repair"});
    refuse(insured,InsureSite{"primary","insurance.extended"});
    refuse(insured,InsureSite{"backup","insurance.repair"});
    apply(protected_site,BuySiteProtection{"primary","protection.backup"});
    apply(protected_site,BuySiteProtection{"primary","protection.hardening"});
    refuse(protected_site,BuySiteProtection{"primary","protection.backup"});
    apply(replicated,ReplicateService{"product.documents","owned/backup"});
    refuse(ordinary,ReplicateService{"product.documents","owned/primary"});
    refuse(replicated,StartResearch{"research.reproducible_tooling",{"owned/backup",MilliCUPerDay{60000},1}});
    refuse(replicated,DeployModel{"product.documents","model.service","evidence.service","owned/backup",false});
    refuse(replicated,RetireOwnedHardware{"backup"});
    advance(ordinary,OrdinalDay{151});advance(insured,OrdinalDay{151});advance(protected_site,OrdinalDay{151});advance(replicated,OrdinalDay{151});
    const auto& samples=state(ordinary).resilience->samples;
    const auto sample=std::find_if(samples.begin(),samples.end(),[](const auto& row){return row.site_id=="primary" && row.sampled_on==OrdinalDay{151};});
    check(sample!=samples.end() && sample->outcome==5,"The matched seasonal fixture did not produce its named hazard");
    const auto onset=sample->event_on;
    const auto pending=exact(program,replicated);
    runtime::Runtime resumed{program,persistence::decode_save(program,pending).checkpoint};
    advance(ordinary,onset);advance(insured,onset);advance(protected_site,onset);advance(replicated,onset);advance(resumed,onset);
    check(replicated.checkpoint()==resumed.checkpoint(),"A saved future site occurrence rerolled after reload");
    check(required_site_alert(state(ordinary)) && state(ordinary).paused,"Physical disruption did not pause before further work");
    refuse(ordinary,AdvanceDay{});
    check(effective(ordinary,"owned/primary").value()==0 && effective(insured,"owned/primary").value()==0 &&
        incident(ordinary).repair_due==incident(insured).repair_due && incident(ordinary).restoration_days==incident(insured).restoration_days,
        "Insurance changed physical hardware availability");
    check(incident(protected_site).repair_cost.value()*2==incident(ordinary).repair_cost.value() &&
        incident(protected_site).repair_due<incident(ordinary).repair_due && incident(protected_site).restoration_days.value()==0,
        "Preparedness and offsite backups failed their distinct damage and data restoration mechanisms");
    const auto& replica=state(replicated).resilience->replicas.front();
    const auto replica_id=replica.id;
    check(replica.ready_on && *replica.ready_on<onset &&
        find(state(replicated).capacity->projects,find(state(replicated).market->products,"product.documents").capacity_project_id).source_id=="owned/backup",
        "A funded synchronized replica did not fail over onto the geographically separate rack");
    const auto claim_day=incident(insured).claim_due;
    advance(ordinary,claim_day);advance(insured,claim_day);advance(replicated,claim_day);
    check(incident(insured).paid_on==claim_day && incident(insured).claim==Cents{1680000} &&
        effective(insured,"owned/primary").value()==0,"Insurance did not reimburse its actual deductible loss during the physical outage");
    const auto& report=state(replicated).market->days.back();
    check(!report.discretionary.empty() && std::all_of(report.discretionary.begin(),report.discretionary.end(),[](const auto& row){return row.product_id=="product.documents";}),
        "The replica lost delivery attribution or created a second product vendor");
    check(state(replicated).market->products.size()==1 && state(replicated).resilience->replicas.size()==1,
        "Replication duplicated the actual market service");
    const auto encoded=exact(program,replicated);
    auto damaged=Json::parse(encoded);
    damaged["checkpoint"]["world"]["simulation"]["resilience"]["replicas"][0]["ready_on"]=0;
    forged(program,damaged);
    damaged=Json::parse(encoded);
    damaged["checkpoint"]["world"]["simulation"]["resilience"]["incidents"][0]["repair_cost_cents"]=1;
    forged(program,damaged);
    damaged=Json::parse(exact(program,insured));
    damaged["checkpoint"]["world"]["simulation"]["resilience"]["incidents"][0]["claim_cents"]=2000000;
    forged(program,damaged);
    damaged=Json::parse(encoded);
    damaged["checkpoint"]["world"]["simulation"]["resilience"]["alerts"][0]["required"]=false;
    forged(program,damaged);
    advance(protected_site,advance_days(incident(protected_site).repair_due,Days{1}));
    check(effective(protected_site,"owned/primary")==MilliCUPerDay{100000},
        "Paid repairs and an installed offsite copy did not restore the site at the shortened boundary");
    const auto fully_restored=advance_days(incident(ordinary).repair_due,Days{7});
    advance(ordinary,fully_restored);advance(insured,fully_restored);advance(replicated,fully_restored);
    check(effective(ordinary,"owned/primary")==MilliCUPerDay{100000} &&
        find(state(replicated).capacity->projects,find(state(replicated).market->products,"product.documents").capacity_project_id).source_id=="owned/primary",
        "Physical repairs, data recovery or return to the primary rack failed");
    apply(replicated,StopServiceReplica{replica_id});
    check(find(state(replicated).capacity->sources,"owned/backup").replica_reserved.value()==0,"Stopping the replica retained hidden standby capacity");
    (void)exact(program,replicated);(void)exact(program,ordinary);(void)exact(program,insured);
    check(std::any_of(insured.current_state().runtime.activations.begin(),insured.current_state().runtime.activations.end(),[](const auto& row){
        return row.definition_id=="operations.site_outcome" && row.committed_source.has_value();
    }),"Actual site outcomes did not reach the shared committed news runtime");
}
void emergency_power(const Json& source) {
    const auto program=compile(source,SiteHazard::grid);
    auto unprotected=prepared(program),protected_site=unprotected;
    apply(protected_site,BuySiteProtection{"primary","protection.generator"});
    advance(unprotected,OrdinalDay{151});advance(protected_site,OrdinalDay{151});
    const auto& samples=state(unprotected).resilience->samples;
    const auto sample=std::find_if(samples.begin(),samples.end(),[](const auto& row){return row.site_id=="primary" && row.sampled_on==OrdinalDay{151};});
    check(sample!=samples.end() && sample->outcome==1,"The grid fixture missed its sampled outage");
    const auto onset=sample->event_on;
    advance(unprotected,onset);advance(protected_site,onset);
    check(effective(unprotected,"owned/primary").value()==0 && effective(protected_site,"owned/primary")==MilliCUPerDay{100000},
        "The paid generator failed to supply its finite compatible facility power");
    advance(protected_site,advance_days(onset,Days{1}));
    const auto& meter=find(state(protected_site).infrastructure->sites,"primary");
    check(meter.last_energy_wh>0 && meter.last_utility_cost.value()>0,"Emergency generation used free fuel or failed to meter work");
    advance(protected_site,advance_days(onset,Days{3}));
    check(effective(protected_site,"owned/primary").value()==0,"Emergency generation continued after its finite fuel endurance");
    advance(protected_site,advance_days(onset,Days{4}));
    check(effective(protected_site,"owned/primary")==MilliCUPerDay{100000},"The grid failed to return at its actual recovery date");
    (void)exact(program,protected_site);
}
void heat_and_prevention(const Json& source) {
    const auto program=compile(source,SiteHazard::heat);
    auto ordinary=prepared(program),cooled=ordinary,maintenance=ordinary;
    apply(cooled,BuySiteProtection{"primary","protection.cooling"});
    apply(maintenance,BuySiteProtection{"primary","protection.maintenance"});
    advance(ordinary,OrdinalDay{151});advance(cooled,OrdinalDay{151});advance(maintenance,OrdinalDay{151});
    const auto& rows=state(ordinary).resilience->samples;
    const auto sample=std::find_if(rows.begin(),rows.end(),[](const auto& row){return row.site_id=="primary" && row.sampled_on==OrdinalDay{151};});
    check(sample!=rows.end() && sample->outcome==2,"The heat fixture missed its sampled event");
    const auto onset=sample->event_on;
    advance(ordinary,onset);advance(cooled,onset);advance(maintenance,onset);
    check(effective(ordinary,"owned/primary")==MilliCUPerDay{55000} && effective(cooled,"owned/primary")==MilliCUPerDay{84250},
        "Cooling preparedness did not reduce the actual thermal capacity loss");
    check(effective(maintenance,"owned/primary")==effective(ordinary,"owned/primary") &&
        incident(maintenance).repair_due==incident(ordinary).repair_due,"Fire prevention altered an unrelated heat hazard");
    const auto stable=ordinary.checkpoint();
    (void)preview_site_power(state(ordinary),"primary",state(ordinary).date);
    (void)project_company_cash(state(ordinary),advance_days(state(ordinary).date,Days{7}));
    check(ordinary.checkpoint()==stable,"A site preview consumed future randomness or changed actual work");
    (void)exact(program,cooled);
}
}
int main(int argc,char** argv) {
    try {
        const auto source=read(argc==2?argv[1]:PGP_RESILIENCE_SOURCE);
        physical_and_financial(source);emergency_power(source);heat_and_prevention(source);
        std::cout<<checks<<" site resilience checks passed\n";return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
