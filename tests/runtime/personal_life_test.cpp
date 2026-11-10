#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include "objectives_internal.hpp"
#include <pgp/runtime/legacy.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>

namespace {void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}}
int main() {
    using namespace pgp;
    try {
        const auto registry=content::foundation_registry();
        auto document=nlohmann::json::parse(persistence::read_bounded(PGP_PERSONAL_SOURCE,content::maximum_document_bytes));
        // An unpaid founder exercises upkeep arrears without unrelated payroll income.
        for(auto& definition:document["definitions"]) if(definition["id"]=="scenario.founder_campaign")
            definition["founding"]["founder_salary_cents"]=0;
        const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selection;
        selection.family="rich";selection.leg="genius";selection.route="bootstrap";selection.office="home";selection.researcher="founder";
        for(const auto& scene:scenario.founding->scenes) selection.choices[scene.id]=scene.choices.front().id;
        const auto funds=content::founding_terms(*scenario.founding,selection).personal_contribution_cents;
        selection.personal_investment_cents=funds-25080000;
        runtime::ScenarioOptions options;
        options.founding=selection;options.identity=runtime::CampaignIdentity{"Private life test","Alex Vale","they/them"};
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        opening.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime game{program,opening};
        check(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Prepare the first morning.");
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto apply=[&](core::CommandAction action) {
            const auto result=game.submit({state().revision,std::move(action)});
            if(!result.accepted()) throw std::runtime_error(result.explanation);
        };
        const auto personal=[&](core::PersonalLifeAction action){apply(core::ManagePersonalLife{std::move(action)});};
        const auto refuse=[&](core::PersonalLifeAction action) {
            const auto before=game.current_state();
            check(!game.submit({state().revision,core::ManagePersonalLife{std::move(action)}}).accepted() && game.current_state()==before,"A refused personal transaction must be atomic.");
        };
        const auto company_cash=state().cash;
        const auto company_postings=state().accounts->postings;
        refuse(core::BuyPersonalProperty{"gallery_estate"});
        personal(core::BuyPersonalProperty{"valley_condo"});
        check(state().cash==company_cash && state().accounts->postings==company_postings && state().succession->personal_cash==core::Cents{0},
            "Property and first-month upkeep must use only the founder's savings.");
        refuse(core::BuyPersonalProperty{"valley_condo"});
        refuse(core::BuyPersonalDecor{"painting"});
        while(state().date<core::parse_iso_date("2015-02-01")) apply(core::AdvanceDay{});
        const auto& fees=*state().succession->personal_life;
        check(fees.bills.size()==2 && fees.unpaid==core::Cents{80000} && state().succession->personal_cash==core::Cents{0},
            "Unaffordable monthly upkeep must become a personal liability once, without spending company cash.");
        refuse(core::PayPersonalUpkeep{});
        refuse(core::BuyPersonalDecor{"personal_plant"});
        personal(core::SellPersonalProperty{1});
        personal(core::PayPersonalUpkeep{});
        personal(core::BuyPersonalDecor{"sofa"});
        personal(core::BuyPersonalDecor{"painting"});
        const auto historical=core::personal_valuation(state(),core::parse_iso_date("2015-02-01"));
        check(historical.assets==core::Cents{22500000} && historical.spent==core::Cents{25080000},"Historical wealth must exclude a sale, bill and purchases on the cutoff date.");
        auto valued=game.current_state().world;
        valued.objectives->deadline=core::parse_iso_date("2016-01-01");
        const auto table=runtime::objective_detail::terminal_profit(valued,*scenario.objectives);
        check(table.front().personal_cash==state().succession->personal_cash && table.front().personal_assets==core::Cents{4125000} && table.front().personal_liabilities==core::Cents{0},
            "The legacy calculation must include actual private spending and remaining art and furniture at resale value.");
        const auto book=runtime::founder_legacy(game.current_state().world,game.current_state().world.company_id);
        check(book.assets==table.front().personal_assets && book.cash==table.front().personal_cash && book.liabilities==table.front().personal_liabilities,
            "Current book wealth and the terminal valuation must agree on private assets and balances.");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Private ownership, arrears payments and sales must reload exactly.");

        options.founding->personal_investment_cents=funds-40000000;
        auto second=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        second.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime decorated{program,second};
        check(decorated.prepare_opening(decorated.current_state().world.simulation.revision).accepted(),"Prepare the furnishing scenario.");
        const auto arrange=[&](core::CommandAction action) {
            const auto result=decorated.submit({decorated.current_state().world.simulation.revision,std::move(action)});
            if(!result.accepted())throw std::runtime_error(result.explanation);
        };
        const auto furnish=[&](core::PersonalLifeAction action){arrange(core::ManagePersonalLife{std::move(action)});};
        const auto decline=[&](core::PersonalLifeAction action) {
            const auto before=decorated.current_state();
            check(!decorated.submit({before.world.simulation.revision,core::ManagePersonalLife{std::move(action)}}).accepted() && decorated.current_state()==before,"Invalid personal placements must refuse without moving or charging anything.");
        };
        furnish(core::BuyPersonalProperty{"valley_condo"});furnish(core::BuyPersonalDecor{"sofa"});furnish(core::BuyPersonalDecor{"painting"});
        decline(core::PlacePersonalDecor{1,1,0,0,false});
        arrange(core::AdvanceDay{});arrange(core::AdvanceDay{});
        furnish(core::PlacePersonalDecor{1,1,0,0,false});
        decline(core::PlacePersonalDecor{2,1,0,0,false});
        decline(core::PlacePersonalDecor{2,1,4,1,false});
        furnish(core::PlacePersonalDecor{2,1,5,1,false});
        furnish(core::PlacePersonalDecor{1,1,0,0,true});
        check(persistence::decode_save(program,persistence::encode_save(program,decorated.current_state())).checkpoint==decorated.current_state(),"Rotated furniture and art placement must reload exactly.");
        furnish(core::SellPersonalProperty{1});
        for(const auto& item:decorated.current_state().world.simulation.succession->personal_life->decor)
            check(item.property==0 && item.x==0 && item.y==0 && !item.rotated,"Selling a home must put its contents back in personal storage.");
        const auto before_sale=decorated.current_state().world.simulation.succession->personal_cash;
        furnish(core::SellPersonalDecor{2});
        decline(core::SellPersonalDecor{2});
        decline(core::PlacePersonalDecor{2,1,0,0,false});
        check(decorated.current_state().world.simulation.succession->personal_cash==core::checked_unit_add(before_sale,core::Cents{4000000}) &&
            persistence::decode_save(program,persistence::encode_save(program,decorated.current_state())).checkpoint==decorated.current_state(),
            "An art sale must pay its disclosed resale value once and reload with the sold artwork removed from assets.");
        std::cout<<"Separate personal spending, monthly arrears, resale, art, placement, historical legacy valuation and fresh reload passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
