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
        const auto make=[&](content::FoundingSelection s) {
            runtime::ScenarioOptions options;
            options.identity=runtime::CampaignIdentity{"Test research",s.first_name+" "+s.last_name,"she/her"};
            options.founding=std::move(s);
            return runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
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
        revised.family="poor";refuses([&]{(void)make(revised);});
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
        require(make(revised).world.simulation.management->people->employees.size()==catalog.opening->cofounder_staff,"A cofounder route supplies only its agreed founding partner.");
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
