#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){using namespace pgp;try {
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_SOCIAL_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.route="hired_ceo";f.office="shared";
    for(const auto& scene:scenario.founding->scenes)f.choices.emplace(scene.id,scene.choices.front().id);
    runtime::ScenarioOptions options;options.founding=f;options.identity=runtime::CampaignIdentity{"Social Works","Alex Vale","they/them"};
    auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    checkpoint.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,checkpoint};
    const auto state=[&]()->const auto&{return game.current_state().world.simulation;};
    const auto apply=[&](core::CommandAction action){const auto result=game.submit({state().revision,std::move(action)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    check(game.prepare_opening(state().revision).accepted(),"Prepared opening");
    std::vector<std::string> ids;for(const auto& e:state().management->people->employees)if(e.terms.profile.role!="Founder / researcher")ids.push_back(e.terms.profile.id);
    check(ids.size()>=2,"Inherited HQ offers several actual coworkers");
    const auto& target=std::ranges::find_if(state().management->people->employees,[&](const auto& e){return e.terms.profile.id==ids[0];})->terms.profile;
    const auto public_traits=core::visible_traits(state(),target);
    check(!public_traits.empty() && public_traits.size()<target.traits.size(),"Professional traits public, personal traits initially hidden");
    check(std::ranges::none_of(public_traits,[](const auto& t){return t.starts_with("Upbringing: ");}),"Private upbringing is not public");
    apply(core::Socialize{ids[0],"chat"});
    const auto* first=core::social_link(state(),"founder",ids[0]);check(first && first->affinity>0 && first->encounters==1,"Real command starts friendship");
    const auto before=game.current_state();const auto refused=game.submit({state().revision,core::Socialize{ids[1],"chat"}});
    check(!refused.accepted() && game.current_state()==before,"One daily meetup; refusal is atomic");
    auto saved=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));check(saved.checkpoint==game.current_state(),"Social save reload is exact");
    runtime::Runtime resumed{program,saved.checkpoint};
    for(int day=0;day<14;++day){apply(core::AdvanceDay{});check(resumed.submit({resumed.current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(),"Resumed day");}
    check(game.current_state()==resumed.current_state(),"Weekly social RNG continues identically after reload");
    check(core::social_blocker(state(),ids[0]).empty(),"A week restores invitation availability");
    // Bounded unit projection of social time only: no century simulation or fake
    // campaign save. Proves opposites can become multiple best friends.
    auto social=state();auto& roster=social.management->people->employees;auto& network=*social.management->people->social;
    network.founder_traits={"Social: introverted","Temperament: deliberate","Interest: music"};
    for(auto& e:roster)if(e.terms.profile.role!="Founder / researcher")e.terms.profile.traits={"Social: outgoing","Temperament: impulsive","Interest: games","Upbringing: poor"};
    network.known_traits.clear();network.links.clear();network.last_outing.reset();
    for(int week=0;week<170;++week) {
        social.date=core::advance_days(social.date,core::Days{7});core::prepare_social(social);
        core::socialize(social,{ids[0],"listen"});
        social.date=core::advance_days(social.date,core::Days{1});core::socialize(social,{ids[1],"hobby"});
    }
    check(core::social_link(social,"founder",ids[0])->affinity>=850 && core::social_link(social,"founder",ids[1])->affinity>=850,"Opposites can become multiple best friends");
    check(core::friendship_mood(social,ids[0])>0,"Friendships benefit actual morale");
    const auto& person=std::ranges::find_if(roster,[&](const auto& e){return e.terms.profile.id==ids[0];})->terms.profile;
    check(core::visible_traits(social,person).size()==person.traits.size(),"Trust reveals private traits");
    core::validate_social(social);
    auto* departed=&*std::ranges::find_if(roster,[&](const auto& e){return e.terms.profile.id==ids[0];});departed->departed_on=social.date;
    const auto old=core::social_link(social,"founder",ids[0])->affinity;
    for(int week=0;week<6;++week){social.date=core::advance_days(social.date,core::Days{7});core::prepare_social(social);}
    check(core::social_link(social,"founder",ids[0])->affinity<old,"Lost routine contact slowly weakens a tie");
    check(core::visible_traits(social,person).size()==person.traits.size(),"A weaker tie does not erase what was shared");
    check(core::social_blocker(social,ids[0]).empty(),"Former coworkers can still catch up");
    const auto before_chat=core::social_link(social,"founder",ids[0])->affinity;core::socialize(social,{ids[0],"chat"});
    check(core::social_link(social,"founder",ids[0])->affinity>before_chat,"An established former coworker tie can be maintained online");
    auto forged=social;forged.management->people->social->links.front().second="missing";bool invalid=false;try{core::validate_social(forged);}catch(const std::invalid_argument&){invalid=true;}check(invalid,"Unknown relationship endpoint refused");
    std::cout<<"Social: private/public knowledge, interaction limits, opposing traits, multiple best friends, morale, loss of contact and exact continuation passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
