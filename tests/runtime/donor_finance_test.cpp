#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;
void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});
    if(!result.accepted())throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,core::CommandAction action) {
    const auto before=game.current_state();const auto result=game.submit({before.world.simulation.revision,std::move(action)});
    check(!result.accepted() && before==game.current_state(),"Refused gift and ownership actions must preserve all records and funds.");
}
template<class F> void invalid(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged gift or evaluation evidence was accepted.");}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Donor proposals, evidence, contributions and trustees must reload exactly.");
}
}
int main() {
    using namespace pgp;
    try {
        const auto registry=content::foundation_registry();
        const auto source=content::load_collection(persistence::read_bounded(PGP_DONOR_SOURCE,content::maximum_document_bytes),registry);
        const auto program=runtime::compile_collection(source,registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection founder;founder.family="rich";founder.advantages={"genius"};founder.legal_form="nonprofit";
        founder.route="mission_director";founder.office="home";founder.researcher="founder";founder.backers={"safety"};
        for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
        runtime::ScenarioOptions options;options.founding=founder;options.seed=719;
        options.identity=runtime::CampaignIdentity{"Open institution","Alex Vale","they/them"};
        options.opening_contacts={{"opening/open_science",scenario.starts_on,{},core::OpeningConversation{"pitch","unsigned"}},
            {"opening/safety",scenario.starts_on,{},core::OpeningConversation{"pitch","signed"}}};
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        opening.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,opening};
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto contact=[&](std::string_view id) -> const auto& {return *std::ranges::find(state().management->people->contacts,id,&core::PeopleContact::person);};
        const auto command=[](core::ContactAction action)->core::CommandAction{return core::ManagePeopleContact{std::move(action)};};
        const auto request=[&](const std::string& person){apply(game,command(core::RequestContactFunding{person,"tablet",0}));};
        check(game.prepare_opening(state().revision).accepted(),"Prepare the nonprofit opening.");
        check(core::nonprofit_organization(state()) && state().management->catalog.governance->contact_finance->offers.size()==2,"Nonprofits must retain donor programs and remove private equity rounds.");
        refuse(game,core::InvestEquity{"outsider",core::Cents{1000000},core::BasisPoints{100},"nonprofit.test"});
        refuse(game,core::ProposePoolExpansion{core::BasisPoints{100}});
        refuse(game,core::NegotiateCorporateFinance{"corporate.open_field_gift"});
        apply(game,core::BuyPersonalDevice{"tablet"});apply(game,core::AdvanceDay{});apply(game,core::AdvanceDay{});
        request("opening/safety");
        const auto early=contact("opening/safety").funding.front();
        check(early.status=="not_ready" && early.evaluations==0,"Founding prestige alone must not satisfy an independent evaluation requirement.");
        const auto cash=state().cash,private_cash=state().succession->personal_cash;
        const auto holdings=state().accounts->holdings;
        const auto accounts=core::accounts_report(*state().accounts);
        request("opening/open_science");
        check(contact("opening/open_science").funding.front().offers==std::vector<std::string>{"corporate.open_field_gift"},"The institution must receive its actual gift offer.");
        apply(game,command(core::RespondContactFunding{"opening/open_science","tablet",1,"defer",{},0}));
        exact(program,game);
        auto declined=game;apply(declined,command(core::RespondContactFunding{"opening/open_science","tablet",1,"decline",{},0}));
        refuse(declined,command(core::RespondContactFunding{"opening/open_science","tablet",1,"submit","corporate.open_field_gift",0}));exact(program,declined);
        apply(game,command(core::RespondContactFunding{"opening/open_science","tablet",1,"submit","corporate.open_field_gift",0}));
        check(state().cash==cash && state().accounts->holdings==holdings,"Submitting a gift must not transfer money or ownership.");
        const auto motion=contact("opening/open_science").funding.front().motion;
        apply(game,core::VoteBoardMotion{motion,true});
        check(state().management->governance->motions.back().executed && state().cash==core::checked_unit_add(cash,core::Cents{15000000}) &&
            state().succession->personal_cash==private_cash && state().accounts->holdings==holdings,
            "An approved gift must add institutional cash without changing private wealth or ownership.");
        const auto after=core::accounts_report(*state().accounts);
        check(after.revenue==accounts.revenue && after.profit==accounts.profit,"A contribution cannot manufacture sales revenue or operating profit.");
        check(state().management->governance->capital.back().kind=="donation" && state().management->governance->capital.back().shares==core::BasisPoints{0},"A gift must retain its non-diluting contribution journal.");
        check(std::ranges::any_of(core::current_board(state()),[](const auto& member){return member.id=="seat/corporate.open_field_gift" && member.name.ends_with("trustee");}),"The agreed trustee seat must affect the actual board.");
        apply(game,command(core::MessagePeopleContact{"opening/open_science","board_role","tablet",0}));
        check(contact("opening/open_science").messages.back().reply=="has_seat","A donor must report their actual new trustee seat.");
        exact(program,game);
        auto forged=game.current_state();forged.world.simulation.management->governance->capital.back().shares=core::BasisPoints{1};
        invalid([&]{runtime::validate_checkpoint(program,forged);});
        forged=game.current_state();forged.world.simulation.management->people->contacts.back().funding.front().evaluations=1;
        invalid([&]{runtime::validate_checkpoint(program,forged);});
        refuse(game,core::VoteBoardMotion{motion,true});

        apply(game,core::AcceptBusinessOffer{"offer.supplier_small"});
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"desk"}});
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"desktop"}});
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"evaluation"}});
        const auto delivery=std::ranges::max_element(state().infrastructure->workplace->headquarters->items,{},&core::HQEquipment::delivered_on)->delivered_on;
        while(state().date<delivery)apply(game,core::AdvanceDay{});
        apply(game,core::ManageHeadquarters{core::PlaceHQEquipment{1,0,0,0,false}});
        apply(game,core::ManageHeadquarters{core::AssignHQDesk{1,"founder"}});
        apply(game,core::ManageHeadquarters{core::PlaceHQEquipment{2,1,0,0,false}});
        apply(game,core::ManageHeadquarters{core::PlaceHQEquipment{3,0,4,0,false}});
        const auto researcher=state().management->people->employees.front().terms.profile.id;
        const core::LabAllocation allocation{"lease/offer.supplier_small",core::MilliCUPerDay{30000},1,{researcher}};
        const auto finish=[&](std::string project) {
            for(int n=0;n<40;++n) {
                const auto& work=*std::ranges::find(state().capacity->projects,project,&core::CapacityProject::id);
                if(work.status==core::ProjectStatus::completed)return;
                apply(game,core::AdvanceDay{});
            }throw std::runtime_error("The focused research/evaluation project did not complete.");
        };
        apply(game,core::StartResearch{"research.reproducible_tooling",allocation});finish(state().lab->research.back().project_id);
        apply(game,core::PublishResearch{"research.reproducible_tooling"});
        apply(game,core::StartResearch{"research.held_out_evaluation",allocation});finish(state().lab->research.back().project_id);
        apply(game,core::BuyData{"data.civic_forms_open"});
        apply(game,core::StartTraining{{"gift-model","recipe.document_baseline",{}, {"data.civic_forms_open"},allocation.source_id,allocation.daily_compute,1,core::MilliCU{60000}}});
        finish(state().lab->models.back().project_id);
        apply(game,core::StartEvaluation{"headline","gift-model","evaluation.headline",allocation});finish(state().lab->evaluations.back().project_id);
        check(core::contact_funding_evaluations(state(),state().date)==0,"A self-reported headline test cannot satisfy an independent review gate.");
        apply(game,core::StartEvaluation{"independent","gift-model","evaluation.held_out",allocation});
        check(core::contact_funding_evaluations(state(),state().date)==0,"Starting a test is not completing one.");
        finish(state().lab->evaluations.back().project_id);
        check(core::contact_funding_evaluations(state(),state().date)==1 && core::contact_funding_evaluations(state(),early.requested_on)==0,
            "Only completed independent work may count, from its actual next prepared morning.");
        const auto retry=core::advance_days(early.requested_on,core::Days{30});while(state().date<retry)apply(game,core::AdvanceDay{});
        request("opening/safety");
        check(contact("opening/safety").funding.back().evaluations==1 && contact("opening/safety").funding.back().offers==std::vector<std::string>{"corporate.careful_futures_gift"} &&
            contact("opening/safety").funding.front()==early,"Actual published work and an independent test must unlock the gift without rewriting earlier replies.");
        const auto before_second=state().cash;const auto board_count=core::current_board(state()).size();
        apply(game,command(core::RespondContactFunding{"opening/safety","tablet",2,"submit","corporate.careful_futures_gift",0}));
        apply(game,core::VoteBoardMotion{contact("opening/safety").funding.back().motion,true});
        check(state().cash==core::checked_unit_add(before_second,core::Cents{25000000}) && state().accounts->holdings==holdings && core::current_board(state()).size()==board_count,
            "An evidence-backed gift with no new trustee must keep exactly those terms.");
        request("opening/open_science");check(contact("opening/open_science").funding.back().status=="not_ready","A gift program cannot be paid twice.");exact(program,game);
        auto corporation=options;corporation.founding->legal_form="corporation";corporation.founding->route="bootstrap";corporation.founding->backers.clear();corporation.opening_contacts.clear();
        const auto ordinary=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},corporation);
        check(!core::contact_funding_available(ordinary.world.simulation,"opening/open_science") && std::ranges::none_of(ordinary.world.simulation.management->catalog.governance->financing,[](const auto& offer){return offer.donation;}),
            "Private companies must not receive nonprofit-only gift programs.");
        std::cout<<"Nonprofit donor offers, non-diluting gifts, trustee seats, independent evaluation evidence, no private windfall, duplicate refusal and exact saves passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
