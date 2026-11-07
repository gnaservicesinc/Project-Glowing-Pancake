#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <iostream>
#include <stdexcept>

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
        auto original=make(selected);
        require(core::iso_date(original.world.simulation.date)=="2015-01-01" && core::iso_date(original.world.simulation.end_date)=="2115-01-01","The biography must lead to the actual 2015–2115 calendar.");
        require(original.world.simulation.lab->models.empty() && original.world.simulation.deployments->records.empty(),"Founding must not give the player a finished commercial model.");
        require(core::opening_staff_count(*original.world.simulation.capacity)==1,"A genius supplies one finite research assignment.");
        require(original.world.simulation.management->catalog.succession->founder_birth_year==1997,"Succession must retain the new founder's real birth year.");
        const auto expected=content::founding_terms(catalog,selected);
        require(original.world.simulation.cash.value()==expected.contributed_cents-expected.paid_setup_cents,"Capital and paid office costs must reconcile.");
        require(persistence::decode_save(program,persistence::encode_save(program,original)).checkpoint==original,"The complete biography must roundtrip exactly.");
        runtime::Runtime live{program,original};
        const auto prepared=live.prepare_opening(live.current_state().world.simulation.revision);
        require(prepared.accepted(),prepared.explanation.c_str());
        const auto advanced=live.submit({live.current_state().world.simulation.revision,core::AdvanceDay{}});
        require(advanced.accepted(),advanced.explanation.c_str());
        require(core::iso_date(live.current_state().world.simulation.date)=="2015-01-02","The first morning must hand off to real daily play.");
        require(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Played biography state must reload exactly.");
        auto forged=original;forged.world.founding->backers.push_back("commercial");
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        auto invalid=selected;invalid.family="poor";invalid.leg="wealthy";refuses([&]{(void)make(invalid);});
        invalid=selected;invalid.choices.clear();refuses([&]{(void)make(invalid);});
        invalid=selected;invalid.leg="charisma";refuses([&]{(void)make(invalid);});
        selected.family="poor";selected.leg="charisma";selected.researcher="first_hire";
        auto hired=make(selected);
        require(hired.world.simulation.cash<original.world.simulation.cash,"A first hire must have a real cost.");
        selected.legal_form="nonprofit";selected.route="mission_director";selected.backers={"safety","open_science"};
        auto nonprofit=make(selected);
        require(!nonprofit.world.simulation.accounts->taxes,"Recognized mission-serving nonprofit income should use its exemption.");
        for(const auto& holder:nonprofit.world.simulation.accounts->holdings) if(holder.holder=="founder_trust") require(holder.shares.value()==0,"Nonprofit assets cannot be founder equity.");
        require(nonprofit.world.simulation.management->catalog.financing.size()==1,"Nonprofit cannot sell ordinary VC equity.");
        require(persistence::decode_save(program,persistence::encode_save(program,nonprofit)).checkpoint==nonprofit,"Nonprofit charter and donors must reload.");
        invalid=selected;invalid.backers.push_back("commercial");refuses([&]{(void)make(invalid);});
        selected.legal_form="corporation";selected.route="partnership";selected.backers.clear();selected.leg="political";
        const auto political=make(selected);
        require(political.world.simulation.cash>hired.world.simulation.cash,"Political-family rise must change accessible capital at 18.");
        selected.mode="sandbox";selected.choices.clear();selected.sandbox_cash_cents=1234500;selected.sandbox_staff=7;
        refuses([&]{ (void)make(selected); });
        std::cout<<"Founder biography, calendar, costs, eligibility, nonprofit ownership and exact reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
