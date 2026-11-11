#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/romance.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}}
int main(){using namespace pgp;try {
    auto doc=nlohmann::json::parse(persistence::read_bounded(PGP_ROMANCE_SOURCE,content::maximum_document_bytes));
    for(auto& d:doc["definitions"])if(d["id"]=="scenario.founder_campaign"){d["competition"]["domestic"]=2;d["competition"]["international"]=0;d["competition"]["heavyweights"]=0;}
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.leg="political";f.route="hired_ceo";f.office="shared";f.researcher="none";f.gender="woman";f.orientation="bisexual";
    for(const auto& scene:scenario.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Personal Chapters","Alex Vale","she/her"};
    auto base=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);base.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime start{program,base};check(start.prepare_opening(base.world.simulation.revision).accepted(),"Opening");base=start.current_state();
    const auto& rules=*runtime::romance_rules(base.world);check(rules.dating_days==180 && rules.engagement_days==90,"Production milestones are six months and three months");
    auto without_romance=doc;for(auto& d:without_romance["definitions"])if(d["id"]=="scenario.founder_campaign"){d["management"]["people"].erase("romance");d["management"]["people"].erase("family");}
    const auto ordinary_program=runtime::compile_collection(content::load_collection(without_romance.dump(),registry),registry);
    auto ordinary=runtime::begin_scenario(ordinary_program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);ordinary.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime ordinary_start{ordinary_program,ordinary};check(ordinary_start.prepare_opening(ordinary.world.simulation.revision).accepted(),"Ordinary comparison opening");ordinary=ordinary_start.current_state();
    const auto compare_people=[&](const core::SimulationState& first,const core::SimulationState& second){
        const auto& a=*first.management->people;const auto& b=*second.management->people;check(a.stream==b.stream && a.employees.size()==b.employees.size(),"Identity generation does not consume professional RNG");
        for(std::size_t index=0;index<a.employees.size();++index){auto profile=a.employees[index].terms.profile;profile.romantic.reset();check(profile==b.employees[index].terms.profile,"Romantic identity does not change professional quality or skills");}
    };
    compare_people(base.world.simulation,ordinary.world.simulation);
    for(std::size_t index=0;index<base.world.competition->companies.size();++index)compare_people(base.world.competition->companies[index].simulation,ordinary.world.competition->companies[index].simulation);
    auto malformed=doc;for(auto& d:malformed["definitions"])if(d["id"]=="scenario.founder_campaign")d["management"]["people"]["romance"]["dating_days"]=180.5;
    bool rejected_rules=false;try{(void)content::load_collection(malformed.dump(),registry);}catch(const std::exception&){rejected_rules=true;}check(rejected_rules,"Authored romance rules reject fractional integer tokens");
    // Only mature friendship and an explicit deterministic consent stream are
    // seeded. Relationship stages and calendar work use the real operations.
    std::string target;for(const auto& [id,p]:base.world.simulation.management->people->industry)if(p.profile.romantic && core::mutual_romantic_interest("woman",base.world.romance->founder.preferences,p.profile.gender,p.profile.romantic->preferences,800)){target=id;break;}
    check(!target.empty(),"Generated compatible adult contact");auto& network=*base.world.simulation.management->people->social;
    auto edge=core::SocialLink{target,"founder",800,60,base.world.simulation.date,base.world.simulation.date,{},"Established friendship fixture",true};if(edge.first>edge.second)std::swap(edge.first,edge.second);network.links.push_back(edge);
    base.world.romance->stream=1;
    const auto before=base;const auto preview=runtime::romance_quote(base.world,{target,"ask"});check(preview.blocker.empty() && base==before,"Preview neither mutates nor reveals a future answer");
    core::RomanticPreferences all{true,false,{"woman","man","nonbinary"},400};
    check(core::mutual_romantic_interest("woman",all,"nonbinary",all,500),"Nonbinary romance checks both people's saved preferences");
    check(!core::mutual_romantic_interest("woman",all,"man",core::RomanticPreferences{true,false,{"man"},400},800),"Attraction is mutual");
    const auto asexual=core::generate_romantic_preferences("woman","asexual",17,rules);check(!asexual.genders.empty(),"Asexuality does not erase romantic attraction");
    const auto demi=core::generate_romantic_preferences("woman","demisexual",17,rules);check(demi.minimum_friendship==rules.demisexual_friendship,"Demisexual trust floor");
    const core::RomanticProfile birthday{core::to_ordinal({2000,2,29}),all};check(!core::adult_on(birthday,core::to_ordinal({2018,2,27})) && core::adult_on(birthday,core::to_ordinal({2018,2,28})),"Exact adulthood boundary including leap birthday");
    auto refused=base;refused.world.romance->founder.preferences.genders={"nonbinary"};
    // A closed-to-romance founder is refused before any random draw.
    refused.world.romance->founder.preferences.interested=false;const auto refused_stream=refused.world.romance->stream;
    check(!runtime::romance_quote(refused.world,{target,"ask"}).blocker.empty() && refused.world.romance->stream==refused_stream,"Own explicit preference blocks without RNG");
    auto no=base;no.world.romance->founder.preferences.minimum_friendship=1000;
    // This isolated world checks a private refusal without altering either person's friendship.
    const auto friendship=no.world.simulation.management->people->social->links;(void)runtime::pursue_romance(no.world,{target,"ask"});
    check(no.world.romance->active.empty() && !no.world.romance->replies.back().accepted && no.world.simulation.management->people->social->links==friendship,"Polite refusal keeps friendship");
    check(runtime::romance_quote(no.world,{target,"ask"}).blocker.find("today")!=std::string::npos,"Daily personal budget applies after refusal");
    runtime::Runtime game{program,base};
    const auto act=[&](std::string action){auto result=game.pursueRomance(game.current_state().world.simulation.revision,{target,action});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto step=[&](){const auto r=game.submit({game.current_state().world.simulation.revision,core::AdvanceDay{}});if(!r.accepted())throw std::runtime_error(r.explanation);};
    act("ask");check(runtime::romantic_partner(game.current_state().world,"founder"),"Actual accepted invitation creates dating");
    const auto dated=game.current_state();check(!game.pursueRomance(dated.world.simulation.revision,{target,"engage"}).accepted() && game.current_state()==dated,"Engagement cannot skip the calendar");
    std::string other;for(const auto& [id,p]:base.world.simulation.management->people->industry){(void)p;if(id!=target){other=id;break;}}
    check(!runtime::romance_quote(game.current_state().world,{other,"ask"}).blocker.empty(),"One partner across company rosters");
    // Bounded production-calendar check: two rivals, 271 days, weekly dates.
    // No century simulation or payroll/stat shortcuts.
    for(int day=1;day<=271;++day) {
        step();const auto& w=game.current_state().world;const auto* partner=runtime::romantic_partner(w,"founder");check(partner,"Maintained relationship survives");
        if(day==180 || day==270)act(day==180?"engage":"marry");
        else if(day%7==0)act("listen");
        if(day==140){auto saved=persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint;check(saved==game.current_state(),"Mid-dating save is exact");runtime::Runtime replay{program,saved};const auto r=replay.submit({saved.world.simulation.revision,core::AdvanceDay{}});check(r.accepted(),"Reload can advance");auto next=game;const auto n=next.submit({saved.world.simulation.revision,core::AdvanceDay{}});check(n.accepted() && next.current_state()==replay.current_state(),"Next romance and world RNG replay exactly");}
    }
    const auto* married=runtime::romantic_partner(game.current_state().world,"founder");check(married && married->married_on && married->engaged_on,"Actual dates, engagement and marriage at production durations");
    const auto saved=persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint;check(saved==game.current_state(),"Marriage and identity disclosure reload exactly");
    auto invalid=saved;invalid.world.romance->active.begin()->second.started_on=core::to_ordinal({2014,12,31});bool rejected=false;try{runtime::validate_checkpoint(program,invalid);}catch(const std::exception&){rejected=true;}check(rejected,"Forged pre-adult and pre-campaign history rejected");
    const auto bond_id=married->id;act("separate");check(!runtime::romantic_partner(game.current_state().world,"founder") && core::romance_status(game.current_state().world.romance->history.back())=="ex_spouse" && game.current_state().world.romance->history.back().id==bond_id,"Separation archives actual ex-spouse history");
    check(core::social_link(game.current_state().world.simulation,"founder",target),"Friendship remains independently recorded");
    check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Ended relationship and immutable replies save exactly");
    // NPC progression uses the same adult, mutual, single-person requirements.
    auto npc=base.world;npc.romance->active.clear();npc.romance->history={};npc.romance->replies={};npc.romance->next_id=1;npc.romance->next_review=runtime::campaign_day(npc);
    auto& r=*npc.simulation.management->catalog.people->romance;r.npc_start_bp=10000;r.npc_progress_bp=10000;
    std::vector<core::PersonProfile*> team;for(auto& c:npc.competition->companies)for(auto& e:c.simulation.management->people->employees)if(e.terms.profile.romantic){e.terms.profile.romantic->preferences=all;team.push_back(&e.terms.profile);}
    auto& social=*npc.simulation.management->people->social;social.links.clear();
    const auto& c=npc.competition->companies.front();check(c.simulation.management->people->employees.size()>=2,"NPC coworkers");
    const auto x=c.definition_id+"/"+c.simulation.management->people->employees[0].terms.profile.id,y=c.definition_id+"/"+c.simulation.management->people->employees[1].terms.profile.id;
    social.links.push_back({std::min(x,y),std::max(x,y),800,60,npc.simulation.date,npc.simulation.date,{},"NPC friendship fixture",false});runtime::prepare_romance(npc);
    check(npc.romance->active.size()==1 && !runtime::romantic_partner(npc,"founder"),"NPC pair can date without committing the founder");
    for(int week=0;week<42;++week){npc.simulation.date=core::advance_days(npc.simulation.date,core::Days{7});runtime::prepare_romance(npc);}
    check(npc.romance->active.begin()->second.married_on.has_value(),"Nearby NPCs progress through actual dates, engagement and marriage");
    // A tiny strain fixture isolates the warning and grace period. It is not
    // presented as an independently simulated multi-year separation campaign.
    auto& npc_bond=npc.romance->active.begin()->second;npc_bond.affection=r.strain_affection+r.remote_decay;
    npc_bond.last_time_together=core::OrdinalDay{npc.simulation.date.value()-r.neglect_days};
    npc.competition->companies.front().simulation.management->people->employees[0].departed_on=npc.simulation.date;
    npc.simulation.date=core::advance_days(npc.simulation.date,core::Days{7});runtime::prepare_romance(npc);
    check(npc.romance->active.size()==1 && npc.romance->active.begin()->second.strained_on,"Distant partners receive a strain warning before any breakup");
    for(int week=0;week<5;++week){npc.simulation.date=core::advance_days(npc.simulation.date,core::Days{7});runtime::prepare_romance(npc);}
    check(npc.romance->active.empty() && core::romance_status(npc.romance->history.back())=="ex_spouse","Sustained unaddressed strain archives the marriage after the grace period");
    auto young=base.world;auto& actual_peer=young.competition->companies.front().simulation.management->people->employees.front().terms.profile;
    actual_peer.romantic->born_on=core::to_ordinal({2000,1,1});
    const auto young_id=young.competition->companies.front().definition_id+"/"+actual_peer.id;
    check(runtime::romance_quote(young,{young_id,"ask"}).blocker.find("adults")!=std::string::npos,"Invitation checks the actual source person's adulthood before friendship");
    auto disclosure=game.current_state().world;check(disclosure.romance->known_identities.contains(target),"Trusted disclosure is saved");
    auto& friend_edge=*std::ranges::find_if(disclosure.simulation.management->people->social->links,[&](const auto& link){return (link.first==target && link.second=="founder") || (link.second==target && link.first=="founder");});friend_edge.affinity=0;
    check(runtime::romantic_identity_known(disclosure,target),"An already shared identity stays known after friendship weakens");
    std::cout<<"Romance: generated identity, exact adulthood, mutual preferences, private refusal, independent friendship, cross-HQ monogamy, production 180+90-day consent progression, NPC dating/marriage, geographic strain with warning/grace, separation and exact reload passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
