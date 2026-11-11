#include "family_internal.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}}
int main(){using namespace pgp;try {
    auto doc=nlohmann::json::parse(persistence::read_bounded(PGP_FAMILY_SOURCE,content::maximum_document_bytes));
    for(auto& d:doc["definitions"])if(d["id"]=="scenario.founder_campaign") {
        d["competition"]["domestic"]=2;d["competition"]["international"]=0;d["competition"]["heavyweights"]=0;
        // Isolate player care and application clocks. A separate fixture below
        // checks the ordinary NPC household policy at a guaranteed review rate.
        d["management"]["people"]["family"]["npc_family_bp"]=0;
    }
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.leg="political";f.route="bootstrap";f.office="home";f.researcher="none";f.gender="woman";f.orientation="bisexual";
    for(const auto& scene:scenario.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Family Chapters","Alex Vale","she/her"};
    auto base=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);base.world.enabled_flows=std::vector<std::string>{"life.family.first_child","life.family.first_application","life.family.marriage"};
    runtime::Runtime opening{program,base};check(opening.prepare_opening(base.world.simulation.revision).accepted(),"Opening");base=opening.current_state();
    check(base.world.family && base.world.family->members.size()==2 && base.world.family->edges.size()==2,"Named founder parents and exact birth-family links");
    check(runtime::family_related(base.world,"founder","family/person/1") && !runtime::romance_quote(base.world,{"family/person/1","ask"}).blocker.empty(),"Relatives cannot enter romance");
    runtime::FamilyRequest adopt;adopt.action="adopt";adopt.band="baby";
    const auto before=base;const auto preview=runtime::family_quote(base.world,adopt);check(preview.blocker.empty() && preview.due_on==core::advance_days(base.world.simulation.date,core::Days{90}) && before==base,"Reviewed adoption date is read-only");
    auto teen=adopt;teen.band="teen";check(runtime::family_quote(base.world,teen).blocker.find("older")!=std::string::npos,"Young caregivers must choose an age-appropriate care group");
    auto no=base;no.world.family->stream=3;runtime::Runtime refusal{program,no};const auto declined=refusal.manageFamily(no.world.simulation.revision,adopt);
    check(declined.accepted() && refusal.current_state().world.family->pending.empty() && !refusal.current_state().world.family->replies.back().accepted && refusal.current_state().world.simulation.cash==no.world.simulation.cash,"A saved no has no invented relationship or money penalty");
    base.world.family->stream=1;runtime::Runtime game{program,base};
    const auto act=[&](runtime::FamilyRequest request){const auto result=game.manageFamily(game.current_state().world.simulation.revision,request);if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto step=[&](){const auto r=game.submit({game.current_state().world.simulation.revision,core::AdvanceDay{}});if(!r.accepted())throw std::runtime_error(r.explanation);};
    const auto advance=[&](int days){for(int i=0;i<days;++i)step();};
    const auto exact=[&](){check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Exact family checkpoint roundtrip");};
    act(adopt);check(game.current_state().world.family->pending.size()==1 && game.current_state().world.family->members.size()==2,"An agreed plan does not create an instant child");
    advance(45);exact();runtime::Runtime replay{program,persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint};
    const auto r=replay.submit({replay.current_state().world.simulation.revision,core::AdvanceDay{}});check(r.accepted(),"Reloaded plan advances");step();check(replay.current_state()==game.current_state(),"Care plans and independent RNG resume identically");advance(44);
    check(game.current_state().world.family->members.size()==3 && game.current_state().world.family->pending.empty(),"Actual adoption review produces one stable child");
    const auto child=game.current_state().world.family->members[2].profile;check(child.skills.empty() && runtime::family_age(child.romantic->born_on,game.current_state().world.simulation.date)<2,"A child has an identity, not advance professional skills");
    check(!runtime::family_quote(game.current_state().world,{"track","baby",{},child.id,"programming",false}).blocker.empty(),"Learning conversations wait for school age");
    advance(275);check(std::ranges::any_of(game.current_state().runtime.presentations,[](const auto& row){return row.flow_id=="life.family.first_child";}),"The ordinary authored event runtime reports the actual new family chapter");runtime::FamilyRequest foster;foster.action="foster";act(foster);check(!game.current_state().world.family->pending.empty(),"A separate foster-care choice is agreed");advance(30);
    const auto foster_child=game.current_state().world.family->members.back().profile.id;check(game.current_state().world.family->members.size()==4,"Foster placement arrives after its real review period");
    check(runtime::family_related(game.current_state().world,child.id,foster_child),"Shared caregivers establish sibling kinship");
    runtime::FamilyRequest convert;convert.action="adopt_placement";convert.child=foster_child;
    check(!runtime::family_quote(game.current_state().world,convert).blocker.empty(),"Adoption from foster care cannot skip sustained care");advance(180);act(convert);advance(90);
    check(std::ranges::any_of(game.current_state().world.family->edges,[&](const auto& e){return e.child==foster_child && e.kind=="foster" && e.ended_on;}) && std::ranges::any_of(game.current_state().world.family->edges,[&](const auto& e){return e.child==foster_child && e.kind=="adoptive" && !e.ended_on;}),"Adoption retains ended foster history and actual permanent parenthood");exact();
    // A separately agreed older-child placement isolates birthdays in less than
    // one year, instead of running an eighteen-year financial campaign.
    auto older=base;older.world.family->stream=17;runtime::FamilyRequest older_care;older_care.action="adopt";older_care.band="teen";
    (void)runtime::family_detail::plan(older.world,"family/person/1",{},older_care,true);
    runtime::Runtime growing{program,older};
    const auto grow=[&](){const auto result=growing.submit({growing.current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);};
    for(int i=0;i<90;++i)grow();const auto sibling=growing.current_state().world.family->members.back().profile;
    check(runtime::family_age(sibling.romantic->born_on,growing.current_state().world.simulation.date)==17 && sibling.skills.empty(),"Older adoptee keeps a complete identity while still a minor");
    check(runtime::family_related(growing.current_state().world,"founder",sibling.id),"A parent's adopted child is a sibling");
    int birthdays=0;while(!runtime::family_member(growing.current_state().world,sibling.id)->qualified_on){check(++birthdays<=366,"Bounded birthday check");grow();}
    auto ready=growing.current_state();const auto& graduate=*runtime::family_member(ready.world,sibling.id);check(graduate.profile.name==sibling.name && graduate.profile.portrait==sibling.portrait && graduate.profile.gender==sibling.gender && graduate.profile.romantic==sibling.romantic && !graduate.profile.skills.empty(),"Adulthood preserves identity and completes an actual learning path");
    check(!graduate.application,"The graduate has not been silently hired");
    // Public-disclosure fixture exercises canonical IDs when a relative becomes staff.
    auto public_relative=ready.world.family->members.back();public_relative.profile.romantic->preferences.public_orientation=true;ready.world.family->members.replace(ready.world.family->members.size()-1,public_relative);
    ready.world.family->stream=1;growing=runtime::Runtime{program,ready};
    runtime::FamilyRequest invite;invite.action="apply";invite.child=sibling.id;const auto invited=growing.manageFamily(ready.world.simulation.revision,invite);if(!invited.accepted())throw std::runtime_error(invited.explanation);
    const auto& after=growing.current_state();const auto& application=*runtime::family_member(after.world,sibling.id)->application;const auto applicant_id=application.person;
    check(after.world.simulation.management->people->applicants==ready.world.simulation.management->people->applicants && after.world.simulation.management->people->queued_applicants.size()==1,"Recommended candidates wait without replacing untouched applications");
    const auto& queued=after.world.simulation.management->people->queued_applicants.front();check(queued.recommendation && queued.recommendation->kind=="family" && queued.recommendation->name=="Alex Vale" && queued.profile.name==sibling.name,"Employee recommended candidate retains its source and identity");
    check(persistence::decode_save(program,persistence::encode_save(program,after)).checkpoint==after,"Queued recommendations reload exactly");
    int wait=0;while(std::ranges::none_of(growing.current_state().world.simulation.management->people->applicants,[&](const auto& a){return a.profile.id==applicant_id;})){check(++wait<=31,"Next ordinary recruitment review");grow();}
    const auto submit=[&](core::CommandAction action){const auto result=growing.submit({growing.current_state().world.simulation.revision,std::move(action)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto& live=*std::ranges::find_if(growing.current_state().world.simulation.management->people->applicants,[&](const auto& a){return a.profile.id==applicant_id;});
    check(!growing.submit({growing.current_state().world.simulation.revision,core::HireEmployee{applicant_id,0}}).accepted(),"A family recommendation does not waive compensation agreement");
    const auto asks=*live.recruitment;submit(core::ManageRecruitment{core::ProposeEmployment{applicant_id,asks.ask_salary,asks.ask_signing}});grow();
    check(!growing.submit({growing.current_state().world.simulation.revision,core::HireEmployee{applicant_id,0}}).accepted(),"A family recommendation does not waive the furnished desk");
    submit(core::ManageHeadquarters{core::BuyHQEquipment{"desk"}});const auto desk=growing.current_state().world.simulation.infrastructure->workplace->headquarters->items.back().id;
    submit(core::ManageHeadquarters{core::BuyHQEquipment{"desktop"}});const auto computer=growing.current_state().world.simulation.infrastructure->workplace->headquarters->items.back().id;
    for(int i=0;i<10;++i)grow();const auto& site=core::current_headquarters(growing.current_state().world.simulation);std::optional<core::PlaceHQEquipment> position;
    for(int y=0;y<site.depth && !position;++y)for(int x=0;x<site.width && !position;++x){core::PlaceHQEquipment place{desk,0,x,y,false};if(core::hq_placement_blocker(growing.current_state().world.simulation,place).empty())position=place;}
    check(position.has_value(),"Real free workspace");submit(core::ManageHeadquarters{*position});submit(core::ManageHeadquarters{core::PlaceHQEquipment{computer,desk,0,0,false}});
    submit(core::HireEmployee{applicant_id,desk});check(runtime::romance_person_id(growing.current_state().world,applicant_id)==sibling.id && runtime::family_related(growing.current_state().world,"founder",applicant_id),"The ordinary employee keeps the same sibling identity and kinship");
    for(int i=0;i<8;++i)grow();check(core::assigned_hq_desk(growing.current_state().world.simulation,applicant_id)==desk,"Actually hired and onboarded at a real desk");
    check(persistence::decode_save(program,persistence::encode_save(program,growing.current_state())).checkpoint==growing.current_state(),"Hired family identity and recommendation persist");
    check(std::ranges::any_of(growing.current_state().runtime.presentations,[](const auto& row){return row.flow_id=="life.family.first_application";}),"Authored application news reads the real received recommendation fact");
    // Explicit mature-marriage fixture isolates step/birth/co-parenting. The
    // romance check separately verifies natural production-duration milestones.
    auto married=game.current_state();const auto day=married.world.simulation.date;std::string spouse;
    for(const auto& [id,person]:married.world.simulation.management->people->industry)if(person.profile.romantic && !runtime::romantic_partner(married.world,id) && core::mutual_romantic_interest("woman",married.world.romance->founder.preferences,person.profile.gender,person.profile.romantic->preferences,800)){spouse=id;break;}
    check(!spouse.empty(),"Compatible single spouse fixture");core::RomanceBond bond;bond.id=married.world.romance->next_id++;bond.first=std::min(std::string{"founder"},spouse);bond.second=std::max(std::string{"founder"},spouse);bond.started_on=core::OrdinalDay{day.value()-270};bond.engaged_on=core::OrdinalDay{day.value()-90};bond.married_on=day;bond.last_time_together=day;bond.affection=900;bond.dates=3;bond.known=true;bond.memory="Mature marriage fixture for family progression";married.world.romance->active.emplace(bond.id,bond);runtime::prepare_family(married.world);game=runtime::Runtime{program,married};
    check(std::ranges::count_if(game.current_state().world.family->edges,[&](const auto& e){return e.parent==spouse && e.kind=="step" && !e.ended_on;})==2,"A new spouse becomes a stepparent to existing children");
    check(runtime::family_detail::dependents(game.current_state().world,spouse)==2,"Stepchildren count toward the blended household care limit");
    advance(90);auto conception=game.current_state();conception.world.family->stream=1;game=runtime::Runtime{program,conception};runtime::FamilyRequest birth;birth.action="birth";birth.name="River Vale";act(birth);const auto due=game.current_state().world.family->pending.begin()->second.due_on;
    check(due==core::advance_days(conception.world.simulation.date,core::Days{280}),"Production birth/assisted-parenthood waiting period");advance(280);
    const auto baby=game.current_state().world.family->members.back().profile;check(baby.name=="River Vale" && baby.romantic->born_on==due && std::ranges::count_if(game.current_state().world.family->edges,[&](const auto& e){return e.child==baby.id && e.kind=="biological";})==2,"Named arrival has both intended birth-family parents");
    check(!runtime::family_related(game.current_state().world,"founder",spouse),"Co-parenthood does not mistakenly make spouses blood relatives");exact();
    const auto separation=game.pursueRomance(game.current_state().world.simulation.revision,{spouse,"separate"});if(!separation.accepted())throw std::runtime_error(separation.explanation);
    check(std::ranges::none_of(game.current_state().world.family->edges,[&](const auto& e){return e.kind=="step" && e.parent==spouse && !e.ended_on;}) && std::ranges::count_if(game.current_state().world.family->edges,[&](const auto& e){return e.child==baby.id && e.kind=="biological" && !e.ended_on;})==2,"Separation ends step context while preserving co-parenthood");exact();
    // Tiny family-calendar fixtures cover care expiry and learning choices.
    // They deliberately do not claim replay of skipped company finances.
    auto placement=base.world;placement.family->stream=1;runtime::FamilyRequest temporary;temporary.action="foster";temporary.band="child";
    const auto arrival=runtime::family_detail::plan(placement,"family/person/1",{},temporary,true).due_on;
    placement.simulation.date=arrival;runtime::prepare_family(placement);const auto foster_id=placement.family->members.back().profile.id;
    placement.simulation.date=core::advance_days(arrival,core::Days{365});runtime::prepare_family(placement);runtime::validate_family(placement);
    check(std::ranges::any_of(placement.family->edges,[&](const auto& e){return e.child==foster_id && e.kind=="foster" && e.ended_on==placement.simulation.date;}) && runtime::family_member(placement,foster_id),"An agreed foster term ends care without erasing the person");
    auto schooling=game.current_state().world;const auto learner=child.id;const auto school_birthday=runtime::family_member(schooling,learner)->profile.romantic->born_on;
    schooling.simulation.date=core::to_ordinal(runtime::family_detail::anniversary(school_birthday,17));schooling.simulation.date=core::advance_days(schooling.simulation.date,core::Days{180});runtime::prepare_family(schooling);schooling.family->stream=1;
    const auto study_started=schooling.simulation.date;runtime::FamilyRequest support;support.action="track";support.child=learner;support.track="ui_design";
    (void)runtime::manage_family(schooling,support);check(runtime::family_member(schooling,learner)->track=="ui_design" && runtime::family_member(schooling,learner)->profile.skills.empty(),"A reviewed learning choice records practice without instantly granting a skill");
    schooling.simulation.date=core::to_ordinal(runtime::family_detail::anniversary(school_birthday,18));runtime::prepare_family(schooling);check(!runtime::family_member(schooling,learner)->qualified_on,"Turning eighteen does not skip the chosen study period");
    schooling.simulation.date=core::advance_days(study_started,core::Days{365});runtime::prepare_family(schooling);check(core::has_skill(runtime::family_member(schooling,learner)->profile,"ui_design"),"The supported practical skill arrives after adulthood and actual study time");runtime::validate_family(schooling);
    auto corrupt=growing.current_state().world;corrupt.family->aliases.begin()->second="family/person/1";bool rejected=false;try{runtime::validate_family(corrupt);}catch(const std::exception&){rejected=true;}check(rejected,"Forged employment aliases cannot change a child's identity");
    // One bounded review tests NPC family choice through the same plan owner.
    // This is a mature-marriage and review-rate fixture, not natural courtship evidence.
    auto npc=game.current_state().world;auto& tuning=*npc.simulation.management->catalog.people->family;tuning.npc_family_bp=10000;tuning.birth_weight=0;tuning.adoption_weight=100;
    std::vector<std::pair<std::string,const core::PersonProfile*>> eligible;
    for(const auto& [id,p]:npc.simulation.management->people->industry)if(id!=spouse && p.profile.romantic && !runtime::romantic_partner(npc,id))eligible.emplace_back(id,&p.profile);
    std::optional<std::pair<std::string,std::string>> pair;
    for(std::size_t i=0;i<eligible.size() && !pair;++i)for(std::size_t j=i+1;j<eligible.size() && !pair;++j)if(core::mutual_romantic_interest(eligible[i].second->gender,eligible[i].second->romantic->preferences,eligible[j].second->gender,eligible[j].second->romantic->preferences,900))pair=std::pair{eligible[i].first,eligible[j].first};
    check(pair.has_value(),"Compatible NPC household fixture");core::RomanceBond parents;parents.id=npc.romance->next_id++;parents.first=pair->first;parents.second=pair->second;parents.started_on=core::OrdinalDay{npc.simulation.date.value()-370};parents.engaged_on=core::OrdinalDay{npc.simulation.date.value()-190};parents.married_on=core::OrdinalDay{npc.simulation.date.value()-100};parents.last_time_together=npc.simulation.date;parents.affection=900;parents.dates=30;parents.memory="Mature NPC household fixture";npc.romance->active.emplace(parents.id,parents);npc.family->next_review=npc.simulation.date;runtime::prepare_family(npc);
    check(std::ranges::any_of(npc.family->pending,[&](const auto& row){return row.second.first==pair->first && row.second.second==pair->second && row.second.kind=="adopt";}),"NPC spouses select an actual agreed adoption plan under the shared policy");runtime::validate_family(npc);
    std::cout<<"Family: reviewed/refused care, real 90/30/180/280-day clocks, saved identity, adoption and past foster care, siblings/step/co-parenting, actual adulthood and learning, queued recommended candidates, ordinary pay/desk/onboarding and exact replay passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
