#include <pgp/runtime/runtime.hpp>
#include <pgp/runtime/society.hpp>
#include <pgp/runtime/authority.hpp>

#include "duration.hpp"
#include "committed.hpp"
#include "campaign_facts.hpp"
#include "group_bindings.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pgp::runtime {
namespace {
[[noreturn]] void invalid(std::string detail) { throw std::invalid_argument(std::move(detail)); }
bool binding_exists(const WorldState& world, const ScopeBinding& binding) {
    switch (binding.scope) {
    case content::Scope::company: return binding.id == world.company_id;
    case content::Scope::world: return binding.id == world.world_id;
    case content::Scope::state: return binding.id == world.state_id;
    case content::Scope::capacity_source: case content::Scope::project: case content::Scope::contract: return false;
    }
    return false;
}
template<class Row> const Row& related_row(const std::vector<Row>& rows, std::string_view id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [id](const auto& row) { return row.id == id; });
    if (found == rows.end()) invalid("Related entity binding does not exist");
    return *found;
}
bool business_member_scope(content::Scope scope) {
    return scope == content::Scope::capacity_source || scope == content::Scope::project || scope == content::Scope::contract;
}
void scalar_valid(const content::Scalar& value, const content::FactMetadata& metadata) {
    switch (metadata.type) {
    case content::ValueType::integer: {
        const auto* number = std::get_if<std::int64_t>(&value);
        if (!number || *number < metadata.minimum || *number > metadata.maximum)
            invalid("Fact type or unit range mismatch: " + metadata.id);
        return;
    }
    case content::ValueType::boolean:
        if (!std::holds_alternative<bool>(value)) invalid("Fact requires boolean: " + metadata.id);
        return;
    case content::ValueType::text: {
        const auto* text = std::get_if<std::string>(&value);
        if (!text || (!metadata.default_value && text->empty()) || text->size() > 4096 || text->find('\0') != std::string::npos)
            invalid("Fact requires bounded text: " + metadata.id);
        return;
    }
    case content::ValueType::reference: invalid("Reference facts have no runtime provider");
    }
    invalid("Unknown fact type");
}
const content::FlowDefinition& flow(const CompiledProgram& program, std::string_view id) {
    const auto* definition = program.collection.find(id);
    if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
        invalid("Missing runtime flow definition: " + std::string(id));
    return std::get<content::FlowDefinition>(definition->data);
}
const content::Node& node(const CompiledProgram& program, std::string_view id, std::string_view node_id) {
    const auto& definition = flow(program, id);
    const auto found = definition.nodes.find(node_id);
    if (found == definition.nodes.end()) invalid("Missing runtime flow node");
    return found->second;
}
void date_valid(core::OrdinalDay date) { (void)core::from_ordinal(date); }
bool incident_date(const WorldState& world, const content::FlowDefinition& definition, core::OrdinalDay date) {
    if (date >= world.simulation.end_date) return false;
    const auto& lifecycle = definition.lifecycle;
    if (lifecycle.mode == content::LifecycleMode::scheduled) return lifecycle.due_date == date;
    if (lifecycle.mode != content::LifecycleMode::interval) return false;
    const auto created = definition.scope == content::Scope::world ? world.campaign_start : world.scope_created;
    const auto anchor = resolve_anchor(*lifecycle.anchor, world.campaign_start, created);
    if (date < anchor.first_due) return false;
    auto elapsed = core::checked_subtract(date.value(), anchor.first_due.value());
    if (lifecycle.interval->unit == content::IntervalUnit::calendar_months) {
        const auto first = core::from_ordinal(anchor.first_due);
        const auto actual = core::from_ordinal(date);
        elapsed = (actual.year - first.year) * 12 + static_cast<int>(actual.month) - static_cast<int>(first.month);
    }
    const auto period = lifecycle.interval->value;
    return elapsed % period == 0 &&
        interval_due(anchor, *lifecycle.interval, static_cast<std::uint64_t>(elapsed / period)) == date;
}
// A saved stack explicitly identifies each currently open numbered child or
// subflow. Do not traverse child ports here: entering one would need another
// frame, while its normal return may legitimately have completed already.
using BranchKey = std::tuple<std::uint64_t, std::string, std::string>;
using BranchSelections = std::map<BranchKey, const RandomResolution*>;
bool region_reaches(const content::FlowDefinition& definition, const std::string& start,
                     const std::string& target, std::string_view flow_id,
                     std::uint64_t activation_id, const BranchSelections& selections) {
    std::vector<std::string> pending{start};
    std::set<std::string> visited;
    while (!pending.empty()) {
        const auto id = std::move(pending.back()); pending.pop_back();
        if (id == target) return true;
        if (!visited.insert(id).second) continue;
        const auto found = definition.nodes.find(id);
        if (found == definition.nodes.end()) return false;
        const auto& entry = found->second;
        if (!entry.next.empty()) pending.push_back(entry.next);
        for (const auto& choice : entry.choices) pending.push_back(choice.next);
        if (entry.kind == content::NodeKind::random) {
            const auto selected = selections.find({activation_id, std::string(flow_id), id});
            // Crossing a random node requires its committed outcome. An
            // unvisited node cannot explain a saved delay/choice after it.
            if (selected != selections.end())
                for (const auto& branch : entry.branches)
                    if (branch.id == selected->second->branch_id) pending.push_back(branch.next);
        }
    }
    return false;
}
template<class Row> const Row& by_id(const std::vector<Row>& rows, std::uint64_t id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [id](const auto& row) { return row.id == id; });
    if (found == rows.end()) invalid("Missing runtime record identity");
    return *found;
}
void validate_site_random_history(const CompiledProgram& program,const Checkpoint& checkpoint) {
    const auto& world=checkpoint.world;
    if (!world.simulation.resilience) return;
    struct Draw {
        core::OrdinalDay day{0};
        unsigned phase=0;
        std::size_t sequence=0;
        std::uint64_t before=0,after=0;
    };
    std::vector<Draw> history;
    const auto& samples=world.simulation.resilience->samples;
    for (std::size_t index=0;index<samples.size();++index) {
        const auto& sample=samples[index];
        history.push_back({sample.sampled_on,0,index,sample.stream_before,sample.stream_after});
    }
    const auto& branches=checkpoint.runtime.random_resolutions;
    for (std::size_t index=0;index<branches.size();++index) {
        const auto& branch=branches[index];
        if (node(program,branch.flow_id,branch.node_id).random_stream=="hazards")
            history.push_back({branch.day,1,index,branch.stream_before,branch.stream_after});
    }
    std::sort(history.begin(),history.end(),[](const auto& a,const auto& b){
        return std::tie(a.day,a.phase,a.sequence)<std::tie(b.day,b.phase,b.sequence);
    });
    const auto domain=static_cast<std::size_t>(core::RandomStream::hazards);
    auto expected=core::RandomStreams{world.root_seed}.snapshot()[domain];
    for (const auto& draw:history) {
        if (draw.before!=expected) invalid("Site and content hazards lost their shared saved random progression");
        expected=draw.after;
    }
    if (expected!=world.simulation.random[domain]) invalid("A site hazard consumed an unrecorded or rerolled random draw");
}
template<class Row> void identities(const std::vector<Row>& rows, std::uint64_t next,
                                    std::size_t maximum = maximum_records) {
    if (next == 0 || rows.size() > maximum) invalid("Runtime identifier/count limit");
    std::uint64_t previous = 0;
    for (const auto& row : rows) {
        if (row.id <= previous || row.id >= next) invalid("Runtime IDs must be unique and canonical");
        previous = row.id;
    }
}
} // namespace

