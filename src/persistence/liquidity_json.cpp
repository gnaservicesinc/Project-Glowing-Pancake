#include "liquidity_json.hpp"

#include <algorithm>
#include <array>

namespace pgp::persistence::detail {
namespace {
constexpr std::array<std::string_view,3> status_names{"operating","workout","closed"};
constexpr std::array<std::string_view,2> cause_names{"payable","matured_debt"};
template<class Enum,std::size_t Size> Json enum_json(Enum value,const std::array<std::string_view,Size>& names) {
    const auto index=static_cast<std::size_t>(value);
    if (index>=names.size()) invalid("liquidity/enum","Unsupported enum value");
    return names[index];
}
template<class Enum,std::size_t Size> Enum parse_enum(const Json& value,const std::array<std::string_view,Size>& names) {
    const auto name=text(value,"liquidity/enum",32);
    const auto found=std::find(names.begin(),names.end(),name);
    if (found==names.end()) invalid("liquidity/enum","Unsupported enum value");
    return static_cast<Enum>(found-names.begin());
}
Json optional_day(const std::optional<core::OrdinalDay>& value) {
    return value?Json(value->value()):Json(nullptr);
}
core::OrdinalDay day(const Json& value) { return core::OrdinalDay{i64(value,"liquidity/day")}; }
std::optional<core::OrdinalDay> parse_optional_day(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{day(value)};
}
core::Cents cents(const Json& value) { return core::Cents{i64(value,"liquidity/cents")}; }
Json cause_json(const core::LiquidityCause& value) {
    return {{"kind",enum_json(value.kind,cause_names)},{"reference",value.reference},
        {"due",value.due.value()},{"unpaid_cents",value.unpaid.value()},{"cause",value.cause}};
}
core::LiquidityCause parse_cause(const Json& value) {
    fields(value,"liquidity/cause",{"kind","reference","due","unpaid_cents","cause"});
    return {parse_enum<core::LiquidityCauseKind>(value.at("kind"),cause_names),
        text(value.at("reference"),"liquidity/reference",256),day(value.at("due")),
        cents(value.at("unpaid_cents")),text(value.at("cause"),"liquidity/cause",256)};
}
}
Json liquidity_json(const core::LiquidityState& state) {
    Json workouts=Json::array();
    for (const auto& workout:state.workouts) workouts.push_back({
        {"id",workout.id},{"warned_on",workout.warned_on.value()},{"deadline",workout.deadline.value()},
        {"initial_shortfall_cents",workout.initial_shortfall.value()},{"trigger",cause_json(workout.trigger)},
        {"acknowledged",workout.acknowledged},{"resolved_on",optional_day(workout.resolved_on)},
        {"closed_on",optional_day(workout.closed_on)}});
    return {{"owner",state.owner},{"starts_on",state.starts_on.value()},{"observed_on",state.observed_on.value()},
        {"checked_through",optional_day(state.checked_through)},{"status",enum_json(state.status,status_names)},
        {"next_workout_id",state.next_workout_id},{"exposure",{
            {"unpaid_payables_cents",state.exposure.unpaid_payables.value()},
            {"matured_principal_cents",state.exposure.matured_principal.value()},
            {"oldest",state.exposure.oldest?cause_json(*state.exposure.oldest):Json(nullptr)}}},
        {"workouts",std::move(workouts)}};
}
core::LiquidityState parse_liquidity(const Json& value) {
    fields(value,"liquidity",{"owner","starts_on","observed_on","checked_through","status","next_workout_id","exposure","workouts"});
    core::LiquidityState result;
    result.owner=text(value.at("owner"),"liquidity/owner",128);
    result.starts_on=day(value.at("starts_on"));result.observed_on=day(value.at("observed_on"));
    result.checked_through=parse_optional_day(value.at("checked_through"));
    result.status=parse_enum<core::LiquidityStatus>(value.at("status"),status_names);
    result.next_workout_id=u64(value.at("next_workout_id"),"liquidity/next_workout_id");
    const auto& exposure=value.at("exposure");
    fields(exposure,"liquidity/exposure",{"unpaid_payables_cents","matured_principal_cents","oldest"});
    result.exposure.unpaid_payables=cents(exposure.at("unpaid_payables_cents"));
    result.exposure.matured_principal=cents(exposure.at("matured_principal_cents"));
    if (!exposure.at("oldest").is_null()) result.exposure.oldest=parse_cause(exposure.at("oldest"));
    const auto& workouts=value.at("workouts");array(workouts,"liquidity/workouts",core::liquidity_workout_limit);
    for (const auto& workout:workouts) {
        fields(workout,"liquidity/workout",{"id","warned_on","deadline","initial_shortfall_cents","trigger","acknowledged","resolved_on","closed_on"});
        result.workouts.push_back({u64(workout.at("id"),"liquidity/workout/id"),
            day(workout.at("warned_on")),day(workout.at("deadline")),cents(workout.at("initial_shortfall_cents")),
            parse_cause(workout.at("trigger")),boolean(workout.at("acknowledged"),"liquidity/workout/acknowledged"),
            parse_optional_day(workout.at("resolved_on")),parse_optional_day(workout.at("closed_on"))});
    }
    core::validate_liquidity(result);
    return result;
}
}
