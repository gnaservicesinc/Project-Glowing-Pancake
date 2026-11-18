#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/content/infrastructure_catalog.hpp>
#include "../../src/runtime/competition_internal.hpp"
#include "../../src/runtime/competition_people.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){using namespace pgp;try{
    auto doc=nlohmann::json::parse(persistence::read_bounded(PGP_WORLD_HIRING_SOURCE,content::maximum_document_bytes));
    for(auto& d:doc["definitions"])if(d["id"]=="scenario.founder_campaign") {d["competition"]["domestic"]=1;d["competition"]["international"]=1;d["competition"]["heavyweights"]=0;}
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.route="bootstrap";f.office="home";f.personal_investment_cents=10000000;
    for(const auto& chapter:scenario.founding->scenes)f.choices.emplace(chapter.id,chapter.choices.front().id);
    runtime::ScenarioOptions options;options.founding=f;options.identity=runtime::CampaignIdentity{"World Hiring","Alex Vale","they/them"};
    auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    checkpoint.world.enabled_flows=std::vector<std::string>{};runtime::Runtime initial{program,checkpoint};check(initial.prepare_opening(checkpoint.world.simulation.revision).accepted(),"Opening");checkpoint=initial.current_state();
    check(checkpoint.world.simulation.infrastructure->catalog.states.size()==51,"Player's US map retains its state profiles");
    check(checkpoint.world.competition->companies.size()==2,"Only configured companies enter");
    auto profile_json=content::canonical_site_profile(scenario.management->people->international_headquarters.front().region);
    profile_json.pop_back();profile_json+=",\"id\":\"US-CA\"}";bool duplicate_refused=false;
    try{(void)content::load_site_profile(profile_json);}catch(const std::invalid_argument&){duplicate_refused=true;}check(duplicate_refused,"Duplicate regional profile fields are refused");
    std::set<std::string> overseas;
    for(const auto& rival:scenario.rivals)if(rival.planning && rival.planning->group=="international") {
        const auto state=runtime::competition_detail::opening_rival(checkpoint.world,program,rival,checkpoint.world.simulation.date);
        check(state.management->people && state.management->people->employees.size()==rival.staff,"Every international major has a real opening roster");
        check(state.infrastructure->catalog.states.size()==52,"Overseas HQ extends the shared tariff catalogue by one actual region");
        check(!state.infrastructure->executive_base.starts_with("US-"),"International HQ is not assigned a US state");
        overseas.insert(state.infrastructure->executive_base);core::validate_simulation(state);
        for(const auto& employee:state.management->people->employees)check(core::hq_workstation_ready(state,employee.terms.profile.id),"Overseas employees have powered computers");
    }
    check(overseas.size()==6,"All six overseas HQ regions have separate authored identities");
    bool canadian=false;for(const auto& [id,person]:checkpoint.world.simulation.management->people->industry)if(person.state=="CA-ON"){canadian=true;check(core::social_distance(checkpoint.world.simulation,"founder",id)=="remote","International contacts use the remote geography rules");}
    check(canadian,"Actually entered Canadian HQ appears in the industry network");
    // Create real temporary staffing pressure through an ordinary paid training
    // command, then let the normal weekly industry planner fill the vacancy.
    auto& employer=checkpoint.world.competition->companies.front();const auto& definition=runtime::rival_definition(program,employer.definition_id);
    if(std::ranges::none_of(employer.simulation.lab->licenses,[&](const auto& license){return license.offer_id==definition.data_id;}))check(runtime::competition_detail::submit_rival(employer,core::BuyData{definition.data_id}),"Paid training data");
    const auto opening=employer.simulation.management->people->employees.size();
    core::TrainingRequest work{"staffing-pressure",definition.recipe_id,"",{definition.data_id},"lease.initial",core::MilliCUPerDay{30000},3,definition.training_work};
    check(runtime::competition_detail::submit_rival(employer,core::StartTraining{work}),"Real work reserves three employees");
    const auto no_budget=runtime::competition_detail::plan_individual_hire(employer.simulation,core::Cents{0},core::Cents{0});check(!no_budget,"No free staffing when company funds are unavailable");
    runtime::synchronize_industry_people(checkpoint.world,program);runtime::Runtime game{program,checkpoint};
    const auto save=[&]{check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Exact international and hiring save");};save();
    bool hired=false;std::string person;
    for(int day=0;day<84 && !hired;++day) {
        const auto result=game.submit({game.current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);
        const auto& company=game.current_state().world.competition->companies.front();
        if(company.simulation.management->people->employees.size()>opening) {
            const auto& employee=company.simulation.management->people->employees.back();person=employee.terms.profile.id;
            check(employee.terms.recruitment && employee.terms.recruitment->hired_on==employee.joined_on,"AI hire has a signed negotiation history");
            check(core::hq_workstation_ready(company.simulation,person),"AI hire has a real furnished workstation");
            check(employee.ready_on>employee.joined_on,"AI hire still completes onboarding");hired=true;
        }
    }
    if(!hired)for(const auto& decision:game.current_state().world.competition->companies.front().planning->decisions)std::cerr<<decision.action<<" "<<decision.explanation<<'\n';
    check(hired,"Weekly planner actually completes an individual hire");save();
    auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));runtime::Runtime resumed{program,loaded.checkpoint};
    for(int day=0;day<8;++day)for(auto* live:{&game,&resumed}) {const auto result=live->submit({live->current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);}
    check(game.current_state()==resumed.current_state(),"International hiring and onboarding continue identically after reload");
    const auto& company=game.current_state().world.competition->companies.front().simulation;
    const auto worker=std::ranges::find_if(company.management->people->employees,[&](const auto& e){return e.terms.profile.id==person;});check(worker->ready_on<=company.date,"Hired employee becomes productive on the actual date");
    check(std::ranges::any_of(company.accounts->postings,[&](const auto& posting){return posting.key==person && posting.credit==core::Account::cash && posting.amount==worker->terms.signing;}),"Signing payment leaves actual company cash");
    {
        auto grow_cp=game.current_state();
        auto& rival=grow_cp.world.competition->companies.front().simulation;
        const auto& start_hq=core::current_headquarters(rival);
        const int start_area=start_hq.width*start_hq.depth;
        const std::size_t start_premises=rival.infrastructure->workplace->headquarters->premises.size();
        for(int y=0;y<start_hq.depth;++y) for(int x=0;x<start_hq.width;++x) {
            core::manage_headquarters(rival,core::BuyHQEquipment{"desk"},core::Cents{0});
            const auto id=rival.infrastructure->workplace->headquarters->items.back().id;
            rival.infrastructure->workplace->headquarters->items.back().playtest_delivery=true;
            rival.infrastructure->workplace->headquarters->items.back().delivered_on=rival.date;
            core::PlaceHQEquipment place{id,0,x,y,false};
            if(core::hq_placement_blocker(rival,place).empty())
                core::manage_headquarters(rival,place,core::Cents{0});
        }
        std::string extra_offer;
        for(const auto& offer:rival.infrastructure->catalog.workplace->headquarters->equipment)
            if(!core::hq_desk_device(offer.kind) && offer.width>0 && offer.depth>0) {extra_offer=offer.id;break;}
        check(!extra_offer.empty(),"Rival HQ equipment catalog must include floor equipment.");
        core::manage_headquarters(rival,core::BuyHQEquipment{extra_offer},core::Cents{0});
        rival.infrastructure->workplace->headquarters->items.back().playtest_delivery=true;
        rival.infrastructure->workplace->headquarters->items.back().delivered_on=rival.date;
        const int wait_days=std::max(70,static_cast<int>(grow_cp.world.competition->setup->review_interval.value())*6);
        runtime::Runtime grow_game{program,grow_cp};
        for(int d=0;d<wait_days;++d) {
            const auto& ws=grow_game.current_state().world;
            const auto& cur_rival=ws.competition->companies.front().simulation;
            const auto& cur_hq=core::current_headquarters(cur_rival);
            if(cur_rival.infrastructure->workplace->headquarters->premises.size()>start_premises &&
               cur_hq.width*cur_hq.depth>start_area) break;
            if(ws.simulation.liquidity) if(const auto w=core::liquidity_report(*ws.simulation.liquidity).pending_warning)
                check(grow_game.submit({ws.simulation.revision,core::AcknowledgeLiquidityWarning{*w}}).accepted(),"Acknowledge liquidity warning.");
            if(ws.simulation.management) {
                const auto notices=ws.simulation.management->notices;
                for(const auto& n:notices) if(n.required && !n.acknowledged)
                    check(grow_game.submit({ws.simulation.revision,core::AcknowledgeManagementNotice{n.id}}).accepted(),"Acknowledge notice.");
            }
            check(grow_game.submit({ws.simulation.revision,core::AdvanceDay{}}).accepted(),"Advance day during rival HQ expansion.");
        }
        const auto& after_rival=grow_game.current_state().world.competition->companies.front().simulation;
        const auto& after_hq=core::current_headquarters(after_rival);
        if(!(after_rival.infrastructure->workplace->headquarters->premises.size()>start_premises &&
             after_hq.width*after_hq.depth>start_area)) {
            const auto& comp=grow_game.current_state().world.competition->companies.front();
            const std::string last_act=comp.planning->decisions.empty()?std::string{"none"}:
                (comp.planning->decisions.back().action+":"+comp.planning->decisions.back().subject);
            throw std::runtime_error("Rival HQ expansion did not trigger: premises="+
                std::to_string(after_rival.infrastructure->workplace->headquarters->premises.size())+
                " start="+std::to_string(start_premises)+" area="+std::to_string(after_hq.width*after_hq.depth)+
                " start_area="+std::to_string(start_area)+" cash="+std::to_string(after_rival.cash.value())+
                " last="+last_act);
        }
    }
    std::cout<<"World hiring: six overseas regions, real rosters/costs, weekly offers, paid workstation, signed hire, onboarding and exact continuation passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
