#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}}
int main() {
    using namespace pgp;
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_EVENTS_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selection;
        selection.family="rich";selection.leg="political";selection.route="hired_ceo";selection.office="shared";selection.researcher="none";
        for(const auto& scene:scenario.founding->scenes)selection.choices[scene.id]=scene.choices.front().id;
        runtime::ScenarioOptions options;options.founding=selection;
        options.identity=runtime::CampaignIdentity{"Alder Research","Alex Vale","they/them"};
        options.opening_contacts.push_back({"opening/frontier",scenario.starts_on,{},core::OpeningConversation{"pitch","unsigned"}});
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        opening.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,opening};
        check(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Prepare the first morning.");
        const auto state=[&]() -> const auto& {return game.current_state().world.simulation;};
        const auto apply=[&](core::CommandAction action) {
            const auto result=game.submit({state().revision,std::move(action)});
            if(!result.accepted())throw std::runtime_error(result.explanation);
        };
        const auto personal=[&](core::PersonalLifeAction action){apply(core::ManagePersonalLife{std::move(action)});};
        const auto refuse=[&](core::PersonalLifeAction action) {
            const auto before=game.current_state();
            check(!game.submit({state().revision,core::ManagePersonalLife{std::move(action)}}).accepted() && before==game.current_state(),"Invalid event commitments must refuse atomically.");
        };
        const auto person=state().management->people->employees.front().terms.profile.id;
        const auto room=core::employee_contact_room(state(),person);
        const auto day=core::advance_days(state().date,core::Days{2});
        refuse(core::BookPersonalEvent{"office_party",0,day,{person}});
        apply(core::ManagePeopleContact{core::ExchangePeopleContact{person,room}});
        refuse(core::BookPersonalEvent{"dinner",1,day,{person}});
        refuse(core::BookPersonalEvent{"office_party",0,state().date,{person}});
        refuse(core::BookPersonalEvent{"office_party",0,day,{person,person}});
        refuse(core::BookPersonalEvent{"office_party",0,day,{"unknown"}});
        const auto cash=state().cash,private_cash=state().succession->personal_cash;
        const auto postings=state().accounts->postings;
        personal(core::BookPersonalEvent{"office_party",0,day,{person}});
        check(state().cash==cash && state().accounts->postings==postings && state().succession->personal_cash==core::checked_unit_subtract(private_cash,core::Cents{34000}),
            "The office party must use the host's personal cash and leave company accounts alone.");
        personal(core::CancelPersonalEvent{1});
        refuse(core::CancelPersonalEvent{1});
        check(state().succession->personal_cash==core::checked_unit_subtract(private_cash,core::Cents{8500}),"Cancellation must refund exactly 75 percent once.");
        personal(core::BuyPersonalProperty{"valley_condo"});
        const auto before_quote=game.current_state();
        const core::BookPersonalEvent dinner{"dinner",1,day,{person,"opening/frontier"}};
        check(core::personal_event_quote(state(),dinner)==core::Cents{24000} && before_quote==game.current_state(),"A quote must price every invitation without mutating state.");
        const auto mood_before=core::workplace_moods(state()).at(person);
        const auto creativity_before=core::distinct_person_traits(state().management->people->employees.front().terms.profile);
        personal(dinner);
        const auto& booked=state().succession->personal_life->events.back();
        check(booked.guests[0].reply=="accepted" && booked.guests[1].reply=="declined","Replies must reflect a current colleague and an unsigned outside relationship.");
        refuse(core::SellPersonalProperty{1});
        refuse(core::BookPersonalEvent{"office_party",0,day,{person}});
        check(core::workplace_moods(state()).at(person).gatherings==0,"A future gathering must not improve mood.");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Invitations and the paid future booking must reload.");
        apply(core::AdvanceDay{});apply(core::AdvanceDay{});
        const auto& hosted=state().succession->personal_life->events.back();
        check(hosted.outcome=="hosted" && hosted.guests[0].attended && !hosted.guests[1].attended && state().succession->personal_life->pending_events.empty(),"The event must complete on its date with actual attendance.");
        const auto after=core::workplace_moods(state()).at(person);
        check(after.gatherings==4 && after.score==std::min(100,mood_before.score+4),"Only attending employees receive the disclosed mood contribution.");
        check(core::distinct_person_traits(state().management->people->employees.front().terms.profile)==creativity_before,"Attending a gathering must not create extra creativity traits.");
        refuse(core::CancelPersonalEvent{2});
        const auto refund_day=core::advance_days(state().date,core::Days{9});
        personal(core::BookPersonalEvent{"office_party",0,refund_day,{person}});
        apply(core::ManageHeadquarters{core::ReturnHomeHQ{}});
        while(state().date<refund_day)apply(core::AdvanceDay{});
        const auto& moved=state().succession->personal_life->events.back();
        check(moved.outcome=="venue_changed" && moved.refund==moved.paid && core::workplace_moods(state()).at(person).gatherings==0,
            "A changed office must refund its booking and old gathering mood must expire.");
        const auto scheduled=core::advance_days(state().date,core::Days{7});
        personal(core::BookPersonalEvent{"dinner",1,scheduled,{person}});
        {
            // A focused away-host fixture exercises the event rule using the
            // same appointment/location state consumed by the normal calendar.
            auto away=state();away.date=scheduled;
            core::OwnedSite site;site.id="away.fixture";site.state_id=away.infrastructure->executive_base;
            away.infrastructure->sites.push_back(site);
            core::SiteAppointment appointment;appointment.id="away.event";appointment.site_id=site.id;
            appointment.mode=core::AppointmentMode::onsite;appointment.starts_on=scheduled;appointment.visit_on=scheduled;
            appointment.ends_before=core::advance_days(scheduled,core::Days{1});
            away.infrastructure->appointments.push_back(appointment);
            const auto balance=away.succession->personal_cash;
            core::prepare_personal_events(away);
            const auto& missed=away.succession->personal_life->events.back();
            check(missed.outcome=="host_absent" && !missed.guests.front().attended && away.succession->personal_cash==balance &&
                core::personal_event_moods(away).empty(),"An on-site host must miss the event without creating a refund or mood benefit.");
        }
        personal(core::CancelPersonalEvent{4});
        personal(core::SellPersonalProperty{1});
        const auto saved=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
        check(saved.checkpoint==game.current_state(),"Hosted events, refunds, expiry and venue sales must reload exactly.");
        auto forged=game.current_state();forged.world.simulation.succession->personal_life->events[1].guests[1].attended=true;
        bool rejected=false;try{runtime::validate_checkpoint(program,forged);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"A declined guest cannot be recorded as attending.");
        std::cout<<"Known invitations, separate spending, cancellation, hosting, attendance, mood expiry, venue refund and fresh reload passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
