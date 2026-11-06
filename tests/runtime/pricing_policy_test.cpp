#include <pgp/runtime/scenario.hpp>
#include <pgp/session/slice_run.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
using namespace core;
int checks=0;
void check(bool ok,const char* why) {++checks;if(!ok) throw std::runtime_error(why);}
void apply(runtime::Runtime& game,CommandAction command) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(command)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction command) {
    const auto before=game.current_state();
    check(!game.submit({before.world.simulation.revision,std::move(command)}).accepted(),"Invalid price authorization was accepted");
    check(game.current_state()==before,"Refused price authorization changed state");
}
DelegationPolicy policy(std::string product) {
    DelegationPolicy p;p.id="pricing.test";p.target_id=std::move(product);p.kind=DelegationKind::pricing;
    p.pricing=PricingBounds{Cents{5},Cents{15},Cents{3},BasisPoints{2500},BasisPoints{8500}};
    p.interval=Days{7};p.maximum_runs=1200;return p;
}
void tiny_case() {
    SimulationState s;s.market.emplace();s.capacity.emplace();
    s.market->products.push_back({"product","serving",Cents{10},true});
    CapacityProject project;project.id="serving";project.source_id="source";project.kind=ProjectKind::serving;
    project.tasks_per_day=100;project.requested=MilliCUPerDay{100};project.inference_milli_CU_per_task=MilliCU{1};
    s.capacity->projects.push_back(project);
    CapacitySource source;source.id="source";source.nominal=MilliCUPerDay{1000};source.effective=source.nominal;s.capacity->sources.push_back(source);
    const auto sample=[&](std::uint64_t sales,std::uint64_t protected_tasks=0,bool outage=false,bool occupied=false) {
        s.market->days={};s.capacity->days={};
        MarketDay market;market.day=OrdinalDay{0};market.discretionary.push_back({"segment","product",sales,Cents{0}});s.market->days.push_back(market);
        CapacityDay day;day.day=OrdinalDay{0};day.projects.push_back({"serving",MilliCU{sales},sales,0,protected_tasks});
        day.sources.push_back({"source",MilliCUPerDay{outage?500:1000},MilliCU{occupied?1000:sales}});s.capacity->days.push_back(day);
    };
    const auto p=policy("product");
    sample(0);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{7},"Weak utilization did not lower one step");
    check(pricing_review(s,p,OrdinalDay{1},Cents{6}).quote==Cents{5},"Lower price escaped floor");
    sample(100);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{13},"High utilization did not raise one step");
    check(pricing_review(s,p,OrdinalDay{1},Cents{14}).quote==Cents{15},"Higher price escaped ceiling");
    sample(50);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{10},"Middle utilization changed price");
    sample(0,100);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{10},"Protected contracts were counted as discretionary slots");
    sample(0,0,true);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{10},"Known capacity reduction became a price cut");
    sample(1,0,false,true);check(pricing_review(s,p,OrdinalDay{1},Cents{10}).quote==Cents{10},"Fully occupied source became a weak-demand price cut");
}
void persisted_policy() {
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_PRICING_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto checkpoint=session::run_slice_baseline(program,OrdinalDay{80}).checkpoint;
    runtime::Runtime game{program,checkpoint};
    const auto& product=game.current_state().world.simulation.market->products.front();
    const auto id=product.id;const auto opening=product.price_per_task;
    auto p=policy(id);p.pricing->floor=Cents{0};p.pricing->ceiling=Cents{std::max<std::int64_t>(100,opening.value()*2)};p.pricing->step=Cents{1};
    auto bad=p;bad.pricing->floor=bad.pricing->ceiling;refuse(game,AddDelegationPolicy{bad});
    bad=p;bad.interval=Days{1};refuse(game,AddDelegationPolicy{bad});
    bad=p;bad.budget=Cents{1};refuse(game,AddDelegationPolicy{bad});
    apply(game,AddDelegationPolicy{p});bad=p;bad.id="duplicate";refuse(game,AddDelegationPolicy{bad});
    runtime::Runtime control{program,checkpoint};
    apply(game,AdvanceDay{});apply(control,AdvanceDay{});
    const auto& s=game.current_state().world.simulation;const auto& c=control.current_state().world.simulation;
    check(s.management->policies.back().runs==1 && s.management->decisions.back().accepted,"Scheduled price review did not commit");
    check(s.cash==c.cash && s.accounts==c.accounts && s.random==c.random,"Price review spent money, changed contracted settlement or sampled random outcomes");
    const auto cursor=*s.management->decisions.back().price_event_cursor;
    const auto event=s.market->price_events[static_cast<std::size_t>(cursor-1)];
    apply(game,SetMarketPrice{id,Cents{event.after.value()+1}});
    const auto& overridden=game.current_state().world.simulation;
    check(!overridden.management->policies.back().enabled && overridden.management->policies.back().runs==1 && overridden.management->decisions.back().overridden,"Manual override did not suspend with preserved counters");
    check(overridden.market->price_events[static_cast<std::size_t>(cursor-1)]==event,"Same-day override rewrote the authorized decision");
    const auto loaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
    check(loaded.checkpoint==game.current_state(),"Price event/override save did not reload exactly");
    runtime::Runtime resumed{program,loaded.checkpoint};
    for (auto* engine:{&game,&resumed}) {
        apply(*engine,SetDelegationEnabled{p.id,true});
        apply(*engine,SetDelegationEnabled{p.id,false});apply(*engine,SetDelegationEnabled{p.id,true});
        for (int day=0;day<7;++day) apply(*engine,AdvanceDay{});
    }
    check(game.current_state()==resumed.current_state() && game.current_state().world.simulation.management->policies.back().runs==2,"Resume reset review counters or diverged after reload");
    apply(game,SetMarketPrice{id,Cents{p.pricing->ceiling.value()+1}});
    refuse(game,SetDelegationEnabled{p.id,true});
    apply(game,SetMarketPrice{id,opening});apply(game,SetDelegationEnabled{p.id,true});
    check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Out-of-range override and explicit recovery did not persist");
    runtime::Runtime exhausted{program,checkpoint};p.maximum_runs=1;
    apply(exhausted,AddDelegationPolicy{p});apply(exhausted,AdvanceDay{});
    check(!exhausted.current_state().world.simulation.management->policies.back().enabled,"Price policy exceeded its finite review limit");
    refuse(exhausted,SetDelegationEnabled{p.id,true});
    auto forged=game.current_state();auto decision=forged.world.simulation.management->decisions.front();decision.explanation="Invented evidence";
    forged.world.simulation.management->decisions.replace(0,std::move(decision));
    bool rejected=false;try {runtime::validate_checkpoint(program,forged);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"Forged price explanation survived historical replay");
}
}
int main() {try {tiny_case();persisted_policy();std::cout<<checks<<" pricing policy checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
