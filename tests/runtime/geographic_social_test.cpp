#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/competition.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){using namespace pgp;try{
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_NETWORK_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.route="bootstrap";f.office="home";f.personal_investment_cents=10000000;
    for(const auto& scene:scenario.founding->scenes)f.choices.emplace(scene.id,scene.choices.front().id);
    runtime::ScenarioOptions options;options.founding=f;options.identity=runtime::CampaignIdentity{"Network Test","Alex Vale","they/them"};
    auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime start{program,initial};check(start.prepare_opening(initial.world.simulation.revision).accepted(),"Opening");
    const auto base=start.current_state();const auto& actual=base.world.simulation;
    check(actual.management->people->industry.size()>=8,"Actual opened major HQ people populate the network");
    const auto first=actual.management->people->industry.begin()->first;
    const auto simulate=[&](std::string state,std::string city) {
        auto s=actual;auto person=s.management->people->industry.begin()->second;s.management->people->industry.clear();
        person.state=std::move(state);person.city=std::move(city);s.management->people->industry.emplace(first,std::move(person));
        auto& social=*s.management->people->social;social.links.clear();social.known_traits.clear();
        for(int week=0;week<20;++week){s.date=core::advance_days(s.date,core::Days{7});core::prepare_industry_social(s);}
        core::validate_social(s);const auto* bond=core::social_link(s,"founder",first);return bond?bond->affinity:0;
    };
    const auto city=simulate("US-CA","Palo Alto"),state=simulate("US-CA","San Francisco"),remote=simulate("US-TX","Austin");
    check(city>state && state>remote,"Same seed produces city > state > remote progression");
    check(remote<=core::social_rules(actual)->remote_ceiling,"New online-only ties remain shallow");
    // Deliberate long-distance contact maintains an established friendship,
    // while a newly introduced online tie cannot be clicked into best friendship.
    auto online=actual;auto& network=*online.management->people->social;network.links.clear();network.known_traits.clear();
    auto& peer=online.management->people->industry.at(first);peer.state="US-TX";peer.city="Austin";
    check(!core::social_blocker(online,first).empty(),"A cold online invitation needs an introduction");
    network.links.push_back({first,"founder",200,20,online.date,online.date,{},"Online acquaintance",true});
    for(int week=0;week<12;++week){online.date=core::advance_days(online.date,core::Days{7});core::socialize(online,{first,"chat"});}
    check(core::social_link(online,"founder",first)->affinity==200,"Intentional online contact honors the shallow-tie ceiling");
    network.links.front().affinity=700;online.date=core::advance_days(online.date,core::Days{7});core::socialize(online,{first,"listen"});
    check(network.links.front().affinity>700,"An established friend can maintain trust from afar");
    auto mature=base;auto& social=*mature.world.simulation.management->people->social;
    social.links.push_back({first,"founder",700,80,actual.date,actual.date,{},"Mature referral fixture",true});
    runtime::Runtime game{program,mature};
    const auto submit=[&](core::CommandAction action){const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto before=game.current_state();
    // Explicit capacity fixture: recommendations use the same bounded waiting
    // list as relatives, without dropping anybody's current application.
    auto full_state=before;for(int i=0;i<12;++i)core::add_referred_applicant(full_state.world.simulation,first);
    runtime::Runtime full{program,full_state};check(!full.submit({full_state.world.simulation.revision,core::Socialize{first,"referral"}}).accepted() && full.current_state()==full_state,"A full recommendation waiting list refuses atomically");
    submit(core::Socialize{first,"referral"});
    check(game.current_state().world.simulation.management->people->applicants==before.world.simulation.management->people->applicants && game.current_state().world.simulation.management->people->queued_applicants.size()==1,"A friend's recommendation joins the waiting list while current candidates keep their place");
    const auto& referred=game.current_state().world.simulation.management->people->queued_applicants.back();
    check(referred.recommendation && referred.recommendation->person==first && referred.recommendation->kind=="friend" && referred.recommendation->name==actual.management->people->industry.at(first).profile.name,"Employee recommended friends retain their actual named source");
    check(core::has_skill(referred.profile,core::talent_rules(game.current_state().world.simulation)->roles.at(referred.profile.role)),"Referral qualifies for their job");
    check(game.current_state().world.simulation.management->people->social->referrals.contains(first),"Referral cooldown is saved");
    auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));check(loaded.checkpoint==game.current_state(),"Exact geographic network save");
    runtime::Runtime resumed{program,loaded.checkpoint};
    for(int day=0;day<8;++day)for(auto* live:{&game,&resumed}) {
        const auto result=live->submit({live->current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);
    }
    check(game.current_state()==resumed.current_state(),"Industry snapshots and random contact resume identically");
    check(!core::social_referral_blocker(game.current_state().world.simulation,first).empty(),"Referral cannot be farmed weekly");
    // Introductions use a real close-friend edge, not arbitrary target access.
    auto introduction=actual;auto& n=*introduction.management->people->social;n.links.clear();n.known_traits.clear();
    const auto second=std::next(introduction.management->people->industry.begin())->first;
    n.links.push_back({first,"founder",700,80,introduction.date,introduction.date,{},"Close friend",true});
    n.links.push_back({std::min(first,second),std::max(first,second),700,80,introduction.date,introduction.date,{},"Their close friend",false});
    const auto target=core::social_introduction(introduction,first);check(target==second,"Friend identifies a genuine close connection");
    core::socialize(introduction,{first,"introduce"});check(core::social_link(introduction,"founder",second)!=nullptr,"Introduction creates a known acquaintance");
    auto forged=game.current_state();forged.world.simulation.management->people->industry.begin()->second.city="Invented City";bool refused=false;
    try{runtime::validate_industry_people(forged.world,program);}catch(const std::invalid_argument&){refused=true;}check(refused,"Saved locations reconcile with actual company HQs");
    std::cout<<"Geographic social: distance rates, online ceiling, established remote friendship, introductions, qualified referrals, source reconciliation and exact continuation passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
