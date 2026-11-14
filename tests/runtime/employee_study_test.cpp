#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}}
int main(){using namespace pgp;try{
    auto doc=nlohmann::json::parse(persistence::read_bounded(PGP_STUDY_SOURCE,content::maximum_document_bytes));
    for(auto& d:doc["definitions"])if(d["id"]=="scenario.founder_campaign") {d["competition"]["domestic"]=1;d["competition"]["international"]=0;d["competition"]["heavyweights"]=0;}
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection founder;founder.family="rich";founder.advantages={"political"};founder.route="hired_ceo";founder.office="shared";founder.researcher="none";
    for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;options.founding=founder;options.seed=719;options.identity=runtime::CampaignIdentity{"Learning Jars","Alex Vale","they/them"};
    auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,initial};
    const auto state=[&]()->const auto&{return game.current_state().world.simulation;};
    const auto apply=[&](core::CommandAction a){const auto result=game.submit({state().revision,std::move(a)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto enroll=[&](std::string person,std::string skill,std::string payer){apply(core::ManageTalent{core::EnrollSkill{person,skill,payer}});};
    const auto refuse=[&](core::EnrollSkill a,std::string_view reason){const auto before=game.current_state();const auto result=game.submit({state().revision,core::ManageTalent{a}});check(!result.accepted() && game.current_state()==before,"Rejected tuition preserves every balance and course");if(result.explanation.find(reason)==std::string::npos)throw std::runtime_error(result.explanation);};
    const auto advance=[&](int days){for(int i=0;i<days;++i)apply(core::AdvanceDay{});};
    const auto exact=[&]{check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Study budgets and courses reload exactly");};
    check(game.prepare_opening(state().revision).accepted(),"Opening");
    std::string person,course;for(const auto& e:state().management->people->employees) {person=e.terms.profile.id;for(const auto& skill:core::talent_rules(state())->skills)if(skill.teachable && (skill.engineering || skill.research) && skill.tuition_cents>0 && skill.prerequisite.empty() && !core::person_has_skill(state(),person,skill.id)){course=skill.id;break;}if(!course.empty())break;}
    check(!course.empty(),"Professional course available");
    check(state().management->people->talent->study_funds.at(person).compensation==core::Cents{0},"Promised salary does not create savings");
    refuse({person,course,"personal"},"still saving");
    // An unpaid wage liability is not a receipt; this is an explicit cash-poor
    // settlement projection, separate from the funded playable company below.
    auto unpaid=state();core::record_cash(*unpaid.accounts,unpaid.date,core::Cents{-unpaid.cash.value()},core::Account::expenses,"Study fixture cash drain","study-fixture");unpaid.cash=core::Cents{0};
    for(int day=0;day<31;++day){core::close_accounts_day(*unpaid.accounts,unpaid.date,core::Cents{0},false);unpaid.date=core::advance_days(unpaid.date,core::Days{1});}core::reconcile_employee_study(unpaid);
    check(unpaid.management->people->talent->study_funds.at(person).compensation==core::Cents{0},"Unpaid accrued wages cannot finance classes");
    const auto payroll_start=state().date;advance(32);check(state().date==core::advance_days(payroll_start,core::Days{32}),"The funded company actually crosses month-end payday");const auto before_quote=game.current_state();const auto quote=core::course_quote(state(),{person,course,"personal"});
    if(!quote.blocker.empty())std::cerr<<person<<" "<<course<<" "<<quote.blocker<<'\n';
    check(quote.blocker.empty() && quote.budget>=quote.fee && game.current_state()==before_quote,"Read-only preview uses actual saved compensation");
    const auto company=state().cash,private_cash=state().succession->personal_cash,old_spent=state().management->people->talent->study_funds.at(person).spent;
    enroll(person,course,"personal");check(state().cash==company && state().succession->personal_cash==private_cash,"Employee tuition affects neither company nor founder cash");
    check(state().management->people->talent->study_funds.at(person).spent==core::checked_unit_add(old_spent,quote.fee),"Employee pays exact course fee once");
    check(core::personal_tuition(state(),core::advance_days(state().date,core::Days{1}))==core::Cents{0},"Employee fees never count as founder spending");
    refuse({person,course,"personal"},"current");exact();
    auto forged=game.current_state();forged.world.simulation.management->people->talent->study_funds.at(person).compensation=core::Cents{999999999};bool rejected=false;
    try{runtime::validate_checkpoint(program,forged);}catch(const std::exception&){rejected=true;}check(rejected,"Invented study income is rejected");
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint};
    for(int day=0;day<45;++day)for(auto* live:{&game,&resumed}) {const auto result=live->submit({live->current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);}
    check(game.current_state()==resumed.current_state(),"Autonomous study and wages continue identically after reload");
    check(std::ranges::any_of(state().management->people->talent->courses,[&](const auto& c){return c.person==person && c.skill==course && c.completed;}),"Employee acquires the paid skill on completion");
    check(std::ranges::any_of(state().management->people->talent->courses,[&](const auto& c){return c.person!=person && c.payer=="personal";}),"Other employees independently choose affordable classes");
    std::string sponsor;for(const auto& skill:core::talent_rules(state())->skills)if(skill.teachable && core::course_quote(state(),{"founder",skill.id,"company"}).blocker.empty()){sponsor=skill.id;break;}
    const auto sponsor_fee=core::course_quote(state(),{"founder",sponsor,"company"}).fee;const auto cash_before=state().cash;
    enroll("founder",sponsor,"company");check(state().cash==core::checked_unit_subtract(cash_before,sponsor_fee),"Company sponsorship remains an actual separate expense");exact();
    std::cout<<"Employee study: actual paid wages, unpaid refusal, voluntary budget, tuition separation, acquired skill, autonomous classes, corruption rejection and exact continuation passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
