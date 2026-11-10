#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;
void check(bool okay,const char* why) {if(!okay)throw std::runtime_error(why);}
runtime::Runtime start(const runtime::CompiledProgram& program,bool genius=false) {
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection founder;
    founder.family="rich";founder.leg=genius?"genius":"political";
    founder.route="hired_ceo";founder.office="shared";founder.researcher=genius?"founder":"none";
    for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;options.founding=founder;options.seed=719;
    options.identity=runtime::CampaignIdentity{"Departure test","Alex Vale","they/them"};
    auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,initial};
    const auto result=game.prepare_opening(game.current_state().world.simulation.revision);
    check(result.accepted(),result.explanation.c_str());return game;
}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});
    if(!result.accepted())throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,core::ContactAction action,std::string_view reason) {
    const auto before=game.current_state();
    const auto result=game.submit({before.world.simulation.revision,core::ManagePeopleContact{std::move(action)}});
    check(!result.accepted() && before==game.current_state(),"A refused dismissal must preserve people, money and commitments.");
    if(result.explanation.find(reason)==std::string::npos)throw std::runtime_error("Expected refusal: "+std::string{reason}+"; got: "+result.explanation);
}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Employment, equipment, cash and project history must reload exactly.");
}
template<class F> void corrupt(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged employment history was accepted.");}
}

