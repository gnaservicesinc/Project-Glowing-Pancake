#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/authority.hpp>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){using namespace pgp;try{
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_TALENT_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    // Seeded roots, conditional follow-ups and rewards share one selection function.
    const auto& catalog=*scenario.founding;
    std::set<std::string> openings, seen;
    std::set<std::vector<std::string>> elementary_paths, opening_choice_orders, university_paths;
    for(std::uint64_t seed=0;seed<64;++seed) {
        content::FoundingSelection selection;selection.story_seed=seed;
        selection.family=seed%2?"rich":"poor";
        if(seed%2) selection.advantages.clear();
        const auto roots=content::formative_scenes(catalog,selection);
        check(roots.size()>=12,"School sections include milestones and sampled events");
        check(roots.front()->pool=="first_classroom","School starts with a sampled opening");
        openings.insert(roots.front()->id);
        std::vector<std::string> early_path, opening_choices;
        for(const auto* option:content::formative_options(*roots.front(),selection)) opening_choices.push_back(option->id);
        opening_choice_orders.insert(opening_choices);
        check(std::set<std::string>(opening_choices.begin(),opening_choices.end()).size()==roots.front()->choices.size(),
              "Shuffling displays every authored choice exactly once");
        for(const auto* root:roots) {
            if(root->section=="elementary") early_path.push_back(root->id);
            selection.choices[root->id]=root->choices.front().id;
        }
        elementary_paths.insert(early_path);
        const auto before_cond=content::formative_scenes(catalog,selection).size();
        selection.choices["grad_choice"]="college";
        selection.choices["grad_school"]="phd_ml";
        const auto with_college=content::formative_scenes(catalog,selection);
        check(with_college.size()>before_cond,"Choosing college opens undergrad and grad chains");
        std::vector<std::string> university_path;
        for(const auto* scene:with_college) {
            if(!scene->pool.empty()) seen.insert(scene->id);
            if(scene->section=="undergrad" || scene->section=="grad") university_path.push_back(scene->id);
            const auto traits=content::formative_traits_before(catalog,selection,scene->id);
            const auto eligible=std::ranges::count_if(scene->choices,[&](const auto& option) {
                return content::option_ineligibility(option,selection,traits).empty();
            });
            check(eligible>=3,"Every shown school scene has at least three meaningful available choices");
        }
        university_paths.insert(university_path);
        const auto reloaded=content::load_founding_selection(content::canonical_founding_selection(selection));
        check(reloaded==selection && content::formative_scenes(catalog,reloaded)==with_college &&
            content::formative_options(*roots.front(),reloaded)==content::formative_options(*roots.front(),selection),
            "Story seed restores both event and option order exactly");
        selection.choices["grad_choice"]="workforce";
        check(content::formative_scenes(catalog,selection).size()==before_cond,"Changing parent choice hides its follow-up");
        const auto before=content::formative_skills(catalog,selection);
        const auto active_after=content::formative_scenes(catalog,selection);
        for(const auto& scene:catalog.scenes)
            if(std::ranges::none_of(active_after,[&](const auto* s){return s->id==scene.id;}))
                selection.choices[scene.id]=scene.choices.front().id;
        check(content::formative_skills(catalog,selection)==before,"Inactive follow-ups cannot grant rewards");
    }
    check(openings.size()==4 && elementary_paths.size()>32 && opening_choice_orders.size()>16 && university_paths.size()>8,
          "Fresh seeds vary the opening, elementary sequence, option order and university sequence");
    check(std::ranges::all_of(catalog.scenes,[&](const auto& scene){return scene.pool.empty() || seen.contains(scene.id);}),
          "Seeded paths reach every sampled event and camp across all stages");
    content::FoundingSelection f;f.family="rich";f.route="bootstrap";f.office="home";f.personal_investment_cents=10000000;
    for(const auto& scene:scenario.founding->scenes)f.choices.emplace(scene.id,scene.choices.front().id);
    f.choices["elem_playground"]="sedentary";
    f.choices["ms_bully"]="report";
    f.choices["hs_exclude"]="cut_out";
    f.choices["grad_choice"]="workforce";
    runtime::ScenarioOptions options;options.founding=f;options.identity=runtime::CampaignIdentity{"Talent Works","Alex Vale","they/them"};
    auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    checkpoint.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,checkpoint};
    const auto state=[&]()->const auto&{return game.current_state().world.simulation;};
    const auto apply=[&](core::CommandAction a){const auto result=game.submit({state().revision,std::move(a)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto act=[&](core::TalentAction a){apply(core::ManageTalent{std::move(a)});};
    const auto refuse=[&](core::TalentAction a,std::string_view why){const auto before=game.current_state();const auto result=game.submit({state().revision,core::ManageTalent{std::move(a)}});if(result.accepted() || result.explanation.find(why)==std::string::npos || game.current_state()!=before)throw std::runtime_error("Expected refusal: "+std::string(why)+"; got "+result.explanation);};
    const auto advance=[&](int days){for(int i=0;i<days;++i)apply(core::AdvanceDay{});};
    const auto save=[&]{check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Exact talent save/reload");};
    check(game.prepare_opening(state().revision).accepted(),"Opening");
    check(core::person_has_skill(state(),"founder","programming"),"Childhood programming skill");
    for(const auto& a:state().management->people->applicants){const auto& rules=*core::talent_rules(state());check(core::has_skill(a.profile,rules.roles.at(a.profile.role)),"Applicants qualified for advertised role");check(a.profile.orientation!="unrecorded","Stable full adult identity");}
    core::SoftwareDesign d;d.brief="chat";d.name="Clear Chat";d.workers={"founder"};
    refuse(core::StartSoftware{d},"desk");
    apply(core::ManageHeadquarters{core::BuyHQEquipment{"desk"}});apply(core::ManageHeadquarters{core::BuyHQEquipment{"desktop"}});advance(3);
    apply(core::ManageHeadquarters{core::PlaceHQEquipment{1,0,0,0,false}});apply(core::ManageHeadquarters{core::AssignHQDesk{1,"founder"}});apply(core::ManageHeadquarters{core::PlaceHQEquipment{2,1,0,0,false}});
    auto advanced=d;advanced.brief="coding";refuse(core::StartSoftware{advanced},"Architecture");
    const auto cash=state().cash;const auto quote=core::software_quote(state(),d);check(quote.blocker.empty(),"Valid software quote");act(core::StartSoftware{d});
    check(state().cash==core::checked_unit_subtract(cash,quote.fee),"Software fee is real expense");
    refuse(core::StartSoftware{d},"Already assigned");refuse(core::ReleaseSoftware{1},"Complete");
    advance(2);const auto work=[&]()->const auto&{return *std::ranges::find(state().capacity->projects,std::string("software/1"),&core::CapacityProject::id);};
    check(work().completed_staff_days==2 && work().completed_work.value()==0,"Software consumes staff-days without model compute");
    apply(core::ManageHeadquarters{core::PowerHQEquipment{2,false}});check(work().status==core::ProjectStatus::paused,"Power loss pauses software");advance(1);check(work().completed_staff_days==2,"Paused software gains no work");refuse(core::ResumeSoftware{1},"computer");save();
    apply(core::ManageHeadquarters{core::PowerHQEquipment{2,true}});act(core::ResumeSoftware{1});advance(quote.staff_days-2);act(core::ReleaseSoftware{1});check(core::software_market_bonus(state(),core::ProductChannel::consumer).value()>0,"Released design improves matching customer utility");check(core::software_market_bonus(state(),core::ProductChannel::api).value()==0,"No interface benefit for an API");save();
    core::CapacitySource supply{"source","physical","company.player","Test compute",core::CapacitySourceKind::leased,core::MilliCUPerDay{10000},core::MilliCUPerDay{10000},4096,"general",true};
    auto pool=core::opening_capacity({supply},1,state().date);
    core::CapacityProject serving;serving.id="serve";serving.source_id="source";serving.kind=core::ProjectKind::serving;serving.starts_on=state().date;serving.staff=1;serving.tasks_per_day=1000;serving.inference_milli_CU_per_task=core::MilliCU{1};serving.requested=core::MilliCUPerDay{1000};serving.cause="Test service";
    core::reserve_project(pool,state().date,serving);
    auto market=core::opening_market(state().date);core::add_market_product(market,pool,{"product.chat","serve",core::Cents{1},true,core::ProductChannel::consumer});
    core::MarketSegment buyers;buyers.id="buyers";buyers.daily_tasks=1000;buyers.daily_budget=core::Cents{100000};buyers.outside_utility=1000;buyers.preferences.push_back({"product.chat",1000,core::Cents{10},0,true});core::add_market_segment(market,buyers);
    auto projection=game.current_state().world;projection.simulation.market=market;projection.simulation.capacity=pool;projection.society.reset();projection.authority.reset();
    const auto conditions=runtime::regional_market_conditions(projection,projection.simulation,state().date);
    const auto plain=core::market_opportunity(market,pool,state().date);const auto designed=core::market_opportunity(market,pool,state().date,nullptr,conditions);
    check(designed.segments.front().edges.front().weight>plain.segments.front().edges.front().weight,"Released UI reaches actual market allocation weights");
    check(core::software_market_bonus(state(),core::ProductChannel::consumer,core::OrdinalDay{state().date.value()-1}).value()==0,"No interface benefit before release date");
    auto revision=d;revision.previous=1;revision.name="Clear Chat II";const auto refined=core::software_quote(state(),revision);check(refined.staff_days<quote.staff_days && refined.usability>quote.usability,"Released design gives concrete refinement benefit");act(core::StartSoftware{revision});
    refuse(core::EnrollSkill{"founder","law","company"},"qualification");refuse(core::EnrollSkill{"founder","programming","personal"},"already");
    const auto personal=state().succession->personal_cash;const auto company=state().cash;act(core::EnrollSkill{"founder","architecture","personal"});
    check(state().succession->personal_cash<personal && state().cash==company,"Personal tuition stays separate from company");refuse(core::EnrollSkill{"founder","drawing","company"},"current");save();
    advance(90);check(core::person_has_skill(state(),"founder","architecture"),"Dated course grants capability");check(state().management->people->talent->courses.front().completed,"Course completed once");save();
    act(core::StartSoftware{advanced});
    auto assisted=advanced;assisted.ai_source="hq.equipment/2";
    check(core::software_ai_level(state())==0,"No AI staffing waiver without independent evidence");
    // Pure eligibility projection: three conservative panel results spanning ninety days.
    // Real panel execution and evidence validation remain owned by Frontier tests.
    auto ai=state();for(auto& project:ai.capacity->projects)if(project.status==core::ProjectStatus::active)project.status=core::ProjectStatus::paused;
    ai.management->people->talent->founder_skills.clear();
    const auto service=std::string("product.ai-fixture");
    core::CapacityProject backend;backend.id="ai-fixture";backend.kind=core::ProjectKind::serving;backend.status=core::ProjectStatus::active;ai.capacity->projects.push_back(backend);
    ai.deployments->records.push_back({service,"model-fixture","evidence-fixture",backend.id});
    ai.market->products.push_back({service,backend.id});
    const auto release=static_cast<std::uint64_t>(ai.deployments->records.size());
    ai.frontier->systems.push_back({1,"Qualified development assistant",ai.date,{release}});
    for(int i=0;i<3;++i){core::FrontierTrial trial;trial.id=static_cast<std::uint64_t>(i+1);trial.system=1;trial.kind=core::FrontierTrialKind::assessment;core::FrontierResult result;result.valid=true;result.observed_on=core::OrdinalDay{ai.date.value()-90+i*45};result.lower.fill(9500);trial.result=result;ai.frontier->trials.push_back(trial);}
    check(core::software_ai_level(ai)==2,"Independent high software capability covers both technical roles");
    const auto ai_quote=core::software_quote(ai,assisted);check(ai_quote.blocker.empty() && ai_quote.ai_level==2 && ai_quote.staff_days<120,"AI qualification reduces work only with allocated compute");
    ai.market->products.back().available=false;check(core::software_ai_level(ai)==0,"Unavailable deployment removes AI staffing waiver");
    auto all_in=f;all_in.personal_investment_cents=-1;all_in.choices["elem_playground"]="athletic";all_in.choices["ms_clubs"]="debate";all_in.choices["hs_identity"]="organize";all_in.choices["code_camp"]="design";all_in.choices["hs_exclude"]="make_room";
    for(const auto& scene:catalog.scenes)if(!scene.pool.empty()) {
        for(const auto& choice:scene.choices) {
            const auto rewards=catalog.skill_rewards.find(scene.id+"/"+choice.id);
            if(rewards==catalog.skill_rewards.end() || std::ranges::find(rewards->second,"programming")==rewards->second.end()) {all_in.choices[scene.id]=choice.id;break;}
        }
    }
    auto choice_options=options;choice_options.founding=all_in;
    const auto all_in_company=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},choice_options);
    check(all_in_company.world.simulation.succession->personal_cash==core::Cents{0},"Investing all savings remains a valid player choice");
    check(!core::person_has_skill(all_in_company.world.simulation,"founder","programming"),"Genius alone does not confer Programming");
    std::cout<<"Talent: qualified applicants, childhood skill, real tuition, dates, software work, power pause/resume, architecture gate, refinement and exact save passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
