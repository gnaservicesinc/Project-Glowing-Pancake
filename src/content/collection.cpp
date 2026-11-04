#include <pgp/content/collection.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <deque>
#include <limits>
#include <set>

namespace pgp::content {
namespace {
using Json = nlohmann::json;
constexpr auto maximum_integer = std::numeric_limits<std::int64_t>::max();
[[noreturn]] void fail(std::string code, const std::string& path, std::string detail) {
    throw ValidationError(std::move(code), path, std::move(detail));
}
std::string child(const std::string& path, std::string_view key) { return path + "/" + std::string(key); }
void fields(const Json& value, const std::string& path,
            std::initializer_list<std::string_view> allowed, std::initializer_list<std::string_view> required) {
    if (!value.is_object()) fail("type", path, "Expected object");
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            fail("unknown_field", child(path, key), "Unsupported field; content cannot add executable or resource loaders");
    }
    for (auto key : required) if (!value.contains(key)) fail("missing_field", child(path, key), "Required field is absent");
}
std::string text(const Json& value, const std::string& path, std::size_t maximum = 4096) {
    if (!value.is_string()) fail("type", path, "Expected string");
    const auto result = value.get<std::string>();
    if (result.empty() || result.size() > maximum || result.find('\0') != std::string::npos)
        fail("range", path, "String is empty, too long, or contains NUL");
    return result;
}
std::string id(const Json& value, const std::string& path) {
    auto result = text(value, path, 128);
    if (!valid_id(result)) fail("identifier", path, "Expected a bounded lowercase stable identifier");
    return result;
}
std::int64_t integer(const Json& value, const std::string& path, std::int64_t low = 0,
                     std::int64_t high = maximum_integer) {
    if (!value.is_number_integer() || value.is_boolean()) fail("type", path, "Expected an integer token without fraction or exponent");
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(maximum_integer))
        fail("range", path, "Integer exceeds signed storage");
    const auto result = value.get<std::int64_t>();
    if (result < low || result > high) fail("range", path, "Integer is outside the field's declared unit/range");
    return result;
}
std::uint64_t unsigned_integer(const Json& value, const std::string& path) {
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0))
        fail("type", path, "Expected unsigned integer token");
    return value.get<std::uint64_t>();
}
std::uint32_t positive(const Json& value, const std::string& path, std::uint32_t maximum = 1'000'000) {
    return static_cast<std::uint32_t>(integer(value, path, 1, maximum));
}
bool boolean(const Json& value, const std::string& path) {
    if (!value.is_boolean()) fail("type", path, "Expected boolean without numeric coercion");
    return value.get<bool>();
}
void array(const Json& value, const std::string& path, std::size_t maximum, bool nonempty = false) {
    if (!value.is_array()) fail("type", path, "Expected array");
    if (value.size() > maximum || (nonempty && value.empty())) fail("budget", path, "Array is empty or exceeds its declared budget");
}
std::vector<std::string> identifiers(const Json& value, const std::string& path, std::size_t maximum = 512) {
    array(value, path, maximum);
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto item = id(value[i], child(path, std::to_string(i)));
        if (!seen.insert(item).second) fail("duplicate_id", path, "Duplicate reference");
        result.push_back(std::move(item));
    }
    return result;
}
Scope scope(const Json& value, const std::string& path) {
    const auto name = text(value, path, 32);
    if (name == "company") return Scope::company;
    if (name == "world") return Scope::world;
    if (name == "state") return Scope::state;
    fail("scope", path, "Unsupported scope");
}
pgp::core::OrdinalDay date(const Json& value, const std::string& path) {
    const auto iso = text(value, path, 10);
    if (iso.size() != 10 || iso[4] != '-' || iso[7] != '-') fail("date", path, "Expected YYYY-MM-DD");
    std::array<int, 3> parts{};
    constexpr std::array<std::size_t, 3> starts{0, 5, 8};
    constexpr std::array<std::size_t, 3> lengths{4, 2, 2};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto* begin = iso.data() + starts[i];
        const auto* end = begin + lengths[i];
        if (!std::all_of(begin, end, [](char c) { return c >= '0' && c <= '9'; })) fail("date", path, "Non-numeric date");
        const auto parsed = std::from_chars(begin, end, parts[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != end) fail("date", path, "Invalid date");
    }
    try {
        return pgp::core::to_ordinal({parts[0], static_cast<unsigned>(parts[1]), static_cast<unsigned>(parts[2])});
    } catch (const std::out_of_range&) { fail("date", path, "Invalid Gregorian date"); }
}
bool semver(std::string_view value) {
    if (value.empty() || value.size() > 128) return false;
    const auto plus = value.find('+');
    const auto hyphen = value.find('-');
    const auto core_end = std::min(plus, hyphen);
    const auto core = value.substr(0, core_end);
    std::size_t start = 0;
    for (int part = 0; part < 3; ++part) {
        auto end = core.find('.', start);
        if ((part < 2) != (end != std::string_view::npos)) return false;
        if (end == std::string_view::npos) end = core.size();
        const auto component = core.substr(start, end - start);
        if (component.empty() || (component.size() > 1 && component.front() == '0') ||
            !std::all_of(component.begin(), component.end(), [](char c) { return c >= '0' && c <= '9'; })) return false;
        start = end + 1;
    }
    auto suffix = [](std::string_view input, bool prerelease) {
        std::size_t begin = 0;
        while (begin <= input.size()) {
            auto end = input.find('.', begin);
            if (end == std::string_view::npos) end = input.size();
            const auto token = input.substr(begin, end - begin);
            if (token.empty()) return false;
            bool numeric = true;
            for (char c : token) {
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
                numeric = numeric && c >= '0' && c <= '9';
            }
            if (prerelease && numeric && token.size() > 1 && token.front() == '0') return false;
            if (end == input.size()) break;
            begin = end + 1;
        }
        return true;
    };
    if (hyphen != std::string_view::npos && (plus == std::string_view::npos || hyphen < plus)) {
        if (!suffix(value.substr(hyphen + 1, plus == std::string_view::npos ? plus : plus - hyphen - 1), true)) return false;
    }
    if (plus != std::string_view::npos && !suffix(value.substr(plus + 1), false)) return false;
    return true;
}

