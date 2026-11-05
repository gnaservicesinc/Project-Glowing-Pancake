#include <pgp/content/registry.hpp>
#include <pgp/core/market.hpp>

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
    boolean("company.has_research_partner", "Whether the company has a current research partnership.");
    boolean("company.is_operating", "Whether company operations remain active.");
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