WorldState foundation_world(core::Cents cash) {
    WorldState world;
    world.simulation = core::Simulation{cash}.checkpoint();
    world.facts = {{"company.monthly_compute_bill_cents", std::int64_t{0}},
                   {"company.workload_stress_basis_points", std::int64_t{0}},
                   {"world.public_breakthrough_count", std::int64_t{0}},
                   {"company.has_exclusive_compute_supplier", false},
                   {"company.has_compute_lease", false},
                   {"company.has_research_partner", false},
                   {"company.is_operating", true}};
    return world;
}
std::vector<std::string> supported_effects() {
    return {"adjust_cash", "record_decision", "open_negotiation", "register_owned_modifier", "remove_owned_modifier", "set_variable",
        "accept_business_offer", "temporary_demand_change", "record_world_catastrophe"};
}
content::Scalar read_fact(const WorldState& world, const content::FactMetadata& metadata,
                          const ScopeBinding& binding) {
    using content::Scope;
    if (binding.scope != metadata.scope)
        invalid("Fact scope binding has the wrong type");
    const auto& pool = world.simulation.capacity;
    const auto& market = world.simulation.market;
    if (binding.scope == Scope::capacity_source && pool && pool->owner == world.company_id) {
        const auto& source = related_row(pool->sources, binding.id);
        if (metadata.id == "capacity_source.effective_milli_cu_per_day") return source.effective.value();
        if (metadata.id=="capacity_source.has_site_outcome") return world.simulation.resilience &&
            std::any_of(world.simulation.resilience->alerts.begin(),world.simulation.resilience->alerts.end(),[&](const auto& row){
                return row.source_id==source.id && row.day<=world.simulation.date;
            });
    }
    if (binding.scope == Scope::project && pool && pool->owner == world.company_id) {
        const auto& project = related_row(pool->projects, binding.id);
        if (metadata.id == "project.completed_milli_cu") return project.completed_work.value();
        if (metadata.id == "project.is_active") return project.status == core::ProjectStatus::active;
        if (metadata.id == "project.is_completed") return project.status == core::ProjectStatus::completed;
        if (metadata.id == "project.lab_research_completed") {
            if (!world.simulation.lab) return false;
            return std::any_of(world.simulation.lab->research.begin(),world.simulation.lab->research.end(),[&](const auto& row){return row.project_id==project.id && row.completed;});
        }
        if (metadata.id == "project.has_evaluation_finding") {
            if (!world.simulation.lab) return false;
            return std::any_of(world.simulation.lab->evaluations.begin(),world.simulation.lab->evaluations.end(),[&](const auto& row) {
                return row.project_id==project.id && row.completed && !row.findings.empty();
            });
        }
    }
    if (binding.scope == Scope::contract && market && market->owner == world.company_id) {
        const auto& contract = related_row(market->contracts, binding.id);
        if (metadata.id == "contract.is_active") return contract.status == core::ContractStatus::active;
        if (metadata.id == "contract.missed_start") return contract.status == core::ContractStatus::missed;
        if (metadata.id == "contract.outcome_tasks") {
            for (auto day=market->days.rbegin();day!=market->days.rend();++day)
                for (const auto& row:day->contracts) if (row.contract_id==contract.id) return static_cast<std::int64_t>(row.delivered_tasks);
            return std::int64_t{0};
        }
    }
    if (!binding_exists(world, binding))
        invalid("Fact scope binding does not exist or has the wrong type");
    if (metadata.default_value) {
        const auto found = std::find_if(world.variables.begin(), world.variables.end(), [&](const auto& value) {
            return value.binding == binding && value.variable_id == metadata.id;
        });
        const auto& value = found == world.variables.end() ? *metadata.default_value : found->value;
        scalar_valid(value, metadata);
        return value;
    }
    if (metadata.id == "company.cash_cents") return world.simulation.cash.value();
    if (metadata.provider == "campaign") return detail::campaign_fact(world, metadata.id);
    if(metadata.provider=="succession") {
        const auto& state=world.simulation;
        if(metadata.id=="company.has_succession_charter") return state.succession.has_value();
        if(!state.succession) return false;
        if(metadata.id=="company.founder_is_chair") return core::founder_role(*state.succession)==core::FounderRole::chair;
        return state.succession->terms.size()>1 && state.succession->terms.back().starts_on==state.date;
    }
    if (metadata.provider=="frontier") {
        const auto& state=world.simulation;
        if (!state.frontier) return false;
        if (metadata.id=="company.frontier_era_today")
            return state.date==std::max(state.frontier->catalog.branches_after,state.accounts->starts_on);
        return core::earliest_frontier_qualification(*state.frontier,metadata.id=="company.game_asi_today")==state.date;
    }
    if(metadata.provider=="authority") {
        const auto& a=world.authority;const auto day=world.simulation.date;
        if(metadata.id=="world.authority_population_coverage_bp" || metadata.id=="world.authority_output_coverage_bp") {
            const auto value=authority_snapshot(world,day);
            return static_cast<std::int64_t>(metadata.id=="world.authority_population_coverage_bp"?value.population_coverage:value.output_coverage);
        }
        if(metadata.id=="world.authority_month_today") return a && !a->months.empty() && a->months.back().month_end.value()+1==day.value();
        if(metadata.id=="world.executive_disclosure_today") {
            if(!a || !world.simulation.management || !world.simulation.management->governance) return false;
            return std::any_of(world.simulation.management->governance->disputes.begin(),world.simulation.management->governance->disputes.end(),[&](const auto& c){return c.kind=="publication" && c.status==core::CorporateDisputeStatus::departed && c.resolved_on==day;});
        }
        if(metadata.id=="world.authority_notice_today") {
            if(!a) return false;
            for(const auto& m:a->mandates) if((!m.reviews.empty() && m.reviews.front().day==day) || m.revoked_on==day || m.withdrawn_on==day) return true;
            for(const auto& c:a->cases) if(c.opened_on==day || core::advance_days(c.opened_on,a->catalog.concern_period)==day || c.resolved_on==day || c.disclosed_on==day) return true;
            return false;
        }
    }
    if(metadata.provider=="society") {
        const auto& society=world.society;
        if(metadata.id=="world.society_month_today") return society && !society->months.empty() &&
            society->months.back().month_end.value()+1==world.simulation.date.value();
        if(metadata.id=="world.forum_changed_today") {
            if(!society) return false;
            const auto day=world.simulation.date;
            for(const auto& round:society->rounds) {
                if(round.opened_on==day || round.ballot_on==day || round.expires_before==day) return true;
                for(const auto& member:round.members) if(member.effective_from==day) return true;
            }
            return false;
        }
        for(std::size_t i=0;i<5;++i) {
            const auto key=std::string(core::welfare_index_key(i));
            if(metadata.id=="world.welfare_"+key+"_bp") return static_cast<std::int64_t>(society?core::global_welfare(*society)[i]:0);
            if(metadata.id=="world.minimum_"+key+"_bp") return static_cast<std::int64_t>(society?core::minimum_welfare(*society)[i]:0);
        }
    }
    if (metadata.id == "world.civic_election_today") {
        const auto& civic = world.simulation.civic;
        return civic && !civic->elections.empty() && civic->elections.back().day == world.simulation.date;
    }
    if (metadata.id == "world.civic_rule_effective_today")
        return world.simulation.civic && core::civic_rule_effective_today(*world.simulation.civic, world.simulation.date);
    if (world.simulation.infrastructure && world.simulation.accounts &&
        (metadata.id=="company.has_compute_lease" || metadata.id=="company.monthly_compute_bill_cents")) {
        core::Cents total{0};
        for (const auto& term:world.simulation.accounts->recurring)
            if ((term.id=="compute_lease" || term.id=="founder_compute" || term.id.starts_with("offer.lease/") || term.id.starts_with("offer.renew/")) &&
                term.starts_on<=world.simulation.date && (!term.ends_before || *term.ends_before>world.simulation.date)) total=core::checked_unit_add(total,term.monthly_amount);
        if (metadata.id=="company.has_compute_lease") return total.value()>0;
        return total.value();
    }
    if (metadata.id == "company.monthly_compute_bill_cents" && world.simulation.business_offers) {
        auto bill=std::get<std::int64_t>(world.facts.at(metadata.id));
        const auto& offers=*world.simulation.business_offers;
        for (const auto& accepted:offers.accepted) if (accepted.accepted_on<=world.simulation.date && world.simulation.date<accepted.expires_before) {
            const auto offer=std::find_if(offers.catalog.begin(),offers.catalog.end(),[&](const auto& row){return row.id==accepted.offer_id;});
            if (offer!=offers.catalog.end() && offer->kind==core::BusinessOfferKind::compute_lease)
                bill=core::checked_add(bill,offer->monthly_amount.value());
        }
        return bill;
    }
    if (metadata.id == "company.in_liquidity_workout") return world.simulation.liquidity && world.simulation.liquidity->status==core::LiquidityStatus::workout;
    if (metadata.id == "company.has_evaluation_finding") return world.simulation.lab && std::any_of(
        world.simulation.lab->evaluations.begin(),world.simulation.lab->evaluations.end(),[](const auto& row){return row.completed && !row.findings.empty();});
    if (metadata.id == "company.competitive_notice_count") return static_cast<std::int64_t>(world.competition?world.competition->notices.size():0);
    if (metadata.id == "world.public_breakthrough_count" && world.competition && world.competition->setup) {
        std::int64_t count=world.simulation.lab?static_cast<std::int64_t>(world.simulation.lab->publications.size()):0;
        for (const auto& rival:world.competition->companies) if (rival.planning->entered_on) count=core::checked_add(count,static_cast<std::int64_t>(rival.simulation.lab->publications.size()));
        return count;
    }
    if (metadata.id == "company.has_enterprise_segment") return market && std::any_of(market->segments.begin(),market->segments.end(),[](const auto& row){return row.id=="segment.enterprise";});
    if (metadata.id == "company.market_shift_active") return market && std::any_of(market->demand_changes.begin(),market->demand_changes.end(),[&](const auto& row) {
        return row.starts_on<=world.simulation.date && world.simulation.date<row.expires_before;
    });
    if (metadata.id == "company.supplier_offer_eligible" || metadata.id == "company.recovery_offer_eligible") {
        const auto& state=world.simulation;
        if (!state.business_offers || !state.accounts || !state.capacity) return false;
        const auto kind=metadata.id=="company.supplier_offer_eligible"?core::BusinessOfferKind::compute_lease:core::BusinessOfferKind::recovery_pilot;
        core::Cents protected_cash{0}; for (const auto& row:state.reservations) protected_cash=core::checked_unit_add(protected_cash,row.reserved);
        return std::any_of(state.business_offers->catalog.begin(),state.business_offers->catalog.end(),[&](const auto& offer) {
            return offer.kind==kind && core::business_offer_eligible(*state.business_offers,offer,*state.accounts,*state.capacity,state.market,state.date,protected_cash);
        });
    }
    const auto found = world.facts.find(metadata.id);
    if (found == world.facts.end()) invalid("Missing owning fact provider value: " + metadata.id);
    scalar_valid(found->second, metadata);
    return found->second;
}
std::vector<ScopeBinding> read_relation(const WorldState& world, const content::RelationMetadata& metadata,
                                       const ScopeBinding& owner) {
    using content::Scope;
    if (owner.scope != metadata.owner_scope) invalid("Relation owner has the wrong type");
    std::vector<ScopeBinding> members;
    const auto& pool = world.simulation.capacity;
    const auto& market = world.simulation.market;
    if (owner.scope == Scope::company && owner.id == world.company_id) {
        if (metadata.id == "company.capacity_sources") {
            if (pool && pool->owner == owner.id)
                for (const auto& source : pool->sources) members.push_back({Scope::capacity_source, source.id});
            return members;
        }
        if (metadata.id == "company.projects") {
            if (pool && pool->owner == owner.id)
                for (const auto& project : pool->projects) members.push_back({Scope::project, project.id});
            return members;
        }
        if (metadata.id == "company.contracts") {
            if (market && market->owner == owner.id)
                for (const auto& contract : market->contracts) members.push_back({Scope::contract, contract.id});
            return members;
        }
    }
    if (owner.scope == Scope::capacity_source && metadata.id == "capacity_source.projects" &&
        pool && pool->owner == world.company_id) {
        (void)related_row(pool->sources, owner.id);
        for (const auto& project : pool->projects)
            if (project.source_id == owner.id) members.push_back({Scope::project, project.id});
        return members;
    }
    invalid("Relation has no runtime provider or owning binding");
}
std::int64_t modifier_total(const RuntimeState& state, const ScopeBinding& binding, std::string_view field) {
    std::int64_t total = 0;
    for (const auto& modifier : state.modifiers)
        if (modifier.binding == binding && modifier.field == field)
            total = core::checked_add(total, modifier.add_basis_points);
    return total;
}