Condition condition(const Json& value, const std::string& path, Scope context, const Registry& registry,
                    std::size_t depth = 0, std::size_t* inherited_count = nullptr) {
    std::size_t own_count = 0;
    auto* count = inherited_count ? inherited_count : &own_count;
    if (depth > 16 || ++*count > 128) fail("budget", path, "Condition nesting/count budget exceeded");
    if (!value.is_object()) fail("type", path, "Expected condition object");
    if (value.contains("holds_for_days")) {
        fields(value, path, {"holds_for_days"}, {"holds_for_days"});
        const auto temporal_path = child(path, "holds_for_days");
        const auto& temporal = value.at("holds_for_days");
        fields(temporal, temporal_path, {"id", "days", "condition"}, {"id", "days", "condition"});
        Condition result{ConditionKind::holds_for_days, std::nullopt, {}};
        result.predicate_id = id(temporal.at("id"), child(temporal_path, "id"));
        result.duration_days = positive(temporal.at("days"), child(temporal_path, "days"));
        result.children.push_back(condition(temporal.at("condition"), child(temporal_path, "condition"), context, registry, depth + 1, count));
        return result;
    }
    for (auto name : {"all_of", "any_of", "not"}) {
        if (value.contains(name)) {
            fields(value, path, {name}, {name});
            Condition result{name == std::string_view("all_of") ? ConditionKind::all_of :
                             name == std::string_view("any_of") ? ConditionKind::any_of : ConditionKind::not_condition,
                             std::nullopt, {}};
            if (name == std::string_view("not")) result.children.push_back(condition(value.at(name), child(path, name), context, registry, depth + 1, count));
            else {
                array(value.at(name), child(path, name), 128, true);
                for (std::size_t i = 0; i < value.at(name).size(); ++i)
                    result.children.push_back(condition(value.at(name)[i], child(child(path, name), std::to_string(i)), context, registry, depth + 1, count));
            }
            return result;
        }
    }
    fields(value, path, {"fact", "op", "value", "unit"}, {"fact", "op", "value"});
    const auto fact_id = id(value.at("fact"), child(path, "fact"));
    const auto* fact = registry.fact(fact_id);
    if (!fact) fail("unknown_fact", child(path, "fact"), "Required operation/system is not registered");
    if (fact->scope != context) fail("scope", path, "Fact scope differs from flow context");
    if (value.contains("unit") && text(value.at("unit"), child(path, "unit"), 64) != unit_name(fact->unit))
        fail("unit", child(path, "unit"), "Fact comparison unit mismatch; no implicit conversions");
    const auto op = text(value.at("op"), child(path, "op"), 8);
    Comparison comparison;
    if (op == "eq") comparison = Comparison::equal;
    else if (op == "ne") comparison = Comparison::not_equal;
    else if (op == "lt") comparison = Comparison::less;
    else if (op == "lte") comparison = Comparison::less_equal;
    else if (op == "gt") comparison = Comparison::greater;
    else if (op == "gte") comparison = Comparison::greater_equal;
    else fail("operation", child(path, "op"), "Unregistered comparison");
    if (fact->type != ValueType::integer && comparison != Comparison::equal && comparison != Comparison::not_equal)
        fail("type", path, "Only integer facts permit ordering comparisons");
    Scalar literal;
    if (fact->type == ValueType::integer) literal = integer(value.at("value"), child(path, "value"), fact->minimum, fact->maximum);
    else if (fact->type == ValueType::boolean) literal = boolean(value.at("value"), child(path, "value"));
    else literal = text(value.at("value"), child(path, "value"));
    return {ConditionKind::clause, FactClause{fact_id, comparison, std::move(literal), fact->unit}, {}};
}
void unique_predicates(const Condition& condition_value, const std::string& path, std::set<std::string>& seen) {
    if (condition_value.kind == ConditionKind::holds_for_days && !seen.insert(condition_value.predicate_id).second)
        fail("duplicate_id", path, "Duration predicate IDs must be unique across a flow's trigger and exit condition");
    for (std::size_t i = 0; i < condition_value.children.size(); ++i)
        unique_predicates(condition_value.children[i], child(path, std::to_string(i)), seen);
}
Effect effect(const Json& value, const std::string& path, Scope context, const Registry& registry) {
    if (!value.is_object() || !value.contains("op") || !value.contains("target"))
        fail("missing_field", path, "Every effect needs explicit op and target");
    const auto operation = id(value.at("op"), child(path, "op"));
    const auto* metadata = registry.effect(operation);
    if (!metadata) fail("unknown_effect", path, "Required operation/system is not registered");
    const auto target = text(value.at("target"), child(path, "target"), 128);
    Scope target_scope;
    if (target == "self") target_scope = context;
    else if (target == "world") target_scope = Scope::world;
    else fail("target", child(path, "target"), "Unregistered target binding; no path or expression evaluation");
    if (std::find(metadata->target_scopes.begin(), metadata->target_scopes.end(), target_scope) == metadata->target_scopes.end())
        fail("scope", path, "Effect does not accept this target scope");
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (key != "op" && key != "target" && key != "units" &&
            std::none_of(metadata->arguments.begin(), metadata->arguments.end(), [&](const auto& arg) { return arg.name == key; }))
            fail("unknown_field", child(path, key), "Unregistered effect argument");
    }
    if (value.contains("units") && !value.at("units").is_object()) fail("type", child(path, "units"), "Expected argument/unit map");
    Effect result{operation, target, target_scope, {}};
    for (const auto& arg : metadata->arguments) {
        if (!value.contains(arg.name)) {
            if (arg.required) fail("missing_field", child(path, arg.name), "Required effect argument absent");
            continue;
        }
        const auto arg_path = child(path, arg.name);
        Scalar literal;
        if (arg.type == ValueType::integer) literal = integer(value.at(arg.name), arg_path, arg.minimum, arg.maximum);
        else if (arg.type == ValueType::boolean) literal = boolean(value.at(arg.name), arg_path);
        else {
            auto string = id(value.at(arg.name), arg_path);
            if (!arg.choices.empty() && std::find(arg.choices.begin(), arg.choices.end(), string) == arg.choices.end())
                fail("operation", arg_path, "Text value is outside the registered vocabulary");
            literal = std::move(string);
        }
        result.arguments.emplace(arg.name, std::move(literal));
    }
    if (value.contains("units")) {
        for (const auto& [name, unit] : value.at("units").items()) {
            const auto arg = std::find_if(metadata->arguments.begin(), metadata->arguments.end(), [&](const auto& item) { return item.name == name; });
            if (arg == metadata->arguments.end() || !value.contains(name) || text(unit, child(path, "units"), 64) != unit_name(arg->unit))
                fail("unit", child(path, "units"), "Unknown or mismatched effect argument unit");
        }
    }
    return result;
}
Interval interval(const Json& value, const std::string& path, bool allow_zero = false) {
    fields(value, path, {"unit", "value"}, {"unit", "value"});
    const auto unit = text(value.at("unit"), child(path, "unit"), 32);
    if (unit != "days" && unit != "calendar_months") fail("unit", path, "Expected days or calendar_months");
    return {unit == "days" ? IntervalUnit::days : IntervalUnit::calendar_months,
            static_cast<std::uint32_t>(integer(value.at("value"), child(path, "value"), allow_zero ? 0 : 1, 1'000'000))};
}
Lifecycle lifecycle(const Json& value, const std::string& path, Scope context, const Registry& registry) {
    if (!value.is_object() || !value.contains("mode")) fail("missing_field", path, "Lifecycle mode required");
    const auto mode = text(value.at("mode"), child(path, "mode"), 32);
    Lifecycle result{LifecycleMode::once, context, 1, 0, std::nullopt, std::nullopt, std::nullopt, std::nullopt, false};
    if (mode == "once") {
        fields(value, path, {"mode", "occurrence_scope", "max_occurrences_per_scope", "overlap"}, {"mode", "max_occurrences_per_scope"});
        if (integer(value.at("max_occurrences_per_scope"), path, 1, 1) != 1) fail("range", path, "Once means exactly one occurrence");
    } else if (mode == "recurring") {
        fields(value, path, {"mode", "occurrence_scope", "max_occurrences_per_scope", "overlap", "cooldown_days", "repeat_policy"},
               {"mode", "max_occurrences_per_scope", "cooldown_days", "repeat_policy"});
        result.mode = LifecycleMode::recurring;
        result.max_occurrences = positive(value.at("max_occurrences_per_scope"), child(path, "max_occurrences_per_scope"));
        result.cooldown_days = positive(value.at("cooldown_days"), child(path, "cooldown_days"));
        if (text(value.at("repeat_policy"), path) != "after_cooldown_while_eligible") fail("operation", path, "Unsupported repeat policy");
    } else if (mode == "interval") {
        fields(value, path, {"mode", "occurrence_scope", "max_occurrences_per_scope", "overlap", "every", "anchor", "miss_policy"},
               {"mode", "max_occurrences_per_scope", "every", "anchor", "miss_policy"});
        result.mode = LifecycleMode::interval;
        result.max_occurrences = positive(value.at("max_occurrences_per_scope"), child(path, "max_occurrences_per_scope"));
        result.interval = interval(value.at("every"), child(path, "every"));
        const auto& anchor = value.at("anchor");
        fields(anchor, child(path, "anchor"), {"kind", "date", "offset", "invalid_day"}, {"kind", "offset", "invalid_day"});
        const auto kind = text(anchor.at("kind"), child(path, "anchor/kind"), 32);
        Anchor parsed{AnchorKind::campaign_start, std::nullopt, interval(anchor.at("offset"), child(path, "anchor/offset"), true)};
        if (kind == "campaign_start") parsed.kind = AnchorKind::campaign_start;
        else if (kind == "scope_created") parsed.kind = AnchorKind::scope_created;
        else if (kind == "month_start") parsed.kind = AnchorKind::month_start;
        else if (kind == "fixed_date") {
            parsed.kind = AnchorKind::fixed_date;
            if (!anchor.contains("date")) fail("missing_field", path, "Fixed anchor requires date");
            parsed.date = date(anchor.at("date"), child(path, "anchor/date"));
        } else fail("operation", path, "Unsupported anchor kind");
        if (kind != "fixed_date" && anchor.contains("date")) fail("unknown_field", path, "Relative anchor cannot also name fixed date");
        if (text(anchor.at("invalid_day"), path) != "clamp_to_month_end") fail("operation", path, "Unsupported invalid-day rule");
        result.anchor = parsed;
        if (text(value.at("miss_policy"), path) != "skip") fail("operation", path, "Unsupported miss policy");
    } else if (mode == "scheduled") {
        fields(value, path, {"mode", "occurrence_scope", "max_occurrences_per_scope", "overlap", "due_date", "miss_policy"},
               {"mode", "max_occurrences_per_scope", "due_date", "miss_policy"});
        result.mode = LifecycleMode::scheduled;
        integer(value.at("max_occurrences_per_scope"), path, 1, 1);
        result.due_date = date(value.at("due_date"), child(path, "due_date"));
        if (text(value.at("miss_policy"), path) != "skip") fail("operation", path, "Unsupported miss policy");
    } else if (mode == "continuous") {
        fields(value, path, {"mode", "occurrence_scope", "exit_when", "stacking", "allow_reentry"},
               {"mode", "exit_when", "stacking", "allow_reentry"});
        result.mode = LifecycleMode::continuous;
        result.exit_when = condition(value.at("exit_when"), child(path, "exit_when"), context, registry);
        result.allow_reentry = boolean(value.at("allow_reentry"), child(path, "allow_reentry"));
        if (text(value.at("stacking"), path) != "one_per_scope") fail("operation", path, "Continuous modifiers require one owner per scope");
    } else fail("operation", path, "Unsupported lifecycle mode");
    if (value.contains("occurrence_scope") && scope(value.at("occurrence_scope"), path) != context)
        fail("scope", path, "Occurrence scope differs from flow context");
    if (value.contains("overlap") && text(value.at("overlap"), path) != "forbid") fail("operation", path, "Only bounded non-overlap is supported");
    return result;
}
Node node(const Json& value, const std::string& path, Scope context, const Registry& registry) {
    if (!value.is_object() || !value.contains("kind")) fail("missing_field", path, "Node kind required");
    const auto kind = text(value.at("kind"), child(path, "kind"), 32);
    Node result{};
    if (kind == "end") {
        fields(value, path, {"kind"}, {"kind"}); result.kind = NodeKind::end;
    } else if (kind == "effect") {
        fields(value, path, {"kind", "effects", "next"}, {"kind", "effects", "next"}); result.kind = NodeKind::effect;
        array(value.at("effects"), child(path, "effects"), 64, true);
        for (std::size_t i = 0; i < value.at("effects").size(); ++i)
            result.effects.push_back(effect(value.at("effects")[i], child(child(path, "effects"), std::to_string(i)), context, registry));
    } else if (kind == "news" || kind == "result") {
        fields(value, path, {"kind", "text_key", "next", "show_committed_changes"}, {"kind", "text_key", "next"});
        result.kind = kind == "news" ? NodeKind::news : NodeKind::result;
        result.text_key = id(value.at("text_key"), child(path, "text_key"));
        if (value.contains("show_committed_changes") && !boolean(value.at("show_committed_changes"), path))
            fail("presentation", path, "Results must show committed changes");
    } else if (kind == "dialogue") {
        fields(value, path, {"kind", "title_key", "text_key", "choices", "default_choice_if_dismissed"},
               {"kind", "title_key", "text_key", "choices", "default_choice_if_dismissed"});
        result.kind = NodeKind::dialogue;
        result.title_key = id(value.at("title_key"), child(path, "title_key"));
        result.text_key = id(value.at("text_key"), child(path, "text_key"));
        array(value.at("choices"), child(path, "choices"), 32, true);
        std::set<std::string> choices;
        for (std::size_t i = 0; i < value.at("choices").size(); ++i) {
            const auto& choice = value.at("choices")[i];
            const auto choice_path = child(child(path, "choices"), std::to_string(i));
            fields(choice, choice_path, {"id", "label_key", "next"}, {"id", "label_key", "next"});
            auto choice_id = id(choice.at("id"), child(choice_path, "id"));
            if (!choices.insert(choice_id).second) fail("duplicate_id", choice_path, "Duplicate choice ID");
            result.choices.push_back({std::move(choice_id), id(choice.at("label_key"), child(choice_path, "label_key")),
                                      id(choice.at("next"), child(choice_path, "next"))});
        }
        result.default_choice = id(value.at("default_choice_if_dismissed"), child(path, "default_choice_if_dismissed"));
        if (!choices.contains(result.default_choice)) fail("dangling_reference", path, "Dismissal choice does not exist");
    } else if (kind == "delay") {
        fields(value, path, {"kind", "duration_days", "next"}, {"kind", "duration_days", "next"});
        result.kind = NodeKind::delay; result.delay_days = positive(value.at("duration_days"), child(path, "duration_days"));
    } else if (kind == "sequence") {
        fields(value, path, {"kind", "children", "next"}, {"kind", "children", "next"});
        result.kind = NodeKind::sequence; result.children = identifiers(value.at("children"), child(path, "children"), 64);
        if (result.children.empty()) fail("budget", path, "Sequence needs at least one child");
    } else if (kind == "subflow") {
        fields(value, path, {"kind", "flow_id", "next"}, {"kind", "flow_id", "next"});
        result.kind = NodeKind::subflow; result.subflow_id = id(value.at("flow_id"), child(path, "flow_id"));
    } else if (kind == "random") {
        fields(value, path, {"kind", "stream", "branches"}, {"kind", "stream", "branches"});
        result.kind = NodeKind::random;
        result.random_stream = text(value.at("stream"), child(path, "stream"), 32);
        if (result.random_stream != "narrative") fail("operation", child(path, "stream"), "Story branches require the saved narrative stream");
        const auto branches_path = child(path, "branches");
        array(value.at("branches"), branches_path, 32, true);
        if (value.at("branches").size() < 2) fail("budget", branches_path, "Random nodes require 2..32 explicit outcomes");
        std::set<std::string> branches;
        for (std::size_t i = 0; i < value.at("branches").size(); ++i) {
            const auto& branch = value.at("branches")[i];
            const auto branch_path = child(branches_path, std::to_string(i));
            fields(branch, branch_path, {"id", "weight", "next"}, {"id", "weight", "next"});
            auto branch_id = id(branch.at("id"), child(branch_path, "id"));
            if (!branches.insert(branch_id).second) fail("duplicate_id", branch_path, "Duplicate random branch ID");
            result.branches.push_back({std::move(branch_id), positive(branch.at("weight"), child(branch_path, "weight")),
                                       id(branch.at("next"), child(branch_path, "next"))});
        }
    } else fail("operation", path, "Unsupported node kind");
    if (value.contains("next")) result.next = id(value.at("next"), child(path, "next"));
    return result;
}
std::vector<std::string> edges(const Node& node) {
    std::vector<std::string> result = node.children;
    if (!node.next.empty()) result.push_back(node.next);
    for (const auto& choice : node.choices) result.push_back(choice.next);
    for (const auto& branch : node.branches) result.push_back(branch.next);
    return result;
}
FlowDefinition flow(const Json& value, const std::string& path, const Registry& registry) {
    fields(value, path, {"id", "kind", "revision", "scope", "trigger", "lifecycle", "entry_node", "exit_node", "nodes", "max_node_visits", "tags"},
           {"id", "kind", "revision", "scope", "trigger", "lifecycle", "entry_node", "nodes"});
    const auto context = scope(value.at("scope"), child(path, "scope"));
    FlowDefinition result{context, condition(value.at("trigger"), child(path, "trigger"), context, registry),
                          lifecycle(value.at("lifecycle"), child(path, "lifecycle"), context, registry),
                          id(value.at("entry_node"), child(path, "entry_node")), "", {}, 0, {}};
    std::set<std::string> predicate_ids;
    unique_predicates(result.trigger, child(path, "trigger"), predicate_ids);
    if (result.lifecycle.exit_when) unique_predicates(*result.lifecycle.exit_when, child(path, "lifecycle/exit_when"), predicate_ids);
    if (value.contains("exit_node")) result.exit_node = id(value.at("exit_node"), child(path, "exit_node"));
    if ((result.lifecycle.mode == LifecycleMode::continuous) != !result.exit_node.empty())
        fail("lifecycle", path, "Only continuous flows require an exit node");
    if (value.contains("max_node_visits")) result.max_node_visits = positive(value.at("max_node_visits"), child(path, "max_node_visits"), 100'000);
    if (value.contains("tags")) result.tags = identifiers(value.at("tags"), child(path, "tags"), 32);
    const auto& nodes = value.at("nodes");
    if (!nodes.is_object() || nodes.empty() || nodes.size() > 512) fail("budget", child(path, "nodes"), "Expected 1..512 named nodes");
    for (const auto& [name, item] : nodes.items()) {
        if (!valid_id(name)) fail("identifier", child(path, "nodes"), "Invalid node ID");
        result.nodes.emplace(name, node(item, child(child(path, "nodes"), name), context, registry));
    }
    if (!result.nodes.contains(result.entry_node) || (!result.exit_node.empty() && !result.nodes.contains(result.exit_node)))
        fail("dangling_reference", path, "Entry/exit node does not exist");
    for (const auto& [name, item] : result.nodes) for (const auto& target : edges(item))
        if (!result.nodes.contains(target)) fail("dangling_reference", child(path, name), "Node link does not exist");
    // Every immediate path must be acyclic. A positive simulated delay plus an
    // explicit finite visit bound is the only permitted graph recurrence.
    auto has_cycle = [&](bool remove_delay_edges) {
        std::map<std::string, unsigned, std::less<>> colors;
        std::function<bool(const std::string&)> visit = [&](const std::string& name) {
            if (colors[name] == 1) return true;
            if (colors[name] == 2) return false;
            colors[name] = 1;
            const auto& item = result.nodes.at(name);
            if (!(remove_delay_edges && item.kind == NodeKind::delay))
                for (const auto& target : edges(item)) if (visit(target)) return true;
            colors[name] = 2; return false;
        };
        for (const auto& [name, ignored] : result.nodes) { (void)ignored; if (visit(name)) return true; }
        return false;
    };
    if (has_cycle(true)) fail("immediate_cycle", path, "Immediate node cycle is forbidden");
    if (has_cycle(false) && result.max_node_visits == 0) fail("unbounded_cycle", path, "Delayed recurrence requires an explicit finite visit budget");
    std::set<std::string> reached;
    std::function<void(const std::string&)> reach = [&](const std::string& name) {
        if (!reached.insert(name).second) return;
        for (const auto& target : edges(result.nodes.at(name))) reach(target);
    };
    reach(result.entry_node); if (!result.exit_node.empty()) reach(result.exit_node);
    if (reached.size() != result.nodes.size()) fail("unreachable_node", path, "Every node must be reachable from entry or continuous exit");
    return result;
}
ScenarioDefinition scenario(const Json& value, const std::string& path) {
    fields(value, path, {"id", "kind", "revision", "starts_on", "ends_before", "seed", "cash_cents", "capacity_milli_CU_per_day", "ownership_basis_points", "research", "flows",
                        "lease_energy_included", "team_people", "headquarters_choices", "monthly_commitments_cents"},
           {"id", "kind", "revision", "starts_on", "ends_before", "seed", "cash_cents", "capacity_milli_CU_per_day", "ownership_basis_points", "research", "flows",
            "lease_energy_included", "team_people", "headquarters_choices", "monthly_commitments_cents"});
    ScenarioDefinition result{date(value.at("starts_on"), child(path, "starts_on")), date(value.at("ends_before"), child(path, "ends_before")),
        unsigned_integer(value.at("seed"), child(path, "seed")), pgp::core::Cents{integer(value.at("cash_cents"), child(path, "cash_cents"))},
        pgp::core::MilliCUPerDay{integer(value.at("capacity_milli_CU_per_day"), child(path, "capacity_milli_CU_per_day"))},
        boolean(value.at("lease_energy_included"), child(path, "lease_energy_included")),
        positive(value.at("team_people"), child(path, "team_people"), 1'000'000), {}, {}, {},
        identifiers(value.at("research"), child(path, "research")), identifiers(value.at("flows"), child(path, "flows"))};
    if (result.ends_before <= result.starts_on) fail("range", path, "Scenario end must follow start");
    const auto& hqs = value.at("headquarters_choices"); array(hqs, child(path, "headquarters_choices"), 64, true);
    std::set<std::string> headquarters;
    for (const auto& item : hqs) {
        const auto name = text(item, child(path, "headquarters_choices"), 16);
        if (name != "US-CA" && name != "US-TX" && name != "US-WA") fail("scope", path, "Unregistered slice headquarters");
        if (!headquarters.insert(name).second) fail("duplicate_id", path, "Duplicate headquarters");
        result.headquarters_choices.push_back(name);
    }
    const auto& ownership = value.at("ownership_basis_points");
    if (!ownership.is_object() || ownership.empty() || ownership.size() > 128) fail("budget", path, "Expected bounded ownership table");
    std::int64_t total = 0;
    for (const auto& [owner, share] : ownership.items()) {
        if (!valid_id(owner)) fail("identifier", path, "Invalid owner ID");
        const auto amount = integer(share, child(path, owner), 0, 10000);
        total += amount; // At most 128 * 10,000, checked by the table budget.
        result.ownership.emplace(owner, pgp::core::BasisPoints{amount});
    }
    if (total != 10000) fail("ownership_total", path, "Ownership must total exactly 10,000 basis points");
    const auto& commitments = value.at("monthly_commitments_cents");
    if (!commitments.is_object() || commitments.size() > 128) fail("budget", path, "Expected bounded commitment table");
    std::int64_t total_commitments = 0;
    for (const auto& [name, amount] : commitments.items()) {
        if (!valid_id(name)) fail("identifier", path, "Invalid commitment ID");
        const auto cents = integer(amount, child(path, name));
        if (cents > maximum_integer - total_commitments) fail("range", path, "Commitment sum overflows cents storage");
        total_commitments += cents;
        result.monthly_commitments.emplace(name, pgp::core::Cents{cents});
    }
    return result;
}
ResearchDefinition research(const Json& value, const std::string& path) {
    fields(value, path, {"id", "kind", "revision", "prerequisites", "cash_cents", "staff_days", "work_milli_CU"},
           {"id", "kind", "revision", "prerequisites", "cash_cents", "staff_days", "work_milli_CU"});
    const auto& prerequisites = value.at("prerequisites");
    fields(prerequisites, child(path, "prerequisites"), {"all_of", "any_of"}, {"all_of", "any_of"});
    return {identifiers(prerequisites.at("all_of"), child(path, "prerequisites/all_of")),
            identifiers(prerequisites.at("any_of"), child(path, "prerequisites/any_of")),
            pgp::core::Cents{integer(value.at("cash_cents"), child(path, "cash_cents"))},
            pgp::core::Days{integer(value.at("staff_days"), child(path, "staff_days"))},
            pgp::core::MilliCU{integer(value.at("work_milli_CU"), child(path, "work_milli_CU"))}};
}

void validate_references(const Collection& collection, const Registry& registry) {
    auto reference = [&](std::string_view target, std::string_view kind, const std::string& path) -> const Definition& {
        const auto* definition = collection.find(target);
        if (!definition) fail("dangling_reference", path, "Referenced definition does not exist");
        if (definition_kind(*definition) != kind) fail("reference_kind", path, "Referenced definition has the wrong kind");
        return *definition;
    };
    std::map<std::string, std::vector<std::string>, std::less<>> research_edges;
    std::map<std::string, std::vector<std::string>, std::less<>> subflow_edges;
    for (const auto& [name, definition] : collection.definitions) {
        const auto path = child("/definitions", name);
        if (const auto* scenario_definition = std::get_if<ScenarioDefinition>(&definition.data)) {
            for (const auto& target : scenario_definition->research) (void)reference(target, "research", path);
            for (const auto& target : scenario_definition->flows) (void)reference(target, "flow", path);
        } else if (const auto* research_definition = std::get_if<ResearchDefinition>(&definition.data)) {
            auto& links = research_edges[name];
            links = research_definition->all_of;
            links.insert(links.end(), research_definition->any_of.begin(), research_definition->any_of.end());
            for (const auto& target : links) (void)reference(target, "research", path);
        } else if (const auto* flow_definition = std::get_if<FlowDefinition>(&definition.data)) {
            auto& links = subflow_edges[name];
            for (const auto& [node_id, node_value] : flow_definition->nodes) {
                if (node_value.kind == NodeKind::subflow) {
                    const auto& target = reference(node_value.subflow_id, "flow", child(path, node_id));
                    if (std::get<FlowDefinition>(target.data).scope != flow_definition->scope)
                        fail("scope", path, "Subflow context scope mismatch");
                    links.push_back(node_value.subflow_id);
                }
                for (const auto& effect_value : node_value.effects) {
                    const auto* metadata = registry.effect(effect_value.operation);
                    for (const auto& argument : metadata->arguments) {
                        if (argument.type == ValueType::reference) {
                            const auto found = effect_value.arguments.find(argument.name);
                            if (found != effect_value.arguments.end())
                                (void)reference(std::get<std::string>(found->second), argument.reference_kind, child(path, node_id));
                        }
                    }
                }
            }
        }
    }
    auto acyclic = [&](const auto& graph, const std::string& error_code, std::size_t max_depth) {
        std::map<std::string, unsigned, std::less<>> colors;
        std::map<std::string, std::size_t, std::less<>> heights;
        std::function<std::size_t(const std::string&)> visit = [&](const std::string& name) -> std::size_t {
            if (colors[name] == 1) fail(error_code, child("/definitions", name), "Recursive dependency cycle is forbidden");
            if (colors[name] == 2) return heights[name];
            colors[name] = 1;
            std::size_t height = 1;
            for (const auto& target : graph.at(name)) height = std::max(height, 1 + visit(target));
            if (height > max_depth) fail("budget", child("/definitions", name), "Dependency nesting budget exceeded");
            colors[name] = 2; heights[name] = height; return height;
        };
        for (const auto& [name, ignored] : graph) { (void)ignored; (void)visit(name); }
    };
    acyclic(research_edges, "prerequisite_cycle", 128);
    acyclic(subflow_edges, "subflow_cycle", 9);
    // Frame depth is a bounded longest-path problem. Propagate through reverse
    // edges instead of enumerating graph paths (diamonds otherwise explode).
    // A positive-weight cycle is also rejected: repeating a sequence/subflow
    // would accumulate continuations despite a delay or finite visit budget.
    using Link = std::pair<std::string, std::size_t>;
    std::map<std::string, std::vector<Link>, std::less<>> dependents;
    std::map<std::string, std::size_t, std::less<>> depths;
    std::deque<std::string> pending;
    auto key = [](const std::string& flow_id, const std::string& node_id) { return flow_id + ":" + node_id; };
    for (const auto& [flow_id, definition] : collection.definitions) {
        const auto* current = std::get_if<FlowDefinition>(&definition.data);
        if (!current) continue;
        for (const auto& [name, item] : current->nodes) {
            const auto source = key(flow_id, name);
            depths.emplace(source, 0); pending.push_back(source);
            if (!item.next.empty()) dependents[key(flow_id, item.next)].emplace_back(source, 0);
            for (const auto& choice : item.choices) dependents[key(flow_id, choice.next)].emplace_back(source, 0);
            for (const auto& branch : item.branches) dependents[key(flow_id, branch.next)].emplace_back(source, 0);
            for (const auto& target : item.children) dependents[key(flow_id, target)].emplace_back(source, 1);
            if (item.kind == NodeKind::subflow) {
                const auto& nested = std::get<FlowDefinition>(collection.definitions.at(item.subflow_id).data);
                dependents[key(item.subflow_id, nested.entry_node)].emplace_back(source, 1);
            }
        }
    }
    while (!pending.empty()) {
        const auto target = std::move(pending.front()); pending.pop_front();
        for (const auto& [source, weight] : dependents[target]) {
            const auto candidate = depths.at(target) + weight;
            if (candidate > 8) fail("budget", "/definitions", "Sequence/subflow continuation exceeds eight frames");
            if (candidate > depths.at(source)) { depths.at(source) = candidate; pending.push_back(source); }
        }
    }
}
} // namespace

