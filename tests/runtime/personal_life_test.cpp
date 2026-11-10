#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include "objectives_internal.hpp"
#include <pgp/runtime/legacy.hpp>
#include <pgp/session/milestone.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
void check_family_visit() {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(
        persistence::read_bounded(PGP_VISIT_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection founder;
    founder.family="rich";founder.leg="charisma";founder.researcher="none";
    founder.route="partnership";founder.office="shared";founder.partner="partner_okafor";founder.personal_investment_cents=0;
    for(const auto& scene:scenario.founding->scenes) founder.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;
    options.founding=founder;options.identity=runtime::CampaignIdentity{"Travel test","Alex Vale","they/them"};
    auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",std::nullopt,options);
    opening.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime game{program,opening};
    check(game.prepare_opening(opening.world.simulation.revision).accepted(),"Prepare the travel test.");
    const auto state=[&]()->const auto&{return game.current_state().world.simulation;};
    const auto apply=[&](core::CommandAction action) {
        const auto result=game.submit({state().revision,std::move(action)});
        check(result.accepted(),result.explanation.c_str());
    };
    const auto refuse=[&](core::CommandAction action,std::string_view why) {
        const auto before=game.current_state();
        const auto result=game.submit({state().revision,std::move(action)});
        check(!result.accepted() && result.explanation.find(why)!=std::string::npos && before==game.current_state(),
            "A refused family calendar action must explain the conflict and preserve state.");
    };
    const auto exact=[&] {
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),
            "Travel bookings and actual location must reload exactly.");
    };
    const auto local=core::family_visit_quote(state(),core::Days{1});
    check(local.fare==core::Cents{5000} && local.departs_on==local.arrives_on && local.leaves_on==local.returns_on,
        "A local day visit must use the actual round-trip fare without extra transit days.");
    apply(core::ManageHeadquarters{core::LeaseHQ{"austin","compact"}});
    const auto premises=core::current_headquarters(state());
    const auto employee=state().management->people->employees.front().terms.profile.id;
    apply(core::ManageHeadquarters{core::PlaceHQEquipment{1,0,0,0,false}});
    apply(core::ManageHeadquarters{core::AssignHQDesk{1,employee}});
    check(core::employee_contact_room(state(),employee)!=0,"The employee must be reachable in their real office before the visit.");
    const auto party=core::BookPersonalEvent{"office_party",0,core::advance_days(state().date,core::Days{2}),{"opening/partner_okafor"}};
    const auto booking=core::ManagePersonalLife{core::BookFamilyVisit{core::Days{2}}};
    const auto cancel=core::ManagePersonalLife{core::CancelFamilyVisit{}};
    apply(core::ManagePersonalLife{party});refuse(booking,"gathering");
    apply(core::ManagePersonalLife{core::CancelPersonalEvent{1}});
    const auto private_cash=state().succession->personal_cash;
    const auto company_cash=state().cash;
    const auto trip=core::family_visit_quote(state(),core::Days{2});
    check(trip.fare==core::Cents{76000} && trip.arrives_on==core::advance_days(trip.departs_on,core::Days{1}) &&
        trip.returns_on==core::advance_days(trip.leaves_on,core::Days{1}),"The Austin itinerary must use both authored directions and a real stay.");
    apply(booking);
    check(state().cash==company_cash && state().succession->personal_cash==core::checked_unit_subtract(private_cash,trip.fare),
        "Personal travel must only charge the founder's savings.");
    apply(cancel);
    check(state().cash==company_cash && state().succession->personal_cash==private_cash,"Cancellation before departure must restore exactly the personal fare.");
    apply(booking);
    refuse(booking,"current family visit");
    refuse(core::ManagePersonalLife{party},"outside your family visit");
    refuse(core::ManageHeadquarters{core::LeaseHQ{"seattle","compact"}},"family visit");
    auto forged=game.current_state();
    forged.world.simulation.succession->personal_life->family_visits.back().fare=core::Cents{1};
    bool rejected=false;
    try {runtime::validate_checkpoint(program,forged);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"A forged fare must be refused during reload validation.");
    exact();
    const auto advance=[&](core::OrdinalDay expected) {
        const auto milestone=session::plan_milestone(game.current_state(),"family_visit");
        check(milestone.deadline==expected,"The next travel action must stop at the displayed milestone.");
        while(state().date<expected) apply(core::AdvanceDay{});
    };
    advance(trip.departs_on);
    check(core::executive_location(state()).traveling && !core::executive_at_headquarters(state()),"Outbound travel must take the founder away from headquarters.");
    refuse(cancel,"before departure");exact();
    advance(trip.arrives_on);
    check(core::executive_location(state()).family_home && core::executive_location(state()).state_id=="US-CA" &&
        core::employee_contact_room(state(),employee)==0 && !core::resolve_app_device(state(),"laptop","people",false).empty() &&
        core::current_headquarters(state())==premises,"Arrival must allow portable work without relocating HQ or allowing same-room office actions.");
    exact();
    advance(trip.leaves_on);
    check(core::executive_location(state()).traveling && !core::executive_location(state()).family_home,"The return leg must be actual transit.");
    advance(trip.returns_on);
    check(core::executive_at_headquarters(state()) && core::employee_contact_room(state(),employee)!=0 &&
        core::current_headquarters(state())==premises && !core::pending_family_visit(state()),"Returning must restore the same headquarters and in-person actions.");
    exact();
}
}
int main() {
    using namespace pgp;
    try {
        check_family_visit();
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
