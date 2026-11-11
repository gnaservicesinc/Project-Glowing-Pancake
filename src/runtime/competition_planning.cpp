#include "competition_internal.hpp"
#include <pgp/runtime/society.hpp>
#include <pgp/core/workplace_research.hpp>
#include <pgp/runtime/authority.hpp>
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
void acknowledge_leadership(RivalCompany& rival) {
    while (const auto* notice=required_succession_notice(rival.simulation)) {
        std::string explanation;
        if (!submit(rival,AcknowledgeSuccession{notice->id},&explanation))
            throw std::invalid_argument("Rival "+rival.definition_id+" could not acknowledge leadership. "+explanation);
    }
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
    const auto day=campaign_day(world);
    for (auto& rival:world.competition->companies) {
        auto& plan=*rival.planning;if (plan.entered_on) continue;
        const auto& definition=rival_definition(program,rival.definition_id);
        if (definition.response) continue;
        const auto earliest=advance_days(world.campaign_start,definition.start_after);
        if (day<earliest || day>=campaign_calendar_limit(world)) continue;
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
        if (!owner || !owner->market || ((owner->phase==DayPhase::closed || owner->phase==DayPhase::finished ||
            (observing_world(world) && owner==&world.simulation)) && owner->date<=quote_day)) continue;
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
std::vector<Candidate> candidates(const RivalCompany& rival,const content::RivalDefinition& definition,
    const RivalObservation& observation,std::uint64_t draw,const CivicState* civic,const SocietyState* society,Days review_interval) {
    const auto& s=rival.simulation;const auto& profile=*definition.planning;const auto& lab=*s.lab;
    std::vector<Candidate> result;SplitMix64 variation{draw};
    std::array<std::uint32_t,10> noise{};for (auto& value:noise) value=static_cast<std::uint32_t>(variation.bounded(31));
    const auto offer=[&](std::uint32_t kind,std::string action,std::string subject,std::int64_t score,CommandAction command) {
        result.push_back({kind,std::move(action),std::move(subject),score+profile.priorities[kind]+noise[kind],std::move(command)});
    };
    Cents monthly{0};for (const auto& row:s.accounts->recurring) if (row.starts_on<=s.date && (!row.ends_before || s.date<*row.ends_before)) monthly=checked_unit_add(monthly,row.monthly_amount);
    auto usable=s.cash;
    for (const auto& funds:s.reservations) usable=checked_unit_subtract(usable,funds.reserved);
    const auto protected_runway=checked_unit_add(profile.reserve_floor,Cents{mul_div_floor(static_cast<std::uint64_t>(monthly.value()),profile.runway_months,1).quotient});
    const auto affordable=[&](Cents immediate,Cents extra_monthly=Cents{0}) {
        return usable>=checked_unit_add(protected_runway,checked_unit_add(immediate,Cents{checked_add(extra_monthly.value(),extra_monthly.value())}));
    };
    const auto report=capacity_report(*s.capacity);
    const auto staff=s.capacity->available_staff>report.assigned_staff?s.capacity->available_staff-report.assigned_staff:0;
    std::string source, hosting_source;
    MilliCUPerDay compute{0}, hosting_compute{0};
    bool imported=false;
    for (const auto& row:s.capacity->sources) {
        std::int64_t reserved=0;for (const auto& work:s.capacity->projects) if (work.status==ProjectStatus::active && work.source_id==row.id) reserved=checked_add(reserved,work.requested.value());
        const auto available=std::max<std::int64_t>(0,checked_subtract(company_capacity(row).value(),reserved));
        if (available==0) continue;
        const bool bought=row.kind==CapacitySourceKind::imported;
        // Use the funded term for finite lab work. Long-lived hosted services
        // still plan against the buyer's ordinary supply.
        if ((bought && !imported) || (bought==imported && available>compute.value())) {
            source=row.id;compute=MilliCUPerDay{available};imported=bought;
        }
        if (!bought && available>hosting_compute.value()) {
            hosting_source=row.id;hosting_compute=MilliCUPerDay{available};
        }
    }
    const LabAllocation allocation{source,MilliCUPerDay{std::min(compute.value(),definition.training_rate.value())},std::min(staff,2U)};
    const bool can_work=staff>0 && compute.value()>0 && affordable(Cents{0});
    if (frontier_technology_block(s,FrontierTechnology::data,definition.data_id,s.date).empty() &&
        std::none_of(lab.licenses.begin(),lab.licenses.end(),[&](const auto& row){return row.offer_id==definition.data_id;})) {
        const auto& data=find(lab.catalog.data,definition.data_id);
        if (affordable(data.price)) offer(0,"data",data.id,500,BuyData{data.id});
    }
    for (std::size_t i=0;i<profile.research.size();++i) {
        const auto& node=find(lab.catalog.research,profile.research[i]);
        const bool committed=std::ranges::any_of(lab.research,[&](const auto& run) {
            return run.research_id == node.id && !run.failed && find(s.capacity->projects,run.project_id).status != ProjectStatus::cancelled;
        });
        if (can_work && !committed && frontier_technology_block(s,FrontierTechnology::research,node.id,s.date).empty() &&
            requires_met(lab,node.all_of) && (node.any_of.empty() || std::any_of(node.any_of.begin(),node.any_of.end(),[&](const auto& id){return completed(lab,id);})) && affordable(node.cash))
        {
            auto research=allocation;
            if(s.management->people) {
                research.researchers.clear();
                for(const auto& worker:research_workers(s))if(worker.unavailable.empty() && research.researchers.size()<research.staff)research.researchers.push_back(worker.id);
                research.staff=static_cast<std::uint32_t>(research.researchers.size());
            }
            if(research.staff>0 && (!s.management->people || research_workplace_blocker(s,node.id,research).empty()))offer(1,"research",node.id,350-static_cast<std::int64_t>(i),StartResearch{node.id,research});
        }
        if (profile.publish_results && completed(lab,node.id) && std::none_of(lab.publications.begin(),lab.publications.end(),[&](const auto& row){return row.research_id==node.id;}))
            offer(9,"publish",node.id,80,PublishResearch{node.id});
    }
    if (can_work && lab.models.size()<profile.recipes.size()) for (std::size_t i=0;i<profile.recipes.size();++i) {
        const auto& recipe=find(lab.catalog.recipes,profile.recipes[i]);
        if (!frontier_technology_block(s,FrontierTechnology::recipe,recipe.id,s.date).empty()) continue;
        if (std::any_of(lab.models.begin(),lab.models.end(),[&](const auto& row){return row.recipe_id==recipe.id;})) continue;
        TrainingRequest request{"model."+recipe.id,recipe.id,"",{definition.data_id},source,allocation.daily_compute,allocation.staff,
            MilliCU{std::clamp(definition.training_work.value(),recipe.minimum_work.value(),recipe.maximum_work.value())}};
        const auto preview=preview_training(lab,request);
        if (preview.licensed && preview.prerequisites_met) offer(2,"train",recipe.id,400-static_cast<std::int64_t>(i),StartTraining{std::move(request)});
    }
    const auto useful_assessment=[&](const EvaluationPlan& plan,const LabModel& model) {
        const auto axes=stored_lab_domains(plan.coverage);
        if (axes==introductory_lab_domains) return true;
        return std::ranges::any_of(s.deployments->catalog,[&](const auto& service) {
            if ((!service.required_recipe.empty() && service.required_recipe!=model.recipe_id) ||
                std::ranges::find(profile.services,service.product.id)==profile.services.end()) return false;
            for (std::size_t axis=introductory_lab_domains;axis<axes;++axis)
                if (service.required[axis].value()>0 && plan.coverage[axis].value()>0) return true;
            return false;
        });
    };
    for (const auto& model:lab.models) {
        const auto& work=find(s.capacity->projects,model.project_id);if (work.status!=ProjectStatus::completed) continue;
        for (const auto& plan:lab.catalog.evaluations) if (can_work && useful_assessment(plan,model) &&
            requires_met(lab,plan.requires_research) && affordable(plan.cash) &&
            std::none_of(lab.evaluations.begin(),lab.evaluations.end(),[&](const auto& row){return row.model_id==model.id && row.plan_id==plan.id;}))
            offer(3,"evaluate",model.id,500,StartEvaluation{"evidence."+model.recipe_id+"."+plan.id,model.id,plan.id,allocation});
        for (const auto& evidence:lab.evaluations) if (evidence.completed && evidence.model_id==model.id) for (const auto& id:profile.services) {
            const auto service_it=std::find_if(s.deployments->catalog.begin(),s.deployments->catalog.end(),[&](const auto& row){return row.product.id==id;});
            if (service_it==s.deployments->catalog.end()) invalid("Rival service priority is absent");
            const auto& service=*service_it;bool qualified=true;
            if (!service.required_recipe.empty() && service.required_recipe!=model.recipe_id) continue;
            for (std::size_t axis=0;axis<lab_domain_count;++axis) if (evidence.lower[axis]<service.required[axis]) qualified=false;
            const bool local=customer_hosted(service.product.channel);
            if (qualified && requires_met(lab,service.required_research) && staff>=service.serving.staff &&
                (local || !hosting_source.empty()) && std::none_of(s.deployments->records.begin(),s.deployments->records.end(),[&](const auto& row){return row.service_id==id;}) && affordable(Cents{0},service.serving.monthly_support_cost)) {
                DeployModel launch{id,model.id,evidence.id,local?"":hosting_source,service.product.channel==ProductChannel::agent};
                if (civic_launch_block(civic,s,launch,s.date).empty() && society_launch_block(society,s,launch,s.date).empty())
                    offer(4,"deploy",id,650,std::move(launch));
            }
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
        if (product.channel==ProductChannel::open_weights) continue;
        const auto desired=observation.public_price.value()>0?Cents{std::clamp(checked_add(observation.public_price.value(),static_cast<std::int64_t>(profile.priorities[5]%21)-10),viable_price.value(),profile.price_ceiling.value())}:product.price_per_task;
        if (desired!=product.price_per_task) offer(5,"price",product.id,60,SetMarketPrice{product.id,desired});
    }
    if(talent_rules(s)) {
        const auto& hq=*s.infrastructure->workplace->headquarters;
        const auto& premises=current_headquarters(s);
        std::set<std::string> needed;
        for(const auto& topic:profile.research) {
            const auto& node=find(lab.catalog.research,topic);
            if(completed(lab,topic) || !requires_met(lab,node.all_of) || (!node.any_of.empty() && std::ranges::none_of(node.any_of,[&](const auto& id){return completed(lab,id);})))continue;
            for(const auto& equipment:research_equipment(s,topic))needed.insert(equipment);
        }
        for(const auto& item:hq.items)if(!item.premises && item.delivered_on<=s.date) {
            bool found=false;
            for(int y=0;y<premises.depth && !found;++y)for(int x=0;x<premises.width && !found;++x) {
                PlaceHQEquipment placement{item.id,0,x,y,false};
                if(hq_placement_blocker(s,placement).empty()){offer(8,"place",std::to_string(item.id),160,ManageHeadquarters{placement});found=true;}
            }
        }
        for(const auto& id:needed)if(std::ranges::none_of(hq.items,[&](const auto& item){return item.offer==id;})) {
            const auto& equipment=find(s.infrastructure->catalog.workplace->headquarters->equipment,id);
            if(affordable(equipment.price))offer(8,"equipment",id,150,ManageHeadquarters{BuyHQEquipment{id}});
        }
        const auto& talents=*s.management->people->talent;
        for(const auto& employee:s.management->people->employees)if(!employee.departed_on && employee.ready_on<=s.date &&
            std::ranges::none_of(talents.courses,[&](const auto& c){return c.person==employee.terms.profile.id && !c.completed;})) {
            const auto& person=employee.terms.profile;
            const std::string desired=has_skill(person,"ml_research")?(has_skill(person,"programming")?"architecture":"programming"):(has_skill(person,"programming")?"ml_research":"programming");
            if(!has_skill(person,desired)) {
                const auto& course=find(talent_rules(s)->skills,desired);
                if(affordable(Cents{course.tuition_cents}))offer(6,"study",person.id+"/"+desired,70,ManageTalent{EnrollSkill{person.id,desired,"company"}});
            }
        }
        for(const auto& project:talents.software) {
            const auto& work=find(s.capacity->projects,project.project);
            if(work.status==ProjectStatus::completed && !project.released_on)offer(9,"software_release",project.project,100,ManageTalent{ReleaseSoftware{project.id}});
        }
        if(talents.software.empty()) {
            SoftwareDesign design;design.name=definition.name+" Workspace";design.brief="chat";
            for(const auto& employee:s.management->people->employees)if(person_has_skill(s,employee.terms.profile.id,"programming") && software_worker_blocker(s,employee.terms.profile.id).empty()){design.workers={employee.terms.profile.id};break;}
            const auto quote=software_quote(s,design);
            if(quote.blocker.empty() && affordable(quote.fee))offer(8,"software",design.brief,75,ManageTalent{StartSoftware{design}});
        }
    }
    if (staff==0) for (const auto& specialist:s.management->catalog.specialists)
        if (affordable(specialist.signing_cost,specialist.monthly_salary) && std::none_of(s.management->specialists.begin(),s.management->specialists.end(),[&](const auto& row){return row.offer_id==specialist.id;}))
            offer(6,"hire",specialist.id,120,HireSpecialist{specialist.id});
    for (const auto& debt:s.accounts->debts) if (debt.principal.value()>0 && advance_days(s.date,review_interval)>=debt.due && affordable(debt.principal))
        offer(7,"repay",debt.id,900,RepayDebt{debt.id,debt.principal,"rival.repay/"+debt.id});
    if (rival.parent_company.empty() && usable<protected_runway) for (const auto& financing:s.management->catalog.financing)
        if (std::none_of(s.management->financing.begin(),s.management->financing.end(),[&](const auto& row){return row.offer_id==financing.id;}))
            offer(7,"finance",financing.id,800,TakeFinancing{financing.id});
    bool independent_supply_needed=false;
    if (s.frontier && s.date>=s.frontier->catalog.branches_after) {
        const auto& future=*s.frontier;
        const auto independently_measured=[&](const DeploymentRecord& release) {
            const auto& evidence=find(lab.evaluations,release.evidence_id);
            return find(lab.catalog.evaluations,evidence.plan_id).independent;
        };
        for (std::size_t i=0; i<s.deployments->records.size(); ++i) {
            const auto& release=s.deployments->records[i];
            const auto& model=find(lab.models,release.model_id);
            if (find(s.capacity->projects,release.project_id).status!=ProjectStatus::active ||
                !independently_measured(release) || std::ranges::none_of(future.catalog.recipes,[&](const auto& row){return row.id==model.recipe_id;}) ||
                std::ranges::any_of(future.systems,[&](const auto& system){return std::ranges::find(system.deployments,i+1)!=system.deployments.end();}))
                continue;
            offer(3,"frontier_system",release.service_id,510,RegisterFrontierSystem{"Frontier service assessment",{release.service_id}});
        }
        for (const auto& system:future.systems) {
            const auto status=frontier_qualification(future,system.id);
            const auto try_trial=[&](FrontierTrialKind kind,std::string_view protocol) {
                if (!can_work) return;
                for (const auto& supply:s.capacity->sources) {
                    const auto rate=std::min(company_capacity(supply).value(),allocation.daily_compute.value());
                    if (rate<=0) continue;
                    StartFrontierTrial request{system.id,kind,std::string(protocol),{supply.id,MilliCUPerDay{rate},allocation.staff}};
                    try {
                        const auto quote=quote_frontier_trial(s,request,Cents{0});
                        if (affordable(quote.fee)) {
                            offer(3,"frontier_trial",std::string(protocol),520,std::move(request));
                            return;
                        }
                    } catch (const std::invalid_argument&) {
                        // A future window, unavailable supply or failed evidence
                        // leaves the firm free to take another feasible action.
                    }
                }
            };
            if (status.assessments<3) {
                for (const auto& panel:future.catalog.panels) try_trial(FrontierTrialKind::assessment,panel.id);
            } else if (future.catalog.asi_route && !status.asi_on) {
                for (const auto& experiment:future.catalog.experiments) {
                    try_trial(FrontierTrialKind::experiment,experiment.id);
                    try_trial(FrontierTrialKind::replication,experiment.id);
                }
                if (std::ranges::any_of(future.trials,[&](const auto& trial){
                    return trial.system==system.id && trial.kind==FrontierTrialKind::experiment && trial.result && trial.result->valid;
                }) && std::ranges::count_if(s.capacity->sources,[](const auto& supply){return company_capacity(supply).value()>0;})<2)
                    independent_supply_needed=true;
            }
        }
    }
    if ((independent_supply_needed || compute.value()<definition.training_rate.value()/2) && s.business_offers) for (const auto& growth:s.business_offers->catalog)
        if (affordable(growth.setup_cost,growth.monthly_amount) && business_offer_eligible(*s.business_offers,growth,*s.accounts,*s.capacity,s.market,s.date,Cents{0}))
            offer(8,"expand",growth.id,independent_supply_needed?540:100,AcceptBusinessOffer{growth.id});
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
    SimulationState result;result.date=world.campaign_start;result.end_date=campaign_calendar_limit(world);result.random=RandomStreams{rival_seed(world,definition)}.snapshot();return result;
}
SimulationState opening_rival(const WorldState& world,const CompiledProgram& program,const content::RivalDefinition& definition,OrdinalDay day) {
    const auto& authored=scenario(world,program);SimulationState state;
    std::string headquarters_market;
    if(authored.management && authored.management->people) {
        const auto& markets=authored.management->people->industry_headquarters;
        if(const auto market=markets.find(definition.id);market!=markets.end())headquarters_market=market->second;
    }
    state.date=day;state.end_date=campaign_calendar_limit(world);state.cash=definition.cash;state.random=RandomStreams{rival_seed(world,definition)}.snapshot();
    state.accounts=opening_accounts(definition.cash,day,definition.id);
    add_recurring(*state.accounts,{headquarters_market.empty()?"payroll":"payroll_with_benefits",definition.monthly_payroll,day,std::nullopt,"Rival payroll"});
    add_recurring(*state.accounts,{"compute_lease",definition.monthly_lease,day,std::nullopt,"Rival compute lease"});
    state.capacity=opening_capacity({{"lease.initial","cloud/"+definition.id,definition.id,"supplier.rival",CapacitySourceKind::leased,definition.capacity,definition.capacity,0,"general",true}},definition.staff,day,definition.id);
    state.liquidity=opening_liquidity(day,definition.id);
    LabState lab;lab.owner=definition.id;lab.starts_on=day;lab.catalog=world.simulation.lab->catalog;
    lab.research_stream_origin=state.random[static_cast<std::size_t>(RandomStream::research)];state.lab=std::move(lab);
    state.market=opening_market(day,definition.id);
    state.market->segments=content::scenario_market_setup(program.collection,authored).segments;
    for (auto& segment:state.market->segments) segment.preferences.clear();
    state.deployments=DeploymentState{content::scenario_services(program.collection,authored),{}};
    initialize_deployment_risk(state);
    if (world.simulation.frontier)
        state.frontier=opening_frontier(world.simulation.frontier->catalog,world.simulation.frontier->seed);
    state.management=ManagementState{*authored.management,{},{},{},{},{},{}};state.management->catalog.setbacks.clear();
    // Only explicitly authored major HQs receive individual simulation.
    if(headquarters_market.empty())state.management->catalog.people.reset();
    if (authored.management->staff_workload) state.capacity->workload=StaffWorkload{*authored.management->staff_workload};
    if (authored.management->corporate_tax) state.accounts->taxes=CorporateTaxes{*authored.management->corporate_tax,day};
    state.management->catalog.governance.reset();
    state.management->catalog.scoring.founder="founder";
    if(state.management->catalog.succession) state.management->catalog.succession->founder_name=definition.planning->leader;
    initialize_succession(state);
    if (definition.response) {state.management->catalog.financing.clear();state.capacity->sources.front().effective=MilliCUPerDay{0};}
    if (!definition.response && world.competition->setup->catchup_cash.value()>0) state.management->catalog.financing.push_back({"finance.disclosed_catchup","Disclosed finite catch-up equity",world.competition->setup->catchup_cash,world.competition->setup->catchup_acquired,BasisPoints{0},Days{0}});
    BusinessOffersState growth;
    for (auto offer:authored.business_offers) if (offer.kind==BusinessOfferKind::compute_lease) {offer.id+="."+definition.id;growth.catalog.push_back(std::move(offer));}
    if (!growth.catalog.empty()) state.business_offers=std::move(growth);
    if(!headquarters_market.empty()) {
        if(!state.management->catalog.people || !authored.infrastructure || !authored.infrastructure->workplace || !authored.infrastructure->workplace->headquarters)
            invalid("A detailed industry HQ needs the shared people and property rules.");
        InfrastructureState infrastructure;infrastructure.catalog=*authored.infrastructure;
        auto& markets=infrastructure.catalog.workplace->headquarters->markets;
        const auto selected=std::ranges::find(markets,headquarters_market,&HQMarket::id);
        if(selected==markets.end())invalid("The major company has an unknown headquarters market.");
        infrastructure.executive_base=selected->state;
        // The ordinary opening helper picks the first market in this state.
        std::rotate(markets.begin(),selected,selected+1);
        state.infrastructure=std::move(infrastructure);initialize_workplace(state,"leased",false);
        initialize_people(state,rival_seed(world,definition),"","",false,0,"research");
        initialize_headquarters(state,rival_seed(world,definition));
        auto& premises=state.infrastructure->workplace->headquarters->premises.front();
        const auto& market=find(state.infrastructure->catalog.workplace->headquarters->markets,premises.market);
        premises.rent=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(market.rent_per_cell.value()),static_cast<std::uint64_t>(premises.width)*static_cast<std::uint64_t>(premises.depth),1)};
        add_recurring(*state.accounts,{"office_and_admin",premises.rent,day,{},"HQ lease: "+market.city});
        initialize_departments(state);initialize_access(state,false);
    }
    return Simulation{std::move(state)}.checkpoint();
}
RivalObservation public_observation(const WorldState& world,OrdinalDay day,std::uint64_t draw) {return observe(world,day,draw);}
bool submit_rival(RivalCompany& rival,CommandAction action) {return submit(rival,std::move(action));}
void review_rival(WorldState& world,const CompiledProgram& program,RivalCompany& rival) {
    auto& plan=*rival.planning;if (!plan.entered_on || !active(rival.simulation)) return;
    acknowledge_leadership(rival);
    if (rival.simulation.date<plan.next_review) return;
    if (rival.response && rival.response->phase!=HeavyweightPhase::compete) return;
    if (plan.decisions.size()>=rival_decision_limit) invalid("Rival strategy decision history exceeds its retention bound");
    const auto& definition=rival_definition(program,rival.definition_id);diffuse(world,rival);
    const auto stream_index=static_cast<std::size_t>(RandomStream::rivals);const auto before=rival.simulation.random[stream_index];
    SplitMix64 stream{before};const auto draw=stream.next();rival.simulation.random[stream_index]=stream.state();
    const auto observation=observe(world,rival.simulation.date,draw);
    auto actions=candidates(rival,definition,observation,draw,world.simulation.civic ? &*world.simulation.civic : nullptr,
        world.society?&*world.society:nullptr,world.competition->setup->review_interval);
    RivalDecision decision{rival.simulation.date,"wait","","Conserve runway; no feasible action met the company's resource and reserve limits.",false,before,stream.state(),observation};
    if (!actions.empty()) {
        auto selected=std::move(actions.front());decision.action=std::move(selected.action);decision.subject=std::move(selected.subject);
        decision.accepted=submit(rival,std::move(selected.command),&decision.explanation);
        rival.attempted=true;
    }
    plan.decisions.push_back(std::move(decision));plan.next_review=advance_days(plan.next_review,world.competition->setup->review_interval);
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
    const auto day=campaign_day(world);
    acknowledge_leadership(rival);
    while (const auto* alert=required_deployment_incident(rival.simulation)) {
        if (!submit(rival,AcknowledgeDeploymentIncident{alert->id})) invalid("Rival incident notice could not be acknowledged");
    }
    if (rival.simulation.deployments && rival.simulation.deployments->risk &&
        !rival.simulation.deployments->risk->open_incidents.empty()) {
        // Use the same paid command, free allocation and authored cash buffer
        // as other rival work. An unfunded response remains contained.
        const auto& state=rival.simulation;
        const auto& risk=*state.deployments->risk;
        const auto report=capacity_report(*state.capacity);
        const auto floor=rival_definition(program,rival.definition_id).planning->reserve_floor;
        if (report.available_staff>report.assigned_staff) {
            for (const auto index:risk.open_incidents) {
                const auto& incident=risk.incidents.at(static_cast<std::size_t>(index));
                if (incident.last_response) continue;
                bool started=false;
                for (const auto& source:state.capacity->sources) {
                    std::int64_t used=0;
                    for (const auto& project:state.capacity->projects)
                        if (project.source_id==source.id && project.status==ProjectStatus::active)
                            used=checked_add(used,project.requested.value());
                    const auto spare=company_capacity(source).value()-used;
                    if (spare<=0) continue;
                    RespondDeploymentIncident request{incident.id,source.id,MilliCUPerDay{std::min<std::int64_t>(spare,20000)},
                        std::min<std::uint32_t>(report.available_staff-report.assigned_staff,2)};
                    const auto quote=preview_deployment_response(state,request);
                    if (discretionary_cash(state)>=checked_unit_add(floor,quote.cash_required))
                        started=submit(rival,std::move(request));
                    if (started) break;
                }
                if (started) break;
            }
        }
    }
    std::vector<std::string> workload_alerts;
    for (const auto& row:rival.simulation.management->notices)
        if ((row.kind==service_workload_notice || row.kind==service_monitoring_limit_notice) &&
            row.required && !row.acknowledged) workload_alerts.push_back(row.id);
    for (const auto& id:workload_alerts)
        if (!submit(rival,AcknowledgeManagementNotice{id})) invalid("Rival service warning was not acknowledged");
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
            next.market_allocation=it->second;next.industry_adoptions=industry_adoption_snapshot(world,prior);
        }
        next.market_influences=regional_market_conditions(world,rival.simulation,prior);
        advance(rival,std::move(next));
        if (rival.simulation.phase==DayPhase::closed) world.competition->notices.push_back({rival.definition_id,"closure",rival.simulation.date,std::nullopt});
    }
    refresh_wholesale(world,program);
    collect_public_results(world,prior);append_industry_adoption(world,prior);enter_due(world,program);
    update_competitive_offers(world,program);update_heavyweight_responses(world,program);
    for (auto& rival:world.competition->companies) review_rival(world,program,rival);
    collect_public_results(world,prior);
}
}
