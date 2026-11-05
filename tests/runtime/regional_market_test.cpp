#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>

namespace {
using namespace pgp;
using namespace core;
using Json=nlohmann::json;
std::size_t checks=0;
void check(bool condition,const char* failure) {
    ++checks;
    if (!condition) throw std::runtime_error(failure);
}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& entry) {return entry.id==id;});
    if (row==rows.end()) throw std::runtime_error("Missing expected record: "+std::string(id));
    return *row;
}
const runtime::WorldState& world(const runtime::Runtime& game) { return game.current_state().world; }
const SimulationState& state(const runtime::Runtime& game) { return world(game).simulation; }
void apply(runtime::Runtime& game,CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction action) {
    const auto before=game.checkpoint();
    check(!game.submit({state(game).revision,std::move(action)}).accepted() && game.checkpoint()==before,
        "A refused regional or infrastructure transaction mutated the world");
}
void advance(runtime::Runtime& game,OrdinalDay through) {
    while (state(game).date<through) {
        if (const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        apply(game,AdvanceDay{});
    }
}
runtime::Runtime begin(const runtime::CompiledProgram& program,std::string_view scenario) {
    runtime::Runtime game{program,runtime::begin_scenario(program,scenario,"US-WA")};
    check(game.prepare_opening(state(game).revision).accepted(),"Campaign morning did not prepare");
    return game;
}
std::string exact(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    const auto bytes=persistence::encode_save(program,game.checkpoint());
    check(persistence::decode_save(program,bytes).checkpoint==game.checkpoint(),"Regional/wholesale checkpoint did not reload exactly");
    return bytes;
}

void local_launches(const runtime::CompiledProgram& program) {
    auto game=begin(program,"scenario.first_playable");
    const auto& simulation=state(game);
    check(simulation.market->segments.size()==48 && simulation.deployments->catalog.size()==15,
        "The playable preset did not receive all 48 segments and twelve new templates");
    std::set<std::string> regions;
    for (const auto& row:simulation.market->segments) regions.insert(row.region_id);
    check(regions.size()==8 && find(simulation.market->segments,"segment.europe.public_services").requires_local,
        "Regional profiles did not retain their distinct local hardware requirements");
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartResearch{"research.reproducible_tooling",{"lease.initial",MilliCUPerDay{10000},1}});
    const auto start=state(game).date;
    apply(game,StartTraining{{"model.open","recipe.document_baseline","",{"data.open_documents"},
        "lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    advance(game,advance_days(start,Days{60}));
    apply(game,StartEvaluation{"evidence.open","model.open","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,advance_days(state(game).date,Days{4}));
    refuse(game,DeployModel{"product.education_model","model.open","evidence.open","lease.initial",false,"region.europe"});
    refuse(game,DeployModel{"product.education_model","model.open","evidence.open","",false,"region.invalid"});
    apply(game,DeployModel{"product.education_model","model.open","evidence.open","",false,"region.europe"});
    apply(game,DeployModel{"product.private_organization","model.open","evidence.open","",false,"region.europe"});
    refuse(game,SetMarketPrice{"product.education_model",Cents{1}});
    refuse(game,DeployModel{"product.private_organization","model.open","evidence.open","",false,"region.us"});
    advance(game,advance_days(state(game).date,Days{1}));
    const auto& settled=state(game).market->days.back();
    std::set<std::string> products;
    for (const auto& row:settled.discretionary) {
        check(row.segment_id.starts_with("segment.europe.") && row.customer_compute==required_inference(row.tasks,MilliCU{20}),
            "Local launch escaped its territory or used unmetered buyer work");
        if (row.product_id=="product.education_model") check(row.charged==Cents{0},"Open-weight release generated a paid invoice");
        else check(row.charged.value()>0,"Licensed useful work omitted its actual charge");
        products.insert(row.product_id);
    }
    check(products.size()==2 && find(state(game).capacity->projects,"serving.education_model").completed_work==MilliCU{0} &&
        find(state(game).capacity->projects,"serving.private_organization").completed_work==MilliCU{0},
        "Open and licensed work did not reach real customers using only their hardware");
    check(std::any_of(settled.discretionary.begin(),settled.discretionary.end(),[](const auto& row){return row.segment_id=="segment.europe.public_services";}),
        "Locally required public customers did not receive eligible useful work");
    advance(game,to_ordinal({2014,6,1}));
    const auto opportunity=market_opportunity(*state(game).market,*state(game).capacity,state(game).date);
    const auto& customers=find(opportunity.segments,"segment.europe.enterprise");
    const auto incumbent=std::find_if(customers.edges.begin(),customers.edges.end(),[](const auto& row) {
        return row.product_id=="product.private_organization";
    });
    check(incumbent!=customers.edges.end() && incumbent->retained_tasks>0 && incumbent->task_limit &&
        *incumbent->task_limit>incumbent->retained_tasks,"Monthly settled adoption did not preserve a feasible incumbent");
    apply(game,SetMarketPrice{"product.private_organization",Cents{10000}});
    advance(game,advance_days(state(game).date,Days{1}));
    check(std::none_of(state(game).market->days.back().discretionary.begin(),state(game).market->days.back().discretionary.end(),
        [](const auto& row){return row.product_id=="product.private_organization";}),"Churn forced purchases above willingness to pay");
    (void)exact(program,game);
}

void provider_route(const runtime::CompiledProgram& program) {
    auto game=begin(program,"scenario.world_competition");
    constexpr auto included="wholesale.power_included";
    constexpr auto metered="wholesale.metered";
    constexpr auto first="company.roster.domestic.1";
    constexpr auto second="company.roster.domestic.2";
    constexpr auto third="company.roster.domestic.3";
    constexpr auto fourth="company.roster.international.1";
    const BuildOwnedSite build{"washington","Washington wholesale","US-WA",32000,CoolingMode::air};
    const auto quote=preview_site_build(state(game),build);
    const auto visit=std::max(quote.built_on,quote.grid_on);
    apply(game,build);
    apply(game,OrderOwnedHardware{"reference_a","washington","rack.reference"});
    apply(game,OrderOwnedHardware{"reference_b","washington","rack.reference"});
    apply(game,HireSiteOperators{"operators","washington",2});
    apply(game,ScheduleSiteAppointment{"inspect","washington",visit,AppointmentMode::remote});
    apply(game,NoticeInitialLeaseTermination{});
    const auto sign=[&](std::string_view plan,std::string_view source,std::string_view buyer,bool accepted) {
        const auto before=game.checkpoint();
        const auto result=game.signWholesale(state(game).revision,plan,source,buyer);
        if (accepted && !result.accepted()) throw std::runtime_error(result.explanation);
        check(result.accepted()==accepted && (accepted || game.checkpoint()==before),"Wholesale quote bypassed ownership, funding or exclusive supply");
    };
    sign(metered,"owned/reference_a",first,false);
    sign(metered,"lease.initial",first,false);
    advance(game,visit);
    sign(metered,"owned/reference_a",first,true);
    sign(included,"owned/reference_a",second,true);
    sign(included,"owned/reference_a",third,false);
    sign(included,"owned/reference_b",first,false);
    sign(included,"owned/reference_b",third,true);
    sign(included,"owned/reference_b",fourth,true);
    check(company_capacity(find(state(game).capacity->sources,"owned/reference_a"))==MilliCUPerDay{0},"Seller reused reserved buyer capacity");
    refuse(game,RetireOwnedHardware{"reference_a"});
    AdvanceDay forged_use;
    forged_use.wholesale_work={{"owned/reference_a",MilliCU{1}}};
    refuse(game,std::move(forged_use));
    const auto profit=accounts_report(*state(game).accounts).profit;
    const auto opening=state(game).date;
    apply(game,SetSitePowerLimit{"washington",0});
    advance(game,advance_days(opening,Days{1}));
    for (const auto& contract:world(game).competition->wholesale) {
        const auto& row=contract.days.back();
        check(row.available==MilliCUPerDay{0} && row.used==MilliCU{0} && row.electricity==Cents{0} &&
            row.service_credit.value()>0 && row.seller_posting==0 && row.buyer_posting==0,
            "A supplier outage charged either company for unavailable work");
    }
    apply(game,SetSitePowerLimit{"washington",32000});
    advance(game,advance_days(opening,Days{47}));
    const auto& contracts=world(game).competition->wholesale;
    check(std::any_of(contracts.front().days.begin(),contracts.front().days.end(),[](const auto& row) {
        return row.used.value()>0 && row.electricity.value()>0 && row.seller_posting!=0 && row.buyer_posting!=0;
    }),"Paid imported work never reached a rival's real Lab or metered the owned rack");
    const auto net=checked_unit_subtract(accounts_report(*state(game).accounts).profit,profit);
    check(net.value()>0 && state(game).lab->models.empty() && state(game).market->products.empty(),
        "The actual funded provider route could not cover operating costs without a model");
    check(std::all_of(state(game).market->days.begin(),state(game).market->days.end(),[](const auto& day){return day.discretionary.empty();}),
        "The compute host claimed its buyer's delivered customer work");
    const auto bytes=exact(program,game);
    auto tampered=Json::parse(bytes);
    tampered["checkpoint"]["world"]["competition"]["wholesale"][0]["days"][0][2]=0;
    tampered.erase("sha256");
    tampered["sha256"]=persistence::sha256(tampered.dump());
    bool rejected=false;
    try { (void)persistence::decode_save(program,tampered.dump()); }
    catch (const persistence::PersistenceError&) { rejected=true; }
    check(rejected,"Checksum-valid forged wholesale credit survived restore");
    runtime::Runtime resumed{program,persistence::decode_save(program,bytes).checkpoint};
    const auto expiry=contracts.front().ends_before;
    advance(game,expiry);advance(resumed,expiry);
    check(game.checkpoint()==resumed.checkpoint(),"Wholesale term boundary differed after exact reload");
    check(company_capacity(find(state(game).capacity->sources,"owned/reference_a"))==MilliCUPerDay{50000},
        "Expired compute did not return to its physical owner");
    (void)exact(program,game);
    std::cout<<"Provider operating profit over 47 days "<<net.value()<<" cents. Saved checkpoint "<<bytes.size()<<" bytes.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::ifstream input{argc==2?argv[1]:PGP_MARKET_SOURCE};
        auto document=Json::parse(input);
        for (auto& row:document["definitions"]) {
            if (row["kind"]!="scenario" || !row.contains("service_catalog")) continue;
            row["flows"]=Json::array();
            row["management"]["setbacks"]=Json::array();
            if (row["id"]=="scenario.world_competition") {
                row["competition"]["domestic"]=3;
                row["competition"]["international"]=1;
                row["competition"]["heavyweights"]=0;
            } else if (row["id"]=="scenario.first_playable") row["rivals"]=Json::array();
        }
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry,runtime::supported_effects());
        local_launches(program);
        provider_route(program);
        std::cout<<checks<<" regional and wholesale checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
