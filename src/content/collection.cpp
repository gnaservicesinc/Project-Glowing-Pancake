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
Unit unit(const Json& value, const std::string& path) {
    const auto name = text(value, path, 64);
    for (unsigned ordinal = 0; ordinal <= static_cast<unsigned>(Unit::count); ++ordinal) {
        const auto candidate = static_cast<Unit>(ordinal);
        if (unit_name(candidate) == name) return candidate;
    }
    fail("unit", path, "Unknown registered unit");
}
Scalar expression_scalar(const Json& value, const std::string& path) {
    if (value.is_boolean()) return boolean(value, path);
    if (value.is_number_integer()) return integer(value, path, std::numeric_limits<std::int64_t>::min());
    if (!value.is_string()) fail("type", path, "Expression literal must be integer, boolean or text");
    const auto result = value.get<std::string>();
    if (result.size() > 4096 || result.find('\0') != std::string::npos) fail("range", path, "Expression text exceeds its bound or contains NUL");
    return result;
}
Expression expression(const Json& value, const std::string& path, std::size_t depth = 0, std::size_t* inherited_count = nullptr) {
    std::size_t own_count = 0;
    auto* count = inherited_count ? inherited_count : &own_count;
    if (depth > 16 || ++*count > 128) fail("budget", path, "Expression nesting/term budget exceeded");
    if (!value.is_object()) fail("type", path, "Expected a typed expression object");
    Expression result;
    if (value.contains("literal")) {
        fields(value, path, {"literal", "unit"}, {"literal", "unit"});
        result.kind = ExpressionKind::literal; result.value = expression_scalar(value.at("literal"), child(path, "literal"));
        result.unit = unit(value.at("unit"), child(path, "unit"));
    } else if (value.contains("fact")) {
        fields(value, path, {"fact"}, {"fact"});
        result.kind = ExpressionKind::fact; result.fact = id(value.at("fact"), child(path, "fact"));
    } else if (value.contains("add") || value.contains("subtract")) {
        const auto name = value.contains("add") ? "add" : "subtract";
        fields(value, path, {name}, {name}); array(value.at(name), child(path, name), 2, true);
        if (value.at(name).size() != 2) fail("type", path, "Binary arithmetic requires exactly two expressions");
        result.kind = value.contains("add") ? ExpressionKind::add : ExpressionKind::subtract;
        for (std::size_t i = 0; i < 2; ++i)
            result.children.push_back(expression(value.at(name)[i], child(child(path, name), std::to_string(i)), depth + 1, count));
    } else if (value.contains("scale")) {
        fields(value, path, {"scale"}, {"scale"});
        const auto scale_path = child(path, "scale"); const auto& scaled = value.at("scale");
        fields(scaled, scale_path, {"value", "numerator", "denominator"}, {"value", "numerator", "denominator"});
        result.kind = ExpressionKind::scale;
        result.numerator = static_cast<std::uint32_t>(integer(scaled.at("numerator"), child(scale_path, "numerator"), 0, 1'000'000));
        result.denominator = positive(scaled.at("denominator"), child(scale_path, "denominator"));
        result.children.push_back(expression(scaled.at("value"), child(scale_path, "value"), depth + 1, count));
    } else fail("operation", path, "Unknown expression operation");
    return result;
}
std::pair<ValueType, Unit> expression_type_impl(const Expression& value, Scope context, const Registry& registry,
                                              std::size_t depth, std::size_t& count) {
    const std::string path = "/expression";
    if (depth > 16 || ++count > 128) fail("budget", path, "Expression nesting/term budget exceeded");
    const auto zero = value.value == Scalar{std::int64_t{0}};
    if (value.kind == ExpressionKind::literal) {
        if (!value.fact.empty() || !value.children.empty() || value.numerator != 0 || value.denominator != 1)
            fail("typed_shape", path, "Literal has incompatible expression payloads");
        if (static_cast<unsigned>(value.unit) > static_cast<unsigned>(Unit::count)) fail("unit", path, "Unknown expression unit");
        if (std::holds_alternative<std::int64_t>(value.value)) {
            if (value.unit == Unit::none || value.unit == Unit::boolean) fail("unit", path, "Integer literal requires an integer unit");
            return {ValueType::integer, value.unit};
        }
        if (std::holds_alternative<bool>(value.value)) {
            if (value.unit != Unit::boolean) fail("unit", path, "Boolean literal requires boolean unit");
            return {ValueType::boolean, value.unit};
        }
        const auto& literal = std::get<std::string>(value.value);
        if (value.unit != Unit::none) fail("unit", path, "Text literal requires unit none");
        if (literal.size() > 4096 || literal.find('\0') != std::string::npos) fail("range", path, "Text literal is outside its bound");
        return {ValueType::text, value.unit};
    }
    if (!zero || value.unit != Unit::none) fail("typed_shape", path, "Nonliteral expressions cannot carry literal value/unit payloads");
    if (value.kind == ExpressionKind::fact) {
        if (!value.children.empty() || value.numerator != 0 || value.denominator != 1 || !valid_id(value.fact))
            fail("typed_shape", path, "Fact expression has incompatible payloads or invalid ID");
        const auto* metadata = registry.fact(value.fact);
        if (!metadata) fail("unknown_fact", path, "Expression fact has no registered provider");
        if (metadata->scope != context) fail("scope", path, "Expression cannot read another scope");
        return {metadata->type, metadata->unit};
    }
    if (!value.fact.empty()) fail("typed_shape", path, "Arithmetic expressions cannot carry a fact ID");
    if (value.kind == ExpressionKind::scale) {
        if (value.children.size() != 1 || value.numerator > 1'000'000 || value.denominator == 0 || value.denominator > 1'000'000)
            fail("typed_shape", path, "Scale requires one value and bounded integer fraction");
        const auto result = expression_type_impl(value.children.front(), context, registry, depth + 1, count);
        if (result.first != ValueType::integer) fail("type", path, "Scale requires an integer expression");
        return result;
    }
    if ((value.kind != ExpressionKind::add && value.kind != ExpressionKind::subtract) || value.children.size() != 2 ||
        value.numerator != 0 || value.denominator != 1) fail("typed_shape", path, "Binary arithmetic requires exactly two values");
    const auto left = expression_type_impl(value.children[0], context, registry, depth + 1, count);
    const auto right = expression_type_impl(value.children[1], context, registry, depth + 1, count);
    if (left.first != ValueType::integer || right.first != ValueType::integer) fail("type", path, "Arithmetic requires integer expressions");
    if (left.second != right.second) fail("unit", path, "Arithmetic operand units must match exactly");
    return left;
}
FactMetadata variable(const Json& value, const std::string& path) {
    fields(value, path, {"id", "scope", "type", "unit", "minimum", "maximum", "default", "help", "visibility"},
                        {"id", "scope", "type", "unit", "minimum", "maximum", "default", "help"});
    FactMetadata result{};
    result.id = id(value.at("id"), child(path, "id")); result.scope = scope(value.at("scope"), child(path, "scope"));
    const auto type = text(value.at("type"), child(path, "type"), 16);
    if (type == "integer") result.type = ValueType::integer;
    else if (type == "boolean") result.type = ValueType::boolean;
    else if (type == "text") result.type = ValueType::text;
    else fail("type", child(path, "type"), "Variable must have integer, boolean or text type");
    result.unit = unit(value.at("unit"), child(path, "unit"));
    result.minimum = integer(value.at("minimum"), child(path, "minimum"), std::numeric_limits<std::int64_t>::min());
    result.maximum = integer(value.at("maximum"), child(path, "maximum"), std::numeric_limits<std::int64_t>::min());
    result.default_value = expression_scalar(value.at("default"), child(path, "default"));
    result.help = text(value.at("help"), child(path, "help"), 2048);
    result.ports = {{"scope", PortType::scope, result.scope, Unit::none, false},
                    {"value", PortType::value, result.scope, result.unit, true}};
    if (value.contains("visibility")) {
        const auto visibility = text(value.at("visibility"), child(path, "visibility"), 32);
        if (visibility == "hidden_truth") result.visibility = Visibility::hidden_truth;
        else if (visibility != "player_known") fail("visibility", child(path, "visibility"), "Unknown variable visibility");
    }
    return result;
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

Comparison comparison_op(const Json& value, const std::string& path) {
    const auto op = text(value, path, 8);
    if (op == "eq") return Comparison::equal;
    if (op == "ne") return Comparison::not_equal;
    if (op == "lt") return Comparison::less;
    if (op == "lte") return Comparison::less_equal;
    if (op == "gt") return Comparison::greater;
    if (op == "gte") return Comparison::greater_equal;
    fail("operation", path, "Unregistered comparison");
}

Condition condition(const Json& value, const std::string& path, Scope context, const Registry& registry,
                    std::size_t depth = 0, std::size_t* inherited_count = nullptr, bool member = false) {
    std::size_t own_count = 0;
    auto* count = inherited_count ? inherited_count : &own_count;
    if (depth > 16 || ++*count > 128) fail("budget", path, "Condition nesting/count budget exceeded");
    if (!value.is_object()) fail("type", path, "Expected condition object");
    if (value.contains("related")) {
        fields(value, path, {"related"}, {"related"});
        const auto related_path = child(path, "related");
        const auto& related = value.at("related");
        fields(related, related_path, {"relation", "match", "where", "op", "value"}, {"relation", "match", "where"});
        auto relation_id = id(related.at("relation"), child(related_path, "relation"));
        const auto* relation = registry.relation(relation_id);
        if (!relation) fail("unknown_relation", related_path, "Selector has no registered provider");
        if (relation->owner_scope != context) fail("scope", related_path, "Selector owner scope differs from condition context");
        const auto match = text(related.at("match"), child(related_path, "match"), 8);
        Quantifier quantified{QuantifierKind::any, std::move(relation_id)};
        if (match == "all") quantified.kind = QuantifierKind::all;
        else if (match == "count") {
            if (!related.contains("op") || !related.contains("value"))
                fail("missing_field", related_path, "Count requires a comparison and an integer threshold");
            quantified.kind = QuantifierKind::count;
            quantified.count = CountComparison{comparison_op(related.at("op"), child(related_path, "op")),
                static_cast<std::uint32_t>(integer(related.at("value"), child(related_path, "value"), 0,
                    static_cast<std::int64_t>(relation->maximum_members)))};
        } else if (match != "any") fail("operation", related_path, "Unregistered related-entity quantifier");
        if (match != "count" && (related.contains("op") || related.contains("value")))
            fail("typed_shape", related_path, "Only count accepts a comparison and threshold");
        Condition result{ConditionKind::quantified, std::nullopt, {}};
        result.quantifier = std::move(quantified);
        result.children.push_back(condition(related.at("where"), child(related_path, "where"),
                                            relation->member_scope, registry, depth + 1, count, true));
        return result;
    }
    if (value.contains("holds_for_days")) {
        if (member) fail("scope", path, "A duration can wrap a selector, but member conditions cannot own duration history");
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
            if (name == std::string_view("not")) result.children.push_back(condition(value.at(name), child(path, name), context, registry, depth + 1, count, member));
            else {
                array(value.at(name), child(path, name), 128, true);
                for (std::size_t i = 0; i < value.at(name).size(); ++i)
                    result.children.push_back(condition(value.at(name)[i], child(child(path, name), std::to_string(i)), context, registry, depth + 1, count, member));
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
    const auto comparison = comparison_op(value.at("op"), child(path, "op"));
    if (fact->type != ValueType::integer && comparison != Comparison::equal && comparison != Comparison::not_equal)
        fail("type", path, "Only integer facts permit ordering comparisons");
    Scalar literal;
    if (value.at("value").is_object()) {
        auto rhs = expression(value.at("value"), child(path, "value"));
        const auto type = expression_type(rhs, context, registry);
        if (type.first != fact->type) fail("type", path, "Expression comparison type mismatch");
        if (type.second != fact->unit) fail("unit", path, "Expression comparison unit mismatch");
        FactClause clause{fact_id, comparison, std::int64_t{0}, fact->unit}; clause.expression = std::move(rhs);
        return {ConditionKind::clause, std::move(clause), {}};
    }
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
    if (operation == "set_variable") {
        fields(value, path, {"op", "target", "variable", "value"}, {"op", "target", "variable", "value"});
        if (text(value.at("target"), child(path, "target"), 128) != "self") fail("target", path, "Variable assignment requires explicit self target");
        const auto name = id(value.at("variable"), child(path, "variable"));
        const auto* registered = registry.fact(name);
        if (!registered || !registered->default_value) fail("unknown_variable", path, "Assignment requires a registered writable variable");
        if (registered->scope != context) fail("scope", path, "Variable assignment scope mismatch");
        auto rhs = expression(value.at("value"), child(path, "value"));
        const auto type = expression_type(rhs, context, registry);
        if (type.first != registered->type) fail("type", path, "Variable expression type mismatch");
        if (type.second != registered->unit) fail("unit", path, "Variable expression unit mismatch");
        Effect result{operation, "self", context, {{"variable", name}}}; result.value_expression = std::move(rhs);
        return result;
    }
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
    fields(value, path, {"id", "kind", "revision", "scope", "trigger", "lifecycle", "entry_node", "exit_node", "nodes", "max_node_visits", "tags", "on_committed"},
           {"id", "kind", "revision", "scope", "trigger", "lifecycle", "entry_node", "nodes"});
    const auto context = scope(value.at("scope"), child(path, "scope"));
    FlowDefinition result{context, condition(value.at("trigger"), child(path, "trigger"), context, registry),
                          lifecycle(value.at("lifecycle"), child(path, "lifecycle"), context, registry),
                          id(value.at("entry_node"), child(path, "entry_node")), "", {}, 0, {}};
    if (value.contains("on_committed")) {
        const auto source_path = child(path, "on_committed");
        result.on_committed = id(value.at("on_committed"), source_path);
        const auto* source = registry.committed_fact(result.on_committed);
        if (!source) fail("unknown_committed_fact", source_path, "No registered committed fact provider");
        if (source->owner_scope != context) fail("scope", source_path, "Committed fact owner differs from flow scope");
        if (result.lifecycle.mode != LifecycleMode::once && result.lifecycle.mode != LifecycleMode::recurring)
            fail("lifecycle", source_path, "Committed fact flows require once or bounded recurring lifecycle");
    }
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
MarketSetup market_setup(const Json& value, const std::string& path, const ScenarioDefinition& scenario) {
    fields(value, path, {"offers","segments","contracts"}, {"offers","segments","contracts"});
    MarketSetup result;
    array(value.at("offers"), child(path,"offers"), core::market_product_limit);
    for (const auto& offer : value.at("offers")) {
        fields(offer,path,{"id","capacity_project_id","price_cents_per_task","available","tasks_per_day",
            "inference_milli_CU_per_task","staff","minimum_memory_mib","interconnect","monthly_support_cost_cents"},
            {"id","capacity_project_id","price_cents_per_task","available","tasks_per_day",
            "inference_milli_CU_per_task","staff","minimum_memory_mib","interconnect","monthly_support_cost_cents"});
        MarketOfferDefinition row;
        row.product = {id(offer.at("id"),path),id(offer.at("capacity_project_id"),path),
            core::Cents{integer(offer.at("price_cents_per_task"),path,0,core::market_price_limit)},
            boolean(offer.at("available"),path)};
        row.serving.id = row.product.capacity_project_id;
        row.serving.source_id = "lease.initial";
        row.serving.kind = core::ProjectKind::serving;
        row.serving.starts_on = scenario.starts_on;
        row.serving.tasks_per_day = static_cast<std::uint64_t>(integer(offer.at("tasks_per_day"),path,1,core::market_task_limit));
        row.serving.inference_milli_CU_per_task = core::MilliCU{integer(offer.at("inference_milli_CU_per_task"),path,1)};
        row.serving.requested = core::MilliCUPerDay{core::required_inference(
            row.serving.tasks_per_day,row.serving.inference_milli_CU_per_task).value()};
        row.serving.staff = positive(offer.at("staff"),path,1'000'000);
        row.serving.minimum_memory_mib = unsigned_integer(offer.at("minimum_memory_mib"),path);
        row.serving.interconnect = id(offer.at("interconnect"),path);
        row.serving.monthly_support_cost = core::Cents{integer(offer.at("monthly_support_cost_cents"),path)};
        row.serving.cause = "Market service: " + row.product.id;
        result.offers.push_back(std::move(row));
    }
    array(value.at("segments"),child(path,"segments"),core::market_segment_limit);
    for (const auto& segment : value.at("segments")) {
        fields(segment,path,{"id","daily_tasks","daily_budget_cents","outside_utility","reservation_threshold",
            "maximum_monthly_migration_basis_points","preferences"},
            {"id","daily_tasks","daily_budget_cents","outside_utility","reservation_threshold",
            "maximum_monthly_migration_basis_points","preferences"});
        core::MarketSegment row;
        row.id = id(segment.at("id"),path);
        row.daily_tasks = static_cast<std::uint64_t>(integer(segment.at("daily_tasks"),path,0,core::market_task_limit));
        row.daily_budget = core::Cents{integer(segment.at("daily_budget_cents"),path)};
        row.outside_utility = static_cast<std::uint64_t>(integer(segment.at("outside_utility"),path,0,core::market_utility_limit));
        row.reservation_threshold = static_cast<std::uint64_t>(integer(segment.at("reservation_threshold"),path,0,core::market_utility_limit));
        row.maximum_monthly_migration = core::BasisPoints{integer(segment.at("maximum_monthly_migration_basis_points"),path,0,10000)};
        array(segment.at("preferences"),child(path,"preferences"),core::market_product_limit);
        for (const auto& preference : segment.at("preferences")) {
            fields(preference,path,{"product_id","utility","willingness_to_pay_cents","switching_cost","eligible"},
                {"product_id","utility","willingness_to_pay_cents","switching_cost","eligible"});
            row.preferences.push_back({id(preference.at("product_id"),path),
                static_cast<std::uint64_t>(integer(preference.at("utility"),path,0,core::market_utility_limit)),
                core::Cents{integer(preference.at("willingness_to_pay_cents"),path,0,core::market_price_limit)},
                static_cast<std::uint64_t>(integer(preference.at("switching_cost"),path,0,core::market_utility_limit)),
                boolean(preference.at("eligible"),path)});
        }
        result.segments.push_back(std::move(row));
    }
    array(value.at("contracts"),child(path,"contracts"),core::market_contract_limit);
    for (const auto& contract : value.at("contracts")) {
        fields(contract,path,{"id","segment_id","product_id","tasks_per_day","monthly_fee_cents","usage_price_cents",
            "service_credit_basis_points","missing_task_penalty_cents","missed_start_penalty_cents","cancellation_penalty_cents",
            "start_by","ends_before","cause","start_immediately"},
            {"id","segment_id","product_id","tasks_per_day","monthly_fee_cents","usage_price_cents",
            "service_credit_basis_points","missing_task_penalty_cents","missed_start_penalty_cents","cancellation_penalty_cents",
            "start_by","ends_before","cause","start_immediately"});
        core::MarketContract row;
        row.id = id(contract.at("id"),path); row.segment_id = id(contract.at("segment_id"),path);
        row.product_id = id(contract.at("product_id"),path);
        row.tasks_per_day = static_cast<std::uint64_t>(integer(contract.at("tasks_per_day"),path,1,core::market_task_limit));
        row.monthly_fee = core::Cents{integer(contract.at("monthly_fee_cents"),path)};
        row.usage_price = core::Cents{integer(contract.at("usage_price_cents"),path,0,core::market_price_limit)};
        row.service_credit_basis_points = core::BasisPoints{integer(contract.at("service_credit_basis_points"),path,0,10000)};
        row.missing_task_penalty = core::Cents{integer(contract.at("missing_task_penalty_cents"),path,0,core::market_price_limit)};
        row.missed_start_penalty = core::Cents{integer(contract.at("missed_start_penalty_cents"),path)};
        row.cancellation_penalty = core::Cents{integer(contract.at("cancellation_penalty_cents"),path)};
        row.accepted_on = scenario.starts_on;
        row.start_by = date(contract.at("start_by"),path); row.ends_before = date(contract.at("ends_before"),path);
        row.cause = text(contract.at("cause"),path,256);
        if (row.ends_before > scenario.ends_before) fail("range",path,"Contract extends past scenario ending");
        result.contracts.push_back({std::move(row),boolean(contract.at("start_immediately"),path)});
    }
    auto ledger = core::opening_accounts(scenario.cash,scenario.starts_on);
    std::vector<core::CapacitySource> sources;
    if (scenario.leased_capacity.value() > 0)
        sources.push_back({"lease.initial","cloud.pool.initial","company.player","supplier.compute.initial",
            core::CapacitySourceKind::leased,scenario.leased_capacity,scenario.leased_capacity,0,"general",scenario.lease_energy_included});
    auto capacity = core::opening_capacity(std::move(sources),scenario.team_people,scenario.starts_on);
    auto market = core::opening_market(scenario.starts_on);
    apply_market_setup(result,market,ledger,capacity,scenario.starts_on);
    return result;
}
ScenarioDefinition scenario(const Json& value, const std::string& path) {
    fields(value, path, {"id", "kind", "revision", "starts_on", "ends_before", "seed", "cash_cents", "capacity_milli_CU_per_day", "ownership_basis_points", "research", "flows",
                        "lease_energy_included", "team_people", "headquarters_choices", "monthly_commitments_cents", "market", "business_plan"},
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
    if (value.contains("market")) {
        try { result.market = market_setup(value.at("market"),child(path,"market"),result); }
        catch (const ValidationError&) { throw; }
        catch (const std::exception& error) { fail("market",child(path,"market"),error.what()); }
    }
    if (value.contains("business_plan")) {
        const auto plan_path = child(path,"business_plan");
        const auto& plan = value.at("business_plan");
        fields(plan,plan_path,{"training","expansion_on","expansion","rescue_proceeds_cents","rescue_shares_basis_points"},
            {"training","expansion_on","expansion","rescue_proceeds_cents","rescue_shares_basis_points"});
        const auto& training = plan.at("training");
        fields(training,plan_path,{"id","requested_milli_CU_per_day","staff","total_work_milli_CU","data_license_cents"},
            {"id","requested_milli_CU_per_day","staff","total_work_milli_CU","data_license_cents"});
        try {
            if (!result.market) fail("market",plan_path,"A business run plan needs a starting market");
            BusinessPlan parsed;
            parsed.training.id = id(training.at("id"),plan_path);
            parsed.training.source_id = "lease.initial";
            parsed.training.starts_on = result.starts_on;
            parsed.training.requested = core::MilliCUPerDay{integer(training.at("requested_milli_CU_per_day"),plan_path,1)};
            parsed.training.staff = positive(training.at("staff"),plan_path,1'000'000);
            parsed.training.total_work = core::MilliCU{integer(training.at("total_work_milli_CU"),plan_path,1)};
            parsed.training.cause = "Scenario training: " + parsed.training.id;
            parsed.data_license = core::Cents{integer(training.at("data_license_cents"),plan_path,1)};
            if (parsed.data_license > result.cash) fail("range",plan_path,"Initial license exceeds opening cash");
            parsed.expansion_on = date(plan.at("expansion_on"),plan_path);
            if (parsed.expansion_on <= result.starts_on || parsed.expansion_on >= result.ends_before)
                fail("range",plan_path,"Expansion must occur within the scenario after its start");
            auto expanded_scenario = result;
            expanded_scenario.starts_on = parsed.expansion_on;
            parsed.expansion = market_setup(plan.at("expansion"),child(plan_path,"expansion"),expanded_scenario);
            parsed.rescue_proceeds = core::Cents{integer(plan.at("rescue_proceeds_cents"),plan_path,1)};
            parsed.rescue_shares = core::BasisPoints{integer(plan.at("rescue_shares_basis_points"),plan_path,1,10000)};
            auto accounts = core::opening_accounts(result.cash,result.starts_on);
            auto capacity = core::opening_capacity({{"lease.initial","cloud.pool.initial","company.player","supplier.compute.initial",
                core::CapacitySourceKind::leased,result.leased_capacity,result.leased_capacity,0,"general",result.lease_energy_included}},
                result.team_people,result.starts_on);
            auto market = core::opening_market(result.starts_on);
            apply_market_setup(*result.market,market,accounts,capacity,result.starts_on);
            core::reserve_project(capacity,result.starts_on,parsed.training);
            // Validate expansion together with initial serving capacity and
            // customer identities, after releasing the finite training claim.
            core::cancel_project(capacity,result.starts_on,parsed.training.id);
            auto opening_expansion = parsed.expansion;
            for (auto& offer : opening_expansion.offers) offer.serving.starts_on = result.starts_on;
            for (auto& contract : opening_expansion.contracts) contract.terms.accepted_on = result.starts_on;
            apply_market_setup(opening_expansion,market,accounts,capacity,result.starts_on);
            (void)canonical_business_plan_bytes(parsed,result.starts_on);
            result.business_plan = std::move(parsed);
        } catch (const ValidationError&) { throw; }
        catch (const std::exception& error) { fail("business_plan",plan_path,error.what()); }
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
            std::set<std::string> subscriptions;
            for (const auto& target : scenario_definition->flows) {
                const auto& source = std::get<FlowDefinition>(reference(target, "flow", path).data).on_committed;
                if (!source.empty() && !subscriptions.insert(source).second)
                    fail("duplicate_subscription", path, "One baseline flow may subscribe to a committed fact per scenario");
            }
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
                    const auto& called = std::get<FlowDefinition>(target.data);
                    if (called.scope != flow_definition->scope)
                        fail("scope", path, "Subflow context scope mismatch");
                    if (!called.on_committed.empty())
                        fail("lifecycle", path, "A committed-fact root cannot be called as a subflow");
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
std::pair<ValueType, Unit> expression_type(const Expression& value, Scope context, const Registry& registry) {
    if (static_cast<unsigned>(context) > static_cast<unsigned>(Scope::contract))
        fail("scope", "/expression", "Expression scope is invalid");
    std::size_t count = 0;
    return expression_type_impl(value, context, registry, 0, count);
}
Registry registry_with_variables(const Registry& registry, const std::vector<FactMetadata>& variables) {
    if (variables.size() > 512) fail("budget", "/variables", "Variable declaration budget exceeded");
    Registry result = registry;
    std::set<std::string> names;
    const auto same_ports = [](const auto& left, const auto& right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](const auto& a, const auto& b) {
            return a.name == b.name && a.type == b.type && a.scope == b.scope && a.unit == b.unit && a.output == b.output;
        });
    };
    for (const auto& metadata : variables) {
        const auto path = child("/variables", metadata.id);
        if (!names.insert(metadata.id).second) fail("duplicate_id", path, "Variable ID is declared twice");
        const std::vector<PortMetadata> ports{{"scope", PortType::scope, metadata.scope, Unit::none, false},
                                               {"value", PortType::value, metadata.scope, metadata.unit, true}};
        if (!metadata.default_value || !same_ports(metadata.ports, ports) || metadata.provider != "foundation" ||
            !metadata.dependencies.empty() || metadata.boundary != Boundary::committed_result)
            fail("typed_shape", path, "Content variables need explicit defaults and standard foundation metadata");
        if (const auto* existing = result.fact(metadata.id)) {
            if (!existing->default_value) fail("duplicate_id", path, "Variable collides with an owning system fact");
            if (existing->scope != metadata.scope || existing->type != metadata.type || existing->unit != metadata.unit ||
                existing->minimum != metadata.minimum || existing->maximum != metadata.maximum || existing->help != metadata.help ||
                existing->default_value != metadata.default_value || existing->visibility != metadata.visibility ||
                existing->boundary != metadata.boundary || existing->provider != metadata.provider ||
                existing->dependencies != metadata.dependencies || !same_ports(existing->ports, metadata.ports))
                fail("variable", path, "Variable declaration differs from the installed registry");
        } else {
            try { result.register_variable(metadata, *metadata.default_value); }
            catch (const std::invalid_argument& error) { fail("variable", path, error.what()); }
        }
    }
    return result;
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
    fields(root, "", {"content_schema", "content_version", "currency_basis", "definitions", "variables"},
           {"content_schema", "content_version", "currency_basis", "definitions"});
    if (integer(root.at("content_schema"), "/content_schema", 1, maximum_integer) != content_schema)
        fail("unsupported_schema", "/content_schema", "Only content schema 1 can be loaded");
    const auto version = text(root.at("content_version"), "/content_version", 128);
    if (!semver(version)) fail("version", "/content_version", "Expected valid SemVer identity");
    if (text(root.at("currency_basis"), "/currency_basis", 64) != "constant_2014_cents")
        fail("unit", "/currency_basis", "All authoritative money uses constant-2014 cents");
    const auto& definitions = root.at("definitions"); array(definitions, "/definitions", 2048, true);
    Collection result{content_schema, version, {}};
    if (root.contains("variables")) {
        array(root.at("variables"), "/variables", 512);
        for (std::size_t i = 0; i < root.at("variables").size(); ++i)
            result.variables.push_back(variable(root.at("variables")[i], child("/variables", std::to_string(i))));
    }
    const auto installed_registry = registry_with_variables(registry, result.variables);
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
            if (kind == "flow") return flow(value, path, installed_registry);
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
    validate_references(result, installed_registry);
    return result;
}
} // namespace pgp::content
