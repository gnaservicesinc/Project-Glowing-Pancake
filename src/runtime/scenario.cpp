#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/family.hpp>
#include <pgp/runtime/heavyweight.hpp>
#include <pgp/runtime/objectives.hpp>
#include <pgp/runtime/romance.hpp>
#include <pgp/runtime/society.hpp>
#include "competition_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace pgp::runtime {
Checkpoint begin_scenario(const CompiledProgram& program, std::string_view scenario_id,
                           std::string_view headquarters,std::optional<core::ObjectiveSelection> objectives,
                           ScenarioOptions options) {
    const auto* definition = program.collection.find(scenario_id);
    if (!definition) throw std::invalid_argument("Scenario definition does not exist");
    const auto* scenario = std::get_if<content::ScenarioDefinition>(&definition->data);
    if (!scenario) throw std::invalid_argument("Selected definition is not a scenario");
    if (options.identity) validate_campaign_identity(*options.identity);
    const auto seed = options.seed.value_or(scenario->seed);
    const auto founding=options.founding?&*options.founding:nullptr;
    if (founding && founding->mode != "campaign")
        throw std::invalid_argument("New games start with the founder campaign.");
    const auto opening=content::scenario_opening(*scenario,options.background,"company.player",options.difficulty,founding);
    const auto management=content::scenario_management(*scenario,options.difficulty,founding);
    const auto* partner=founding && scenario->founding?content::founding_partner(*scenario->founding,founding->partner):nullptr;
    if(partner) {
        const auto mission=*core::objective_kind(partner->objective);
        if(objectives && objectives->primary!=mission) throw std::invalid_argument("The signed cofounder offer sets the opening company mission.");
        if(!objectives) objectives=core::ObjectiveSelection{mission,{}};
    }
    if(founding && founding->legal_form=="nonprofit" && objectives &&
        (objectives->primary==core::ObjectiveKind::profit || std::find(objectives->secondary.begin(),objectives->secondary.end(),core::ObjectiveKind::profit)!=objectives->secondary.end()))
        throw std::invalid_argument("Nonprofit assets belong to the institution; choose a mission other than founder wealth.");
    const auto competition=content::scenario_competition(*scenario,options.difficulty);
    if (std::find(scenario->headquarters_choices.begin(), scenario->headquarters_choices.end(), headquarters) ==
        scenario->headquarters_choices.end())
        throw std::invalid_argument("Headquarters is not offered by this scenario");

    Checkpoint checkpoint;
    auto& world = checkpoint.world;
    world = foundation_world(opening.cash);
    world.scenario_id=std::string(scenario_id);
    world.root_seed = seed;
    world.identity = std::move(options.identity);
    world.background = std::move(options.background);
    world.difficulty = std::move(options.difficulty);
    world.founding = std::move(options.founding);
    world.enabled_flows = scenario->flows;
    world.campaign_start = scenario->starts_on;
    world.scope_created = opening.launch_day;
    world.simulation.date = scenario->starts_on;
    world.simulation.end_date = scenario->ends_before;
    world.simulation.random = core::RandomStreams{seed}.snapshot();
    auto accounts = core::opening_accounts(opening.cash, scenario->starts_on, world.company_id);
    accounts.holdings.clear();
    for (const auto& [holder, shares] : content::scenario_ownership(*scenario,world.founding?&*world.founding:nullptr))
        accounts.holdings.push_back({holder, shares, holder == "option_pool"});
    world.simulation.cash=core::accounts_report(accounts).cash;
    if (management && management->corporate_tax)
        accounts.taxes=core::CorporateTaxes{*management->corporate_tax,accounts.starts_on};
    core::validate_accounts(accounts);
    world.simulation.accounts = std::move(accounts);
    world.simulation.liquidity = core::opening_liquidity(scenario->starts_on,world.company_id);
    world.simulation.capacity = core::opening_capacity(opening.capacity_sources,opening.team_people,
        scenario->starts_on,world.company_id);
    if (scenario->lab) {
        core::LabState lab;
        lab.owner = world.company_id; lab.starts_on = scenario->starts_on;
        lab.catalog = content::scenario_lab_catalog(program.collection, *scenario);
        lab.research_stream_origin = world.simulation.random[static_cast<std::size_t>(core::RandomStream::research)];
        world.simulation.lab = std::move(lab);
    }
    if (scenario->market) {
        world.simulation.market = core::opening_market(scenario->starts_on,world.company_id);
        content::apply_market_setup(content::scenario_market_setup(program.collection,*scenario),*world.simulation.market,*world.simulation.accounts,
            *world.simulation.capacity,scenario->starts_on);
    }
    if (management) {
        world.simulation.management=core::ManagementState{*management,{},{},{},{},{},{}};
        if(management->staff_workload) world.simulation.capacity->workload=core::StaffWorkload{*management->staff_workload};
    }
    if (scenario->infrastructure) {
        core::InfrastructureState owned;owned.catalog=*scenario->infrastructure;owned.executive_base=std::string(headquarters);world.simulation.infrastructure=std::move(owned);
    }
    if (const auto* catalog=content::resilience_catalog(program.collection,*scenario)) {
        core::ResilienceState risks;risks.catalog=*catalog;risks.insurer_cash=catalog->insurer_capital;
        world.simulation.resilience=std::move(risks);
    }
    if (const auto* catalog = content::civic_catalog(program.collection, *scenario)) {
        world.simulation.civic = core::opening_civic(*catalog, scenario->starts_on,
            world.simulation.random[static_cast<std::size_t>(core::RandomStream::politics)]);
        core::prepare_civic(world.simulation);
    }
    if (const auto* catalog = content::society_catalog(program.collection, *scenario)) {
        world.society = core::opening_society(*catalog, scenario->starts_on);
        core::prepare_society(*world.society, scenario->starts_on);
    }
    if(const auto* catalog=content::authority_catalog(program.collection,*scenario)) {
        core::AuthorityState institutions;institutions.catalog=*catalog;institutions.starts_on=scenario->starts_on;
        institutions.prepared_on=scenario->starts_on;world.authority=std::move(institutions);
    }
    if (management && management->governance) {
        core::GovernanceState corporate;corporate.opening_holdings=world.simulation.accounts->holdings;
        world.simulation.management->governance=std::move(corporate);
    }
    if (scenario->service_catalog) world.simulation.deployments=core::DeploymentState{content::scenario_services(program.collection,*scenario),{}};
    core::initialize_deployment_risk(world.simulation);
    if (const auto* catalog=content::frontier_catalog(program.collection,*scenario))
        world.simulation.frontier=core::opening_frontier(*catalog,seed);
    if (!opening.business_offers.empty()) world.simulation.business_offers=core::BusinessOffersState{opening.business_offers,{}};
    world.facts.at("company.has_compute_lease") = !opening.capacity_sources.empty();
    if (const auto lease = opening.monthly_commitments.find("compute_lease");
        lease != opening.monthly_commitments.end())
        world.facts.at("company.monthly_compute_bill_cents") = core::checked_add(lease->second.value(),opening.monthly_commitments.contains("founder_compute")?opening.monthly_commitments.at("founder_compute").value():0);
    world.facts.at("company.is_operating") = true;
    if (headquarters == "US-CA") world.state_id = "state.us_ca";
    else if (headquarters == "US-TX") world.state_id = "state.us_tx";
    else if (headquarters == "US-WA") world.state_id = "state.us_wa";
    else throw std::invalid_argument("Headquarters has no implemented scope binding");
    if(scenario->counties) world.counties=core::initialize_counties(*scenario->counties,world.simulation.date,seed);
    initialize_competition(world,program,scenario->rivals,competition);
    std::optional<core::PeopleRules> delayed_people;
    std::optional<core::SuccessionCatalog> delayed_succession;
    std::optional<core::WorkplaceRules> delayed_workplace;
    if (world.simulation.management && world.simulation.date < opening.launch_day) {
        delayed_people = std::move(world.simulation.management->catalog.people);
        world.simulation.management->catalog.people.reset();
        delayed_succession = std::move(world.simulation.management->catalog.succession);
        world.simulation.management->catalog.succession.reset();
    }
    if (world.simulation.infrastructure && world.simulation.date < opening.launch_day) {
        delayed_workplace = std::move(world.simulation.infrastructure->catalog.workplace);
        world.simulation.infrastructure->catalog.workplace.reset();
    }
    while (world.simulation.date < opening.launch_day) {
        const auto prior_day = world.simulation.date;
        competition_detail::refresh_wholesale(world, program, true);
        const auto assignments = prepare_competition_day(world, program);
        core::AdvanceDay day;
        day.wholesale_work = competition_detail::settle_wholesale(world, program, assignments);
        day.market_influences = regional_market_conditions(world, world.simulation, prior_day);
        if (const auto it = assignments.find(world.company_id); it != assignments.end()) {
            day.market_allocation = it->second;
            day.industry_adoptions = industry_adoption_snapshot(world);
        }
        core::Simulation sim{std::move(world.simulation), core::TrustedSimulationState{}};
        const auto outcome = sim.submit_in_transaction(core::Command{sim.checkpoint().revision, std::move(day)});
        if (!outcome.accepted()) throw std::invalid_argument("Failed to advance timeline to entry date: " + outcome.explanation);
        world.simulation = std::move(sim).release_checkpoint();
        core::prepare_governance(world.simulation);
        advance_competition(world, program, prior_day, assignments);
        advance_society(world, prior_day);
        (void)prepare_authority(world, prior_day);
        update_competitive_offers(world, program);
        update_heavyweight_responses(world, program);
        if (world.counties && scenario->counties) {
            int regional = 0, global = 0;
            if (world.society && !world.society->months.empty()) {
                const auto& outcomes = world.society->months.back().components;
                const auto& baselines = world.society->catalog.regions;
                for (std::size_t i = 0; i < outcomes.size(); ++i) {
                    const auto current = core::welfare_indices(outcomes[i]);
                    const auto base = core::welfare_indices(baselines[i].initial);
                    int delta = 0;
                    for (std::size_t k = 0; k < current.size(); ++k) delta += static_cast<int>(current[k]) - static_cast<int>(base[k]);
                    delta /= static_cast<int>(current.size() * 100);
                    if (baselines[i].id == "region.us") regional = delta;
                    global += delta;
                }
                global /= static_cast<int>(outcomes.size());
            }
            core::advance_counties(*world.counties, *scenario->counties, campaign_day(world), regional, global);
        }
    }
    if (delayed_people && world.simulation.management) {
        world.simulation.management->catalog.people = std::move(*delayed_people);
    }
    if (delayed_succession && world.simulation.management) {
        world.simulation.management->catalog.succession = std::move(*delayed_succession);
    }
    if (delayed_workplace && world.simulation.infrastructure) {
        world.simulation.infrastructure->catalog.workplace = std::move(*delayed_workplace);
    }
    {
        auto& accounts = *world.simulation.accounts;
        if (opening.startup_debt) {
            const auto& lender = scenario->founding->startup_loan->lender;
            core::draw_debt(accounts, opening.launch_day, *opening.startup_debt, "Startup loan from " + lender);
        }
        if (opening.student_debt) {
            core::draw_debt(accounts, opening.launch_day, *opening.student_debt, "University student loans");
            core::record_cash(accounts, opening.launch_day, core::Cents{-opening.student_debt->principal.value()},
                core::Account::expenses, "University tuition covered by student loans", "founding/student_tuition");
        }
        if (opening.setup_cost.value() > 0) {
            core::record_cash(accounts, opening.launch_day, core::Cents{-opening.setup_cost.value()}, core::Account::expenses,
                "Formation, office deposit and first researcher signing costs", "founding/setup");
        }
        world.simulation.cash = core::accounts_report(accounts).cash;
        for (const auto& [id, monthly] : opening.monthly_commitments) {
            if (monthly.value() > 0)
                core::add_recurring(accounts, {id, monthly, opening.launch_day, std::nullopt, id});
        }
    }
    if (world.simulation.infrastructure) {
        core::initialize_workplace(world.simulation, world.founding ? world.founding->office : "leased",
            world.founding && world.founding->researcher != "founder");
    }
    if (!options.charter.empty()) core::initialize_charter(world.simulation, options.charter);
    core::initialize_succession(world.simulation, world.identity ? std::optional{world.identity->founder_name} : std::nullopt);
    const auto terms = (world.founding && scenario->founding)
        ? std::optional{content::founding_terms(*scenario->founding, *world.founding)}
        : std::nullopt;
    if (world.simulation.lab && terms) {
        world.simulation.lab->research_work_scale_bp = core::BasisPoints{terms->research_work_scale_bp};
    }
    std::uint32_t founding_prestige=world.founding && world.founding->has_advantage("genius")?3U:0U;
    std::string recruiting_mission="research";
    if(world.founding && management && management->people && management->people->mood) {
        founding_prestige=std::min(100U, terms->prestige);
        if(world.founding->legal_form=="nonprofit" || (objectives && objectives->primary==core::ObjectiveKind::utopia))
            recruiting_mission="public benefit";
        else if(objectives && (objectives->primary==core::ObjectiveKind::profit || objectives->primary==core::ObjectiveKind::monopoly))
            recruiting_mission="commercial";
    }
    core::initialize_people(world.simulation,seed,world.identity?world.identity->founder_name:std::string{},world.identity?world.identity->founder_pronouns:std::string{},
        terms && terms->founder_researcher,founding_prestige,std::move(recruiting_mission));
    if(world.founding && scenario->founding && world.simulation.management && world.simulation.management->people && content::queer_founder(*world.founding)) {
        auto& people=*world.simulation.management->people;
        people.community_prestige=scenario->founding->queer_community_prestige;
        people.community_buzz=scenario->founding->queer_community_buzz;
    }
    if(world.founding && world.simulation.management && world.simulation.management->people &&
        !world.simulation.management->catalog.people->founder_audiences.empty())
        world.simulation.management->people->founder_gender=world.founding->gender;
    if(!options.opening_contacts.empty()) {
        if(!world.founding || !world.simulation.management || !world.simulation.management->people)
            throw std::invalid_argument("Opening conversations require an authored founder campaign with contacts.");
        for(auto& contact:options.opening_contacts) {
            if(!contact.opening || !contact.messages.empty())
                throw std::invalid_argument("Start with pre-company conversations and no company replies.");
            contact.exchanged_on=opening.launch_day;
        }
        world.simulation.management->people->contacts=std::move(options.opening_contacts);
    }
    if(partner && world.simulation.management && world.simulation.management->people) {
        auto& contacts=world.simulation.management->people->contacts;
        const auto id=std::string{core::opening_contact_prefix}+partner->id;
        if(std::ranges::none_of(contacts,[&](const auto& row){return row.person==id;}))
            contacts.push_back({id,opening.launch_day,{},core::OpeningConversation{"partnership","signed"}});
    }
    const auto push_unique_trait=[](std::vector<std::string>& traits,const std::string& t) {
        if(traits.size()<core::person_trait_limit && std::ranges::find(traits,t)==traits.end()) traits.push_back(t);
    };
    const auto build_founder_traits=[&](const content::FoundingSelection& f) {
        std::vector<std::string> traits={"Social: "+f.sociability,"Activity: "+f.activity,"Temperament: "+f.disposition,"Upbringing: "+f.family};
        // Reserve headline doctoral status ahead of formative flavor traits so extended school histories cannot truncate it at person_trait_limit.
        if(terms && terms->doctorate_count>0) push_unique_trait(traits,"Title: Dr.");
        if(scenario->founding) for(const auto& trait:content::formative_traits(*scenario->founding,f)) push_unique_trait(traits,trait);
        return traits;
    };
    if(world.founding && world.simulation.management && world.simulation.management->people) {
        for(auto& employee:world.simulation.management->people->employees) {
            auto& p=employee.terms.profile;
            if(p.role=="Founder / researcher") {
                p.gender=world.founding->gender;p.portrait=world.founding->appearance;
                if(!world.simulation.management->catalog.people->trait_groups.empty()) p.traits=build_founder_traits(*world.founding);
                if(terms && terms->doctorate_count>0) {
                    if(!terms->education_label.empty()) p.education=terms->education_label;
                    if(terms->research_bonus>0) p.research=std::min<std::uint32_t>(100U,p.research+terms->research_bonus);
                    p.bio=(world.founding->has_advantage("genius")?"Actual Genius with doctoral mastery: ":"Doctoral researcher: ")+
                        (terms->education_label.empty()?std::string{"advanced AI research"}:terms->education_label)+".";
                } else {
                    if(terms && !terms->education_label.empty()) p.education=terms->education_label;
                    p.bio="Actual Genius: exceptional technical ability developed through the founder's formative years.";
                }
            } else if(world.founding->route=="partnership") {
                p.role="Cofounder / researcher";
                if(partner) {
                    p.name=partner->name;p.pronouns=partner->pronouns;p.gender=partner->gender;
                    p.bio=partner->letter;p.traits=partner->traits;p.preferred_mission=world.simulation.management->people->mission;
                    p.portrait=core::make_portrait(19972015,"cofounder/"+partner->id);
                    auto& payroll=world.simulation.accounts->recurring;
                    const auto salary=std::ranges::find(payroll,p.id,&core::RecurringObligation::id);
                    if(salary!=payroll.end()) salary->cause="Employee salary: "+p.name;
                }
            }
        }
    }
    if(world.founding && scenario->founding && core::talent_rules(world.simulation)) {
        auto& people=*world.simulation.management->people;auto& talent=*people.talent;
        talent.founder_skills=content::formative_skills(*scenario->founding,*world.founding);talent.founder_genius=world.founding->has_advantage("genius");
        if(terms && terms->doctorate_count>0) {
            const auto& catalog_skills=world.simulation.management->catalog.people->talent->skills;
            if(std::ranges::any_of(catalog_skills,[](const auto& sk){return sk.id=="ml_research";}) &&
               std::ranges::find(talent.founder_skills,"ml_research")==talent.founder_skills.end())
                talent.founder_skills.push_back("ml_research");
        }
        for(auto& e:people.employees)if(e.terms.profile.role=="Founder / researcher") {e.terms.profile.skills=talent.founder_skills;e.terms.profile.orientation=world.founding->orientation;}
        else if(e.terms.profile.role=="Cofounder / researcher" && !core::has_skill(e.terms.profile,"ml_research"))e.terms.profile.skills.push_back("ml_research");
    }
    if(world.simulation.management && world.simulation.management->people && world.simulation.management->catalog.people->romance)
        for(auto& e:world.simulation.management->people->employees)if(e.terms.profile.role=="Founder / researcher" || e.terms.profile.role=="Cofounder / researcher")
            core::initialize_romantic_profile(e.terms.profile,seed,*world.simulation.management->catalog.people->romance);
    if(world.founding && scenario->founding && core::social_rules(world.simulation)) {
        world.simulation.management->people->social->founder_traits=build_founder_traits(*world.founding);
    }
    if(world.simulation.lab) {
        world.simulation.lab->setback_risk_scale_bp=core::founder_risk_scale_bp(world.simulation);
    }
    core::initialize_headquarters(world.simulation,seed,world.founding?world.founding->family:"");
    core::initialize_departments(world.simulation);
    core::initialize_access(world.simulation,world.founding && world.founding->has_advantage("wealthy"));
    synchronize_industry_people(world,program);
    if(terms && core::social_rules(world.simulation) && !world.simulation.management->people->industry.empty()) {
        auto& people=*world.simulation.management->people;
        auto& social=*people.social;
        const auto is_industry_leader=[&](const std::string& id,const core::IndustryPerson& peer) {
            return id.ends_with("/employee/1") || peer.profile.role.find("CEO")!=std::string::npos ||
                peer.profile.role.find("Founder")!=std::string::npos;
        };
        if(terms->best_friends>0) {
            std::vector<std::string> leaders,others;
            for(const auto& [id,peer]:people.industry) {
                if(!peer.active) continue;
                (is_industry_leader(id,peer)?leaders:others).push_back(id);
            }
            core::SplitMix64 friend_seed{seed ^ UINT64_C(0x465249454E445331)};
            const auto shuffle_ids=[&](std::vector<std::string>& bucket) {
                for(std::size_t i=bucket.size();i>1;--i) std::swap(bucket[i-1],bucket[static_cast<std::size_t>(friend_seed.bounded(i))]);
            };
            shuffle_ids(leaders);shuffle_ids(others);
            leaders.insert(leaders.end(),others.begin(),others.end());
            std::uint32_t added=0;
            for(const auto& id:leaders) {
                if(added>=terms->best_friends) break;
                const std::string founder_id{"founder"};
                const bool founder_first=founder_id<id;
                const auto& first=founder_first?founder_id:id;
                const auto& second=founder_first?id:founder_id;
                const bool exists_link=std::ranges::any_of(social.links,[&](const auto& l){return l.first==first && l.second==second;});
                if(exists_link) continue;
                social.links.push_back({first,second,900,10,people.starts_on,world.simulation.date,std::nullopt,"Best friends since childhood",true});
                auto& disclosed=social.known_traits[id];
                for(const auto& tr:people.industry.at(id).profile.traits) {
                    if(disclosed.size()>=core::person_trait_limit) break;
                    if(std::ranges::find(disclosed,tr)==disclosed.end()) disclosed.push_back(tr);
                }
                ++added;
            }
        }
        if(terms->bully_count>0) {
            const auto already_friend=[&](const std::string& id) {
                return std::ranges::any_of(social.links,[&](const auto& l) {
                    return (l.first=="founder" && l.second==id) || (l.second=="founder" && l.first==id);
                });
            };
            std::vector<std::string> ids;
            ids.reserve(people.industry.size());
            for(const auto& [id,peer]:people.industry) if(peer.active && !already_friend(id)) ids.push_back(id);
            if(ids.empty()) for(const auto& [id,_]:people.industry) if(!already_friend(id)) ids.push_back(id);
            core::SplitMix64 bully_seed{seed ^ UINT64_C(0x42554C4C59504545)};
            for(std::size_t i=ids.size();i>1;--i) std::swap(ids[i-1],ids[static_cast<std::size_t>(bully_seed.bounded(i))]);
            std::ranges::stable_sort(ids,[&](const std::string& a,const std::string& b) {
                const auto& pa=people.industry.at(a);const auto& pb=people.industry.at(b);
                const int da=std::abs(pa.profile.birth_year-1997);
                const int db=std::abs(pb.profile.birth_year-1997);
                if(da!=db) return da<db;
                return !is_industry_leader(a,pa) && is_industry_leader(b,pb);
            });
            const auto limit=std::min<std::size_t>({terms->bully_count,ids.size(),64});
            for(std::size_t i=0;i<limit;++i) social.hostile_peers.push_back(ids[i]);
        }
    }
    initialize_romance(world);
    initialize_family(world);
    if(world.family && opening.launch_day>world.campaign_start) {
        for(std::size_t i=0;i<std::min<std::size_t>(2,world.family->members.size());++i) {
            auto member=world.family->members[i];
            member.joined_on=world.campaign_start;
            member.profile.applied_on=world.campaign_start;
            world.family->members.replace(i,std::move(member));
        }
    }
    if(scenario->objectives) initialize_objectives(world,*scenario->objectives,std::move(objectives));
    else if(objectives) throw std::invalid_argument("This scenario uses its own introductory ending.");
    validate_checkpoint(program, checkpoint);
    return checkpoint;
}
namespace {
const content::CampaignDifficulty* difficulty(const CompiledProgram& program,const WorldState& world) {
    if(world.difficulty.empty()) return nullptr;
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if(!scenario) throw std::invalid_argument("Difficulty needs an authored source scenario");
    return content::scenario_difficulty(*scenario,world.difficulty);
}
}
core::TrainingPreview preview_campaign_training(const CompiledProgram& program,const WorldState& world,const core::TrainingRequest& request) {
    if(!world.simulation.lab) throw std::invalid_argument("This company has no training laboratory");
    auto result=core::preview_training(*world.simulation.lab,request);
    if(const auto* d=difficulty(program,world)) {
        for(std::size_t i=0;i<core::lab_domain_count;++i) {
            if (result.bottlenecks[i].empty()) continue;
            result.lower[i]=core::BasisPoints{std::max(INT64_C(0),result.lower[i].value()-d->forecast_padding.value())};
            result.upper[i]=core::BasisPoints{std::min(INT64_C(10000),result.upper[i].value()+d->forecast_padding.value())};
        }
        result.explanation+=" Difficulty "+d->name+" adds "+std::to_string(d->forecast_padding.value())+" basis points of uncertainty to each side of training forecasts. Time and known bills use their ordinary rules; held-out measurements are reported separately.";
    }
    return result;
}
core::OrdinalDay scripted_rival_start(const CompiledProgram& program,const WorldState& world,const content::RivalDefinition& rival) {
    auto delay=rival.start_after;
    if(const auto* d=difficulty(program,world)) {
        const auto scaled=core::mul_div_floor(static_cast<std::uint64_t>(delay.value()),d->scripted_delay_percent,100);
        delay=core::Days{core::checked_add(static_cast<std::int64_t>(scaled.quotient),scaled.remainder>0?1:0)};
    }
    return core::advance_days(world.campaign_start,delay);
}
} // namespace pgp::runtime
