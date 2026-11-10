#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace {
using namespace pgp;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Invalid founder terms were accepted.");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_FOUNDING_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        const auto& catalog=*scenario.founding;
        content::FoundingSelection selected;
        for(const auto& scene:catalog.scenes) selected.choices.emplace(scene.id,scene.choices.front().id);
        const auto make=[&](content::FoundingSelection s,std::optional<core::ObjectiveSelection> mission=core::ObjectiveSelection{core::ObjectiveKind::survival,{}}) {
            runtime::ScenarioOptions options;
            options.identity=runtime::CampaignIdentity{"Test research",s.first_name+" "+s.last_name,"she/her"};
            options.founding=std::move(s);
            return runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",mission,options);
        };
        content::FoundingSelection revised;
        revised.choices=selected.choices;revised.appearance=core::make_portrait(9,"founder");
        revised.family="rich";revised.route="bootstrap";revised.office="home";
        auto solo=make(revised);
        require(solo.world.difficulty.empty() && solo.world.simulation.capacity->sources.empty(),"New openings must not inherit difficulty or a compute lease.");
        require(solo.world.simulation.management->people->employees.size()==1,"A genius starts alone, without a forced first hire.");
        require(std::ranges::none_of(solo.world.simulation.accounts->recurring,[](const auto& r){return r.id=="office_and_admin" || r.id=="compute_lease" || r.id=="payroll_with_benefits";}),"The parents' house has no rent, employee payroll or compute subscription.");
        require(solo.world.simulation.management->people->employees.front().terms.profile.portrait==revised.appearance,"The research-capable founder must keep the customized portrait.");
        require(persistence::decode_save(program,persistence::encode_save(program,solo)).checkpoint==solo,"Revised biography and appearance must reload exactly.");
        revised.leg="charisma";revised.researcher="none";
        const auto nontechnical=make(revised);
        require(nontechnical.world.simulation.capacity->available_staff==0 && nontechnical.world.simulation.management->people->employees.empty(),"A nontechnical sole founder must not invent a researcher.");
        runtime::Runtime quiet{program,nontechnical};
        require(quiet.prepare_opening(quiet.current_state().world.simulation.revision).accepted(),"A company without research staff must open normally.");
        const auto quiet_day=quiet.submit({quiet.current_state().world.simulation.revision,core::AdvanceDay{}});
        require(quiet_day.accepted(),quiet_day.explanation.c_str());
        revised.family="poor";revised.personal_investment_cents=0;
        refuses([&]{(void)make(revised);});
        revised.personal_investment_cents=catalog.formation_cents+500000;
        const auto small_bootstrap=make(revised);
        require(small_bootstrap.world.simulation.cash==core::Cents{500000},"A founder with a modest background can self-fund with their chosen contribution.");
        {
            require(catalog.startup_loan.has_value(),"The founder campaign must offer startup credit.");
            const auto& loan=*catalog.startup_loan;
            auto application=revised;
            application.route="bank_loan";
            application.personal_investment_cents=loan.minimum_investment_cents-1;
            refuses([&]{(void)make(application);});
            application.personal_investment_cents=loan.minimum_investment_cents;
            auto funded=make(application);
            const auto& opening=funded.world.simulation;
            require(opening.cash==core::Cents{loan.principal_cents+loan.minimum_investment_cents-catalog.formation_cents},"The loan and personal contribution must both fund the new company.");
            require(opening.accounts->postings.front().amount==core::Cents{loan.minimum_investment_cents},"Borrowed cash must enter through the debt ledger rather than an equity posting.");
            require(opening.accounts->debts.size()==1 && opening.accounts->debts.front().principal==core::Cents{loan.principal_cents},"Signing must create one real principal balance.");
            require(opening.succession->personal_cash==core::Cents{content::founding_terms(catalog,application).personal_remaining_cents},"The loan must never enter the founder's personal account.");
            funded.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime borrower{program,funded};
            require(borrower.prepare_opening(opening.revision).accepted(),"The funded opening must prepare.");
            const auto command=[&](core::CommandAction action) {
                const auto result=borrower.submit({borrower.current_state().world.simulation.revision,std::move(action)});
                require(result.accepted(),result.explanation.c_str());
            };
            for(int day=0;day<31;++day) command(core::AdvanceDay{});
            const auto& accounts=*borrower.current_state().world.simulation.accounts;
            const auto interest=accounts.invoices.find("interest/founding/startup_loan/2015-01-01");
            require(interest.has_value() && accounts.invoices[*interest].amount==core::Cents{loan.principal_cents*loan.monthly_interest_bp/10000},"A full month must accrue the disclosed interest on actual principal.");
            command(core::RepayDebt{std::string{content::startup_debt_id},core::Cents{50000},"startup-test-repayment"});
            const auto& repaid=borrower.current_state();
            require(repaid.world.simulation.accounts->debts.front().principal==core::Cents{loan.principal_cents-50000},"Repayment must reduce the outstanding principal.");
            require(persistence::decode_save(program,persistence::encode_save(program,repaid)).checkpoint==repaid,"The loan, accrued interest and partial repayment must reload exactly.");
        }
        revised.route="investment";
        refuses([&]{(void)make(revised);});
        revised.backers={"commercial"};
        const auto financed=make(revised);
        require(financed.world.simulation.management->people->employees.empty(),"Investment does not hire staff.");
        revised.route="hired_ceo";revised.office="shared";revised.backers.clear();
        refuses([&]{(void)make(revised);});
        revised.leg="political";
        const auto employed=make(revised);
        const auto employment=content::founding_terms(catalog,revised);
        require(employed.world.simulation.management->people->employees.size()==catalog.opening->inherited_staff && employment.founder_bp==catalog.opening->ceo_equity_bp,"A hired CEO inherits a small roster and the disclosed minority grant.");
        require(employment.contributed_cents==content::founding_option(catalog.routes,"hired_ceo").capital_cents,"CEO opening cash is the existing company's cash, not family money.");
        require(employed.world.simulation.lab->research.empty() && employed.world.simulation.lab->models.empty(),"An existing office does not force a research or product choice.");
        require(persistence::decode_save(program,persistence::encode_save(program,employed)).checkpoint==employed,"Inherited company terms must reload exactly.");
        revised.backers={"commercial"};refuses([&]{(void)make(revised);});revised.backers.clear();
        runtime::ScenarioOptions with_difficulty;with_difficulty.founding=revised;with_difficulty.difficulty="difficulty.forgiving";
        refuses([&]{(void)runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",{},with_difficulty);});
        revised.route="partnership";
        refuses([&]{(void)make(revised,std::nullopt);});
        require(catalog.partners.size()==3,"The founder campaign must offer three distinct working cofounders.");
        for(const auto& partner:catalog.partners) {
            auto agreement=revised;
            agreement.partner=partner.id;
            agreement.family="poor";agreement.leg="charisma";
            refuses([&]{(void)make(agreement,std::nullopt);});
            agreement.leg=partner.access=="technical"?"genius":"political";
            agreement.researcher=partner.access=="technical"?"founder":"none";
            agreement.personal_investment_cents=0;
            refuses([&]{(void)make(agreement);});
            auto signed_opening=make(agreement,std::nullopt);
            const auto& world=signed_opening.world;
            const auto& state=world.simulation;
            const auto terms=content::founding_terms(catalog,agreement);
            require(state.cash==core::Cents{partner.capital_cents-terms.paid_setup_cents} &&
                state.succession->personal_cash==core::Cents{terms.personal_remaining_cents},"Cofounder capital must fund the company and leave personal savings intact.");
            const auto holding=std::ranges::find(state.accounts->holdings,"cofounder",&core::EquityHolding::holder);
            require(holding!=state.accounts->holdings.end() && holding->shares==core::BasisPoints{partner.shares_bp},"The actual equity ledger must use the agreement.");
            require(core::objective_key(world.objectives->selections.front().selection.primary)==partner.objective,"Signing must set the cofounder's opening mission.");
            const auto& people=*state.management->people;
            const auto employee=std::ranges::find_if(people.employees,[&](const auto& row){return row.terms.profile.name==partner.name;});
            require(people.employees.size()==terms.opening_staff && employee!=people.employees.end() &&
                core::employee_monthly_salary(state,*employee)==core::Cents{partner.monthly_salary_cents},"The signed cofounder must join the roster at the quoted salary.");
            require(people.contacts.size()==1 && people.contacts.front().person=="opening/"+partner.id &&
                people.contacts.front().opening->outcome=="signed","The conversation must carry into the company inbox as signed.");
            const auto board=core::current_board(state);
            const auto seat=std::ranges::find(board,"board."+partner.id,&core::BoardMember::id);
            require(seat!=board.end() && seat->name==partner.name && seat->seats==partner.seats &&
                seat->cash_floor==core::Cents{partner.cash_floor_cents} && seat->maximum_dilution==core::BasisPoints{partner.maximum_dilution_bp},"The actual board must use the disclosed cofounder terms.");
            auto forged=signed_opening;
            forged.world.simulation.management->people->contacts.front().opening->outcome="unsigned";
            refuses([&]{runtime::validate_checkpoint(program,forged);});
            signed_opening.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime partnered{program,signed_opening};
            require(partnered.prepare_opening(state.revision).accepted(),"The cofounder opening must prepare.");
            const auto command=[&](core::CommandAction action) {
                const auto result=partnered.submit({partnered.current_state().world.simulation.revision,std::move(action)});
                require(result.accepted(),result.explanation.c_str());
            };
            command(core::ProposePoolExpansion{core::BasisPoints{800}});
            const auto& governance=*partnered.current_state().world.simulation.management->governance;
            command(core::VoteBoardMotion{governance.motions.back().id,true});
            const auto& saved=partnered.current_state();
            const auto& ballots=saved.world.simulation.management->governance->motions.back().ballots;
            const auto ballot=std::ranges::find(ballots,"board."+partner.id,&core::BoardBallot::member);
            require(ballot!=ballots.end() && ballot->seats==partner.seats && ballot->support==(partner.maximum_dilution_bp>=800),"Cofounders must actually vote according to their own dilution limits.");
            require(persistence::decode_save(program,persistence::encode_save(program,saved)).checkpoint==saved,"Signed terms and the actual cofounder ballot must reload exactly.");
        }
        revised.route="mission_director";revised.legal_form="nonprofit";revised.office="home";
        refuses([&]{(void)make(revised);});
        revised.backers={"safety"};
        const auto donated=make(revised);
        require(donated.world.simulation.capacity->available_staff==0,"A negotiated donor does not hire a researcher.");
        selected.mode="sandbox";selected.choices.clear();
        refuses([&]{ (void)make(selected); });
        std::cout<<"Founder biography, calendar, costs, eligibility, nonprofit ownership and exact reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