int main() {
    using namespace pgp;
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_DEPARTURE_SOURCE,content::maximum_document_bytes),registry),registry);
        auto game=start(program);
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto person=state().management->people->employees.front().terms.profile.id;
        const auto other=state().management->people->employees.back().terms.profile.id;
        const auto room=core::employee_contact_room(state(),person);
        const auto original=game.current_state();
        const auto quote=core::dismissal_quote(state(),person);
        check(original==game.current_state() && quote.monthly_salary==core::Cents{900000} && quote.severance==quote.monthly_salary,
            "The quote must disclose one actual month of inherited salary without changing state.");
        const auto original_other_salary=core::employee_monthly_salary(state(),state().management->people->employees.back());
        const auto freed_desk=core::assigned_hq_desk(state(),person);
        refuse(game,core::DismissPeopleContact{person,"tablet",0},"contact details");
        refuse(game,core::DismissPeopleContact{person,{},room+1},"current headquarters room");
        apply(game,core::ManagePeopleContact{core::ExchangePeopleContact{person,room}});
        refuse(game,core::DismissPeopleContact{person,"tablet",0},"cannot send");
        apply(game,core::BuyPersonalDevice{"tablet"});
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"laptop"}});
        const auto laptop=state().infrastructure->workplace->headquarters->items.back().id;
        apply(game,core::AdvanceDay{});apply(game,core::AdvanceDay{});
        apply(game,core::ManageHeadquarters{core::AssignHQLaptop{laptop,person}});
        const auto named_before=core::research_team(state(),std::vector<std::string>{person,other});
        apply(game,core::StartResearch{"research.reproducible_tooling",{"hq.equipment/2",core::MilliCUPerDay{4000},1,{person}}});
        const auto research=state().lab->research.back().project_id;
        apply(game,core::AdvanceDay{});
        const auto project_before=*std::ranges::find(state().capacity->projects,research,&core::CapacityProject::id);
        check(project_before.completed_work.value()>0,"The departure fixture must include earned research work.");
        apply(game,core::PauseProject{research});
        const auto pending=core::dismissal_quote(state(),person);
        check(pending.projects==std::vector<std::string>{research} && pending.returned_items==2,"The review must include paused named research, a desk and the issued laptop.");
        const auto cash=state().cash,private_cash=state().succession->personal_cash;
        const auto staff=state().capacity->available_staff;
        const auto departed=state().date;
        apply(game,core::ManagePeopleContact{core::DismissPeopleContact{person,"tablet",0}});
        const auto& employee=state().management->people->employees.front();
        check(employee.departed_on==departed && employee.dismissal_severance==pending.severance &&
            state().cash==core::checked_unit_subtract(cash,pending.severance) && state().succession->personal_cash==private_cash,
            "Dismissal must charge company cash exactly once and preserve personal savings.");
        check(state().capacity->available_staff==staff-1 && core::assigned_hq_desk(state(),person)==0 && !core::hq_laptop_ready(state(),person),
            "The departed employee must release one capacity slot and both equipment assignments.");
        check(std::ranges::find(state().infrastructure->workplace->headquarters->items,laptop,&core::HQEquipment::id)->person.empty(),"The company must retain its returned laptop.");
        const auto salary=std::ranges::find(state().accounts->recurring,person,&core::RecurringObligation::id);
        check(salary->ends_before==departed && core::employee_monthly_salary(state(),state().management->people->employees.back())==original_other_salary,
            "Only this person's payroll ends; their colleagues keep their original pay.");
        const auto& stopped=*std::ranges::find(state().capacity->projects,research,&core::CapacityProject::id);
        check(stopped.status==core::ProjectStatus::cancelled && stopped.completed_work==project_before.completed_work && stopped.stopped_on==departed,
            "Cancellation must preserve the actual research work and its stop date.");
        check(core::research_team(state(),std::vector<std::string>{person,other}).creativity==named_before.creativity,
            "Historical research identities and traits must remain available.");
        refuse(game,core::DismissPeopleContact{person,"tablet",0},"already left");
        refuse(game,core::DismissPeopleContact{person,{},room},"current headquarters room");
        apply(game,core::ManagePeopleContact{core::MessagePeopleContact{person,"check_in","tablet",0}});
        check(state().management->people->contacts.front().messages.back().reply=="former","Former colleagues must remain reachable in the saved conversation.");
        exact(program,game);
        auto forged=game.current_state();forged.world.simulation.management->people->employees.front().dismissal_severance=core::Cents{1};
        corrupt([&]{runtime::validate_checkpoint(program,forged);});
        forged=game.current_state();forged.world.simulation.capacity->staff_departures.clear();
        corrupt([&]{runtime::validate_checkpoint(program,forged);});
        forged=game.current_state();std::ranges::find(forged.world.simulation.accounts->recurring,person,&core::RecurringObligation::id)->ends_before.reset();
        corrupt([&]{runtime::validate_checkpoint(program,forged);});
        apply(game,core::AdvanceDay{});exact(program,game);
        auto withdrawn=game;
        apply(withdrawn,core::WithdrawOperations{});
        exact(program,withdrawn);

        const auto applicant=state().management->people->applicants.front();
        apply(game,core::ManageRecruitment{core::ProposeEmployment{applicant.profile.id,applicant.recruitment->ask_salary,applicant.recruitment->ask_signing}});
        apply(game,core::AdvanceDay{});
        apply(game,core::HireEmployee{applicant.profile.id,freed_desk});
        const auto ready=state().management->people->employees.back().ready_on;
        while(state().date<ready)apply(game,core::AdvanceDay{});
        const auto hired_quote=core::dismissal_quote(state(),applicant.profile.id);
        check(hired_quote.monthly_salary==state().management->people->employees.back().terms.salary,"A negotiated hire's severance must use their signed salary.");
        apply(game,core::ManagePeopleContact{core::DismissPeopleContact{applicant.profile.id,{},core::employee_contact_room(state(),applicant.profile.id)}});
        const auto departed_hire=game.current_state();
        const auto evaluation=game.submit({state().revision,core::StartEvaluation{"departed-specialist","model","plan",{"hq.equipment/2",core::MilliCUPerDay{10},1,{}},applicant.profile.id}});
        check(!evaluation.accepted() && evaluation.explanation.find("paid appointment")!=std::string::npos && departed_hire==game.current_state(),"A former evaluation specialist must not receive another assignment.");
        exact(program,game);

        auto busy=start(program);
        const auto& busy_state=busy.current_state().world.simulation;
        const auto busy_person=busy_state.management->people->employees.front().terms.profile.id;
        const auto busy_room=core::employee_contact_room(busy_state,busy_person);
        core::CapacityProject commitment;
        commitment.id="test.other-work";commitment.source_id=busy_state.capacity->sources.front().id;
        commitment.kind=core::ProjectKind::research;commitment.starts_on=busy_state.date;
        commitment.requested=core::MilliCUPerDay{100};commitment.total_work=core::MilliCU{100000};
        commitment.staff=busy_state.capacity->available_staff;commitment.cause="Other committed work";
        apply(busy,core::ReserveProject{commitment});
        refuse(busy,core::DismissPeopleContact{busy_person,{},busy_room},"remaining team");
        apply(busy,core::CancelProject{commitment.id});
        apply(busy,core::ManagePeopleContact{core::DismissPeopleContact{busy_person,{},busy_room}});
        exact(program,busy);

        auto blocked=start(program);
        auto& blocked_state=blocked.current_state().world.simulation;
        const auto blocked_person=blocked_state.management->people->employees.front().terms.profile.id;
        const auto blocked_room=core::employee_contact_room(blocked_state,blocked_person);
        const auto source=blocked_state.capacity->sources.front().id;
        apply(blocked,core::AssignOperationsManager{blocked_person,source});
        check(core::dismissal_quote(blocked.current_state().world.simulation,blocked_person).operations_manager,"A manager's appointment must be in the review.");
        apply(blocked,core::ManagePeopleContact{core::DismissPeopleContact{blocked_person,{},blocked_room}});
        check(!core::operations_manager_on(blocked.current_state().world.simulation,blocked.current_state().world.simulation.date) &&
            blocked.current_state().world.simulation.management->people->contacts.size()==1,"In-person dismissal must close management and retain the conversation without an earlier exchange.");
        exact(program,blocked);

        auto reserved=start(program);
        const auto& reserve_state=reserved.current_state().world.simulation;
        const auto reserve_person=reserve_state.management->people->employees.front().terms.profile.id;
        const auto reserve_room=core::employee_contact_room(reserve_state,reserve_person);
        apply(reserved,core::ReserveFunds{"Protected treasury",core::uncommitted_cash(reserve_state)});
        refuse(reserved,core::DismissPeopleContact{reserve_person,{},reserve_room},"cash");

        auto genius=start(program,true);
        const auto& genius_state=genius.current_state().world.simulation;
        const auto founder=genius_state.management->people->employees.front().terms.profile.id;
        corrupt([&]{(void)core::dismissal_quote(genius_state,founder);});
        exact(program,genius);
        std::cout<<"Dated dismissal, severance, individual payroll, returned equipment, cancelled research, management release, former messages, cash protection and exact saves passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
