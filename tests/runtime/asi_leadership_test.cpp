#include "frontier_fixture.hpp"
#include <pgp/runtime/authority.hpp>
#include <fstream>

int main(int argc,char** argv) {
    try {
        if(argc!=1 && (argc!=3 || std::string_view{argv[1]}!="--smoke-fixture"))
            throw std::invalid_argument("Use --smoke-fixture DIRECTORY or no arguments.");
        auto source=Json::parse(persistence::read_bounded(PGP_ASI_SOURCE,content::maximum_document_bytes));
        const auto& founder=pgp_test::definition(source,"scenario.founder_campaign");
        auto& charter=pgp_test::definition(source,frontier_id)["management"]["succession"];
        charter["asi"]=founder["management"]["succession"]["asi"];
        charter["departure"]=founder["management"]["succession"]["departure"];
        charter["transition_days"]=30;charter["transition_work_milli_CU"]=300000;
        const auto p=late_fixture(source);
        auto game=begin(p,frontier_id);
        const auto eligible=[&] {return leadership_candidates(state(game),advance_days(state(game).date,Days{44}));};
        check(std::ranges::none_of(eligible(),[](const auto& c){return c.asi.has_value();}),"No ASI successor may appear before independent qualification.");
        refuse(game,ProposeSuccession{"asi.1",advance_days(state(game).date,Days{44}),true,{"lease.initial",MilliCUPerDay{10000},1}});
        prepare_system(game,true);panels(game);
        check(frontier_qualification(future(game),1).agi_on && !frontier_qualification(future(game),1).asi_on,"AGI must remain distinct from ASI.");
        check(std::ranges::none_of(eligible(),[](const auto& c){return c.asi.has_value();}),"AGI alone cannot unlock ASI leadership.");
        const auto protocols=future(game).catalog.experiments;
        for(std::size_t i=0;i<protocols.size();++i) {
            research(game,protocols[i].research);
            (void)commission(game,FrontierTrialKind::experiment,protocols[i].id);
            finish(game,future(game).trials.back().project);
            if(i==2) {
                const auto first=std::ranges::find(future(game).trials,FrontierTrialKind::replication,&FrontierTrial::kind);
                advance(game,OrdinalDay{advance_days(first->result->observed_on,frontier_replication_span).value()-24});
            }
            (void)commission(game,FrontierTrialKind::replication,protocols[i].id,"owned/science.fabric");
            finish(game,future(game).trials.back().project);
        }
        const auto qualification=frontier_qualification(future(game),1);
        check(qualification.asi_on==state(game).date,"Actual independent replications must establish the candidate's ASI qualification.");
        const auto before=game.current_state();const auto people=eligible();
        const auto ai=std::ranges::find_if(people,[](const auto& c){return c.asi.has_value();});
        check(ai!=people.end() && ai->id=="asi.1" && ai->asi->qualified_on==qualification.asi_on && ai->asi->control==qualification.conservative.back() &&
            game.current_state()==before,"Listing candidates must preserve state and use this company's actual evidence.");
        refuse(game,ProposeSuccession{"asi.01",advance_days(state(game).date,Days{44}),true,{"lease.initial",MilliCUPerDay{10000},1}});
        bool refused=false;try {(void)leadership_candidate(state(game),"asi.1",OrdinalDay{state(game).date.value()-1});}catch(const std::exception&){refused=true;}
        check(refused,"Later qualification cannot authorize an earlier nomination.");
        game=reload(game);
        const auto directory=argc==3 && std::string_view{argv[1]}=="--smoke-fixture"?std::filesystem::path{argv[2]}:std::filesystem::path{PGP_ASI_SCRATCH};
        std::filesystem::create_directories(directory);
        std::ofstream{directory/"asi.collection.json"}<<p.canonical_content_bytes;
        persistence::save_file(p,game.current_state(),directory/"asi-start.pgpsave");
        const auto due=advance_days(state(game).date,Days{44});
        apply(game,ProposeSuccession{ai->id,due,true,{"lease.initial",MilliCUPerDay{10000},1}});
        check(state(game).succession->plans.back().asi==ai->asi,"The nomination must freeze qualification and control evidence.");
        refuse(game,AnnounceFounderDeparture{1,"quiet_handover","personal"});
        const auto cash=state(game).cash;apply(game,VoteSuccession{1,true});
        check(state(game).succession->plans.back().status==SuccessionStatus::approved && state(game).cash==checked_unit_subtract(cash,ai->signing_cost),"The ASI appointment needs an actual paid board approval.");
        apply(game,AnnounceFounderDeparture{1,"quiet_handover","personal"});
        // This isolated scenario omits institutions. Supply an empty query
        // context to inspect the public events without inventing campaign history.
        auto public_view=game.current_state().world;public_view.authority.emplace();
        const auto reactions=runtime::public_identity_events(public_view,state(game).date);
        check(std::ranges::any_of(reactions,[](const auto& event){return event.cause=="Announced an approved ASI succession" && event.direct_effect && (*event.direct_effect)[1]>0;}) &&
            std::ranges::any_of(reactions,[](const auto& event){return event.cause.starts_with("Independent control evidence") && event.direct_effect && (*event.direct_effect)[4]<0;}),
            "The ASI announcement and its weak-control scrutiny must create actual authored audience effects.");
        auto forged=game.current_state();++forged.world.simulation.succession->plans.front().asi->control;
        refused=false;try {runtime::validate_checkpoint(p,forged);}catch(const std::exception&){refused=true;}
        check(refused,"A changed ASI control snapshot must be rejected.");
        game=reload(game);advance(game,due);
        const auto& term=state(game).succession->terms.back();
        check(term.person==ai->id && term.asi==ai->asi && !term.portrait && !term.caretaker && founder_role(*state(game).succession)==FounderRole::chair,
            "The paid transition must appoint the ASI without inventing a human identity or discarding the founder's chair.");
        check(term.due_on==to_ordinal({from_ordinal(due).year+static_cast<int>(succession_catalog(state(game)).asi->maximum_term_years),from_ordinal(due).month,from_ordinal(due).day}),
            "The ASI mandate must have the authored finite term.");
        check(std::ranges::any_of(succession_board(state(game),{},state(game).succession->controls.size()),[&](const auto& member){return member.id=="leadership.ceo" && member.name.starts_with(term.name) && member.seats==1;}),
            "The approved ASI must receive the actual chief executive board seat.");
        game=reload(game);const auto on=state(game).date;const auto calendar=from_ordinal(on);
        const auto next_month=calendar.month==12?CivilDate{calendar.year+1,1,1}:CivilDate{calendar.year,calendar.month+1,1};
        while(required_succession_notice(state(game))) apply(game,AcknowledgeSuccession{required_succession_notice(state(game))->id});
        advance(game,to_ordinal(next_month));
        const auto invoice=std::ranges::find_if(state(game).accounts->invoices,[](const auto& i){return i.id.starts_with("recurring/leadership/ceo.");});
        check(invoice!=state(game).accounts->invoices.end() && invoice->settled.value()>0,"AI hosting and human oversight must create actual paid company costs.");
        game=reload(game);persistence::save_file(p,game.current_state(),directory/"asi-appointed.pgpsave");
        std::cout<<"ASI succession passed with actual research, field work, independent qualification, board fees, paid exercises, frozen control evidence, finite mandate, operating costs and exact reloads.\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
