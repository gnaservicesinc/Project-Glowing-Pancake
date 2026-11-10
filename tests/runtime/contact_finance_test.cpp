#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;
void check(bool okay,const char* why) {if(!okay)throw std::runtime_error(why);}
template<class F> void invalid(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged investor history was accepted.");}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});
    if(!result.accepted())throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,core::CommandAction action,std::string_view because) {
    const auto before=game.current_state();
    const auto result=game.submit({before.world.simulation.revision,std::move(action)});
    if(result.accepted() || before!=game.current_state() || result.explanation.find(because)==std::string::npos)
        throw std::runtime_error("Expected atomic refusal mentioning "+std::string{because}+"; got: "+result.explanation);
}
void exact(const runtime::CompiledProgram& program,const runtime::Runtime& game) {
    check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Investor replies, offered terms, board motions, capital and ownership must reload exactly.");
}
}
int main() {
    using namespace pgp;
    try {
        const auto registry=content::foundation_registry();
        const auto source=content::load_collection(persistence::read_bounded(PGP_CONTACT_FINANCE_SOURCE,content::maximum_document_bytes),registry);
        const auto program=runtime::compile_collection(source,registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection founder;founder.family="rich";founder.leg="genius";founder.route="bootstrap";founder.office="home";founder.researcher="founder";
        for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
        runtime::ScenarioOptions options;options.founding=founder;options.seed=719;
        options.identity=runtime::CampaignIdentity{"Follow-up research","Alex Vale","they/them"};
        for(const auto* person:{"opening/frontier","opening/commercial"})options.opening_contacts.push_back({person,scenario.starts_on,{},core::OpeningConversation{"pitch","unsigned"}});
        auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,initial};
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto contact=[&](std::string_view id) -> const auto& {
            return *std::ranges::find(state().management->people->contacts,id,&core::PeopleContact::person);
        };
        const auto command=[](core::ContactAction action)->core::CommandAction {return core::ManagePeopleContact{std::move(action)};};
        const auto request=[&](const std::string& person){apply(game,command(core::RequestContactFunding{person,"tablet",0}));};
        const auto respond=[&](std::string reply,std::string terms="") {apply(game,command(core::RespondContactFunding{"opening/frontier","tablet",1,std::move(reply),std::move(terms),0}));};
        check(game.prepare_opening(state().revision).accepted(),"Prepare the company opening.");
        refuse(game,core::NegotiateCorporateFinance{"corporate.round_small"},"Messages");
        refuse(game,command(core::RequestContactFunding{"opening/unknown","tablet",0}),"contact details");
        refuse(game,command(core::RequestContactFunding{"opening/frontier","paper",0}),"powered device");
        apply(game,core::BuyPersonalDevice{"tablet"});
        refuse(game,command(core::RequestContactFunding{"opening/frontier","tablet",0}),"cannot send");
        apply(game,core::AdvanceDay{});apply(game,core::AdvanceDay{});
        const auto cash=state().cash,private_cash=state().succession->personal_cash;
        const auto holdings=state().accounts->holdings;
        const auto quoted_terms=*std::ranges::find(state().management->catalog.governance->financing,std::string_view{"corporate.round_large"},&core::CorporateFinanceTerms::id);
        request("opening/frontier");
        check(contact("opening/frontier").funding.back().offers==std::vector<std::string>{"corporate.round_small","corporate.round_large"} &&
            state().cash==cash && state().succession->personal_cash==private_cash && state().accounts->holdings==holdings && state().management->governance->motions.empty(),
            "A credible pitch must quote real alternatives without transferring cash, shares or board seats.");
        const auto pitched=game.current_state();exact(program,game);
        refuse(game,command(core::RequestContactFunding{"opening/frontier","tablet",0}),"another pitch");
        respond("defer");check(contact("opening/frontier").funding.back().status=="held","Holding terms must persist without accepting them.");exact(program,game);
        auto declined=game;apply(declined,command(core::RespondContactFunding{"opening/frontier","tablet",1,"decline",{},0}));
        check(declined.current_state().world.simulation.cash==cash && declined.current_state().world.simulation.accounts->holdings==holdings,"Declining cannot change treasury or ownership.");
        refuse(declined,command(core::RespondContactFunding{"opening/frontier","tablet",1,"submit","corporate.round_small",0}),"open funding discussion");exact(program,declined);
        refuse(game,command(core::RespondContactFunding{"opening/frontier","tablet",1,"submit","corporate.strategic_equity",0}),"actually offered");
        auto expired=game;
        const auto expiry=contact("opening/frontier").funding.front().expires_on;
        while(expired.current_state().world.simulation.date<expiry)apply(expired,core::AdvanceDay{});
        refuse(expired,command(core::RespondContactFunding{"opening/frontier","tablet",1,"submit","corporate.round_small",0}),"expiry");
        apply(expired,command(core::RequestContactFunding{"opening/frontier","tablet",0}));exact(program,expired);
        respond("submit","corporate.round_large");
        const auto submitted=game.current_state();
        const auto motion=contact("opening/frontier").funding.front().motion;
        check(!motion.empty() && state().cash==cash && state().accounts->holdings==holdings,"Sending terms to the board must not prepay the round.");exact(program,game);
        refuse(game,command(core::RespondContactFunding{"opening/frontier","tablet",1,"submit","corporate.round_small",0}),"open funding discussion");
        apply(game,core::VoteBoardMotion{motion,true});
        const auto& agreement=state().management->governance->motions.back();
        check(agreement.executed && state().cash==core::checked_unit_add(cash,quoted_terms.proceeds) && state().succession->personal_cash==private_cash,
            "Approval must fund company capital once without paying the founder personally.");
        auto expected=holdings;core::dilute_holdings(expected,"investor/corporate.round_large",core::BasisPoints{1000});
        check(expected==state().accounts->holdings,"The new stake must dilute every holding exactly once.");
        check(std::ranges::any_of(core::current_board(state()),[](const auto& member){return member.id=="seat/corporate.round_large";}),"The accepted seat must appear on the actual board.");
        apply(game,command(core::MessagePeopleContact{"opening/frontier","board_role","tablet",0}));
        check(contact("opening/frontier").messages.back().reply=="has_seat" && contact("opening/frontier").opening->outcome=="unsigned",
            "A follow-up board seat must be reported without rewriting the unsigned opening offer.");exact(program,game);
        auto forged=submitted;forged.world.simulation.management->people->contacts.front().funding.front().motion="invented";
        invalid([&]{runtime::validate_checkpoint(program,forged);});
        forged=pitched;forged.world.simulation.management->people->contacts.front().funding.front().reputation=0;
        invalid([&]{runtime::validate_checkpoint(program,forged);});
        forged=pitched;core::negotiate_corporate_finance(forged.world.simulation,"corporate.round_small",true);
        invalid([&]{runtime::validate_checkpoint(program,forged);});

        request("opening/commercial");
        const auto first_pitch=contact("opening/commercial").funding.front();
        check(first_pitch.status=="not_ready" && first_pitch.publications==0,"A publication requirement cannot be replaced with founding prestige.");
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"desk"}});
        apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"desktop"}});
        for(int n=0;n<3;++n)apply(game,core::AdvanceDay{});
        apply(game,core::ManageHeadquarters{core::PlaceHQEquipment{1,0,0,0,false}});
        apply(game,core::ManageHeadquarters{core::AssignHQDesk{1,"founder"}});
        apply(game,core::ManageHeadquarters{core::PlaceHQEquipment{2,1,0,0,false}});
        const auto researcher=state().management->people->employees.front().terms.profile.id;
        apply(game,core::StartResearch{"research.reproducible_tooling",{"hq.equipment/2",core::MilliCUPerDay{4000},1,{researcher}}});
        for(int n=0;n<15;++n)apply(game,core::AdvanceDay{});
        apply(game,core::PublishResearch{"research.reproducible_tooling"});
        const auto retry=core::advance_days(first_pitch.requested_on,core::Days{30});
        while(state().date<retry)apply(game,core::AdvanceDay{});
        request("opening/commercial");
        const auto& eligible=contact("opening/commercial").funding.back();
        check(eligible.status=="offered" && eligible.publications==1 && eligible.offers==std::vector<std::string>{"corporate.strategic_equity"} &&
            contact("opening/commercial").funding.front()==first_pitch,"A real publication must unlock only its qualified terms while preserving the earlier rejection.");
        request("opening/frontier");
        check(contact("opening/frontier").funding.back().status=="not_ready","A completed round cannot be funded twice through the other counteroffer.");
        exact(program,game);
        auto nonprofit_options=options;
        nonprofit_options.founding->legal_form="nonprofit";
        nonprofit_options.founding->route="mission_director";
        nonprofit_options.founding->backers={"safety"};
        auto nonprofit=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},nonprofit_options);
        check(!nonprofit.world.simulation.management->catalog.governance->contact_finance && !core::contact_funding_available(nonprofit.world.simulation,"opening/frontier"),
            "A nonprofit opening must remove equity-only conversations with its equity offers.");
        runtime::validate_checkpoint(program,nonprofit);
        auto demanding_source=source;
        auto& demanding=*std::get<content::ScenarioDefinition>(demanding_source.definitions.at("scenario.founder_campaign").data).management->governance->contact_finance;
        for(auto& offer:demanding.offers)if(offer.person=="opening/frontier")offer.minimum_reputation=100;
        const auto demanding_program=runtime::compile_collection(demanding_source,registry);
        auto demanding_opening=runtime::begin_scenario(demanding_program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        demanding_opening.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime unproven{demanding_program,demanding_opening};
        check(unproven.prepare_opening(unproven.current_state().world.simulation.revision).accepted(),"Prepare the authored reputation-gate fixture.");
        apply(unproven,core::BuyPersonalDevice{"tablet"});apply(unproven,core::AdvanceDay{});apply(unproven,core::AdvanceDay{});
        apply(unproven,command(core::RequestContactFunding{"opening/frontier","tablet",0}));
        check(unproven.current_state().world.simulation.management->people->contacts.front().funding.front().status=="not_ready",
            "Meeting a contact and having no publication requirement cannot bypass an authored reputation threshold.");
        exact(demanding_program,unproven);
        std::cout<<"Known investor pitches, reputation and publication gates, hold/decline/expiry, actual board approval, conserving ownership, cash separation and exact reload passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
