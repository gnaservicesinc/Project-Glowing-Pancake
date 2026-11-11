#include <pgp/content/registry.hpp>
#include <pgp/core/market.hpp>
#include <pgp/core/society.hpp>
#include <pgp/core/authority.hpp>

#include <algorithm>
#include <limits>
#include <set>

namespace pgp::content {
namespace {
bool valid_scope(Scope value) { return static_cast<unsigned>(value) <= static_cast<unsigned>(Scope::contract); }
bool valid_unit(Unit value) { return static_cast<unsigned>(value) <= static_cast<unsigned>(Unit::count); }
bool compatible(ValueType type, Unit unit) {
    if (!valid_unit(unit)) return false;
    switch (type) {
    case ValueType::integer: return unit != Unit::boolean && unit != Unit::none;
    case ValueType::boolean: return unit == Unit::boolean;
    case ValueType::text: case ValueType::reference: return unit == Unit::none;
    }
    return false;
}
bool valid_range(ValueType type, Unit unit, std::int64_t low, std::int64_t high) {
    if (low > high) return false;
    if (type == ValueType::boolean) return low == 0 && high == 1;
    if (type != ValueType::integer) return low == 0 && high == 0;
    if (unit == Unit::constant_2014_cents) return true;
    if (unit == Unit::basis_point_delta) return low >= -10000 && high <= 10000;
    if (unit == Unit::basis_points) return low >= 0 && high <= 10000;
    return low >= 0;
}
void validate_ports(const std::vector<PortMetadata>& ports) {
    if (ports.empty()) throw std::invalid_argument("Registry entry needs typed ports");
    std::set<std::string> names;
    for (const auto& port : ports) {
        if (!valid_id(port.name) || !names.insert(port.name).second || !valid_scope(port.scope) ||
            !valid_unit(port.unit) || static_cast<unsigned>(port.type) > static_cast<unsigned>(PortType::value))
            throw std::invalid_argument("Invalid or duplicate registry port");
        if (port.type != PortType::value && port.unit != Unit::none)
            throw std::invalid_argument("Only value ports have units");
    }
}
void validate_common(std::string_view id, std::string_view help) {
    if (!valid_id(id) || help.empty() || help.size() > 2048)
        throw std::invalid_argument("Registry entries need stable IDs and bounded help");
}
void validate_default(const FactMetadata& metadata) {
    if (!metadata.default_value) return;
    const auto& value = *metadata.default_value;
    if (metadata.type == ValueType::integer) {
        const auto* number = std::get_if<std::int64_t>(&value);
        if (!number || *number < metadata.minimum || *number > metadata.maximum)
            throw std::invalid_argument("Variable default exceeds its registered type/range");
    } else if (metadata.type == ValueType::boolean) {
        if (!std::holds_alternative<bool>(value)) throw std::invalid_argument("Variable default requires boolean without coercion");
    } else if (metadata.type == ValueType::text) {
        const auto* text = std::get_if<std::string>(&value);
        if (!text || text->size() > 4096 || text->find('\0') != std::string::npos)
            throw std::invalid_argument("Variable default requires bounded text without NUL");
    } else throw std::invalid_argument("Reference variables are unavailable");
}
} // namespace

bool valid_id(std::string_view id) {
    if (id.empty() || id.size() > 128 || id.front() < 'a' || id.front() > 'z') return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}
std::string_view scope_name(Scope scope) {
    switch (scope) { case Scope::world: return "world"; case Scope::company: return "company";
    case Scope::state: return "state";
    case Scope::capacity_source: return "capacity_source";
    case Scope::project: return "project";
    case Scope::contract: return "contract"; }
    throw std::invalid_argument("Unknown scope");
}
std::string_view unit_name(Unit unit) {
    switch (unit) {
    case Unit::none: return "none"; case Unit::boolean: return "boolean";
    case Unit::constant_2014_cents: return "constant_2014_cents";
    case Unit::basis_points: return "basis_points"; case Unit::basis_point_delta: return "basis_point_delta";
    case Unit::days: return "days"; case Unit::calendar_months: return "calendar_months";
    case Unit::people: return "people"; case Unit::milli_CU: return "milli_CU";
    case Unit::milli_CU_per_day: return "milli_CU_per_day"; case Unit::count: return "count";
    }
    throw std::invalid_argument("Unknown unit");
}
Registry::Registry() { systems_.emplace("foundation", std::vector<std::string>{}); }
void Registry::register_system(std::string id, std::vector<std::string> dependencies) {
    if (!valid_id(id) || systems_.contains(id)) throw std::invalid_argument("Invalid/duplicate system ID");
    std::set<std::string> seen;
    for (const auto& dependency : dependencies) {
        if (!systems_.contains(dependency) || !seen.insert(dependency).second)
            throw std::invalid_argument("System dependencies must already be registered once; cycles are refused");
    }
    systems_.emplace(std::move(id), std::move(dependencies));
}
void Registry::validate_provider(std::string_view provider, const std::vector<std::string>& dependencies) const {
    if (!systems_.contains(provider)) throw std::invalid_argument("Missing provider system");
    std::set<std::string> seen;
    for (const auto& dependency : dependencies)
        if (!systems_.contains(dependency) || !seen.insert(dependency).second)
            throw std::invalid_argument("Missing/duplicate required system");
}
void Registry::register_fact(FactMetadata metadata) {
    validate_common(metadata.id, metadata.help);
    validate_provider(metadata.provider, metadata.dependencies);
    if (static_cast<unsigned>(metadata.boundary) > static_cast<unsigned>(Boundary::committed_result) ||
        static_cast<unsigned>(metadata.visibility) > static_cast<unsigned>(Visibility::hidden_truth))
        throw std::invalid_argument("Invalid phase/knowledge metadata");
    if (!valid_scope(metadata.scope) || !compatible(metadata.type, metadata.unit) ||
        !valid_range(metadata.type, metadata.unit, metadata.minimum, metadata.maximum) || metadata.type == ValueType::reference)
        throw std::invalid_argument("Invalid fact scope/type/unit/range");
    validate_ports(metadata.ports);
    bool value_output = false;
    for (const auto& port : metadata.ports) {
        if (port.scope != metadata.scope) throw std::invalid_argument("Fact port scope mismatch");
        if (port.type == PortType::value && port.output) {
            if (port.unit != metadata.unit) throw std::invalid_argument("Fact value port unit mismatch");
            value_output = true;
        }
    }
    if (!value_output) throw std::invalid_argument("Fact needs an output value port");
    validate_default(metadata);
    const auto id = metadata.id;
    if (!facts_.emplace(id, std::move(metadata)).second) throw std::invalid_argument("Duplicate fact ID");
}
void Registry::register_variable(FactMetadata metadata, Scalar default_value) {
    metadata.default_value = std::move(default_value);
    register_fact(std::move(metadata));
}
void Registry::register_effect(EffectMetadata metadata) {
    validate_common(metadata.id, metadata.help);
    validate_provider(metadata.provider, metadata.dependencies);
    if (static_cast<unsigned>(metadata.boundary) > static_cast<unsigned>(Boundary::committed_result) ||
        static_cast<unsigned>(metadata.visibility) > static_cast<unsigned>(Visibility::hidden_truth))
        throw std::invalid_argument("Invalid phase/knowledge metadata");
    if (metadata.target_scopes.empty()) throw std::invalid_argument("Effect needs target scopes");
    std::set<Scope> scopes;
    for (auto scope : metadata.target_scopes) {
        if (!valid_scope(scope) || !scopes.insert(scope).second) throw std::invalid_argument("Invalid/duplicate effect scope");
    }
    validate_ports(metadata.ports);
    bool execution_input = false;
    for (const auto& port : metadata.ports) {
        if (!scopes.contains(port.scope)) throw std::invalid_argument("Effect port scope mismatch");
        if (port.type == PortType::execution && !port.output) execution_input = true;
    }
    if (!execution_input) throw std::invalid_argument("Effect needs an execution input port");
    std::set<std::string> arguments;
    for (const auto& arg : metadata.arguments) {
        if (!valid_id(arg.name) || !arguments.insert(arg.name).second || !compatible(arg.type, arg.unit) ||
            !valid_range(arg.type, arg.unit, arg.minimum, arg.maximum) || arg.help.empty()) throw std::invalid_argument("Invalid effect argument metadata");
        if ((arg.type == ValueType::reference) != !arg.reference_kind.empty())
            throw std::invalid_argument("Reference arguments need a definition kind");
        if (!arg.choices.empty() && arg.type != ValueType::text) throw std::invalid_argument("Only text arguments have choices");
    }
    const auto id = metadata.id;
    if (!effects_.emplace(id, std::move(metadata)).second) throw std::invalid_argument("Duplicate effect ID");
}
const FactMetadata* Registry::fact(std::string_view id) const {
    const auto found = facts_.find(id); return found == facts_.end() ? nullptr : &found->second;
}
const EffectMetadata* Registry::effect(std::string_view id) const {
    const auto found = effects_.find(id); return found == effects_.end() ? nullptr : &found->second;
}
void Registry::register_relation(RelationMetadata metadata) {
    validate_common(metadata.id, metadata.help);
    validate_provider(metadata.provider, {});
    if (!valid_scope(metadata.owner_scope) || !valid_scope(metadata.member_scope) ||
        metadata.maximum_members == 0 || metadata.maximum_members > maximum_relation_members ||
        static_cast<unsigned>(metadata.visibility) > static_cast<unsigned>(Visibility::hidden_truth))
        throw std::invalid_argument("Invalid relation scope, member bound or visibility");
    const auto id = metadata.id;
    if (!relations_.emplace(id, std::move(metadata)).second) throw std::invalid_argument("Duplicate relation ID");
}
const RelationMetadata* Registry::relation(std::string_view id) const {
    const auto found = relations_.find(id); return found == relations_.end() ? nullptr : &found->second;
}
void Registry::register_committed_fact(CommittedFactMetadata metadata) {
    validate_common(metadata.id, metadata.help);
    validate_provider(metadata.provider, {});
    const auto* result = fact(metadata.result_fact);
    if (!valid_scope(metadata.owner_scope) || !result || result->scope != metadata.subject_scope ||
        result->visibility != Visibility::player_known || result->default_value ||
        (result->boundary != Boundary::settlement && result->boundary != Boundary::committed_result))
        throw std::invalid_argument("A committed occurrence requires a public owning result fact on its subject");
    const auto id = metadata.id;
    if (!committed_facts_.emplace(id, std::move(metadata)).second) throw std::invalid_argument("Duplicate committed fact ID");
}
const CommittedFactMetadata* Registry::committed_fact(std::string_view id) const {
    const auto found = committed_facts_.find(id); return found == committed_facts_.end() ? nullptr : &found->second;
}
Registry foundation_registry() {
    Registry result;
    result.register_system("slice",{"foundation"});
    result.register_system("resilience",{"slice"});
    result.register_system("deployment_risk",{"slice"});
    result.register_system("civic", {"slice"});
    result.register_system("society", {"slice"});
    result.register_system("authority", {"society","civic"});
    result.register_system("frontier", {"slice"});
    result.register_system("succession", {"slice"});
    result.register_system("objectives", {"authority","frontier","succession"});
    result.register_system("campaign", {"objectives","resilience"});
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    auto number = [&](std::string id, Scope scope, Unit unit, std::int64_t low, std::int64_t high, std::string help,std::string provider="foundation") {
        result.register_fact({std::move(id), scope, ValueType::integer, unit, low, high, std::move(help),
            {{"scope", PortType::scope, scope, Unit::none, false}, {"value", PortType::value, scope, unit, true}}, Boundary::committed_result, Visibility::player_known, std::move(provider), {}});
    };
    auto boolean = [&](std::string id, std::string help, Scope scope = Scope::company,std::string provider="foundation") {
        result.register_fact({std::move(id), scope, ValueType::boolean, Unit::boolean, 0, 1, std::move(help),
            {{"scope", PortType::scope, scope, Unit::none, false},
             {"value", PortType::value, scope, Unit::boolean, true}}, Boundary::committed_result, Visibility::player_known, std::move(provider), {}});
    };
    number("company.cash_cents", Scope::company, Unit::constant_2014_cents, minimum, maximum,
           "Available company cash in constant-2014 cents at the current phase.");
    number("company.monthly_compute_bill_cents", Scope::company, Unit::constant_2014_cents, 0, maximum,
           "Committed monthly compute bill in constant-2014 cents, before a new agreement.");
    number("company.workload_stress_basis_points", Scope::company, Unit::basis_points, 0, 10000,
           "Current workload stress, 0 to 10,000 basis points.");
    number("world.public_breakthrough_count", Scope::world, Unit::count, 0, maximum,
           "Number of committed public breakthroughs; an announcement is not new research work.");
    boolean("company.has_exclusive_compute_supplier", "Whether an exclusive supplier contract is already signed.");
    boolean("company.has_compute_lease", "Whether the company has a current compute lease.");
    boolean("world.civic_election_today", "A US election ballot was counted on this prepared morning.", Scope::world, "civic");
    boolean("world.civic_rule_effective_today", "An implemented US or state rule took effect on this prepared morning.", Scope::world, "civic");
    boolean("world.society_month_today", "Actual regional work closed a monthly observation on the previous day.", Scope::world, "society");
    boolean("world.forum_changed_today", "A standards agenda, ballot, local implementation or expiration occurs today.", Scope::world, "society");
    boolean("world.authority_notice_today","A documented institutional decision or public response milestone occurs today.",Scope::world,"authority");
    boolean("world.authority_month_today","Actual institutional work closed a monthly authority check yesterday.",Scope::world,"authority");
    boolean("world.executive_disclosure_today","An executive departed after a concrete unfulfilled publication promise and response window.",Scope::world,"authority");
    number("world.authority_population_coverage_bp",Scope::world,Unit::basis_points,0,10000,"Population covered by at least 60/100 actual governing authority.","authority");
    number("world.authority_output_coverage_bp",Scope::world,Unit::basis_points,0,10000,"Output covered by at least 60/100 actual governing authority.","authority");
    boolean("company.frontier_era_today","The conditional future opens on this prepared morning.",Scope::company,"frontier");
    boolean("company.game_agi_today","Independent assessments establish the game AGI requirements today.",Scope::company,"frontier");
    boolean("company.game_asi_today","Independent experiments establish the game ASI requirements today.",Scope::company,"frontier");
    boolean("company.has_succession_charter","This institution has a finite executive succession charter.",Scope::company,"succession");
    boolean("company.leadership_handover_today","A prepared successor or charter caretaker took office on this morning.",Scope::company,"succession");
    boolean("company.founder_is_chair","The original founder currently holds the personal chair role.",Scope::company,"succession");
    for(std::size_t i=0;i<5;++i) {
        const auto key=std::string(core::welfare_index_key(i));
        number("world.welfare_"+key+"_bp",Scope::world,Unit::basis_points,0,10000,"Population-weighted regional welfare, 100 bp = 1 point.","society");
        number("world.minimum_"+key+"_bp",Scope::world,Unit::basis_points,0,10000,"Lowest region's welfare, 100 bp = 1 point.","society");
    }
    boolean("company.has_research_partner", "Whether the company has a current research partnership.");
    boolean("company.is_operating", "Whether company operations remain active.");
    number("company.campaign_days",Scope::company,Unit::days,0,maximum,
        "Days elapsed since this campaign opened.", "campaign");
    number("company.industry_contacts",Scope::company,Unit::count,0,8192,"Industry people the founder has actually met or been introduced to.","campaign");
    number("company.industry_close_friends",Scope::company,Unit::count,0,8192,"Known close-or-better friendships outside the company.","campaign");
    number("company.referral_contacts",Scope::company,Unit::count,0,8192,"Friends who have supplied a real recruiting referral.","campaign");
    number("company.founder_children",Scope::company,Unit::count,0,1024,"Known children in the founder's active or past family care.","campaign");
    number("company.founder_dependents",Scope::company,Unit::count,0,1024,"Children under eighteen currently in the founder's care.","campaign");
    number("company.family_plans",Scope::company,Unit::count,0,128,"Pending family plans involving the founder.","campaign");
    number("company.family_applications",Scope::company,Unit::count,0,1024,"Independent family applications received by this company.","campaign");
    number("company.founder_dating",Scope::company,Unit::count,0,1,"Founder is currently dating, before engagement.","campaign");
    number("company.founder_engaged",Scope::company,Unit::count,0,1,"Founder is currently engaged, before marriage.","campaign");
    number("company.founder_married",Scope::company,Unit::count,0,1,"Founder is currently married.","campaign");
    number("company.mission_concerns",Scope::company,Unit::count,0,256,"Open concerns about abandoning a company mission.","campaign");
    number("company.personal_rivals",Scope::company,Unit::count,0,256,"People currently in a personal rivalry with the founder.","campaign");
    number("company.rivalry_resignations",Scope::company,Unit::count,0,256,"Employees who resigned over a broken mission commitment.","campaign");
    number("company.founder_close_friends",Scope::company,Unit::count,0,256,"Known close-or-better founder friendships; multiple best friends are allowed.","campaign");
    number("company.known_close_relationships",Scope::company,Unit::count,0,8192,"Close relationships actually shared with the founder.","campaign");
    boolean("company.founder_programming","The founder has acquired Programming, independently of education.",Scope::company,"campaign");
    boolean("company.has_programmer","At least one current, onboarded HQ employee has Programming.",Scope::company,"campaign");
    boolean("company.has_architect","At least one current, onboarded HQ employee has Software Architecture.",Scope::company,"campaign");
    number("company.completed_courses",Scope::company,Unit::count,0,2048,"Completed online courses with acquired skills.","campaign");
    number("company.released_software",Scope::company,Unit::count,0,256,"Completed and released software designs.","campaign");
    boolean("company.founder_genius","The founder selected Actual Genius in the saved introduction.",Scope::company,"campaign");
    boolean("company.founder_outgoing","The founder's saved sociability tendency is outgoing.",Scope::company,"campaign");
    boolean("company.founder_athletic","The founder's saved activity tendency is athletic.",Scope::company,"campaign");
    boolean("company.is_nonprofit","The founding institution has no privately owned founder equity.",Scope::company,"campaign");
    number("company.founder_age",Scope::company,Unit::count,0,10000,"Founder age from the saved 1997 biography; campaign legacy ends at 118.","campaign");
    for(const auto name:{"founder_gender","founder_leg_up","founder_family","childhood_arrival","childhood_curiosity",
        "childhood_setback","childhood_circle","childhood_promise","childhood_ownership","childhood_record","childhood_threshold"}) {
        result.register_fact({std::string("company.")+name,Scope::company,ValueType::text,Unit::none,0,0,
            "The player's own saved formative history. Unrecorded for older scenarios; not a source of private rival knowledge.",
            {{"scope",PortType::scope,Scope::company,Unit::none,false},{"value",PortType::value,Scope::company,Unit::none,true}},
            Boundary::committed_result,Visibility::player_known,"campaign",{}});
    }
    number("world.campaign_days",Scope::world,Unit::days,0,maximum,
        "Days elapsed since this campaign opened.", "campaign");
    const std::pair<std::string_view,std::string_view> company_counts[]{
        {"data_licenses","Data offers already licensed by the company."},
        {"open_licenses","Licensed data offers that permit open weight release."},
        {"research_only_licenses","Licensed offers restricted to research use."},
        {"trained_models","Model checkpoints with work actually performed."},
        {"independent_evaluations","Completed independent evaluations of actual checkpoints."},
        {"published_research","Research results actually published by this company."},
        {"adopted_research","Public research credits adopted from another company."},
        {"frontier_systems","Registered systems built from deployed services."},
        {"frontier_assessments","Completed Frontier assessment reports, including reports that did not qualify."},
        {"frontier_programs","Authorized Frontier research queues or standing tracks."},
        {"successful_refreshes","Evaluated product refreshes that actually deployed a replacement."},
        {"held_refreshes","Completed refresh runs that retained the previous deployment."},
        {"product_launches","Deployments that actually committed, including replacements."},
        {"active_services","Available product channels with an actual current deployment."},
        {"hosted_services","Available licensed or open-weight products using customer compute."},
        {"active_contracts","Signed contracts currently serving."},
        {"missed_contracts","Signed contracts that missed their service start."},
        {"contract_shortfalls_today","Contracts with unmet promised tasks in the last settled day."},
        {"served_tasks_today","Useful tasks actually settled for this company on the last work day."},
        {"price_changes","Committed changes to live product prices."},
        {"executive_appointments","Executive appointments still in force."},
        {"vesting_grants","Outstanding hybrid compensation grants inside the reserved share pool."},
        {"open_board_votes","Current board motions with a response window."},
        {"board_refusals","Board motions that expired or were rejected."},
        {"executive_remedies","Corporate disputes resolved, waived or accelerated under recorded terms."},
        {"handover_count","Completed chief executive handovers, including caretaker continuity."},
        {"capital_events","New corporate debt or equity entries, excluding opening ownership."},
        {"resolved_workouts","Liquidity workouts resolved by settling the recorded obligations."},
        {"acquisitions","Companies actually acquired by the player's company."},
        {"site_orders","Owned sites under construction or operating."},
        {"online_sites","Owned sites actually commissioned and still open."},
        {"online_racks","Delivered, commissioned racks still in service."},
        {"open_site_incidents","Outstanding disruptions with recovery work still due."},
        {"site_repairs","Site incidents with a documented repair."},
        {"paid_site_claims","Site incidents with an insurance payment actually received."},
        {"ready_replicas","Synchronized and available geographic service replicas."},
        {"wholesale_contracts","Funded wholesale contracts currently in term."},
        {"used_wholesale_contracts","Contracts with at least one day of metered wholesale compute use."},
        {"public_testimonies","Disclosed independent evidence filed in the public record."},
        {"fulfilled_promises","Public commitments marked met by recorded work or publication."},
        {"public_cases","Public cases still awaiting a remedy or review."},
        {"public_remedies","Public cases resolved after actual review or remedial work."},
        {"approved_mandates","Institutional agreements approved and still in term."}
    };
    for (const auto& [id,help] : company_counts)
        number("company."+std::string(id),Scope::company,Unit::count,0,maximum,std::string(help),"campaign");
    number("world.rival_entries",Scope::world,Unit::count,0,maximum,
        "Rival entries publicly announced by the shared competition simulation.","campaign");
    number("world.rival_closures",Scope::world,Unit::count,0,maximum,
        "Rival closures recorded in public news.","campaign");
    number("world.public_regions",Scope::world,Unit::count,0,8,
        "Regions with actual player public-service delivery in the last settled month.","campaign");
    number("world.durable_regions",Scope::world,Unit::count,0,8,
        "Regions with retained gains from measured public delivery and independent evidence.","campaign");
    boolean("company.small_supplier_eligible","The small opening supplier lease can actually be accepted.",Scope::company,"campaign");
    boolean("company.document_recovery_eligible","The opening narrow document pilot can actually be accepted.",Scope::company,"campaign");
    boolean("company.regional_recovery_eligible","The shorter regional recovery pilot can actually be accepted.",Scope::company,"campaign");
    boolean("company.cooperative_burst_eligible","The cooperative's short power-included lease can actually be accepted.",Scope::company,"campaign");
    boolean("company.independent_lab_eligible","The independent compute lab's annual term can actually be accepted.",Scope::company,"campaign");
    result.register_relation({"company.capacity_sources", Scope::company, Scope::capacity_source, core::capacity_source_limit,
        "Compute sources controlled by this company, including leased supply."});
    result.register_relation({"company.projects", Scope::company, Scope::project, core::capacity_project_limit,
        "This company's capacity projects, including completed and cancelled work."});
    result.register_relation({"capacity_source.projects", Scope::capacity_source, Scope::project, core::capacity_project_limit,
        "This company's projects assigned to the selected compute source."});
    result.register_relation({"company.contracts", Scope::company, Scope::contract, core::market_contract_limit,
        "This company's accepted contracts, including pending and historical contracts."});
    number("capacity_source.effective_milli_cu_per_day", Scope::capacity_source, Unit::milli_CU_per_day, 0, maximum,
        "Effective daily compute capacity after supply changes.");
    number("project.completed_milli_cu", Scope::project, Unit::milli_CU, 0, maximum,
        "Actual compute work recorded for this project.");
    boolean("project.is_active", "Whether the project is still active.", Scope::project);
    boolean("project.is_completed", "Whether the finite project finished its work.", Scope::project);
    boolean("contract.is_active", "Whether this contract currently serves.", Scope::contract);
    boolean("contract.missed_start", "Whether this contract missed its start deadline.", Scope::contract);
    boolean("project.lab_research_completed","Whether actual lab research work completed for this project.",Scope::project,"slice");
    boolean("project.has_evaluation_finding","Whether this completed evaluation recorded a concrete covered-subgroup finding.",Scope::project,"slice");
    boolean("project.has_deployment_incident","Whether this serving project has an actual discovered deployment incident.",Scope::project,"deployment_risk");
    boolean("capacity_source.has_site_outcome","Whether this owned source has an actual site disruption, recovery or insurance outcome.",Scope::capacity_source,"resilience");
    number("contract.outcome_tasks",Scope::contract,Unit::count,0,static_cast<std::int64_t>(core::market_task_limit),
        "Actual useful tasks in the committed contract settlement; failure can deliver zero.","slice");
    boolean("company.in_liquidity_workout","Whether an actual warned liquidity workout is active.",Scope::company,"slice");
    boolean("company.has_evaluation_finding","Whether completed player evidence contains a concrete limitation.",Scope::company,"slice");
    boolean("company.has_enterprise_segment","Whether the first-playable enterprise customer segment exists.",Scope::company,"slice");
    boolean("company.market_shift_active","Whether an unexpired external demand change is already active.",Scope::company,"slice");
    boolean("company.supplier_offer_eligible","Whether an unaccepted supplier offer is currently affordable and feasible.",Scope::company,"slice");
    boolean("company.recovery_offer_eligible","Whether a real available product can honor an unaccepted recovery pilot.",Scope::company,"slice");
    number("company.competitive_notice_count",Scope::company,Unit::count,0,4096,
        "Number of visible rival publications, budget decisions, closures and disclosed heavyweight warnings; never private scores.","slice");
    result.register_committed_fact({"capacity.project_completed", Scope::company, Scope::project, "project.completed_milli_cu",
        "Finite project work completed at settlement. Dispatch on the next prepared morning."});
    result.register_committed_fact({"market.contract_missed_start", Scope::company, Scope::contract, "contract.missed_start",
        "An accepted contract missed its start deadline at settlement. Dispatch on the next prepared morning."});
    result.register_committed_fact({"lab.research_completed",Scope::company,Scope::project,"project.lab_research_completed",
        "Actual research completion; publishing this fact grants no additional knowledge.","slice"});
    result.register_committed_fact({"lab.evaluation_finding",Scope::company,Scope::project,"project.has_evaluation_finding",
        "Actual independently recorded evaluation limitation, with its named subgroup evidence.","slice"});
    result.register_committed_fact({"market.pilot_outcome",Scope::company,Scope::contract,"contract.outcome_tasks",
        "Actual first service, final service or missed-start settlement and its useful work.","slice"});
    result.register_committed_fact({"infrastructure.site_outcome",Scope::company,Scope::capacity_source,"capacity_source.has_site_outcome",
        "An owned site's recorded disruption, recovery, insurance payment or funded replica transfer.","resilience"});
    result.register_committed_fact({"deployment.incident_outcome",Scope::company,Scope::project,"project.has_deployment_incident",
        "An actual deployment incident or completed response discovered on this prepared morning.","deployment_risk"});
    auto effect = [&](std::string id, std::vector<ArgumentMetadata> args, std::string help,std::string provider="foundation") {
        result.register_effect({std::move(id), {Scope::company}, std::move(args), std::move(help),
            {{"target", PortType::scope, Scope::company, Unit::none, false},
             {"in", PortType::execution, Scope::company, Unit::none, false},
             {"out", PortType::execution, Scope::company, Unit::none, true}}, Boundary::prepared_morning, Visibility::player_known, std::move(provider), {}});
    };
    effect("adjust_cash", {{"amount_cents", ValueType::integer, Unit::constant_2014_cents, minimum, maximum,
           true, "", "Signed cash delta in constant-2014 cents.", {}}}, "Request a checked company cash delta through a transaction.");
    effect("record_decision", {{"value", ValueType::text, Unit::none, 0, 0, true, "",
           "Stable decision marker, not executable text.", {}}}, "Record a causal decision marker without changing a contract.");
    effect("open_negotiation", {{"template_id", ValueType::reference, Unit::none, 0, 0, true, "deal",
           "Registered deal template; opening it creates no payment or signed contract.", {}}},
           "Open a referenced negotiation; a later confirmed command creates commitments.");
    effect("accept_business_offer",{{"offer_id",ValueType::reference,Unit::none,0,0,true,"business_offer",
        "Scenario-owned business offer ID; acceptance rechecks cash, availability and capacity.",{}}},
        "Accept the disclosed lease or recovery pilot through the same validated business transaction as ordinary commands.","slice");
    effect("temporary_demand_change",{
        {"segment_id",ValueType::reference,Unit::none,0,0,true,"market_segment","Existing customer segment ID.",{}},
        {"delta_bp",ValueType::integer,Unit::basis_point_delta,-2500,2500,true,"","Temporary external demand change, with existing contract promises protected.",{}},
        {"shifted_share_bp",ValueType::integer,Unit::basis_points,0,core::market_shift_share_limit,false,"",
            "Share of agent tasks using the changed workload until the exclusive expiry.",{}},
        {"duration_days",ValueType::integer,Unit::days,1,90,true,"","Exclusive expiry after this many days.",{}}},
        "Commit one bounded dated demand modifier owned by this occurrence; it never changes settled work.","slice");
    effect("register_owned_modifier", {
           {"key", ValueType::text, Unit::none, 0, 0, true, "", "Owner-local modifier key.", {}},
           {"field", ValueType::text, Unit::none, 0, 0, true, "", "Registered modifiable field.", {"research_coordination_basis_points"}},
           {"add_basis_points", ValueType::integer, Unit::basis_point_delta, -10000, 10000, true, "",
            "Signed additive basis-point modifier, removed by owner/key.", {}}},
           "Register one owned modifier; do not repeatedly mutate its base value.");
    effect("remove_owned_modifier", {{"key", ValueType::text, Unit::none, 0, 0, true, "", "Owner-local modifier key.", {}}},
           "Remove only this activation's modifier with the named key.");
    result.register_effect({"record_world_catastrophe",{Scope::world},
        {{"cause_key",ValueType::text,Unit::none,0,0,true,"","Localized message explaining the irreversible world event.",{}}},
        "Record a scenario-ending world event in a scored century campaign. The flow supplies its causal trigger and explanation.",
        {{"target",PortType::scope,Scope::world,Unit::none,false},{"in",PortType::execution,Scope::world,Unit::none,false},
         {"out",PortType::execution,Scope::world,Unit::none,true}},Boundary::prepared_morning,Visibility::player_known,"objectives",{}});
    result.register_effect({"set_variable", {Scope::company, Scope::world, Scope::state},
        {{"variable", ValueType::text, Unit::none, 0, 0, true, "", "Registered scoped variable ID.", {}}},
        "Assign a checked typed expression to an explicitly scoped variable.",
        {{"target", PortType::scope, Scope::company, Unit::none, false},
         {"in", PortType::execution, Scope::company, Unit::none, false},
         {"out", PortType::execution, Scope::company, Unit::none, true}},
        Boundary::prepared_morning, Visibility::player_known, "foundation", {}});
    return result;
}
} // namespace pgp::content
