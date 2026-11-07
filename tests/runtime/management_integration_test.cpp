#include "../content/slice_fixture.hpp"
#include <pgp/core/company_finance.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/session/campaign.hpp>
#include <pgp/session/milestone.hpp>
#include <pgp/session/slice_run.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;
using Json=nlohmann::json;
int checks=0;
void check(bool condition,const char* why) {++checks;if (!condition) throw std::runtime_error(why);}
runtime::CompiledProgram compile(Json doc,bool setbacks=true) {
    pgp_test::scenario_doc(doc)["flows"]=Json::array();pgp_test::scenario_doc(doc)["rivals"]=Json::array();
    if (!setbacks) pgp_test::scenario_doc(doc)["management"]["setbacks"]=Json::array();
    const auto registry=content::foundation_registry();return runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
}
runtime::Runtime begin(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    if (!engine.prepare_opening(engine.current_state().world.simulation.revision).accepted()) throw std::runtime_error("Prepare morning failed");return engine;
}
void apply(runtime::Runtime& engine,core::CommandAction action) {
    const auto result=engine.submit({engine.current_state().world.simulation.revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void refused(runtime::Runtime& engine,core::CommandAction action) {
    const auto before=engine.checkpoint();
    check(!engine.submit({before.world.simulation.revision,std::move(action)}).accepted(),"Invalid management command accepted");
    check(engine.checkpoint()==before,"Refusal mutated budget, resources, notices, clocks or RNG");
}
void days(runtime::Runtime& engine,unsigned count) {while (count--) apply(engine,core::AdvanceDay{});}
const core::SimulationState& state(const runtime::Runtime& engine) {return engine.current_state().world.simulation;}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& engine) {
    check(persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint==engine.checkpoint(),"Management save failed exact state roundtrip");
}
core::DelegationPolicy evaluation(std::string id,std::string target="baseline") {
    core::DelegationPolicy p;p.id=std::move(id);p.target_id=std::move(target);p.plan_id="evaluation.held_out";p.allocation={"lease.initial",core::MilliCUPerDay{20000},1};
    p.budget=core::Cents{400000};p.reserve_floor=core::Cents{10000000};p.interval=core::Days{30};p.maximum_runs=2;return p;
}
void training(runtime::Runtime& engine) {
    apply(engine,core::BuyData{"data.licensed_documents"});
    apply(engine,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(engine,core::StartTraining{{"baseline","recipe.document_baseline","",{"data.licensed_documents"},"lease.initial",core::MilliCUPerDay{60000},4,core::MilliCU{3600000}}});
}
void bounds(const Json& source) {
    for (int i=0;i<8;++i) {
        auto doc=source;auto& c=pgp_test::scenario_doc(doc)["management"];
        if (i==0) c["scoring"]["weights"][0]=99;
        if (i==1) c["maximum_policy_runs"]=100000;
        if (i==2) c["minimum_evaluation_interval_days"]=0;
        if (i==3) c["financing"][0]["proceeds_cents"]=0;
        if (i==4) c["specialists"][0]["monthly_salary_cents"]=-1;
        if (i==5) c["setbacks"][0]["announced_on"]="2014-05-01";
        if (i==6) c["unknown"]=true;
        if (i==7) c["scoring"]["task_target"]=1.5;
        bool rejected=false;try {(void)compile(doc);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed management content accepted");
    }
}
void policies(const Json& source) {
    const auto program=compile(source,false);auto engine=begin(program);training(engine);
    const auto random=state(engine).random;
    auto bad=evaluation("bad");bad.plan_id="evaluation.headline";refused(engine,core::AddDelegationPolicy{bad});
    bad=evaluation("bad");bad.maximum_runs=13;refused(engine,core::AddDelegationPolicy{bad});
    bad=evaluation("bad");bad.interval=core::Days{1};refused(engine,core::AddDelegationPolicy{bad});
    apply(engine,core::AddDelegationPolicy{evaluation("tests")});days(engine,59);
    check(state(engine).management->decisions.empty() && state(engine).lab->evaluations.empty(),"Policy tested a live checkpoint or spent while waiting");
    days(engine,1);
    check(state(engine).management->policies.front().runs==1 && state(engine).lab->evaluations.size()==1 && state(engine).management->policies.front().committed.value()==200000,"Delegated evaluation omitted exact paid project/counters");
    check(state(engine).random==random,"Scheduling an evaluation sampled future evidence before work completion");
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint};
    for (unsigned i=0;i<31;++i) {days(engine,1);days(resumed,1);check(engine.checkpoint()==resumed.checkpoint(),"Delegation resume diverged across evaluation interval or RNG");}
    check(state(engine).management->policies.front().runs==2 && !state(engine).management->policies.front().enabled,"Finite policy run ceiling was bypassed");
    refused(engine,core::SetDelegationEnabled{"tests",true});exact(program,engine);
    auto tampered=engine.checkpoint();tampered.world.simulation.management->policies.front().committed=core::Cents{0};
    bool rejected=false;try {runtime::validate_checkpoint(program,tampered);}catch(const std::exception&){rejected=true;}check(rejected,"Save reset a spent policy counter");
    auto budget=evaluation("over_budget");budget.budget=core::Cents{1};apply(engine,core::AddDelegationPolicy{budget});
    const auto before=state(engine).lab->evaluations.size();days(engine,1);
    check(!state(engine).management->policies.back().enabled && state(engine).lab->evaluations.size()==before && !state(engine).management->decisions.back().accepted,"Budget refusal created an evaluation or left policy running");
    const auto count=state(engine).management->decisions.size();days(engine,3);
    check(state(engine).management->decisions.size()==count,"Blocked policy spammed a daily decision log");
    auto floor=evaluation("cash_floor");floor.reserve_floor=state(engine).cash;apply(engine,core::AddDelegationPolicy{floor});days(engine,1);
    check(!state(engine).management->decisions.back().accepted && state(engine).lab->evaluations.size()==before,"Policy ignored reserve floor");exact(program,engine);
}
void renewals(const Json& source) {
    const auto program=compile(source,false);auto engine=begin(program);apply(engine,core::AcceptBusinessOffer{"offer.supplier_small"});
    auto policy=evaluation("lease");policy.kind=core::DelegationKind::lease_renewal;policy.target_id="offer.supplier_small";policy.plan_id.clear();policy.allocation={};
    policy.budget=core::Cents{3000000};policy.monthly_ceiling=core::Cents{400000};policy.reserve_floor=core::Cents{1000000};policy.maximum_runs=1;
    apply(engine,core::AddDelegationPolicy{policy});refused(engine,core::RenewComputeLease{"offer.supplier_small"});days(engine,179);
    const auto before=state(engine).cash;exact(program,engine);
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint};
    days(engine,1);days(resumed,1);check(engine.checkpoint()==resumed.checkpoint(),"Expiry morning renewal diverged after reload");
    check(state(engine).business_offers->renewals.size()==1 && core::lease_expiry(*state(engine).business_offers,"offer.supplier_small").value()==360,"Renewal missed actual expiry or invented extra term");
    check(state(engine).management->policies.front().committed.value()==2900000 && state(engine).cash<before,"Renewal ignored conservative full-term budget or setup payment");
    check(state(engine).capacity->sources.back().effective.value()==30000,"Paid lease capacity did not remain available");
    refused(engine,core::RenewComputeLease{"offer.supplier_small"});exact(program,engine);
    auto refused_engine=begin(program);apply(refused_engine,core::AcceptBusinessOffer{"offer.supplier_small"});policy.id="too_cheap";policy.monthly_ceiling=core::Cents{399999};
    apply(refused_engine,core::AddDelegationPolicy{policy});days(refused_engine,180);
    check(state(refused_engine).business_offers->renewals.empty() && state(refused_engine).capacity->sources.back().effective.value()==0 && !state(refused_engine).management->decisions.back().accepted,"Renewal bypassed monthly ceiling or expired-source limit");
    exact(program,refused_engine);
}
void recovery_financing(const Json& source) {
    const auto program=compile(source);auto engine=begin(program);
    days(engine,14);const auto earlier=state(engine).capacity->days;
    apply(engine,core::HireSpecialist{"specialist.evaluation"});
    check(state(engine).capacity->available_staff==7 && state(engine).capacity->days==earlier && core::available_staff_on(*state(engine).capacity,core::OrdinalDay{0})==6,"Specialist rewrote historical staff or omitted paid capacity");
    refused(engine,core::HireSpecialist{"specialist.evaluation"});
    const auto profit=core::accounts_report(*state(engine).accounts).profit;
    apply(engine,core::TakeFinancing{"finance.bridge_equity"});
    check(core::accounts_report(*state(engine).accounts).profit==profit && core::slice_score(state(engine)).founder_share.value()==4510,"Equity became profit or did not dilute founder stake");
    refused(engine,core::TakeFinancing{"finance.bridge_equity"});apply(engine,core::TakeFinancing{"finance.working_capital"});
    check(state(engine).accounts->debts.front().principal.value()==25000000 && state(engine).accounts->debts.front().due.value()==379,"Loan omitted real principal/due date");
    refused(engine,core::TakeFinancing{"finance.working_capital"});exact(program,engine);
    days(engine,106);
    check(state(engine).date.value()==120 && core::required_management_notice(state(engine)) && state(engine).capacity->sources.front().effective.value()==60000,"Warned setback did not stop at its actual capacity boundary");
    refused(engine,core::AdvanceDay{});refused(engine,core::SetPaused{false});
    exact(program,engine);const auto saved=engine.checkpoint();
    const auto notice=std::find_if(state(engine).management->notices.begin(),state(engine).management->notices.end(),[](const auto& n){return n.required && !n.acknowledged;});
    const auto id=notice->id;
    auto quiet_settings=persistence::Settings{};quiet_settings.autosave_enabled=false;quiet_settings.auto_pause_alerts=false;quiet_settings.auto_pause_projects=false;quiet_settings.auto_pause_evaluations=false;
    auto before_setback=begin(program);days(before_setback,119);
    session::Campaign quiet{program,before_setback.checkpoint(),std::filesystem::current_path()/"unused-quiet-autosaves",quiet_settings};
    const auto halted=quiet.submit({quiet.checkpoint().world.simulation.revision,core::AdvanceDay{}});
    check(halted.command.accepted() && halted.halt_requested && quiet.checkpoint().world.simulation.date.value()==120 && core::required_management_notice(quiet.checkpoint().world.simulation),"Optional pause preferences suppressed a mandatory setback boundary");
    auto corrupt=Json::parse(persistence::encode_save(program,engine.checkpoint()));
    for (auto& n:corrupt["checkpoint"]["world"]["simulation"]["management"]["notices"]) if (n["required"]==true) n["required"]=false;
    corrupt.erase("sha256");corrupt["sha256"]=persistence::sha256(corrupt.dump());
    bool warning_rejected=false;try {(void)persistence::decode_save(program,corrupt.dump());}catch(const persistence::PersistenceError&){warning_rejected=true;}
    check(warning_rejected,"Rehashed save stripped the required setback response");
    apply(engine,core::AcknowledgeManagementNotice{id});refused(engine,core::AcknowledgeManagementNotice{id});
    days(engine,14);check(state(engine).capacity->sources.front().effective.value()==100000 && !core::required_management_notice(state(engine)),"Finite setback did not restore nominal capacity");exact(program,engine);
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,saved)).checkpoint};
    apply(resumed,core::AcknowledgeManagementNotice{id});days(resumed,14);check(resumed.checkpoint()==engine.checkpoint(),"Recovery acknowledgement/restoration diverged after save");
    const auto debt=state(engine).accounts->debts.front();apply(engine,core::RepayDebt{debt.id,debt.principal,"test.repay"});
    check(state(engine).accounts->debts.front().principal.value()==0,"Principal repayment did not clear the loan");exact(program,engine);
}
void specialist_ending(const Json& source) {
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(source.dump(),registry),registry);
    const auto boundary=session::run_slice_specialist(program,core::OrdinalDay{100});
    check(boundary.stop==session::SliceStop::requested_boundary,"Specialist strategy could not reach its ongoing policy boundary");
    const auto continued=session::run_slice_specialist(program,{},persistence::decode_save(program,persistence::encode_save(program,boundary.checkpoint)).checkpoint);
    const auto& s=continued.checkpoint.world.simulation;const auto score=core::slice_score(s);
    check(continued.stop==session::SliceStop::finished && s.cash.value()>120000000 && core::accounts_report(*s.accounts).profit.value()>0,"Efficient document specialist did not finish with actual operating profit");
    check(s.market->products.size()==1 && s.market->products.front().id=="product.document_specialist" && s.management->financing.empty(),"Specialist survival silently depended on broad-scope products or new capital");
    check(s.management->policies.front().runs==2 && s.lab->evaluations.size()==2 && s.management->specialists.size()==1,"Specialist strategy bypassed finite independent tests or paid hiring");
    check(score.final && score.total==84 && score.points[3]==10 && score.founder_share.value()==5500,"Specialist score omitted its deliberate domain scope or founder stake");
    check(persistence::decode_save(program,persistence::encode_save(program,continued.checkpoint)).checkpoint==continued.checkpoint,"Scored specialist ending did not persist exactly");
}
void pause_milestone(const Json& source) {
    const auto program=compile(source,false);auto engine=begin(program);training(engine);
    auto settings=persistence::Settings{};settings.autosave_enabled=false;settings.auto_pause_alerts=false;
    session::Campaign campaign{program,engine.checkpoint(),std::filesystem::current_path()/"unused-management-autosaves",settings};
    const auto milestone=session::plan_milestone(campaign.checkpoint(),"project");
    for (unsigned i=0;i<3;++i) {
        const auto result=campaign.submit({campaign.checkpoint().world.simulation.revision,core::AdvanceDay{}});
        check(result.command.accepted(),"Project advancement failed");
        if (i==2) check(result.halt_requested && result.pause_notice && session::milestone_reached(campaign.checkpoint(),milestone),"Completion did not auto-pause at actual project boundary");
    }
    settings.auto_pause_projects=false;campaign.set_settings(settings);
    for (unsigned i=0;i<57;++i) {
        const auto result=campaign.submit({campaign.checkpoint().world.simulation.revision,core::AdvanceDay{}});
        check(result.command.accepted() && !result.halt_requested,"Disabling project auto-pause did not keep optional completion running");
    }
    auto eval=campaign.submit({campaign.checkpoint().world.simulation.revision,core::StartEvaluation{"manual.test","baseline","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}}});
    check(eval.command.accepted(),"Manual evaluation failed");
    for (unsigned i=0;i<3;++i) {
        const auto result=campaign.submit({campaign.checkpoint().world.simulation.revision,core::AdvanceDay{}});
        check(result.command.accepted(),"Evaluation advancement failed");
        if (i==2) check(result.halt_requested && result.pause_notice.has_value(),"Evaluation results did not obey their separate pause preference");
    }
    // Settings remain local preferences; saving a campaign does not reset them.
    check(persistence::decode_settings(persistence::encode_settings(settings))==settings,"Auto-pause settings failed exact local roundtrip");
    bool missing=false;try {(void)session::plan_milestone(begin(program).checkpoint(),"deadline");}catch(const std::invalid_argument&){missing=true;}
    check(missing,"Milestone fabricated a deadline for routine news");
    auto leased=begin(program);apply(leased,core::AcceptBusinessOffer{"offer.supplier_small"});
    const auto deadline=session::plan_milestone(leased.checkpoint(),"deadline");
    check(deadline.deadline==core::OrdinalDay{180},"Milestone ignored actual paid lease expiry");
    auto boundary=leased.checkpoint();boundary.world.simulation.date=*deadline.deadline;
    check(session::milestone_reached(boundary,deadline),"Fast-forward missed its exact deadline morning");
    settings.auto_pause_alerts=false;settings.auto_pause_evaluations=false;
    session::Campaign quiet_lease{program,leased.checkpoint(),std::filesystem::current_path()/"unused-expiry-autosaves",settings};
    for (unsigned i=0;i<180;++i) {
        const auto result=quiet_lease.submit({quiet_lease.checkpoint().world.simulation.revision,core::AdvanceDay{}});
        check(result.command.accepted(),"Paid lease day failed");
        if (i==179) check(result.halt_requested && result.pause_notice.has_value() && quiet_lease.checkpoint().world.simulation.capacity->sources.back().effective.value()==0,"Disabled optional alerts skipped actual lease expiry");
    }
    auto borrower=begin(program);apply(borrower,core::TakeFinancing{"finance.working_capital"});days(borrower,364);
    session::Campaign quiet_debt{program,borrower.checkpoint(),std::filesystem::current_path()/"unused-maturity-autosaves",settings};
    const auto maturity=quiet_debt.submit({quiet_debt.checkpoint().world.simulation.revision,core::AdvanceDay{}});
    check(maturity.command.accepted() && maturity.halt_requested && maturity.pause_notice.has_value() && quiet_debt.checkpoint().world.simulation.date.value()==365,"Disabled optional alerts skipped actual debt principal deadline");
}

void funding_support(Json source) {
    const std::string grant="support.methodology_grant",credit="support.opening_cloud_credit";
    auto& offers=pgp_test::scenario_doc(source)["management"]["support_offers"];
    pgp_test::named_row(offers,grant)["limit_cents"]=50'000;
    pgp_test::named_row(offers,credit)["use_days"]=3;
    const auto program=compile(std::move(source),false);
    auto game=begin(program);
    auto control=begin(program);
    const auto cash=state(game).cash;
    const auto ownership=state(game).accounts->holdings;
    apply(game,core::TakeFinancing{grant});
    apply(game,core::TakeFinancing{credit});
    check(state(game).cash==cash && state(game).accounts->holdings==ownership && state(game).accounts->debts.empty(),
          "Restricted awards created unrestricted money, shares or debt");
    refused(game,core::TakeFinancing{grant});
    refused(game,core::TakeFinancing{credit});
    check(core::research_cash_fee(state(game),"research.held_out_evaluation")==core::Cents{50'000},
          "The grant paid a fee before its next-morning activation");
    days(game,1);days(control,1);
    check(state(game).accounts->postings.totals().supplier_credits==0,"The supplier credited the claim date");
    const core::StartResearch research{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20'000},2}};
    auto bad=research;
    bad.allocation.staff=100;
    refused(game,bad);
    check(state(game).management->funding_support.uses.empty(),"A refused project consumed the grant");
    for (auto* engine:{&game,&control}) {
        apply(*engine,research);
        const auto project=state(*engine).lab->research.back().project_id;
        apply(*engine,core::CancelProject{project});
        apply(*engine,research);
        apply(*engine,core::BuyData{"data.licensed_documents"});
    }
    const auto& funded=state(game).lab->research;
    check(funded.front().sponsored_fee==core::Cents{37'500} && funded.back().sponsored_fee==core::Cents{12'500} &&
          state(game).cash==core::checked_unit_add(state(control).cash,core::Cents{50'000}),
          "Grant cap, eligible cash co-payment or cancellation accounting is wrong");
    days(game,3);days(control,3);
    const auto& support=state(game).management->funding_support;
    const auto* award=&support.awards.back();
    check(award->offer_id==credit && award->used.value()>0 && award->used.value()<1'500'000 &&
          state(game).accounts->postings.totals().supplier_credits==3 &&
          state(game).cash==core::checked_unit_add(state(control).cash,core::Cents{50'000}),
          "Cloud credit must settle only its active daily charges without moving cash");
    const auto used=award->used;
    check(core::accounts_report(*state(control).accounts).payables==
          core::checked_unit_add(core::accounts_report(*state(game).accounts).payables,used),
          "Cloud credit did not reduce the actual supplier payable");
    const auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.checkpoint()));
    check(loaded.checkpoint==game.checkpoint(),"Claimed awards and their redemptions failed exact reload");
    runtime::Runtime resumed{program,loaded.checkpoint};
    while (state(game).date<core::OrdinalDay{31}) {days(game,1);days(control,1);days(resumed,1);}
    check(game.checkpoint()==resumed.checkpoint() && state(game).management->funding_support.awards.back().used==used &&
          state(game).cash==core::checked_unit_add(state(control).cash,core::checked_unit_add(core::Cents{50'000},used)),
          "Expired credit was redeemed again or the month's cash bill ignored the credit");
    auto tampered=Json::parse(persistence::encode_save(program,game.checkpoint()));
    tampered["checkpoint"]["world"]["simulation"]["management"].erase("funding_support");
    tampered.erase("sha256");
    tampered["sha256"]=persistence::sha256(tampered.dump());
    bool rejected=false;
    try {(void)persistence::decode_save(program,tampered.dump());}
    catch (const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"Unbacked noncash credits or grants survived restore");
}
void corporate_tax(Json source) {
    using namespace core;
    auto& opening=pgp_test::scenario_doc(source);
    opening.erase("backgrounds");
    for (auto& charge:opening["monthly_commitments_cents"]) charge=10000;
    const auto program=compile(std::move(source),false);
    auto checkpoint=session::run_slice_baseline(program,parse_iso_date("2014-04-01")).checkpoint;
    runtime::Runtime game{program,std::move(checkpoint)};
    const auto& accounts=*state(game).accounts;
    check(accounts.taxes && accounts.taxes->assessments.size()==1,"Playable opening missed its first quarterly return");
    const auto assessment=accounts.taxes->assessments.back();
    const auto id=tax_bill_id(assessment.through);
    const auto invoice=accounts.invoices.find(id);
    check(assessment.through==parse_iso_date("2014-03-31") && assessment.due==parse_iso_date("2014-04-30") &&
          assessment.bill.value()>0 && invoice && accounts.invoices[*invoice].settled==Cents{0},
          "A profitable service did not create an unpaid bill at its authored date");
    const auto actual=recent_company_period(state(game));
    check(actual && actual->corporate_tax_expense==assessment.bill,"Statements omitted the actual quarter-end tax expense");
    const auto projection=forecast_company_finances(state(game));
    check(projection.cases[1].cash.near_term.corporate_tax_paid>=assessment.bill,"The cash scenario lost the already assessed bill");
    const auto bytes=persistence::encode_save(program,game.checkpoint());
    runtime::Runtime resumed{program,persistence::decode_save(program,bytes).checkpoint};
    check(resumed.checkpoint()==game.checkpoint(),"Reload changed the tax liability or relief history");
    auto forged=Json::parse(bytes);
    forged["checkpoint"]["world"]["simulation"]["accounts"]["corporate_taxes"]["assessments"][0]["credit_cents"]=
        checked_add(assessment.credit.value(),1);
    forged.erase("sha256");
    forged["sha256"]=persistence::sha256(forged.dump());
    bool refused=false;
    try {(void)persistence::decode_save(program,forged.dump());}
    catch (const persistence::PersistenceError&) {refused=true;}
    check(refused,"A checksum-valid forged tax credit survived restore");
    days(game,30);days(resumed,30);
    const auto& paid=*state(game).accounts;
    const auto settled=paid.invoices.find(id);
    check(game.checkpoint()==resumed.checkpoint() && settled && paid.invoices[*settled].settled==assessment.bill,
          "Tax did not settle once after reopening");
}
}
int main(int argc,char** argv) {
    try {
        std::ifstream file{PGP_SLICE_CONTENT_PATH};
        const auto source=Json::parse(file);
        if (argc>1 && std::string_view{argv[1]}=="--funding-support") {
            funding_support(source);
        } else if (argc>1 && std::string_view{argv[1]}=="--corporate-tax") {
            corporate_tax(source);
        } else {
            for (const auto& [name,run]:std::vector<std::pair<const char*,void(*)(const Json&)>>{
                {"bounds",bounds},{"policies",policies},{"renewals",renewals},{"recovery_financing",recovery_financing},
                {"pause_milestone",pause_milestone},{"specialist_ending",specialist_ending}}) {
                try {run(source);} catch (const std::exception& e) {throw std::runtime_error(std::string{name}+": "+e.what());}
            }
        }
        std::cout<<checks<<" management integration checks passed\n";
        return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