void validate_checkpoint_with_simulation(const CompiledProgram& program, const Checkpoint& checkpoint,
                                          bool validate_core_simulation) {
    const auto& world = checkpoint.world;
    const auto& state = checkpoint.runtime;
    if((!world.background.empty() || !world.difficulty.empty()) && world.scenario_id.empty()) invalid("Campaign setup needs its authored source scenario");
    if (world.identity) {
        validate_campaign_identity(*world.identity);
        if (world.scenario_id.empty()) invalid("Campaign identity needs its authored source scenario");
    }
    if (world.simulation.succession && world.simulation.succession->founder_name !=
        (world.identity ? std::optional{world.identity->founder_name} : std::nullopt))
        invalid("The founding executive name differs from the saved campaign identity");
    if ((world.simulation.civic || world.society || world.authority || world.simulation.frontier) && world.scenario_id.empty())
        invalid("Institutions, society and frontier research require an authored source scenario");
    if (!world.scenario_id.empty()) {
        const auto* definition=program.collection.find(world.scenario_id);
        const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
        if (!scenario || scenario->starts_on!=world.campaign_start ||
            (!world.objectives && scenario->ends_before!=world.simulation.end_date))
            invalid("Saved source scenario is absent or has different campaign dates");
        (void)content::scenario_difficulty(*scenario,world.difficulty);
        if (validate_core_simulation && (scenario->infrastructure ? (!world.simulation.infrastructure || world.simulation.infrastructure->catalog!=*scenario->infrastructure) : world.simulation.infrastructure.has_value()))
            invalid("Saved owned infrastructure rules differ from the source scenario");
        if (const auto* risks=content::resilience_catalog(program.collection,*scenario)) {
            if (!world.simulation.resilience || (validate_core_simulation && world.simulation.resilience->catalog!=*risks))
                invalid("Saved site risks differ from the authored scenario");
        } else if (world.simulation.resilience) invalid("This scenario did not authorize site resilience rules");
        if (const auto* civic = content::civic_catalog(program.collection, *scenario)) {
            if (!world.simulation.civic || (validate_core_simulation && world.simulation.civic->catalog != *civic))
                invalid("Saved civic rules differ from the authored scenario");
        } else if (world.simulation.civic) invalid("This scenario did not authorize a civic calendar");
        if (const auto* society=content::society_catalog(program.collection,*scenario)) {
            if(!world.society || world.society->prepared_on!=campaign_day(world) ||
                (validate_core_simulation && world.society->catalog!=*society))
                invalid("Saved regional society differs from the authored scenario or prepared morning");
        } else if(world.society) invalid("This scenario did not authorize regional society");
        if(const auto* authority=content::authority_catalog(program.collection,*scenario)) {
            if(!world.authority || world.authority->prepared_on!=world.simulation.date ||
                (validate_core_simulation && world.authority->catalog!=*authority)) invalid("Saved institutions differ from the authored scenario or prepared morning.");
        } else if(world.authority) invalid("This scenario did not authorize institutional authority.");
        if (const auto* frontier=content::frontier_catalog(program.collection,*scenario)) {
            if (!world.simulation.frontier || world.simulation.frontier->seed!=world.root_seed ||
                (validate_core_simulation && world.simulation.frontier->catalog!=*frontier))
                invalid("Saved frontier research differs from the authored scenario and seed.");
        } else if (world.simulation.frontier) invalid("This scenario did not authorize frontier research.");
        if (world.simulation.infrastructure) {
            const auto& base=world.simulation.infrastructure->executive_base;
            if ((base=="US-CA"?"state.us_ca":base=="US-TX"?"state.us_tx":base=="US-WA"?"state.us_wa":"")!=world.state_id ||
                std::find(scenario->headquarters_choices.begin(),scenario->headquarters_choices.end(),base)==scenario->headquarters_choices.end()) invalid("Executive home base differs from the company's chosen headquarters");
        }
    }
    if (world.simulation.lab && !world.simulation.lab->insights.empty() && (!world.competition || !world.competition->setup))
        invalid("Foreign research insights require an authored public diffusion contract");
    // Core validation runs on a copy only when an unfinished opening needs
    // preparation; otherwise validate the core state directly in place.
    if (validate_core_simulation) {
        validate_society_world(world);
        if (world.simulation.phase == core::DayPhase::opening) {
            (void)core::Simulation{world.simulation};
        } else {
            core::validate_simulation(world.simulation);
        }
    }
    if (world.simulation.accounts && world.simulation.accounts->owner != world.company_id)
        invalid("Business accounts belong to a different company scope");
    if (validate_core_simulation) {
        bool matching_lab_scenario = false;
        bool matching_catalog = false;
        for (const auto& [id, definition] : program.collection.definitions) {
            if (!world.scenario_id.empty() && id!=world.scenario_id) continue;
            const auto* scenario = std::get_if<content::ScenarioDefinition>(&definition.data);
            const auto authored_deadline=world.objectives?world.objectives->deadline:world.simulation.end_date;
            if (!scenario || !scenario->lab || scenario->starts_on != world.campaign_start ||
                scenario->ends_before != authored_deadline) continue;
            matching_lab_scenario = true;
            if (world.simulation.lab && world.simulation.lab->catalog == content::scenario_lab_catalog(program.collection, *scenario)) {
                matching_catalog = true;
                const auto opening=content::scenario_opening(*scenario,world.background,world.company_id,world.difficulty);
                const auto management=content::scenario_management(*scenario,world.difficulty);
                const auto competition=content::scenario_competition(*scenario,world.difficulty);
                if(!world.background.empty() || !world.difficulty.empty()) {
                    const auto& accounts=*world.simulation.accounts;
                    if(opening.cash.value()>0 && (accounts.postings.empty() || accounts.postings.front().key!="opening" || accounts.postings.front().amount!=opening.cash))
                        invalid("Founder opening capital differs from its actual contribution");
                    for(const auto& [id,monthly]:opening.monthly_commitments) if(monthly.value()>0) {
                        const auto it=std::find_if(accounts.recurring.begin(),accounts.recurring.end(),[&](const auto& r){return r.id==id;});
                        if(it==accounts.recurring.end() || it->monthly_amount!=monthly || it->starts_on!=scenario->starts_on) invalid("Founder opening commitments differ from their authored package");
                    }
                }
                const auto founder_source=std::find_if(world.simulation.capacity->sources.begin(),world.simulation.capacity->sources.end(),[](const auto& r){return r.id=="lease.founder";});
                const auto expected_founder=std::find_if(opening.capacity_sources.begin(),opening.capacity_sources.end(),[](const auto& r){return r.id=="lease.founder";});
                if((founder_source==world.simulation.capacity->sources.end())!=(expected_founder==opening.capacity_sources.end())) invalid("Founder compute presence differs from its opening package");
                if(expected_founder!=opening.capacity_sources.end()) {
                    auto expected=*expected_founder;
                    const auto& owned=world.simulation.infrastructure;
                    if(owned && owned->initial_lease_ends_before && world.simulation.date>=*owned->initial_lease_ends_before) expected.effective=core::MilliCUPerDay{0};
                    if(*founder_source!=expected) invalid("Founder compute differs from its paid independent opening lease");
                    const auto& bills=world.simulation.accounts->recurring;
                    const auto it=std::find_if(bills.begin(),bills.end(),[](const auto& r){return r.id=="founder_compute";});
                    const auto end=owned && owned->initial_lease_ends_before && world.simulation.date>=*owned->initial_lease_ends_before?owned->initial_lease_ends_before:std::nullopt;
                    if(it==bills.end() || it->ends_before!=end) invalid("Founder compute bill differs from opening lease termination");
                }

                if (management ? (!world.simulation.management || world.simulation.management->catalog!=*management ||
                    core::opening_staff_count(*world.simulation.capacity)!=opening.team_people) : world.simulation.management.has_value())
                    invalid("Saved management rules or opening staff differ from the scenario");
                if (world.simulation.management && world.simulation.management->governance) {
                    std::vector<core::EquityHolding> initial;
                    for (const auto& [holder,shares]:scenario->ownership) initial.push_back({holder,shares,holder=="option_pool"});
                    if (world.simulation.management->governance->opening_holdings!=initial) invalid("Corporate opening ownership differs from the scenario");
                }
                if (scenario->service_catalog ? (!world.simulation.deployments || world.simulation.deployments->catalog!=content::scenario_services(program.collection,*scenario)) : world.simulation.deployments.has_value())
                    invalid("Saved service catalog differs from its authored qualification rules");
                if (!scenario->regional_markets.empty()) {
                    auto expected = content::scenario_market_setup(program.collection,*scenario).segments;
                    if (!world.simulation.market) invalid("Regional scenario has no saved market");
                    auto actual = world.simulation.market->segments;
                    for (auto& segment : expected) segment.preferences.clear();
                    for (auto& segment : actual) segment.preferences.clear();
                    if (actual != expected) invalid("Saved regional customer terms differ from their authored profiles");
                }
                if (opening.business_offers.empty()?world.simulation.business_offers.has_value():
                    (!world.simulation.business_offers || world.simulation.business_offers->catalog!=opening.business_offers))
                    invalid("Saved business offer catalog differs from its scenario");
                std::vector<std::string> expected,actual;
                for (const auto& rival:scenario->rivals) if (!competition || content::selected_rival(rival,*competition)) expected.push_back(rival.id);
                if (world.competition) for (const auto& rival:world.competition->companies) actual.push_back(rival.definition_id);
                if (expected!=actual || (!competition && scenario->rivals.empty() && world.competition) ||
                    (competition && (!world.competition || world.competition->setup!=competition))) invalid("Saved rival roster/settings differ from their scenario");
            }
        }
        if ((matching_lab_scenario || world.simulation.lab) && !matching_catalog)
            invalid("Saved lab catalog or presence differs from the authored scenario");
        if (world.simulation.lab && world.simulation.lab->research_stream_origin !=
            core::RandomStreams{world.root_seed}.snapshot()[static_cast<std::size_t>(core::RandomStream::research)])
            invalid("Lab research stream origin differs from its campaign seed");
    }
    validate_competition(world,program,validate_core_simulation);
    validate_acquisitions(world,program,validate_core_simulation);
    if(validate_core_simulation) validate_authority_world(world);
    validate_objectives(world,program,validate_core_simulation);
    if (validate_core_simulation) {
        if (world.competition) for (const auto& rival : world.competition->companies)
            if (rival.simulation.civic) invalid("US institutions belong to the shared world, not a rival checkpoint");
        if (world.simulation.civic) {
            const auto& civic = *world.simulation.civic;
            if (!world.simulation.market || civic.politics_origin != core::RandomStreams{world.root_seed}.snapshot()[static_cast<std::size_t>(core::RandomStream::politics)])
                invalid("The civic calendar lost its domestic market or campaign politics seed");
            const auto& market = *world.simulation.market;
            const auto& adoption = world.competition && world.competition->setup ? world.competition->adoptions : market.adoptions;
            for (const auto& election : civic.elections)
                if (election.adoption != core::civic_adoption(market, adoption, election.day))
                    invalid("Election adoption inputs differ from the last settled domestic market review");
            if (world.competition) for (const auto& rival : world.competition->companies)
                core::validate_civic_deployments(civic, rival.simulation);
        }
    }
    if (world.simulation.phase == core::DayPhase::closed &&
        (!world.facts.contains("company.is_operating") || world.facts.at("company.is_operating") != content::Scalar{false}))
        invalid("Closed company still claims an operating business fact");
    date_valid(world.campaign_start); date_valid(world.scope_created);
    if (!content::valid_id(world.company_id) || !content::valid_id(world.world_id) ||
        !content::valid_id(world.state_id) || world.campaign_start > world.simulation.date ||
        world.scope_created < world.campaign_start || world.scope_created > world.simulation.date ||
        world.counterparty_cash.value() < 0 || world.facts.size() > 512 ||
        world.decisions.size() > maximum_records || world.negotiations.size() > maximum_records)
        invalid("Invalid world identity, date, counterparty or count");
    for (const auto& [id, value] : world.facts) {
        const auto* metadata = program.registry.fact(id);
        if (!metadata || metadata->default_value || id == "company.cash_cents" || metadata->provider=="slice" ||
            metadata->provider=="civic" || metadata->provider=="society" || metadata->provider=="authority" ||
            metadata->provider=="frontier" || business_member_scope(metadata->scope))
            invalid("Unknown or duplicated owning world fact");
        scalar_valid(value, *metadata);
    }
    if (world.variables.size() > maximum_records) invalid("Variable value count budget exceeded");
    std::optional<std::pair<ScopeBinding, std::string>> previous_variable;
    for (const auto& variable : world.variables) {
        const auto identity = std::pair{variable.binding, variable.variable_id};
        const auto* metadata = program.registry.fact(variable.variable_id);
        if (!metadata || !metadata->default_value || !binding_exists(world, variable.binding) ||
            metadata->scope != variable.binding.scope || (previous_variable && identity <= *previous_variable))
            invalid("Variable value has an unknown owner, wrong scope or duplicate/noncanonical identity");
        scalar_valid(variable.value, *metadata);
        previous_variable = identity;
    }
    if (world.enabled_flows) {
        if (world.enabled_flows->size() > 512) invalid("Enabled flow count budget exceeded");
        std::set<std::string> enabled;
        for (const auto& id : *world.enabled_flows) {
            (void)flow(program, id);
            if (!enabled.insert(id).second) invalid("Duplicate enabled root flow");
        }
    }
    for (const auto& decision : world.decisions)
        if (!content::valid_id(decision)) invalid("Invalid decision marker");
    for (const auto& negotiation : world.negotiations) {
        const auto* definition = program.collection.find(negotiation);
        if (!definition || !std::holds_alternative<content::DealDefinition>(definition->data))
            invalid("Negotiation references missing deal template");
    }
    const auto operations = supported_effects();
    const auto reader = [&](const auto& metadata, const auto& bound) { return read_fact(world, metadata, bound); };
    const auto relations = [&](const auto& metadata, const auto& bound) { return read_relation(world, metadata, bound); };
    std::size_t inspected_nodes = 0;
    const auto inspect = [&](const auto& condition, const auto& binding) {
        const auto inspected = evaluate_condition(condition, binding, program.registry, reader,
            InspectionMode::author_fixture, "checkpoint", {}, {}, relations);
        if (!inspected.selections.empty()) inspected_nodes += inspected.inspected_nodes;
        if (inspected_nodes > 2 * maximum_records)
            throw CompileError("budget", "checkpoint", "Checkpoint condition inspection budget exceeded");
    };
    for (const auto& [id, definition] : program.collection.definitions) {
        (void)id;
        if (const auto* value = std::get_if<content::FlowDefinition>(&definition.data)) {
            const auto binding = ScopeBinding{value->scope, value->scope == content::Scope::company ? world.company_id :
                value->scope == content::Scope::world ? world.world_id : world.state_id};
            inspect(value->trigger, binding);
            if (value->lifecycle.exit_when) inspect(*value->lifecycle.exit_when, binding);
            for (const auto& [node_id, entry] : value->nodes) {
                (void)node_id;
                for (const auto& effect : entry.effects)
                    if (std::find(operations.begin(), operations.end(), effect.operation) == operations.end())
                        invalid("Registered effect has no available runtime provider: " + effect.operation);
            }
        }
    }
    if (state.activations.size() > maximum_activation_history || state.lifecycles.size() > 512 ||
        state.modifiers.size() > maximum_activations || state.receipts.size() > maximum_records ||
        state.presentations.size() > maximum_records || state.random_resolutions.size() > maximum_records)
        invalid("Runtime collection count budget exceeded");
    identities(state.activations, state.next_activation_id, maximum_activation_history);
    identities(state.receipts, state.next_receipt_id);
    identities(state.presentations, state.next_presentation_id);
    if (state.prepared_day && (*state.prepared_day != world.simulation.date || world.simulation.phase != core::DayPhase::work_ready))
        invalid("Prepared runtime day differs from core date");
    detail::validate_duration_states(checkpoint, program);
    BranchSelections selections;
    for (const auto& resolution : state.random_resolutions)
        selections.insert_or_assign({resolution.activation_id, resolution.flow_id, resolution.node_id}, &resolution);
    std::map<std::pair<std::string, ScopeBinding>, std::uint32_t> counts;
    std::size_t live_activations = 0;
    for (const auto& activation : state.activations) {
        if (activation.status != ActivationStatus::completed && ++live_activations > maximum_activations)
            invalid("Live activation count budget exceeded");
        const auto& root = flow(program, activation.definition_id);
        const auto* definition = program.collection.find(activation.definition_id);
        if (root.group || !binding_exists(world, activation.binding) || activation.binding.scope != root.scope ||
            activation.definition_revision != definition->revision || activation.frames.size() > maximum_continuation_frames ||
            flow(program, activation.current_flow).scope != activation.binding.scope)
            invalid("Activation definition revision, scope or frame budget mismatch");
        const auto& current = node(program, activation.current_flow, activation.current_node);
        date_valid(activation.started_on);
        if (activation.started_on < world.campaign_start || activation.started_on > world.simulation.date)
            invalid("Invalid activation start date");
        ++counts[{activation.definition_id, activation.binding}];
        const auto visits_limit = root.max_node_visits ? root.max_node_visits : maximum_steps_per_operation;
        if (activation.node_visits > visits_limit) invalid("Activation exceeded authored node budget");
        std::string expected_flow = activation.definition_id;
        std::string region_entry = activation.exiting ? root.exit_node : root.entry_node;
        for (const auto& frame : activation.frames) {
            const auto& owner = node(program, frame.flow_id, frame.sequence_node);
            const auto& frame_flow = flow(program, frame.flow_id);
            if (frame.flow_id != expected_flow || frame_flow.scope != activation.binding.scope ||
                !region_reaches(frame_flow, region_entry, frame.sequence_node, frame.flow_id, activation.id, selections) ||
                frame.binding != activation.binding || frame.return_node != owner.next ||
                (!frame.return_node.empty() && !flow(program, frame.flow_id).nodes.contains(frame.return_node)))
                invalid("Invalid continuation return/context");
            if (frame.kind == FrameKind::sequence) {
                if (owner.kind != content::NodeKind::sequence || frame.next_child > owner.children.size() || frame.next_child == 0)
                    invalid("Invalid saved sequence continuation");
                region_entry = owner.children[frame.next_child - 1];
            } else if (frame.kind == FrameKind::subflow) {
                if (owner.kind != content::NodeKind::subflow || frame.next_child != 0)
                    invalid("Invalid saved subflow continuation");
                expected_flow = owner.subflow_id;
                region_entry = flow(program, expected_flow).entry_node;
            } else invalid("Unknown continuation frame kind");
        }
        if (activation.current_flow != expected_flow ||
            !region_reaches(flow(program, expected_flow), region_entry, activation.current_node, expected_flow, activation.id, selections))
            invalid("Current node escaped its saved continuation region");
        switch (activation.status) {
        case ActivationStatus::ready:
            if (activation.due || activation.wait_started || activation.presentation_id) invalid("Ready activation retains wait state");
            break;
        case ActivationStatus::delayed:
            if (!activation.due || !activation.wait_started || activation.presentation_id || current.kind != content::NodeKind::delay)
                invalid("Delayed activation lacks its authored delay boundary");
            date_valid(*activation.due);
            date_valid(*activation.wait_started);
            if (*activation.wait_started < activation.started_on || *activation.wait_started > world.simulation.date ||
                *activation.due != core::advance_days(*activation.wait_started, core::Days{current.delay_days}))
                invalid("Saved delay timing differs from its authored duration");
            break;
        case ActivationStatus::awaiting_choice: {
            if (activation.due || activation.wait_started || !activation.presentation_id || current.kind != content::NodeKind::dialogue)
                invalid("Waiting activation is not on a dialogue");
            const auto& presentation = by_id(state.presentations, *activation.presentation_id);
            if (presentation.resolved || presentation.activation_id != activation.id ||
                presentation.node_id != activation.current_node || presentation.flow_id != activation.current_flow)
                invalid("Waiting dialogue identity mismatch");
            break;
        }
        case ActivationStatus::continuous_active:
            if (root.lifecycle.mode != content::LifecycleMode::continuous || current.kind != content::NodeKind::end || activation.exiting || activation.due || activation.wait_started ||
                activation.presentation_id || !activation.frames.empty()) invalid("Invalid active continuous lifecycle");
            break;
        case ActivationStatus::completed:
            if (activation.due || activation.wait_started || activation.presentation_id || !activation.frames.empty() || current.kind != content::NodeKind::end)
                invalid("Completed activation retains work");
            break;
        default: invalid("Unknown activation status");
        }
    }
    std::set<std::pair<std::string, ScopeBinding>> lifecycle_keys;
    for (const auto& entry : state.lifecycles) {
        const auto& definition = flow(program, entry.definition_id);
        if (definition.group || !binding_exists(world, entry.binding) || entry.binding.scope != definition.scope ||
            (world.enabled_flows && std::find(world.enabled_flows->begin(), world.enabled_flows->end(), entry.definition_id) == world.enabled_flows->end()) ||
            !lifecycle_keys.emplace(entry.definition_id, entry.binding).second ||
            counts[{entry.definition_id, entry.binding}] != entry.occurrences ||
            (entry.last_checked && *entry.last_checked > world.simulation.date) ||
            (entry.last_started && *entry.last_started > world.simulation.date) ||
            (entry.occurrences != 0 && !entry.last_started)) invalid("Invalid occurrence scope/counter/date");
        if (entry.last_checked) date_valid(*entry.last_checked);
        if (entry.last_started) date_valid(*entry.last_started);
        if (entry.last_started && *entry.last_started < world.campaign_start) invalid("Occurrence predates the campaign");
        if (definition.lifecycle.mode != content::LifecycleMode::continuous && entry.occurrences > definition.lifecycle.max_occurrences)
            invalid("Occurrence count exceeds authored bound");
        if (entry.active_activation) {
            const auto& active = by_id(state.activations, *entry.active_activation);
            if (active.definition_id != entry.definition_id || active.binding != entry.binding || active.status == ActivationStatus::completed)
                invalid("Active lifecycle points to completed/unrelated activation");
        }
        if (definition.lifecycle.mode == content::LifecycleMode::interval) {
            if (!entry.first_due || entry.calendar_day < 1 || entry.calendar_day > 31) invalid("Interval lacks saved anchor");
            const auto created = definition.scope == content::Scope::world ? world.campaign_start : world.scope_created;
            const auto resolved = resolve_anchor(*definition.lifecycle.anchor, world.campaign_start, created);
            if (resolved.first_due != *entry.first_due || resolved.calendar_day != entry.calendar_day)
                invalid("Saved interval anchor does not match content/context");
            (void)interval_due(resolved, *definition.lifecycle.interval, entry.interval_index);
        } else if (entry.first_due || entry.interval_index != 0) invalid("Non-interval lifecycle carries an anchor");
    }
    for (const auto& activation : state.activations)
        if (!lifecycle_keys.contains({activation.definition_id, activation.binding})) invalid("Activation has no occurrence state");
        else if (activation.status != ActivationStatus::completed) {
            const auto owner = std::find_if(state.lifecycles.begin(), state.lifecycles.end(), [&](const auto& entry) {
                return entry.definition_id == activation.definition_id && entry.binding == activation.binding;
            });
            if (owner->active_activation != activation.id) invalid("Live activation is not its occurrence scope's sole active instance");
        }
    detail::validate_committed_sources(program, checkpoint);
    std::map<std::uint64_t, std::uint32_t> last_random_visit;
    std::optional<core::OrdinalDay> last_random_day;
    std::array<std::optional<std::uint64_t>, core::stream_count> last_random_states{};
    std::array<bool, core::stream_count> seen_streams{};
    last_random_states[static_cast<std::size_t>(core::RandomStream::narrative)] = state.narrative_origin;
    for (const auto& [stream, origin] : state.incident_origins) {
        if (!content::is_incident_stream(stream)) invalid("Unsupported incident stream history");
        last_random_states[static_cast<std::size_t>(stream)] = origin;
    }
    std::set<std::tuple<std::string_view, ScopeBinding, core::OrdinalDay>> incident_boundaries;
    std::set<std::uint64_t> incident_samples;
    std::set<std::uint64_t> quiet_incidents;
    std::map<std::string, std::set<std::string>, std::less<>> called_flows;
    for (const auto& resolution : state.random_resolutions) {
        const auto& activation = by_id(state.activations, resolution.activation_id);
        const auto& owner = node(program, resolution.flow_id, resolution.node_id);
        auto [allowed, inserted] = called_flows.try_emplace(activation.definition_id);
        if (inserted) {
            std::vector<std::string> pending{activation.definition_id};
            while (!pending.empty()) {
                const auto id = std::move(pending.back()); pending.pop_back();
                if (!allowed->second.insert(id).second) continue;
                for (const auto& [node_id, entry] : flow(program, id).nodes) {
                    (void)node_id;
                    if (entry.kind == content::NodeKind::subflow) pending.push_back(entry.subflow_id);
                }
            }
        }
        date_valid(resolution.day);
        const auto domain = content::random_branch_stream(owner.random_stream);
        if (owner.kind != content::NodeKind::random || !domain ||
            !allowed->second.contains(resolution.flow_id) ||
            flow(program, resolution.flow_id).scope != activation.binding.scope ||
            resolution.day < activation.started_on || resolution.day > world.simulation.date ||
            (last_random_day && resolution.day < *last_random_day) ||
            resolution.node_visit == 0 || resolution.node_visit > activation.node_visits ||
            resolution.node_visit <= last_random_visit[activation.id])
            invalid("Random resolution has an invalid node, scope, date or visit");
        const auto index = static_cast<std::size_t>(*domain);
        auto& last_random_state = last_random_states[index];
        const bool shared_hazards=world.simulation.resilience && *domain==core::RandomStream::hazards;
        if ((!shared_hazards || !seen_streams[index]) && last_random_state != resolution.stream_before)
            invalid("Random resolution is missing its stream origin or breaks its stream history");
        seen_streams[index] = true;
        if (content::is_incident_stream(*domain)) {
            const auto& root = flow(program, activation.definition_id);
            if (resolution.flow_id != activation.definition_id || resolution.node_id != root.entry_node ||
                resolution.day != activation.started_on || resolution.node_visit != 1 ||
                !incident_date(world, root, resolution.day) ||
                !incident_boundaries.emplace(activation.definition_id, activation.binding, resolution.day).second ||
                !incident_samples.insert(activation.id).second)
                invalid("A seeded incident sample must belong to a single eligible opening boundary");
        }
        std::uint64_t total = 0;
        for (const auto& branch : owner.branches) total += branch.weight;
        core::SplitMix64 copied{resolution.stream_before};
        const auto sample = copied.bounded(total);
        if (sample != resolution.sample || copied.state() != resolution.stream_after)
            invalid("Random resolution differs from its saved stream draw");
        auto remaining = sample;
        const content::WeightedBranch* selected = nullptr;
        for (const auto& branch : owner.branches) {
            if (remaining < branch.weight) { selected = &branch; break; }
            remaining -= branch.weight;
        }
        if (!selected || selected->id != resolution.branch_id)
            invalid("Random resolution differs from its authored weighted outcome");
        if (content::is_incident_stream(*domain) && selected->id == "no_event") {
            if (activation.status != ActivationStatus::completed || activation.current_node != selected->next || activation.node_visits != 2)
                invalid("Quiet incident outcome must end without running a consequence");
            quiet_incidents.insert(activation.id);
        }
        last_random_visit[activation.id] = resolution.node_visit;
        last_random_day = resolution.day;
        last_random_state = resolution.stream_after;
    }
    for (std::size_t index = 0; index < core::stream_count; ++index)
        if (last_random_states[index].has_value() != seen_streams[index] ||
            (seen_streams[index] && !(world.simulation.resilience && index==static_cast<std::size_t>(core::RandomStream::hazards)) &&
                last_random_states[index] != world.simulation.random[index]))
            invalid("Saved random stream does not continue its resolved branch history");
    if (validate_core_simulation) validate_site_random_history(program,checkpoint);
    for (const auto& activation : state.activations) {
        const auto& root = flow(program, activation.definition_id);
        const auto domain = content::random_branch_stream(root.nodes.at(root.entry_node).random_stream);
        if (domain && content::is_incident_stream(*domain) && !incident_samples.contains(activation.id))
            invalid("Seeded incident activation is missing its saved sample");
    }
    std::set<std::tuple<std::uint64_t, ScopeBinding, std::string>> modifier_keys;
    for (const auto& modifier : state.modifiers) {
        const auto& activation = by_id(state.activations, modifier.activation_id);
        if (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous ||
            activation.status == ActivationStatus::completed || activation.binding != modifier.binding ||
            !content::valid_id(modifier.key) || modifier.field != "research_coordination_basis_points" ||
            modifier.add_basis_points < -10000 || modifier.add_basis_points > 10000 ||
            !modifier_keys.emplace(modifier.activation_id, modifier.binding, modifier.key).second)
            invalid("Invalid/duplicate owned modifier");
    }
    using ModifierKey = std::tuple<std::uint64_t, ScopeBinding, std::string>;
    std::map<ModifierKey, OwnedModifier> recorded_modifiers;
    std::size_t catastrophe_count=0;
    std::set<std::string_view> workload_receipts;
    for (const auto& receipt : state.receipts) {
        if (quiet_incidents.contains(receipt.activation_id)) invalid("Quiet incident has an effect receipt");
        const auto& activation = by_id(state.activations, receipt.activation_id);
        const auto& owner = node(program, receipt.flow_id, receipt.node_id);
        if (receipt.revision == 0 || receipt.revision > world.simulation.revision || receipt.day < activation.started_on || receipt.day > world.simulation.date ||
            receipt.target != activation.binding) invalid("Invalid effect receipt scope/date/revision");
        date_valid(receipt.day);
        if (receipt.operation != "lifecycle_cleanup" &&
            (owner.kind != content::NodeKind::effect || receipt.effect_index >= owner.effects.size() ||
             owner.effects[receipt.effect_index].operation != receipt.operation)) invalid("Effect receipt differs from definition");
        if (receipt.operation != "set_variable" && (!receipt.bound_variable.empty() || !receipt.group_calls.empty()))
            invalid("Only a typed variable output can carry group binding evidence");
        if (receipt.operation == "adjust_cash") {
            const auto* before = std::get_if<std::int64_t>(&receipt.before);
            const auto* after = std::get_if<std::int64_t>(&receipt.after);
            const auto amount = std::get<std::int64_t>(owner.effects[receipt.effect_index].arguments.at("amount_cents"));
            if (!before || !after || *before < 0 || *after < 0 || core::checked_subtract(*after, *before) != amount ||
                core::checked_subtract(*before, *after) != receipt.counterparty_delta.value())
                invalid("Cash transfer receipt is unbalanced");
        } else if (receipt.operation == "set_variable") {
            if (receipt.counterparty_delta != core::Cents{0}) invalid("Variable receipt changes counterparty cash");
            const auto& variable_id = std::get<std::string>(owner.effects[receipt.effect_index].arguments.at("variable"));
            auto target = std::string_view{variable_id};
            if (detail::group_port(flow(program, receipt.flow_id), variable_id)) {
                target = detail::resolve_group_variable(program, activation.definition_id, receipt.flow_id,
                    receipt.group_calls, target, true);
                if (receipt.bound_variable != target || receipt.group_calls.empty())
                    invalid("Saved group output differs from its authored call binding");
            } else if (!receipt.bound_variable.empty() || !receipt.group_calls.empty()) {
                invalid("Saved variable receipt has unexpected group binding evidence");
            }
            const auto* metadata = program.registry.fact(target);
            if (!metadata || !metadata->default_value || metadata->scope != receipt.target.scope)
                invalid("Variable receipt has no registered scoped variable");
            scalar_valid(receipt.before, *metadata);
            scalar_valid(receipt.after, *metadata);
        } else {
            if (receipt.counterparty_delta != core::Cents{0}) invalid("Non-cash receipt changes counterparty");
            const auto* before = std::get_if<std::int64_t>(&receipt.before);
            if (!before) invalid("Effect receipt has the wrong before-value type");
            if(receipt.operation=="record_world_catastrophe") {
                const auto* key=std::get_if<std::string>(&receipt.after);
                const auto* end=world.objectives && world.objectives->end?&*world.objectives->end:nullptr;
                if(!key || !program.collection.message(*key) ||
                   *key!=std::get<std::string>(owner.effects[receipt.effect_index].arguments.at("cause_key")) ||
                   receipt.target.scope!=content::Scope::world || !end ||
                   (*before==0 ? (end->cause!=CampaignEndCause::catastrophe || end->source!=*key || end->day!=receipt.day || ++catastrophe_count!=1) :
                    (*before!=1 || world.objectives->mode!=ObjectiveMode::sandbox || receipt.day<end->day)))
                    invalid("A world-ending event must match a committed authored cause.");
            } else if (receipt.operation == "accept_business_offer") {
                const auto* after=std::get_if<std::int64_t>(&receipt.after);
                if (!after || !world.simulation.business_offers || *before<0 || *after<0) invalid("Business offer receipt lacks its actual committed state");
                const auto& id=std::get<std::string>(owner.effects[receipt.effect_index].arguments.at("offer_id"));
                const auto& offers=world.simulation.business_offers->catalog;
                const auto offer=std::find_if(offers.begin(),offers.end(),[&](const auto& row){return row.id==id;});
                if (offer==offers.end() || core::checked_subtract(*before,*after)!=offer->setup_cost.value() ||
                    std::none_of(world.simulation.business_offers->accepted.begin(),world.simulation.business_offers->accepted.end(),[&](const auto& row){return row.offer_id==id && row.accepted_on==receipt.day;}))
                    invalid("Business offer receipt differs from the actual disclosed cost and acceptance");
            } else if (receipt.operation == "temporary_demand_change") {
                if (!world.simulation.market || !std::holds_alternative<std::int64_t>(receipt.after)) invalid("Demand receipt has no customer market");
                const auto& arguments=owner.effects[receipt.effect_index].arguments;
                const auto identity="occurrence."+std::to_string(activation.id)+"."+std::to_string(receipt.effect_index);
                const auto& changes=world.simulation.market->demand_changes;
                const auto change=std::find_if(changes.begin(),changes.end(),[&](const auto& row){return row.id==identity;});
                const auto after=std::get<std::int64_t>(receipt.after);
                const auto shifted=arguments.find("shifted_share_bp");
                const auto share=shifted==arguments.end()?0:std::get<std::int64_t>(shifted->second);
                if (*before<0 || after<0 || static_cast<std::uint64_t>(*before)>core::market_task_limit || static_cast<std::uint64_t>(after)>core::market_task_limit ||
                    change==changes.end() || change->starts_on!=receipt.day || change->segment_id!=std::get<std::string>(arguments.at("segment_id")) ||
                    change->delta.value()!=std::get<std::int64_t>(arguments.at("delta_bp")) || change->shifted_share.value()!=share ||
                    (change->delta.value()>0 && after<*before) || (change->delta.value()<0 && after>*before) ||
                    change->expires_before!=core::advance_days(receipt.day,core::Days{std::get<std::int64_t>(arguments.at("duration_days"))}))
                    invalid("Demand receipt differs from its actual owned modifier or expiry");
                if (share>0 && !workload_receipts.insert(change->id).second)
                    invalid("Customer workload change has duplicate effect receipts");
            } else if (receipt.operation == "record_decision" || receipt.operation == "open_negotiation") {
                const auto* after = std::get_if<std::string>(&receipt.after);
                const auto argument = receipt.operation == "record_decision" ? "value" : "template_id";
                const auto& authored = std::get<std::string>(owner.effects[receipt.effect_index].arguments.at(argument));
                const auto& records = receipt.operation == "record_decision" ? world.decisions : world.negotiations;
                if (!after || *after != authored || *before < 0 || static_cast<std::uint64_t>(*before) >= records.size() ||
                    records[static_cast<std::size_t>(*before)] != *after)
                    invalid("Recorded decision/negotiation receipt differs from its operation and history");
            } else if (receipt.operation == "register_owned_modifier" || receipt.operation == "remove_owned_modifier" ||
                       receipt.operation == "lifecycle_cleanup") {
                const auto* after = std::get_if<std::int64_t>(&receipt.after);
                if (!after || *before < -10000 || *before > 10000 || *after < -10000 || *after > 10000)
                    invalid("Modifier receipt requires basis-point deltas");
                if (receipt.operation == "register_owned_modifier") {
                    if (*after != std::get<std::int64_t>(owner.effects[receipt.effect_index].arguments.at("add_basis_points")))
                        invalid("Modifier receipt delta differs from its definition");
                } else if (*after != 0) invalid("Modifier removal receipt must remove its owned contribution");
                if (receipt.operation == "lifecycle_cleanup" &&
                    (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous ||
                     activation.status != ActivationStatus::completed || owner.kind != content::NodeKind::end || receipt.effect_index != 0))
                    invalid("Lifecycle cleanup receipt has no continuous end boundary");
                if (receipt.operation == "lifecycle_cleanup") {
                    const auto found = std::find_if(recorded_modifiers.begin(), recorded_modifiers.end(), [&](const auto& pair) {
                        return pair.second.activation_id == activation.id && pair.second.add_basis_points == *before;
                    });
                    if (found == recorded_modifiers.end()) invalid("Lifecycle cleanup has no recorded owned contribution");
                    recorded_modifiers.erase(found);
                } else {
                    if (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous &&
                        receipt.operation == "register_owned_modifier") invalid("Noncontinuous activation registered a persistent modifier");
                    const auto& arguments = owner.effects[receipt.effect_index].arguments;
                    const auto& key = std::get<std::string>(arguments.at("key"));
                    const ModifierKey identity{activation.id, activation.binding, key};
                    const auto found = recorded_modifiers.find(identity);
                    const auto expected_before = found == recorded_modifiers.end() ? 0 : found->second.add_basis_points;
                    if (*before != expected_before) invalid("Modifier receipt does not continue its prior owned contribution");
                    if (receipt.operation == "remove_owned_modifier") recorded_modifiers.erase(identity);
                    else recorded_modifiers.insert_or_assign(identity, OwnedModifier{activation.id, activation.binding, key,
                        std::get<std::string>(arguments.at("field")), *after});
                }
            } else invalid("Effect receipt has an unavailable operation");
        }
    }
    if (world.simulation.market && workload_receipts.size()!=static_cast<std::size_t>(
        std::ranges::count_if(world.simulation.market->demand_changes,[](const auto& row) { return row.shifted_share.value()>0; })))
        invalid("Customer workload change lacks a committed occurrence");
    if (recorded_modifiers.size() != state.modifiers.size()) invalid("Saved modifiers differ from committed effect ownership");
    for (const auto& modifier : state.modifiers) {
        const auto found = recorded_modifiers.find({modifier.activation_id, modifier.binding, modifier.key});
        if (found == recorded_modifiers.end() || found->second != modifier)
            invalid("Saved modifier contribution differs from its committed effects");
    }
    for (const auto& presentation : state.presentations) {
        if (quiet_incidents.contains(presentation.activation_id)) invalid("Quiet incident has a presentation");
        const auto& activation = by_id(state.activations, presentation.activation_id);
        const auto& owner = node(program, presentation.flow_id, presentation.node_id);
        if (presentation.day < activation.started_on || presentation.day > world.simulation.date || presentation.title_key != owner.title_key ||
            presentation.text_key != owner.text_key) invalid("Presentation date/localization differs from definition");
        date_valid(presentation.day);
        if (presentation.kind == PresentationKind::dialogue) {
            if (owner.kind != content::NodeKind::dialogue || presentation.choices.size() != owner.choices.size())
                invalid("Saved dialogue choices differ from definition");
            for (std::size_t i = 0; i < owner.choices.size(); ++i)
                if (presentation.choices[i].id != owner.choices[i].id || presentation.choices[i].label_key != owner.choices[i].label_key)
                    invalid("Saved choice identity/text mismatch");
            if (presentation.resolved) {
                if (std::none_of(owner.choices.begin(), owner.choices.end(), [&](const auto& choice) { return choice.id == presentation.selected_choice; }))
                    invalid("Resolved dialogue selected an unknown choice");
            } else if (activation.presentation_id != presentation.id || activation.status != ActivationStatus::awaiting_choice ||
                       !presentation.selected_choice.empty()) invalid("Unresolved dialogue has no suspended activation");
        } else if ((presentation.kind != PresentationKind::news && presentation.kind != PresentationKind::result) ||
                   owner.kind != (presentation.kind == PresentationKind::news ? content::NodeKind::news : content::NodeKind::result) ||
                   !presentation.resolved || !presentation.choices.empty() || !presentation.selected_choice.empty())
            invalid("Invalid saved news/result presentation");
    }
    if(world.objectives && world.objectives->end && world.objectives->end->cause==CampaignEndCause::catastrophe && catastrophe_count!=1)
        invalid("The catastrophe postmortem lost its world event receipt.");
    if (state.prepared_day && std::any_of(state.activations.begin(), state.activations.end(), [](const auto& activation) {
        return activation.status == ActivationStatus::awaiting_choice || activation.status == ActivationStatus::ready;
    })) invalid("Prepared runtime morning still contains required work");
}
void validate_checkpoint(const CompiledProgram& program, const Checkpoint& checkpoint) {
    validate_checkpoint_with_simulation(program, checkpoint, true);
}
} // namespace pgp::runtime