ValidationError::ValidationError(std::string code, std::string path, std::string detail)
    : std::runtime_error(code + " at " + path + ": " + detail), code_(std::move(code)), path_(std::move(path)) {}
const Definition* Collection::find(std::string_view id_value) const {
    const auto found = definitions.find(id_value); return found == definitions.end() ? nullptr : &found->second;
}
std::string_view definition_kind(const Definition& definition) {
    if (std::holds_alternative<ScenarioDefinition>(definition.data)) return "scenario";
    if (std::holds_alternative<ResearchDefinition>(definition.data)) return "research";
    if (std::holds_alternative<FlowDefinition>(definition.data)) return "flow";
    return "deal";
}
Collection load_collection(std::string_view document, const Registry& registry) {
    if (document.size() > maximum_document_bytes) fail("budget", "/", "Content document exceeds one MiB");
    std::vector<std::set<std::string>> keys;
    std::size_t tokens = 0;
    auto callback = [&](int depth, Json::parse_event_t event, Json& parsed) {
        if (depth > 48 || ++tokens > 100'000) fail("budget", "/", "JSON depth/token budget exceeded");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        else if (event == Json::parse_event_t::object_end) keys.pop_back();
        else if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
            fail("duplicate_key", "/", "Duplicate JSON object key");
        else if (event == Json::parse_event_t::value && parsed.is_number_float())
            fail("integer_token", "/", "Floating-point/exponent authoritative values are forbidden");
        return true;
    };
    Json root;
    try { root = Json::parse(document.begin(), document.end(), callback, true, false); }
    catch (const Json::exception&) { fail("json", "/", "Malformed JSON document"); }
    fields(root, "", {"content_schema", "content_version", "currency_basis", "definitions"},
           {"content_schema", "content_version", "currency_basis", "definitions"});
    if (integer(root.at("content_schema"), "/content_schema", 1, maximum_integer) != content_schema)
        fail("unsupported_schema", "/content_schema", "Only content schema 1 can be loaded");
    const auto version = text(root.at("content_version"), "/content_version", 128);
    if (!semver(version)) fail("version", "/content_version", "Expected valid SemVer identity");
    if (text(root.at("currency_basis"), "/currency_basis", 64) != "constant_2014_cents")
        fail("unit", "/currency_basis", "All authoritative money uses constant-2014 cents");
    const auto& definitions = root.at("definitions"); array(definitions, "/definitions", 2048, true);
    Collection result{content_schema, version, {}};
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const auto& value = definitions[i]; const auto path = child("/definitions", std::to_string(i));
        if (!value.is_object() || !value.contains("id") || !value.contains("kind") || !value.contains("revision"))
            fail("missing_field", path, "Definitions need ID, kind, and positive revision");
        const auto name = id(value.at("id"), child(path, "id"));
        const auto kind = text(value.at("kind"), child(path, "kind"), 64);
        const auto revision = positive(value.at("revision"), child(path, "revision"), std::numeric_limits<std::uint32_t>::max());
        if (result.definitions.contains(name)) fail("duplicate_id", path, "Definition ID already exists");
        auto payload = [&]() -> DefinitionData {
            if (kind == "scenario") return scenario(value, path);
            if (kind == "research") return research(value, path);
            if (kind == "flow") return flow(value, path, registry);
            if (kind == "deal") {
                fields(value, path, {"id", "kind", "revision", "minimum_payment_cents", "minimum_duration_days"},
                       {"id", "kind", "revision", "minimum_payment_cents", "minimum_duration_days"});
                return DealDefinition{pgp::core::Cents{integer(value.at("minimum_payment_cents"), child(path, "minimum_payment_cents"))},
                                      pgp::core::Days{integer(value.at("minimum_duration_days"), child(path, "minimum_duration_days"))}};
            }
            fail("unsupported_kind", child(path, "kind"), "No typed schema/provider for this definition kind");
        }();
        result.definitions.emplace(name, Definition{name, revision, std::move(payload)});
    }
    validate_references(result, registry);
    return result;
}
} // namespace pgp::content
