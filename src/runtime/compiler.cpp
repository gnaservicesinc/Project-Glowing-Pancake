#include <pgp/runtime/compiler.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace pgp::runtime {
namespace {
using Json = nlohmann::json;
using namespace pgp::content;

[[noreturn]] void fail(std::string code, const std::string& path, std::string detail) {
    throw CompileError(std::move(code), path, std::move(detail));
}
void shape(bool valid, const std::string& path, std::string detail) {
    if (!valid) fail("typed_shape", path, std::move(detail));
}
std::string path_child(const std::string& path, std::string_view child) {
    return path + "/" + std::string(child);
}
Json scalar_json(const Scalar& scalar, const std::string& path) {
    if (const auto* text = std::get_if<std::string>(&scalar))
        shape(!text->empty() && text->size() <= 4096 && text->find('\0') == std::string::npos,
              path, "Scalar text must be nonempty and bounded");
    return std::visit([](const auto& value) { return Json(value); }, scalar);
}
Json expression_json(const Expression& expression, const std::string& path, std::size_t depth, std::size_t& count) {
    if (depth > 16 || ++count > 128) fail("budget", path, "Expression nesting/term budget exceeded");
    const bool zero = expression.value == Scalar{std::int64_t{0}};
    if (expression.kind == ExpressionKind::literal) {
        shape(expression.fact.empty() && expression.children.empty() && expression.numerator == 0 && expression.denominator == 1,
              path, "Literal has incompatible expression payloads");
        if (const auto* text = std::get_if<std::string>(&expression.value))
            shape(text->size() <= 4096 && text->find('\0') == std::string::npos, path, "Literal text is outside its bound");
        return {{"literal", std::visit([](const auto& value) { return Json(value); }, expression.value)}, {"unit", unit_name(expression.unit)}};
    }
    shape(zero && expression.unit == Unit::none, path, "Nonliteral expression cannot carry a literal value/unit");
    if (expression.kind == ExpressionKind::fact) {
        shape(expression.children.empty() && expression.numerator == 0 && expression.denominator == 1,
              path, "Fact expression has incompatible payloads");
        return {{"fact", expression.fact}};
    }
    shape(expression.fact.empty(), path, "Arithmetic expression cannot carry a fact ID");
    if (expression.kind == ExpressionKind::scale) {
        shape(expression.children.size() == 1 && expression.numerator <= 1'000'000 && expression.denominator >= 1 && expression.denominator <= 1'000'000,
              path, "Scale requires one value and a bounded integer fraction");
        return {{"scale", {{"value", expression_json(expression.children.front(), path_child(path, "scale/value"), depth + 1, count)},
                           {"numerator", expression.numerator}, {"denominator", expression.denominator}}}};
    }
    shape((expression.kind == ExpressionKind::add || expression.kind == ExpressionKind::subtract) && expression.children.size() == 2 &&
          expression.numerator == 0 && expression.denominator == 1, path, "Binary arithmetic requires exactly two values");
    Json children = Json::array();
    for (std::size_t i = 0; i < 2; ++i) children.push_back(expression_json(expression.children[i], path_child(path, std::to_string(i)), depth + 1, count));
    return {{expression.kind == ExpressionKind::add ? "add" : "subtract", std::move(children)}};
}
Json expression_json(const Expression& expression, const std::string& path) {
    std::size_t count = 0;
    return expression_json(expression, path, 0, count);
}
std::string comparison_name(Comparison comparison, const std::string& path) {
    switch (comparison) {
    case Comparison::equal: return "eq";
    case Comparison::not_equal: return "ne";
    case Comparison::less: return "lt";
    case Comparison::less_equal: return "lte";
    case Comparison::greater: return "gt";
    case Comparison::greater_equal: return "gte";
    }
    fail("operation", path, "Unknown typed comparison");
}
Json condition_json(const Condition& condition, const std::string& path,
                    std::size_t depth, std::size_t& count) {
    if (depth > 16 || ++count > 128) fail("budget", path, "Condition nesting/count budget exceeded");
    if (condition.kind == ConditionKind::holds_for_days) {
        shape(!condition.clause && condition.children.size() == 1 && !condition.predicate_id.empty() && condition.duration_days != 0,
              path, "Duration predicates need one child, stable ID and positive days without a fact payload");
        return {{"holds_for_days", {{"id", condition.predicate_id}, {"days", condition.duration_days},
            {"condition", condition_json(condition.children.front(), path_child(path, "holds_for_days/condition"), depth + 1, count)}}}};
    }
    shape(condition.predicate_id.empty() && condition.duration_days == 0, path, "Only duration predicates carry temporal payloads");
    if (condition.kind == ConditionKind::clause) {
        shape(condition.clause.has_value() && condition.children.empty(), path, "Clause must contain only one fact comparison");
        const auto& clause = *condition.clause;
        if (clause.expression) shape(clause.value == Scalar{std::int64_t{0}}, path, "Expression comparisons need the zero scalar placeholder");
        return {{"fact", clause.fact}, {"op", comparison_name(clause.comparison, path)},
                {"unit", unit_name(clause.unit)}, {"value", clause.expression ? expression_json(*clause.expression, path_child(path, "value")) : scalar_json(clause.value, path)}};
    }
    shape(!condition.clause.has_value() && !condition.children.empty() && condition.children.size() <= 128,
          path, "Aggregate conditions require children and no fact payload");
    if (condition.kind == ConditionKind::not_condition) {
        shape(condition.children.size() == 1, path, "Not requires exactly one child");
        return {{"not", condition_json(condition.children.front(), path_child(path, "not"), depth + 1, count)}};
    }
    shape(condition.kind == ConditionKind::all_of || condition.kind == ConditionKind::any_of,
          path, "Unknown typed condition kind");
    const auto name = condition.kind == ConditionKind::all_of ? "all_of" : "any_of";
    auto children = Json::array();
    for (std::size_t i = 0; i < condition.children.size(); ++i)
        children.push_back(condition_json(condition.children[i], path_child(path_child(path, name), std::to_string(i)), depth + 1, count));
    return {{name, std::move(children)}};
}
Json condition_json(const Condition& condition, const std::string& path) {
    std::size_t count = 0;
    return condition_json(condition, path, 0, count);
}
Json interval_json(Interval interval, const std::string& path) {
    shape(interval.unit == IntervalUnit::days || interval.unit == IntervalUnit::calendar_months,
          path, "Unknown interval unit");
    return {{"unit", interval.unit == IntervalUnit::days ? "days" : "calendar_months"}, {"value", interval.value}};
}
Json anchor_json(const Anchor& anchor, const std::string& path) {
    std::string kind;
    switch (anchor.kind) {
    case AnchorKind::campaign_start: kind = "campaign_start"; break;
    case AnchorKind::scope_created: kind = "scope_created"; break;
    case AnchorKind::month_start: kind = "month_start"; break;
    case AnchorKind::fixed_date: kind = "fixed_date"; break;
    default: fail("operation", path, "Unknown anchor kind");
    }
    shape(anchor.date.has_value() == (anchor.kind == AnchorKind::fixed_date), path,
          "Only a fixed anchor must contain an explicit date");
    Json result{{"kind", kind}, {"offset", interval_json(anchor.offset, path_child(path, "offset"))},
                {"invalid_day", "clamp_to_month_end"}};
    if (anchor.date) result["date"] = core::iso_date(*anchor.date);
    return result;
}
Json lifecycle_json(const Lifecycle& lifecycle, Scope scope, const std::string& path) {
    shape(lifecycle.occurrence_scope == scope, path, "Occurrence scope must match the flow scope");
    const bool continuous = lifecycle.mode == LifecycleMode::continuous;
    const bool interval = lifecycle.mode == LifecycleMode::interval;
    const bool scheduled = lifecycle.mode == LifecycleMode::scheduled;
    const bool recurring = lifecycle.mode == LifecycleMode::recurring;
    shape(lifecycle.exit_when.has_value() == continuous && lifecycle.interval.has_value() == interval &&
              lifecycle.anchor.has_value() == interval && lifecycle.due_date.has_value() == scheduled &&
              (continuous || !lifecycle.allow_reentry) && (recurring || lifecycle.cooldown_days == 0),
          path, "Lifecycle contains missing or incompatible typed fields");
    Json result{{"occurrence_scope", scope_name(scope)}};
    switch (lifecycle.mode) {
    case LifecycleMode::once:
        result["mode"] = "once"; result["max_occurrences_per_scope"] = lifecycle.max_occurrences; break;
    case LifecycleMode::recurring:
        result["mode"] = "recurring"; result["max_occurrences_per_scope"] = lifecycle.max_occurrences;
        result["cooldown_days"] = lifecycle.cooldown_days;
        result["repeat_policy"] = "after_cooldown_while_eligible"; break;
    case LifecycleMode::interval:
        result["mode"] = "interval"; result["max_occurrences_per_scope"] = lifecycle.max_occurrences;
        result["every"] = interval_json(*lifecycle.interval, path_child(path, "every"));
        result["anchor"] = anchor_json(*lifecycle.anchor, path_child(path, "anchor"));
        result["miss_policy"] = "skip"; break;
    case LifecycleMode::scheduled:
        result["mode"] = "scheduled"; result["max_occurrences_per_scope"] = lifecycle.max_occurrences;
        result["due_date"] = core::iso_date(*lifecycle.due_date); result["miss_policy"] = "skip"; break;
    case LifecycleMode::continuous:
        shape(lifecycle.max_occurrences == 1, path, "Continuous lifecycle has no finite occurrence field");
        result["mode"] = "continuous"; result["stacking"] = "one_per_scope";
        result["allow_reentry"] = lifecycle.allow_reentry;
        result["exit_when"] = condition_json(*lifecycle.exit_when, path_child(path, "exit_when")); break;
    default: fail("lifecycle", path, "Unknown typed lifecycle mode");
    }
    return result;
}
Json effect_json(const Effect& effect, Scope scope, const std::string& path) {
    shape(effect.arguments.size() <= 64, path, "Too many effect arguments");
    Scope target_scope;
    if (effect.target == "self") target_scope = scope;
    else if (effect.target == "world") target_scope = Scope::world;
    else fail("target", path, "Explicit registered effect target required");
    shape(effect.target_scope == target_scope, path, "Normalized effect target scope is inconsistent");
    if (effect.operation == "set_variable") {
        shape(effect.target == "self" && effect.value_expression.has_value() && effect.arguments.size() == 1 && effect.arguments.contains("variable"),
              path, "Variable assignment requires self, one variable ID and a typed expression");
        return {{"op", effect.operation}, {"target", effect.target}, {"variable", scalar_json(effect.arguments.at("variable"), path)},
                {"value", expression_json(*effect.value_expression, path_child(path, "value"))}};
    }
    shape(!effect.value_expression, path, "Only variable assignment carries a value expression");
    Json result{{"op", effect.operation}, {"target", effect.target}};
    for (const auto& [name, value] : effect.arguments) {
        shape(name != "op" && name != "target", path, "Effect arguments cannot overwrite operation/target");
        result[name] = scalar_json(value, path_child(path, name));
    }
    return result;
}
Json node_json(const Node& node, Scope scope, const std::string& path) {
    const bool effect = node.kind == NodeKind::effect;
    const bool presentation = node.kind == NodeKind::news || node.kind == NodeKind::result || node.kind == NodeKind::dialogue;
    const bool dialogue = node.kind == NodeKind::dialogue;
    const bool sequence = node.kind == NodeKind::sequence;
    const bool delay = node.kind == NodeKind::delay;
    const bool subflow = node.kind == NodeKind::subflow;
    const bool random = node.kind == NodeKind::random;
    shape((effect ? !node.effects.empty() : node.effects.empty()) && node.effects.size() <= 64 &&
              (presentation ? !node.text_key.empty() : node.text_key.empty()) &&
              (dialogue ? !node.title_key.empty() : node.title_key.empty()) &&
              (dialogue ? !node.choices.empty() : node.choices.empty()) && node.choices.size() <= 32 &&
              (dialogue ? !node.default_choice.empty() : node.default_choice.empty()) &&
              (sequence ? !node.children.empty() : node.children.empty()) && node.children.size() <= 64 &&
              (delay ? node.delay_days != 0 : node.delay_days == 0) &&
              (subflow ? !node.subflow_id.empty() : node.subflow_id.empty()) &&
              (random ? node.branches.size() >= 2 && node.branches.size() <= 32 : node.branches.empty()) &&
              (random ? !node.random_stream.empty() : node.random_stream.empty()),
          path, "Node contains missing or incompatible typed payloads");
    Json result;
    switch (node.kind) {
    case NodeKind::end: result["kind"] = "end"; break;
    case NodeKind::effect:
        result["kind"] = "effect"; result["effects"] = Json::array();
        for (std::size_t i = 0; i < node.effects.size(); ++i)
            result["effects"].push_back(effect_json(node.effects[i], scope, path_child(path_child(path, "effects"), std::to_string(i))));
        break;
    case NodeKind::news: result["kind"] = "news"; result["text_key"] = node.text_key; break;
    case NodeKind::result:
        result["kind"] = "result"; result["text_key"] = node.text_key; result["show_committed_changes"] = true; break;
    case NodeKind::dialogue:
        result["kind"] = "dialogue"; result["title_key"] = node.title_key; result["text_key"] = node.text_key;
        result["default_choice_if_dismissed"] = node.default_choice; result["choices"] = Json::array();
        for (const auto& choice : node.choices)
            result["choices"].push_back({{"id", choice.id}, {"label_key", choice.label_key}, {"next", choice.next}});
        break;
    case NodeKind::delay: result["kind"] = "delay"; result["duration_days"] = node.delay_days; break;
    case NodeKind::sequence: result["kind"] = "sequence"; result["children"] = node.children; break;
    case NodeKind::subflow: result["kind"] = "subflow"; result["flow_id"] = node.subflow_id; break;
    case NodeKind::random:
        result["kind"] = "random"; result["stream"] = node.random_stream; result["branches"] = Json::array();
        for (const auto& branch : node.branches)
            result["branches"].push_back({{"id", branch.id}, {"weight", branch.weight}, {"next", branch.next}});
        break;
    default: fail("operation", path, "Unknown typed node kind");
    }
    if (node.kind == NodeKind::end || dialogue || random) shape(node.next.empty(), path, "Terminal/branch node cannot have an implicit next link");
    else { shape(!node.next.empty(), path, "Node requires an explicit next link"); result["next"] = node.next; }
    return result;
}
Json definition_json(const Definition& definition, const std::string& key) {
    const auto path = path_child("/definitions", key);
    shape(definition.id == key && valid_id(key), path, "Definition map key must equal its stable ID");
    Json result{{"id", definition.id}, {"revision", definition.revision}};
    if (const auto* flow = std::get_if<FlowDefinition>(&definition.data)) {
        shape(!flow->nodes.empty() && flow->nodes.size() <= 512 && flow->tags.size() <= 32, path, "Flow node/tag budget exceeded");
        result["kind"] = "flow"; result["scope"] = scope_name(flow->scope);
        result["trigger"] = condition_json(flow->trigger, path_child(path, "trigger"));
        result["lifecycle"] = lifecycle_json(flow->lifecycle, flow->scope, path_child(path, "lifecycle"));
        result["entry_node"] = flow->entry_node;
        if (!flow->exit_node.empty()) result["exit_node"] = flow->exit_node;
        if (flow->max_node_visits) result["max_node_visits"] = flow->max_node_visits;
        if (!flow->tags.empty()) result["tags"] = flow->tags;
        result["nodes"] = Json::object();
        for (const auto& [name, node] : flow->nodes) result["nodes"][name] = node_json(node, flow->scope, path_child(path_child(path, "nodes"), name));
    } else if (const auto* research = std::get_if<ResearchDefinition>(&definition.data)) {
        shape(research->all_of.size() <= 512 && research->any_of.size() <= 512, path, "Research prerequisite budget exceeded");
        result["kind"] = "research"; result["prerequisites"] = {{"all_of", research->all_of}, {"any_of", research->any_of}};
        result["cash_cents"] = research->cash.value(); result["staff_days"] = research->staff_days.value();
        result["work_milli_CU"] = research->work.value();
    } else if (const auto* scenario = std::get_if<ScenarioDefinition>(&definition.data)) {
        shape(scenario->headquarters_choices.size() <= 64 && scenario->monthly_commitments.size() <= 128 &&
                  scenario->ownership.size() <= 128 && scenario->research.size() <= 512 && scenario->flows.size() <= 512,
              path, "Scenario collection budget exceeded");
        result["kind"] = "scenario"; result["starts_on"] = core::iso_date(scenario->starts_on);
        result["ends_before"] = core::iso_date(scenario->ends_before); result["seed"] = scenario->seed;
        result["cash_cents"] = scenario->cash.value(); result["capacity_milli_CU_per_day"] = scenario->leased_capacity.value();
        result["lease_energy_included"] = scenario->lease_energy_included; result["team_people"] = scenario->team_people;
        result["headquarters_choices"] = scenario->headquarters_choices;
        result["monthly_commitments_cents"] = Json::object(); result["ownership_basis_points"] = Json::object();
        for (const auto& [name, value] : scenario->monthly_commitments) result["monthly_commitments_cents"][name] = value.value();
        for (const auto& [name, value] : scenario->ownership) result["ownership_basis_points"][name] = value.value();
        result["research"] = scenario->research; result["flows"] = scenario->flows;
        if (scenario->market)
            result["market"] = Json::parse(canonical_market_setup_bytes(*scenario->market,scenario->starts_on));
        if (scenario->business_plan)
            result["business_plan"] = Json::parse(canonical_business_plan_bytes(*scenario->business_plan,scenario->starts_on));
    } else if (const auto* deal = std::get_if<DealDefinition>(&definition.data)) {
        result["kind"] = "deal"; result["minimum_payment_cents"] = deal->minimum_payment.value();
        result["minimum_duration_days"] = deal->minimum_duration.value();
    } else fail("unsupported_kind", path, "Unknown typed definition variant");
    return result;
}
Json collection_json(const Collection& collection) {
    shape(!collection.definitions.empty() && collection.definitions.size() <= 512, "/definitions", "Definition count budget exceeded");
    Json result{{"content_schema", collection.schema}, {"content_version", collection.content_version},
                {"currency_basis", "constant_2014_cents"}, {"definitions", Json::array()}};
    if (!collection.variables.empty()) {
        std::vector<const FactMetadata*> variables;
        for (const auto& variable : collection.variables) variables.push_back(&variable);
        std::sort(variables.begin(), variables.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
        result["variables"] = Json::array();
        for (const auto* variable : variables) {
            const auto type = variable->type == ValueType::integer ? "integer" : variable->type == ValueType::boolean ? "boolean" : "text";
            result["variables"].push_back({{"id", variable->id}, {"scope", scope_name(variable->scope)}, {"type", type},
                {"unit", unit_name(variable->unit)}, {"minimum", variable->minimum}, {"maximum", variable->maximum},
                {"default", std::visit([](const auto& value) { return Json(value); }, *variable->default_value)},
                {"help", variable->help}, {"visibility", variable->visibility == Visibility::hidden_truth ? "hidden_truth" : "player_known"}});
        }
    }
    for (const auto& [name, definition] : collection.definitions)
        result["definitions"].push_back(definition_json(definition, name));
    return result;
}
std::vector<std::string> edges(const Node& node) {
    auto result = node.children;
    if (!node.next.empty()) result.push_back(node.next);
    for (const auto& choice : node.choices) result.push_back(choice.next);
    for (const auto& branch : node.branches) result.push_back(branch.next);
    return result;
}
void validate_sequence_regions(const Collection& collection) {
    std::size_t work = 0;
    for (const auto& [flow_id, definition] : collection.definitions) {
        const auto* flow = std::get_if<FlowDefinition>(&definition.data);
        if (!flow) continue;
        for (const auto& [sequence_id, sequence] : flow->nodes) {
            if (sequence.kind != NodeKind::sequence) continue;
            std::map<std::string, std::size_t, std::less<>> child_owner;
            for (std::size_t child = 0; child < sequence.children.size(); ++child) {
                std::set<std::string> visited;
                std::vector<std::string> pending{sequence.children[child]};
                while (!pending.empty()) {
                    if (++work > 1'000'000) fail("budget", "/definitions", "Sequence-region analysis budget exceeded");
                    const auto current = std::move(pending.back()); pending.pop_back();
                    if (!visited.insert(current).second) continue;
                    const auto& node = flow->nodes.at(current);
                    // A shared terminal simply returns to the correct frame;
                    // sharing an effect/presentation path would run it twice.
                    if (node.kind == NodeKind::end) continue;
                    const auto path = "/definitions/" + flow_id + "/nodes/" + sequence_id;
                    if (current == sequence.next)
                        fail("sequence_boundary", path, "A child jumps into its sequence's return path before returning");
                    const auto [owner, inserted] = child_owner.emplace(current, child);
                    if (!inserted && owner->second != child)
                        fail("cross_sibling", path, "Sequence child regions share a nonterminal node; use an explicit subflow instead");
                    for (const auto& target : edges(node)) pending.push_back(target);
                }
            }
        }
    }
}
std::vector<CompileDiagnostic> find_lints(const Collection& collection) {
    std::map<std::string, std::vector<std::string>, std::less<>> dependents;
    std::set<std::string> blocking;
    std::deque<std::string> pending;
    const auto key = [](const std::string& flow, const std::string& node) { return flow + ":" + node; };
    for (const auto& [flow_id, definition] : collection.definitions) {
        const auto* flow = std::get_if<FlowDefinition>(&definition.data);
        if (!flow) continue;
        for (const auto& [node_id, node] : flow->nodes) {
            const auto source = key(flow_id, node_id);
            for (const auto& target : edges(node)) dependents[key(flow_id, target)].push_back(source);
            if (node.kind == NodeKind::subflow) {
                const auto& nested = std::get<FlowDefinition>(collection.definitions.at(node.subflow_id).data);
                dependents[key(node.subflow_id, nested.entry_node)].push_back(source);
            }
            if (node.kind == NodeKind::delay || node.kind == NodeKind::dialogue) {
                blocking.insert(source); pending.push_back(source);
            }
        }
    }
    while (!pending.empty()) {
        const auto target = std::move(pending.front()); pending.pop_front();
        for (const auto& source : dependents[target])
            if (blocking.insert(source).second) pending.push_back(source);
    }
    std::vector<CompileDiagnostic> result;
    for (const auto& [flow_id, definition] : collection.definitions) {
        const auto* flow = std::get_if<FlowDefinition>(&definition.data);
        if (!flow) continue;
        for (const auto& [node_id, node] : flow->nodes) {
            if (node.kind != NodeKind::sequence) continue;
            for (std::size_t i = 0; i + 1 < node.children.size(); ++i) {
                if (blocking.contains(key(flow_id, node.children[i])))
                    result.push_back({"waiting_sibling", "/definitions/" + flow_id + "/nodes/" + node_id + "/children/" + std::to_string(i),
                        "Child " + std::to_string(i + 1) + " may wait for a delay or choice; later numbered children resume only after it completes."});
            }
        }
    }
    return result;
}

void validate_scalar(const Scalar& scalar, const FactMetadata& metadata, const std::string& path) {
    if (metadata.type == ValueType::integer) {
        const auto* number = std::get_if<std::int64_t>(&scalar);
        if (!number) fail("type", path, "Fact value must be an integer without coercion");
        if (*number < metadata.minimum || *number > metadata.maximum) fail("range", path, "Fact value exceeds registered unit/range");
    } else if (metadata.type == ValueType::boolean) {
        if (!std::holds_alternative<bool>(scalar)) fail("type", path, "Fact value must be a boolean without coercion");
    } else if (metadata.type == ValueType::text) {
        if (!std::holds_alternative<std::string>(scalar)) fail("type", path, "Fact value must be text without coercion");
        if (metadata.default_value) {
            const auto& text = std::get<std::string>(scalar);
            shape(text.size() <= 4096 && text.find('\0') == std::string::npos, path, "Variable text is outside its bound");
        } else (void)scalar_json(scalar, path);
    } else fail("type", path, "Reference facts are not registered executable comparisons");
}
void validate_condition(const Condition& condition, const ScopeBinding& binding, const Registry& registry,
                        const std::string& path, std::size_t depth, std::size_t& count,
                        std::set<std::string>& predicates) {
    if (depth > 16 || ++count > 128) fail("budget", path, "Condition nesting/count budget exceeded");
    if (condition.kind == ConditionKind::holds_for_days) {
        shape(!condition.clause && condition.children.size() == 1, path, "Duration predicate requires exactly one child without a fact payload");
        if (!valid_id(condition.predicate_id)) fail("identifier", path, "Duration predicate requires a stable bounded ID");
        if (condition.duration_days == 0 || condition.duration_days > 1'000'000) fail("range", path, "Duration requires 1..1,000,000 elapsed days");
        if (!predicates.insert(condition.predicate_id).second) fail("duplicate_id", path, "Duration predicate IDs must be unique within a condition");
        validate_condition(condition.children.front(), binding, registry, path_child(path, "holds_for_days/condition"), depth + 1, count, predicates);
        return;
    }
    shape(condition.predicate_id.empty() && condition.duration_days == 0, path, "Only duration predicates carry temporal payloads");
    if (condition.kind == ConditionKind::clause) {
        shape(condition.clause.has_value() && condition.children.empty(), path, "Malformed clause payload");
        const auto& clause = *condition.clause;
        const auto* metadata = registry.fact(clause.fact);
        if (!metadata) fail("unknown_fact", path, "Fact has no registered provider");
        if (metadata->scope != binding.scope) fail("scope", path, "Fact scope differs from binding");
        if (metadata->unit != clause.unit) fail("unit", path, "Fact unit differs from comparison unit");
        (void)comparison_name(clause.comparison, path);
        if (metadata->type != ValueType::integer && clause.comparison != Comparison::equal && clause.comparison != Comparison::not_equal)
            fail("type", path, "Only integer facts support ordering comparisons");
        if (clause.expression) {
            shape(clause.value == Scalar{std::int64_t{0}}, path, "Expression comparison needs a zero scalar placeholder");
            const auto type = validate_expression(*clause.expression, binding.scope, registry);
            if (type.type != metadata->type) fail("type", path, "Comparison expression type mismatch");
            if (type.unit != metadata->unit) fail("unit", path, "Comparison expression unit mismatch");
        } else validate_scalar(clause.value, *metadata, path);
        return;
    }
    shape(!condition.clause && !condition.children.empty() && condition.children.size() <= 128 &&
              (condition.kind == ConditionKind::all_of || condition.kind == ConditionKind::any_of ||
               (condition.kind == ConditionKind::not_condition && condition.children.size() == 1)), path, "Malformed condition aggregate");
    for (std::size_t i = 0; i < condition.children.size(); ++i)
        validate_condition(condition.children[i], binding, registry, path_child(path, std::to_string(i)), depth + 1, count, predicates);
}
bool compare(const Scalar& actual, const Scalar& expected, Comparison comparison) {
    switch (comparison) {
    case Comparison::equal: return actual == expected;
    case Comparison::not_equal: return actual != expected;
    case Comparison::less: return std::get<std::int64_t>(actual) < std::get<std::int64_t>(expected);
    case Comparison::less_equal: return std::get<std::int64_t>(actual) <= std::get<std::int64_t>(expected);
    case Comparison::greater: return std::get<std::int64_t>(actual) > std::get<std::int64_t>(expected);
    case Comparison::greater_equal: return std::get<std::int64_t>(actual) >= std::get<std::int64_t>(expected);
    }
    throw std::logic_error("Validated comparison became invalid");
}
bool expression_hidden(const Expression& expression, const Registry& registry) {
    if (expression.kind == ExpressionKind::fact)
        return registry.fact(expression.fact)->visibility == Visibility::hidden_truth;
    return std::any_of(expression.children.begin(), expression.children.end(), [&](const auto& child) {
        return expression_hidden(child, registry);
    });
}
Scalar execute_expression(const Expression& expression, const ScopeBinding& binding, const Registry& registry, const FactReader& read) {
    if (expression.kind == ExpressionKind::literal) return expression.value;
    if (expression.kind == ExpressionKind::fact) {
        if (!read) fail("provider", "/expression", "Expression fact requires a pure reader");
        const auto& metadata = *registry.fact(expression.fact);
        auto actual = read(metadata, binding);
        validate_scalar(actual, metadata, "/expression/" + expression.fact);
        return actual;
    }
    const auto left = std::get<std::int64_t>(execute_expression(expression.children[0], binding, registry, read));
    if (expression.kind == ExpressionKind::scale) {
        const std::uint64_t factors[]{expression.numerator};
        return core::signed_mul_div_round_half_up(left, factors, static_cast<std::uint64_t>(expression.denominator));
    }
    const auto right = std::get<std::int64_t>(execute_expression(expression.children[1], binding, registry, read));
    return expression.kind == ExpressionKind::add ? core::checked_add(left, right) : core::checked_subtract(left, right);
}
bool has_hidden_descendant(const Condition& condition, const Registry& registry) {
    if (condition.kind == ConditionKind::clause)
        return registry.fact(condition.clause->fact)->visibility == Visibility::hidden_truth ||
            (condition.clause->expression && expression_hidden(*condition.clause->expression, registry));
    return std::any_of(condition.children.begin(), condition.children.end(), [&](const auto& child) {
        return has_hidden_descendant(child, registry);
    });
}
void duration_date(core::OrdinalDay day, const std::string& path) {
    try { (void)core::from_ordinal(day); }
    catch (const std::exception&) { fail("duration", path, "Duration observation date is outside the supported Gregorian calendar"); }
}
std::optional<bool> inspect(const Condition& condition, const ScopeBinding& binding, const Registry& registry,
                            const FactReader& read, InspectionMode mode, const std::string& path,
                            std::vector<ClauseTrace>& traces, std::vector<DurationTrace>& duration_traces,
                            const DurationReader& durations, std::optional<core::OrdinalDay> inspection_day) {
    if (condition.kind == ConditionKind::holds_for_days) {
        const auto duration_path = path_child(path, "holds_for_days");
        DurationTrace trace{duration_path, condition.predicate_id, binding, condition.duration_days, {}, {}, {}, {}};
        const auto index = duration_traces.size();
        duration_traces.push_back(trace);
        // Current facts remain inspectable, but cached daily observations own
        // temporal truth even if a later same-day command changed those facts.
        (void)inspect(condition.children.front(), binding, registry, read, mode, path_child(duration_path, "condition"),
                      traces, duration_traces, durations, inspection_day);
        if (mode == InspectionMode::player_known && has_hidden_descendant(condition.children.front(), registry)) {
            trace.redacted = true;
        } else if (durations && inspection_day) {
            duration_date(*inspection_day, duration_path);
            const auto observation = durations(condition.predicate_id, binding);
            if (!observation) trace.result = false;
            else {
                duration_date(observation->observed_on, duration_path);
                if (observation->observed_on > *inspection_day) fail("duration", duration_path, "Duration observation is in the future");
                if (observation->true_since) {
                    duration_date(*observation->true_since, duration_path);
                    if (*observation->true_since > observation->observed_on) fail("duration", duration_path, "Duration start follows its observation date");
                }
                trace.observed_on = observation->observed_on;
                trace.true_since = observation->true_since;
                if (observation->observed_on == *inspection_day) {
                    if (observation->true_since) {
                        trace.elapsed_days = core::checked_subtract(inspection_day->value(), observation->true_since->value());
                        trace.result = *trace.elapsed_days >= condition.duration_days;
                    } else trace.result = false;
                }
            }
        }
        duration_traces[index] = trace;
        return trace.result;
    }
    if (condition.kind == ConditionKind::clause) {
        const auto& clause = *condition.clause;
        const auto& metadata = *registry.fact(clause.fact);
        if (mode == InspectionMode::player_known && (metadata.visibility == Visibility::hidden_truth ||
            (clause.expression && expression_hidden(*clause.expression, registry)))) {
            traces.push_back({path, clause.fact, binding, clause.unit, clause.comparison, {}, {}, {}, true});
            return std::nullopt;
        }
        const auto actual = read(metadata, binding);
        validate_scalar(actual, metadata, path);
        const auto expected = clause.expression ? execute_expression(*clause.expression, binding, registry, read) : clause.value;
        const auto result = compare(actual, expected, clause.comparison);
        traces.push_back({path, clause.fact, binding, clause.unit, clause.comparison, actual, expected, result, false});
        return result;
    }
    if (condition.kind == ConditionKind::not_condition) {
        const auto result = inspect(condition.children.front(), binding, registry, read, mode, path_child(path, "not"),
                                    traces, duration_traces, durations, inspection_day);
        return result ? std::optional<bool>{!*result} : std::nullopt;
    }
    const bool all = condition.kind == ConditionKind::all_of;
    bool unknown = false;
    bool decisive = false;
    for (std::size_t i = 0; i < condition.children.size(); ++i) {
        const auto result = inspect(condition.children[i], binding, registry, read, mode,
                                    path_child(path_child(path, all ? "all_of" : "any_of"), std::to_string(i)),
                                    traces, duration_traces, durations, inspection_day);
        if (!result) unknown = true;
        else if (*result != all) decisive = true;
    }
    if (decisive) return !all;
    if (unknown) return std::nullopt;
    return all;
}
core::OrdinalDay add_months(core::OrdinalDay base, std::uint64_t months, unsigned preferred_day) {
    const auto civil = core::from_ordinal(base);
    const auto original = static_cast<std::uint64_t>(civil.year - 1) * 12 + civil.month - 1;
    if (months > 119'987 || original > 119'987 - months) fail("calendar_range", "anchor", "Month interval exceeds Gregorian years 1..9999");
    const auto absolute = original + months;
    const auto year = static_cast<int>(absolute / 12 + 1);
    const auto month = static_cast<unsigned>(absolute % 12 + 1);
    return core::to_ordinal({year, month, std::min(preferred_day, core::days_in_month(year, month))});
}
std::uint64_t interval_multiple(Interval interval, std::uint64_t multiple, bool allow_zero) {
    if ((interval.unit != IntervalUnit::days && interval.unit != IntervalUnit::calendar_months) ||
        (!allow_zero && interval.value == 0) || interval.value > 1'000'000)
        fail("interval", "anchor", "Invalid interval unit/value");
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (interval.value != 0 && multiple > maximum / interval.value)
        fail("calendar_range", "anchor", "Interval multiplication exceeds its storage range");
    return multiple * interval.value;
}
} // namespace

CompileError::CompileError(std::string code, std::string path, std::string detail)
    : std::runtime_error(code + " at " + path + ": " + detail), code_(std::move(code)), path_(std::move(path)) {}

CompiledProgram compile_collection(const Collection& collection, const Registry& registry,
                                   std::optional<std::vector<std::string>> available_effects) {
    try {
        auto installed_registry = registry_with_variables(registry, collection.variables);
        auto canonical = collection_json(collection).dump();
        auto normalized = load_collection(canonical, installed_registry);
        validate_sequence_regions(normalized);
        if (available_effects) {
            std::set<std::string> available;
            for (const auto& operation : *available_effects)
                if (!installed_registry.effect(operation) || !available.insert(operation).second)
                    fail("provider", "/registry", "Available effect operations must be distinct registered operations");
            for (const auto& [name, definition] : normalized.definitions) {
                const auto* flow = std::get_if<FlowDefinition>(&definition.data);
                if (!flow) continue;
                for (const auto& [node_id, node] : flow->nodes) for (const auto& effect : node.effects)
                    if (!available.contains(effect.operation))
                        fail("provider", "/definitions/" + name + "/nodes/" + node_id, "Effect has metadata but its executor/provider is unavailable");
            }
        }
        auto lints = find_lints(normalized);
        return {std::move(normalized), std::move(installed_registry), std::move(lints), std::move(canonical)};
    } catch (const CompileError&) { throw; }
    catch (const ValidationError& error) { throw CompileError(error.code(), error.path(), error.what()); }
    catch (const std::exception& error) { throw CompileError("typed_shape", "/", error.what()); }
}
ExpressionType validate_expression(const Expression& expression, Scope scope, const Registry& registry) {
    try {
        const auto [type, unit] = content::expression_type(expression, scope, registry);
        return {type, unit};
    } catch (const ValidationError& error) { throw CompileError(error.code(), error.path(), error.what()); }
}
std::optional<Scalar> evaluate_expression(const Expression& expression, const ScopeBinding& binding,
                                         const Registry& registry, const FactReader& read, InspectionMode mode) {
    if (!valid_id(binding.id)) fail("scope", "/expression", "Expression binding needs a stable bounded identifier");
    if (mode != InspectionMode::author_fixture && mode != InspectionMode::player_known)
        fail("visibility", "/expression", "Unknown expression inspection mode");
    (void)validate_expression(expression, binding.scope, registry);
    if (mode == InspectionMode::player_known && expression_hidden(expression, registry)) return std::nullopt;
    return execute_expression(expression, binding, registry, read);
}

ConditionTrace evaluate_condition(const Condition& condition, const ScopeBinding& binding,
                                   const Registry& registry, const FactReader& read,
                                   InspectionMode mode, std::string root_path,
                                   const DurationReader& durations, std::optional<core::OrdinalDay> inspection_day) {
    if (!valid_id(binding.id)) fail("scope", root_path, "Binding needs a stable bounded entity identifier");
    if (mode != InspectionMode::author_fixture && mode != InspectionMode::player_known)
        fail("visibility", root_path, "Unknown inspection mode");
    if (!read) fail("provider", root_path, "A pure fact reader is required");
    std::size_t count = 0;
    std::set<std::string> predicates;
    validate_condition(condition, binding, registry, root_path, 0, count, predicates);
    ConditionTrace result;
    result.result = inspect(condition, binding, registry, read, mode, root_path, result.clauses, result.durations, durations, inspection_day);
    return result;
}

ResolvedAnchor resolve_anchor(const Anchor& anchor, core::OrdinalDay campaign_start, core::OrdinalDay scope_created) {
    try {
        (void)core::from_ordinal(campaign_start); (void)core::from_ordinal(scope_created);
        shape(anchor.date.has_value() == (anchor.kind == AnchorKind::fixed_date), "anchor", "Only fixed anchor contains a date");
        auto base = campaign_start;
        switch (anchor.kind) {
        case AnchorKind::campaign_start: break;
        case AnchorKind::scope_created: base = scope_created; break;
        case AnchorKind::fixed_date: base = *anchor.date; break;
        case AnchorKind::month_start: {
            const auto civil = core::from_ordinal(scope_created);
            base = core::to_ordinal({civil.year, civil.month, 1});
            if (civil.day != 1) base = add_months(base, 1, 1);
            break;
        }
        default: fail("interval", "anchor", "Unknown anchor kind");
        }
        const auto offset = interval_multiple(anchor.offset, 1, true);
        if (anchor.offset.unit == IntervalUnit::calendar_months) {
            const auto preferred = core::from_ordinal(base).day;
            return {add_months(base, offset, preferred), preferred};
        }
        const auto first = core::advance_days(base, core::Days{offset});
        return {first, core::from_ordinal(first).day};
    } catch (const CompileError&) { throw; }
    catch (const std::exception& error) { throw CompileError("calendar_range", "anchor", error.what()); }
}

core::OrdinalDay interval_due(const ResolvedAnchor& anchor, Interval interval, std::uint64_t occurrence_index) {
    try {
        const auto civil = core::from_ordinal(anchor.first_due);
        if (anchor.calendar_day < 1 || anchor.calendar_day > 31 ||
            civil.day != std::min(anchor.calendar_day, core::days_in_month(civil.year, civil.month)))
            fail("interval", "anchor", "Resolved calendar-day preference is inconsistent");
        const auto amount = interval_multiple(interval, occurrence_index, false);
        if (interval.unit == IntervalUnit::calendar_months) return add_months(anchor.first_due, amount, anchor.calendar_day);
        return core::advance_days(anchor.first_due, core::Days{amount});
    } catch (const CompileError&) { throw; }
    catch (const std::exception& error) { throw CompileError("calendar_range", "anchor", error.what()); }
}
} // namespace pgp::runtime
