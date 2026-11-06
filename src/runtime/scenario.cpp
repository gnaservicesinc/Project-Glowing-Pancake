#include <pgp/runtime/scenario.hpp>

#include <algorithm>
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
    const auto opening=content::scenario_opening(*scenario,options.background);
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
    world.enabled_flows = scenario->flows;
    world.campaign_start = scenario->starts_on;
    world.scope_created = scenario->starts_on;
    world.simulation.date = scenario->starts_on;
    world.simulation.end_date = scenario->ends_before;
    world.simulation.random = core::RandomStreams{seed}.snapshot();
    auto accounts = core::opening_accounts(opening.cash, scenario->starts_on, world.company_id);
    accounts.holdings.clear();
    for (const auto& [holder, shares] : scenario->ownership)
        accounts.holdings.push_back({holder, shares, holder == "option_pool"});
    for (const auto& [id, monthly] : opening.monthly_commitments) {
        if (monthly.value() > 0)
            core::add_recurring(accounts, {id, monthly, scenario->starts_on, std::nullopt, id});
    }
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
    if (scenario->management) world.simulation.management=core::ManagementState{*scenario->management,{},{},{},{},{},{}};
    if (scenario->infrastructure) {core::InfrastructureState owned;owned.catalog=*scenario->infrastructure;owned.executive_base=std::string(headquarters);world.simulation.infrastructure=std::move(owned);}
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
    if (scenario->management && scenario->management->governance) {
        core::GovernanceState corporate;corporate.opening_holdings=world.simulation.accounts->holdings;
        world.simulation.management->governance=std::move(corporate);
    }
    if(!options.charter.empty()) core::initialize_charter(world.simulation,options.charter);
    if (scenario->service_catalog) world.simulation.deployments=core::DeploymentState{content::scenario_services(program.collection,*scenario),{}};
    if (const auto* catalog=content::frontier_catalog(program.collection,*scenario))
        world.simulation.frontier=core::opening_frontier(*catalog,seed);
    core::initialize_succession(world.simulation, world.identity ? std::optional{world.identity->founder_name} : std::nullopt);
    if (!opening.business_offers.empty()) world.simulation.business_offers=core::BusinessOffersState{opening.business_offers,{}};
    world.facts.at("company.has_compute_lease") = !opening.capacity_sources.empty();
    if (const auto lease = scenario->monthly_commitments.find("compute_lease");
        lease != scenario->monthly_commitments.end())
        world.facts.at("company.monthly_compute_bill_cents") = core::checked_add(lease->second.value(),opening.monthly_commitments.contains("founder_compute")?opening.monthly_commitments.at("founder_compute").value():0);
    world.facts.at("company.is_operating") = true;
    if (headquarters == "US-CA") world.state_id = "state.us_ca";
    else if (headquarters == "US-TX") world.state_id = "state.us_tx";
    else if (headquarters == "US-WA") world.state_id = "state.us_wa";
    else throw std::invalid_argument("Headquarters has no implemented scope binding");
    initialize_competition(world,program,scenario->rivals,scenario->competition);
    if(scenario->objectives) initialize_objectives(world,*scenario->objectives,std::move(objectives));
    else if(objectives) throw std::invalid_argument("This scenario uses its own introductory ending.");
    validate_checkpoint(program, checkpoint);
    return checkpoint;
}
} // namespace pgp::runtime
