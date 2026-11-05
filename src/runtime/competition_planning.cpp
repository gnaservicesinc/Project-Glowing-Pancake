#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::runtime::competition_detail {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* text) { throw std::invalid_argument(text); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (it==rows.end()) invalid("Rival strategy refers to an unknown definition");return *it;
}
const content::ScenarioDefinition& scenario(const WorldState& world,const CompiledProgram& program) {
    const auto* definition=program.collection.find(world.scenario_id);
    if (!definition || !std::holds_alternative<content::ScenarioDefinition>(definition->data)) invalid("Planned rivals require their explicit source scenario");
    return std::get<content::ScenarioDefinition>(definition->data);
}
bool active(const SimulationState& s) {return s.phase!=DayPhase::closed && s.phase!=DayPhase::finished;}
bool completed(const LabState& lab,std::string_view id) {return std::any_of(lab.research.begin(),lab.research.end(),[&](const auto& row){return row.research_id==id && row.completed;});}
bool requires_met(const LabState& lab,const std::vector<std::string>& ids) {return std::all_of(ids.begin(),ids.end(),[&](const auto& id){return completed(lab,id);});}
bool submit(RivalCompany& rival,CommandAction action,std::string* explanation=nullptr) {
    // One copied weekly/exceptional action, never a copied history per candidate
    // or per ordinary daily close. A refusal preserves the company's old state.
    Simulation candidate{rival.simulation,TrustedSimulationState{}};
    const auto result=candidate.submit_in_transaction({rival.simulation.revision,std::move(action)});
    if (explanation) *explanation=result.explanation;
    if (result.accepted()) rival.simulation=std::move(candidate).release_checkpoint();
    return result.accepted();
}
void advance(RivalCompany& rival,AdvanceDay day) {
    Simulation candidate{std::move(rival.simulation),TrustedSimulationState{}};
    const auto result=candidate.submit_in_transaction({candidate.checkpoint().revision,std::move(day)});
    if (!result.accepted()) throw std::invalid_argument("Rival daily settlement refused: "+result.explanation);
    rival.simulation=std::move(candidate).release_checkpoint();
}
std::uint32_t delivered_player_products(const WorldState& world) {
    std::uint32_t count=0;
    for (const auto& release:world.competition->releases) if (release.company_id==world.company_id) ++count;
    return count;
}
void enter_due(WorldState& world,const CompiledProgram& program) {
    const auto day=world.simulation.date;
    for (auto& rival:world.competition->companies) {
        auto& plan=*rival.planning;if (plan.entered_on) continue;
        const auto& definition=rival_definition(program,rival.definition_id);
        if (definition.response) continue;
        const auto earliest=advance_days(world.campaign_start,definition.start_after);
        if (day<earliest || day>=world.simulation.end_date) continue;
        if (definition.player_deployment_threshold>0) {
            if (!rival.warned_on && delivered_player_products(world)>=definition.player_deployment_threshold) {
                rival.warned_on=day;world.competition->notices.push_back({definition.id,"warning",day,advance_days(day,definition.warning_lead)});
            }
            if (!rival.warned_on || day<advance_days(*rival.warned_on,definition.warning_lead)) continue;
        }
        rival.simulation=opening_rival(world,program,definition,day);plan.entered_on=day;plan.next_review=day;
        world.competition->notices.push_back({definition.id,"entry",day,std::nullopt});
    }
}
RivalObservation observe(const WorldState& world,OrdinalDay day,std::uint64_t draw) {
    RivalObservation result;result.day=day;std::set<std::string> providers;std::vector<std::int64_t> prices;
    // All companies review the same last-settled public day. Later reviews or
    // player price edits this morning cannot rewrite an earlier observation.
    const auto quote_day=day>world.campaign_start?OrdinalDay{day.value()-1}:day;
    for (const auto& release:world.competition->releases) if (release.day<day) {
        const auto* owner=company(world,release.company_id);
        if (!owner || !owner->market || ((owner->phase==DayPhase::closed || owner->phase==DayPhase::finished) && owner->date<=quote_day)) continue;
        const auto source=std::find_if(world.competition->companies.begin(),world.competition->companies.end(),[&](const auto& row){return row.definition_id==release.company_id;});
        if (source!=world.competition->companies.end() && source->response && source->response->retired_on && *source->response->retired_on<=quote_day) continue;
        providers.insert(release.company_id);prices.push_back(market_price_on(*owner->market,release.product_id,quote_day).value());
    }
    result.public_providers=static_cast<std::uint32_t>(providers.size());
    if (!prices.empty()) {std::sort(prices.begin(),prices.end());result.public_price=Cents{prices[prices.size()/2]};}
    std::uint64_t demand=0;for (const auto& row:world.simulation.market->segments)
        demand+=day==world.campaign_start?row.daily_tasks:public_market_demand(*world.simulation.market,row,quote_day);
    const auto error=world.competition->setup->estimate_error;
    const auto offset=error==0?0U:static_cast<std::uint32_t>(draw%(2U*error+1U));
    const auto midpoint=mul_div_floor(demand,100U+offset-error,100).quotient;
    const auto uncertainty=mul_div_floor(demand,error,100).quotient;
    result.demand_lower=static_cast<std::uint64_t>(midpoint)>static_cast<std::uint64_t>(uncertainty)?static_cast<std::uint64_t>(midpoint-uncertainty):0;
    result.demand_upper=static_cast<std::uint64_t>(checked_add(static_cast<std::int64_t>(midpoint),static_cast<std::int64_t>(uncertainty)));
    return result;
}
struct Candidate {std::uint32_t kind;std::string action,subject;std::int64_t score;CommandAction command;};
std::vector<Candidate> candidates(const RivalCompany& rival,const content::RivalDefinition& definition,const RivalObservation& observation,std::uint64_t draw) {
    const auto& s=rival.simulation;const auto& profile=*definition.planning;const auto& lab=*s.lab;
    std::vector<Candidate> result;SplitMix64 variation{draw};
    std::array<std::uint32_t,10> noise{};for (auto& value:noise) value=static_cast<std::uint32_t>(variation.bounded(31));
    const auto offer=[&](std::uint32_t kind,std::string action,std::string subject,std::int64_t score,CommandAction command) {
        result.push_back({kind,std::move(action),std::move(subject),score+profile.priorities[kind]+noise[kind],std::move(command)});
    };
    Cents monthly{0};for (const auto& row:s.accounts->recurring) if (row.starts_on<=s.date && (!row.ends_before || s.date<*row.ends_before)) monthly=checked_unit_add(monthly,row.monthly_amount);
    const auto protected_runway=checked_unit_add(profile.reserve_floor,Cents{mul_div_floor(static_cast<std::uint64_t>(monthly.value()),profile.runway_months,1).quotient});
    const auto affordable=[&](Cents immediate,Cents extra_monthly=Cents{0}) {
        return s.cash>=checked_unit_add(protected_runway,checked_unit_add(immediate,Cents{checked_add(extra_monthly.value(),extra_monthly.value())}));
    };
    const auto report=capacity_report(*s.capacity);
    const auto staff=s.capacity->available_staff>report.assigned_staff?s.capacity->available_staff-report.assigned_staff:0;
    std::string source;MilliCUPerDay compute{0};
    for (const auto& row:s.capacity->sources) {
        std::int64_t reserved=0;for (const auto& work:s.capacity->projects) if (work.status==ProjectStatus::active && work.source_id==row.id) reserved=checked_add(reserved,work.requested.value());
        const auto available=std::max<std::int64_t>(0,checked_subtract(row.effective.value(),reserved));
        if (available>compute.value() || (available==compute.value() && (source.empty() || row.id<source))) {source=row.id;compute=MilliCUPerDay{available};}
    }
    const LabAllocation allocation{source,MilliCUPerDay{std::min(compute.value(),definition.training_rate.value())},std::min(staff,2U)};
    const bool can_work=staff>0 && compute.value()>0 && affordable(Cents{0});
    if (std::none_of(lab.licenses.begin(),lab.licenses.end(),[&](const auto& row){return row.offer_id==definition.data_id;})) {
        const auto& data=find(lab.catalog.data,definition.data_id);
        if (affordable(data.price)) offer(0,"data",data.id,500,BuyData{data.id});
    }
    for (std::size_t i=0;i<profile.research.size();++i) {
        const auto& node=find(lab.catalog.research,profile.research[i]);
        const bool in_progress=std::any_of(lab.research.begin(),lab.research.end(),[&](const auto& run){return run.research_id==node.id && find(s.capacity->projects,run.project_id).status!=ProjectStatus::cancelled;});
        if (can_work && !in_progress && requires_met(lab,node.all_of) && (node.any_of.empty() || std::any_of(node.any_of.begin(),node.any_of.end(),[&](const auto& id){return completed(lab,id);})) && affordable(node.cash))
            offer(1,"research",node.id,350-static_cast<std::int64_t>(i),StartResearch{node.id,allocation});
        if (profile.publish_results && completed(lab,node.id) && std::none_of(lab.publications.begin(),lab.publications.end(),[&](const auto& row){return row.research_id==node.id;}))
            offer(9,"publish",node.id,80,PublishResearch{node.id});
    }
    if (can_work && lab.models.size()<profile.recipes.size()) for (std::size_t i=0;i<profile.recipes.size();++i) {
        const auto& recipe=find(lab.catalog.recipes,profile.recipes[i]);
        if (std::any_of(lab.models.begin(),lab.models.end(),[&](const auto& row){return row.recipe_id==recipe.id;})) continue;
        TrainingRequest request{"model."+recipe.id,recipe.id,"",{definition.data_id},source,allocation.daily_compute,allocation.staff,
            MilliCU{std::clamp(definition.training_work.value(),recipe.minimum_work.value(),recipe.maximum_work.value())}};
        const auto preview=preview_training(lab,request);
        if (preview.licensed && preview.prerequisites_met) offer(2,"train",recipe.id,400-static_cast<std::int64_t>(i),StartTraining{std::move(request)});
    }
    for (const auto& model:lab.models) {
        const auto& work=find(s.capacity->projects,model.project_id);if (work.status!=ProjectStatus::completed) continue;
        for (const auto& plan:lab.catalog.evaluations) if (can_work && requires_met(lab,plan.requires_research) && affordable(plan.cash) &&
            std::none_of(lab.evaluations.begin(),lab.evaluations.end(),[&](const auto& row){return row.model_id==model.id && row.plan_id==plan.id;}))
            offer(3,"evaluate",model.id,500,StartEvaluation{"evidence."+model.recipe_id+"."+plan.id,model.id,plan.id,allocation});
        for (const auto& evidence:lab.evaluations) if (evidence.completed && evidence.model_id==model.id) for (const auto& id:profile.services) {
            const auto service_it=std::find_if(s.deployments->catalog.begin(),s.deployments->catalog.end(),[&](const auto& row){return row.product.id==id;});
            if (service_it==s.deployments->catalog.end()) invalid("Rival service priority is absent");
            const auto& service=*service_it;bool qualified=true;
            if (!service.required_recipe.empty() && service.required_recipe!=model.recipe_id) continue;
            for (std::size_t axis=0;axis<lab_domain_count;++axis) if (evidence.lower[axis]<service.required[axis]) qualified=false;
            if (qualified && staff>=service.serving.staff && !source.empty() && std::none_of(s.deployments->records.begin(),s.deployments->records.end(),[&](const auto& row){return row.service_id==id;}) && affordable(Cents{0},service.serving.monthly_support_cost))
                offer(4,"deploy",id,650,DeployModel{id,model.id,evidence.id,source,false});
        }
    }
    std::uint64_t recent_tasks=0,recent_days=0;
    for (auto day=s.market->days.rbegin();day!=s.market->days.rend() && recent_days<7;++day) {
        ++recent_days;for (const auto& row:day->segments) recent_tasks+=row.contract_tasks+row.discretionary_tasks;
    }
    // Public price pressure never substitutes for this company's real costs.
    // A bounded competitor can still fail when its viable price exceeds demand.
    auto viable_price=profile.price_ceiling;
    if (recent_tasks>0) {
        const auto cost=mul_div_floor(static_cast<std::uint64_t>(monthly.value()),recent_days,28*recent_tasks);
        viable_price=Cents{std::clamp(checked_add(static_cast<std::int64_t>(cost.quotient),cost.remainder>0?1:0),profile.price_floor.value(),profile.price_ceiling.value())};
    }
    for (const auto& product:s.market->products) {
        const auto desired=observation.public_price.value()>0?Cents{std::clamp(checked_add(observation.public_price.value(),static_cast<std::int64_t>(profile.priorities[5]%21)-10),viable_price.value(),profile.price_ceiling.value())}:product.price_per_task;
        if (desired!=product.price_per_task) offer(5,"price",product.id,60,SetMarketPrice{product.id,desired});
    }
    if (staff==0) for (const auto& specialist:s.management->catalog.specialists)
        if (affordable(specialist.signing_cost,specialist.monthly_salary) && std::none_of(s.management->specialists.begin(),s.management->specialists.end(),[&](const auto& row){return row.offer_id==specialist.id;}))
            offer(6,"hire",specialist.id,120,HireSpecialist{specialist.id});
    for (const auto& debt:s.accounts->debts) if (debt.principal.value()>0 && advance_days(s.date,Days{7})>=debt.due && affordable(debt.principal))
        offer(7,"repay",debt.id,900,RepayDebt{debt.id,debt.principal,"rival.repay/"+debt.id});
    if (s.cash<protected_runway) for (const auto& financing:s.management->catalog.financing)
        if (std::none_of(s.management->financing.begin(),s.management->financing.end(),[&](const auto& row){return row.offer_id==financing.id;}))
            offer(7,"finance",financing.id,800,TakeFinancing{financing.id});
    if (compute.value()<definition.training_rate.value()/2 && s.business_offers) for (const auto& growth:s.business_offers->catalog)
        if (affordable(growth.setup_cost,growth.monthly_amount) && business_offer_eligible(*s.business_offers,growth,*s.accounts,*s.capacity,s.market,s.date,Cents{0}))
            offer(8,"expand",growth.id,100,AcceptBusinessOffer{growth.id});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tuple{-a.score,a.action,a.subject}<std::tuple{-b.score,b.action,b.subject};});
    return result;
}
void diffuse(WorldState& world,RivalCompany& rival) {
    const auto& setup=*world.competition->setup;if (setup.publication_credit.value()==0) return;
    std::vector<const SimulationState*> sources{&world.simulation};
    for (const auto& row:world.competition->companies) if (row.planning->entered_on) sources.push_back(&row.simulation);
    // Canonical publisher/result order prevents a source-order advantage.
    std::sort(sources.begin(),sources.end(),[](const auto* a,const auto* b){return a->lab->owner<b->lab->owner;});
    for (const auto* source:sources) if (source->lab->owner!=rival.definition_id) for (const auto& publication:source->lab->publications) {
        auto& state=rival.simulation;
        if (advance_days(publication.day,setup.publication_delay)>state.date ||
            std::any_of(state.lab->insights.begin(),state.lab->insights.end(),[&](const auto& row){return row.research_id==publication.research_id;}) ||
            std::any_of(state.lab->research.begin(),state.lab->research.end(),[&](const auto& row){return row.research_id==publication.research_id;})) continue;
        ResearchInsight insight{publication.research_id,source->lab->owner,publication.day,state.date,setup.publication_credit};
        validate_public_insight(world,insight);if (!submit(rival,AcquireResearchInsight{insight})) invalid("A validated public research adoption was refused");
    }
}
}
std::uint64_t rival_seed(const WorldState& world,const content::RivalDefinition& definition) {return world.root_seed^definition.seed;}
SimulationState dormant_company(const WorldState& world,const content::RivalDefinition& definition) {
    SimulationState result;result.date=world.campaign_start;result.end_date=world.simulation.end_date;result.random=RandomStreams{rival_seed(world,definition)}.snapshot();return result;
}
SimulationState opening_rival(const WorldState& world,const CompiledProgram& program,const content::RivalDefinition& definition,OrdinalDay day) {
    const auto& authored=scenario(world,program);SimulationState state;
    state.date=day;state.end_date=world.simulation.end_date;state.cash=definition.cash;state.random=RandomStreams{rival_seed(world,definition)}.snapshot();
    state.accounts=opening_accounts(definition.cash,day,definition.id);
    add_recurring(*state.accounts,{"payroll",definition.monthly_payroll,day,std::nullopt,"Rival payroll"});
    add_recurring(*state.accounts,{"compute_lease",definition.monthly_lease,day,std::nullopt,"Rival compute lease"});
    state.capacity=opening_capacity({{"lease.initial","cloud/"+definition.id,definition.id,"supplier.rival",CapacitySourceKind::leased,definition.capacity,definition.capacity,0,"general",true}},definition.staff,day,definition.id);
    state.liquidity=opening_liquidity(day,definition.id);
    LabState lab;lab.owner=definition.id;lab.starts_on=day;lab.catalog=world.simulation.lab->catalog;
    lab.research_stream_origin=state.random[static_cast<std::size_t>(RandomStream::research)];state.lab=std::move(lab);
    state.market=opening_market(day,definition.id);state.market->segments=authored.market->segments;
    for (auto& segment:state.market->segments) segment.preferences.clear();
    state.deployments=DeploymentState{content::service_definitions(*authored.service_catalog),{}};
    state.management=ManagementState{*authored.management,{},{},{},{},{},{}};state.management->catalog.setbacks.clear();
    state.management->catalog.scoring.founder="founder";
    if (definition.response) {state.management->catalog.financing.clear();state.capacity->sources.front().effective=MilliCUPerDay{0};}
    if (!definition.response && world.competition->setup->catchup_cash.value()>0) state.management->catalog.financing.push_back({"finance.disclosed_catchup","Disclosed finite catch-up equity",world.competition->setup->catchup_cash,world.competition->setup->catchup_acquired,BasisPoints{0},Days{0}});
    BusinessOffersState growth;
    for (auto offer:authored.business_offers) if (offer.kind==BusinessOfferKind::compute_lease) {offer.id+="."+definition.id;growth.catalog.push_back(std::move(offer));}
    if (!growth.catalog.empty()) state.business_offers=std::move(growth);
    return Simulation{std::move(state)}.checkpoint();
}
RivalObservation public_observation(const WorldState& world,OrdinalDay day,std::uint64_t draw) {return observe(world,day,draw);}
bool submit_rival(RivalCompany& rival,CommandAction action) {return submit(rival,std::move(action));}
void review_rival(WorldState& world,const CompiledProgram& program,RivalCompany& rival) {
    auto& plan=*rival.planning;if (!plan.entered_on || !active(rival.simulation) || rival.simulation.date<plan.next_review) return;
    if (rival.response && rival.response->phase!=HeavyweightPhase::compete) return;
    if (plan.decisions.size()>=8192) invalid("Rival weekly decision history exceeds its century bound");
    const auto& definition=rival_definition(program,rival.definition_id);diffuse(world,rival);
    const auto stream_index=static_cast<std::size_t>(RandomStream::rivals);const auto before=rival.simulation.random[stream_index];
    SplitMix64 stream{before};const auto draw=stream.next();rival.simulation.random[stream_index]=stream.state();
    const auto observation=observe(world,rival.simulation.date,draw);auto actions=candidates(rival,definition,observation,draw);
    RivalDecision decision{rival.simulation.date,"wait","","Conserve runway; no feasible action met the company's resource and reserve limits.",false,before,stream.state(),observation};
    if (!actions.empty()) {
        auto selected=std::move(actions.front());decision.action=std::move(selected.action);decision.subject=std::move(selected.subject);
        decision.accepted=submit(rival,std::move(selected.command),&decision.explanation);
        rival.attempted=true;
    }
    plan.decisions.push_back(std::move(decision));plan.next_review=advance_days(plan.next_review,Days{7});
}
void collect_public_results(WorldState& world,OrdinalDay prior) {
    std::vector<const SimulationState*> sources{&world.simulation};
    for (auto& rival:world.competition->companies) if (rival.planning->entered_on) sources.push_back(&rival.simulation);
    for (const auto* state:sources) if (state->market && !state->market->days.empty() && state->market->days.back().day==prior) {
        std::set<std::string> delivered;
        for (const auto& row:state->market->days.back().discretionary) if (row.tasks>0) delivered.insert(row.product_id);
        for (const auto& row:state->market->days.back().contracts) if (row.delivered_tasks>0) delivered.insert(find(state->market->contracts,row.contract_id).product_id);
        for (const auto& id:delivered) if (std::none_of(world.competition->releases.begin(),world.competition->releases.end(),[&](const auto& row){return row.company_id==state->market->owner && row.product_id==id;}))
            world.competition->releases.push_back({state->market->owner,id,prior});
    }
    for (auto& rival:world.competition->companies) if (rival.planning->entered_on) for (const auto& publication:rival.simulation.lab->publications)
        if (std::none_of(world.competition->notices.begin(),world.competition->notices.end(),[&](const auto& row){return row.company_id==rival.definition_id && row.kind=="publication" && row.subject==publication.research_id;})) {
            if (!rival.published_on) rival.published_on=publication.day;
            world.competition->notices.push_back({rival.definition_id,"publication",publication.day,std::nullopt,publication.research_id});
        }
}
void initialize_planning(WorldState& world,const CompiledProgram& program,const std::vector<content::RivalDefinition>& definitions,const content::CompetitionSetup& setup) {
    CompetitionState state;state.setup=setup;
    for (const auto& definition:definitions) if (content::selected_rival(definition,setup)) {
        RivalCompany rival;rival.definition_id=definition.id;rival.simulation=dormant_company(world,definition);
        rival.planning=RivalPlanState{{},advance_days(world.campaign_start,definition.start_after),{}};
        if (definition.response) {rival.response=HeavyweightState{};rival.response->next_review=world.campaign_start;}
        state.companies.push_back(std::move(rival));
    }
    world.competition=std::move(state);enter_due(world,program);update_heavyweight_responses(world,program);
    for (auto& rival:world.competition->companies) review_rival(world,program,rival);
    collect_public_results(world,world.campaign_start);
}
void prepare_rival_morning(WorldState& world,const CompiledProgram& program,RivalCompany& rival) {
    if (!rival.planning->entered_on || !active(rival.simulation) || (rival.response && rival.response->retired_on)) return;
    const auto day=world.simulation.date;
    if (const auto warning=liquidity_report(*rival.simulation.liquidity).pending_warning) if (!submit(rival,AcknowledgeLiquidityWarning{*warning})) invalid("Rival liquidity warning was not acknowledged");
    std::vector<std::string> accepted_ids;
    if (rival.simulation.business_offers) for (const auto& row:rival.simulation.business_offers->accepted) accepted_ids.push_back(row.offer_id);
    for (const auto& id:accepted_ids) {
        const auto offer=find(rival.simulation.business_offers->catalog,id);
        if (offer.renewable && lease_expiry(*rival.simulation.business_offers,offer.id)==day &&
            rival.simulation.cash>=checked_unit_add(rival_definition(program,rival.definition_id).planning->reserve_floor,lease_commitment_ceiling(offer)))
            (void)submit(rival,RenewComputeLease{offer.id});
    }
}
void advance_planning(WorldState& world,const CompiledProgram& program,OrdinalDay prior,const IndustryAllocations& assignments) {
    for (auto& rival:world.competition->companies) if (rival.planning->entered_on && active(rival.simulation)) {
        if (rival.simulation.date!=prior) invalid("Rival daily cursor differs from the common market day");
        AdvanceDay next;if (const auto it=assignments.find(rival.definition_id);it!=assignments.end()) {
            next.market_allocation=it->second;next.industry_adoptions=industry_adoption_snapshot(world);
        }
        advance(rival,std::move(next));
        if (rival.simulation.phase==DayPhase::closed) world.competition->notices.push_back({rival.definition_id,"closure",rival.simulation.date,std::nullopt});
    }
    collect_public_results(world,prior);append_industry_adoption(world,prior);enter_due(world,program);
    update_competitive_offers(world,program);update_heavyweight_responses(world,program);
    for (auto& rival:world.competition->companies) review_rival(world,program,rival);
    collect_public_results(world,prior);
}
}
