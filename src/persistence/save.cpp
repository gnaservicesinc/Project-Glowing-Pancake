#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/core/build_identity.hpp>
#include "json_support.hpp"

#include <algorithm>
#include <limits>

namespace pgp::persistence {
namespace {
using detail::Json;
using core::OrdinalDay;
using core::Cents;
using namespace runtime;
constexpr auto max_u32=std::numeric_limits<std::uint32_t>::max();
Json optional_day(const std::optional<OrdinalDay>& day) {return day?Json(day->value()):Json(nullptr);}
Json optional_id(const std::optional<std::uint64_t>& id) {return id?Json(*id):Json(nullptr);}
std::optional<OrdinalDay> parse_day(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{OrdinalDay{detail::i64(value,"day")}};
}
std::optional<std::uint64_t> parse_id(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{detail::u64(value,"id")};
}
Json scalar(const content::Scalar& value) {
    return std::visit([](const auto& field)->Json {
        using T=std::decay_t<decltype(field)>;
        if constexpr (std::same_as<T,std::int64_t>) return Json{{"type","integer"},{"value",field}};
        else if constexpr (std::same_as<T,bool>) return Json{{"type","boolean"},{"value",field}};
        else return Json{{"type","text"},{"value",field}};
    },value);
}
content::Scalar parse_scalar(const Json& value) {
    detail::fields(value,"scalar",{"type","value"});
    const auto type=detail::text(value.at("type"),"scalar/type",16);
    if (type=="integer") return detail::i64(value.at("value"),"scalar/value");
    if (type=="boolean") return detail::boolean(value.at("value"),"scalar/value");
    if (type=="text") return detail::text(value.at("value"),"scalar/value",4096,true);
    detail::invalid("scalar/type","Unsupported scalar type");
}
Json binding(const ScopeBinding& value) {return Json{{"scope",content::scope_name(value.scope)},{"id",value.id}};}
ScopeBinding parse_binding(const Json& value) {
    detail::fields(value,"binding",{"scope","id"});
    const auto scope=detail::text(value.at("scope"),"binding/scope",16);
    auto kind=content::Scope::world;
    if (scope=="company") kind=content::Scope::company;
    else if (scope=="state") kind=content::Scope::state;
    else if (scope!="world") detail::invalid("binding/scope","Unsupported scope");
    return {kind,detail::text(value.at("id"),"binding/id",128)};
}
template <class Enum,std::size_t N> Json enumeration(Enum value,const std::array<std::string_view,N>& names) {
    const auto index=static_cast<std::size_t>(value);
    if (index>=N) detail::invalid("enum","Invalid enum value");
    return names[index];
}
template <class Enum,std::size_t N> Enum parse_enum(const Json& value,const std::array<std::string_view,N>& names) {
    const auto name=detail::text(value,"enum",32);
    const auto found=std::find(names.begin(),names.end(),name);
    if (found==names.end()) detail::invalid("enum","Unsupported enum value");
    return static_cast<Enum>(found-names.begin());
}
constexpr std::array<std::string_view,4> phase_names{"opening","awaiting_choice","work_ready","finished"};
constexpr std::array<std::string_view,3> reservation_names{"active","completed","cancelled"};
constexpr std::array<std::string_view,5> activation_names{"ready","delayed","awaiting_choice","continuous_active","completed"};
constexpr std::array<std::string_view,2> frame_names{"sequence","subflow"};
constexpr std::array<std::string_view,3> presentation_names{"news","dialogue","result"};
Json simulation_json(const core::SimulationState& state) {
    Json reservations=Json::array(),opening=Json::array(),receipts=Json::array(),random=Json::object();
    for (const auto& row:state.reservations) reservations.push_back({{"id",row.id},{"name",row.name},
        {"reserved_cents",row.reserved.value()},{"consumed_cents",row.consumed.value()},
        {"reversible",row.reversible},{"status",enumeration(row.status,reservation_names)}});
    for (const auto& action:state.opening_actions) opening.push_back({{"id",action.id},{"due",action.due.value()},
        {"reservation_id",action.reservation_id},{"amount_cents",action.amount.value()},
        {"requires_choice",action.requires_choice},{"cause",action.cause}});
    for (const auto& receipt:state.receipts) receipts.push_back({{"id",receipt.id},{"revision",receipt.revision},
        {"day",receipt.day.value()},{"cause",receipt.cause},{"reservation_id",receipt.reservation_id},
        {"cash_delta_cents",receipt.cash_delta.value()}});
    for (std::size_t index=0;index<core::stream_count;++index) random[core::stream_names[index]]=state.random[index];
    return Json{{"revision",state.revision},{"date",state.date.value()},{"end_date",state.end_date.value()},
        {"phase",enumeration(state.phase,phase_names)},{"paused",state.paused},{"cash_cents",state.cash.value()},
        {"next_reservation_id",state.next_reservation_id},{"next_receipt_id",state.next_receipt_id},
        {"reservations",std::move(reservations)},{"opening_actions",std::move(opening)},
        {"opening_cursor",state.opening_cursor},{"pending_choice",optional_id(state.pending_choice)},
        {"receipts",std::move(receipts)},{"random_streams",std::move(random)}};
}
core::SimulationState parse_simulation(const Json& value) {
    detail::fields(value,"simulation",{"revision","date","end_date","phase","paused","cash_cents",
        "next_reservation_id","next_receipt_id","reservations","opening_actions","opening_cursor",
        "pending_choice","receipts","random_streams"});
    core::SimulationState result;
    result.revision=detail::u64(value.at("revision"),"revision");
    result.date=OrdinalDay{detail::i64(value.at("date"),"date")};
    result.end_date=OrdinalDay{detail::i64(value.at("end_date"),"end_date")};
    result.phase=parse_enum<core::DayPhase>(value.at("phase"),phase_names);
    result.paused=detail::boolean(value.at("paused"),"paused");
    result.cash=Cents{detail::i64(value.at("cash_cents"),"cash_cents")};
    result.next_reservation_id=detail::u64(value.at("next_reservation_id"),"next_reservation_id");
    result.next_receipt_id=detail::u64(value.at("next_receipt_id"),"next_receipt_id");
    result.opening_cursor=static_cast<std::size_t>(detail::u64(value.at("opening_cursor"),"opening_cursor",core::simulation_opening_limit));
    result.pending_choice=parse_id(value.at("pending_choice"));
    const auto& reservations=value.at("reservations");detail::array(reservations,"reservations",core::simulation_reservation_limit);
    for (const auto& row:reservations) {
        detail::fields(row,"reservation",{"id","name","reserved_cents","consumed_cents","reversible","status"});
        result.reservations.push_back({detail::u64(row.at("id"),"id"),detail::text(row.at("name"),"name",256),
            Cents{detail::i64(row.at("reserved_cents"),"reserved_cents")},Cents{detail::i64(row.at("consumed_cents"),"consumed_cents")},
            detail::boolean(row.at("reversible"),"reversible"),parse_enum<core::ReservationStatus>(row.at("status"),reservation_names)});
    }
    const auto& actions=value.at("opening_actions");detail::array(actions,"opening_actions",core::simulation_opening_limit);
    for (const auto& action:actions) {
        detail::fields(action,"opening_action",{"id","due","reservation_id","amount_cents","requires_choice","cause"});
        result.opening_actions.push_back({detail::u64(action.at("id"),"id"),OrdinalDay{detail::i64(action.at("due"),"due")},
            detail::u64(action.at("reservation_id"),"reservation_id"),Cents{detail::i64(action.at("amount_cents"),"amount_cents")},
            detail::boolean(action.at("requires_choice"),"requires_choice"),detail::text(action.at("cause"),"cause",256)});
    }
    const auto& receipts=value.at("receipts");detail::array(receipts,"receipts",core::simulation_receipt_limit);
    for (const auto& receipt:receipts) {
        detail::fields(receipt,"receipt",{"id","revision","day","cause","reservation_id","cash_delta_cents"});
        result.receipts.push_back({detail::u64(receipt.at("id"),"id"),detail::u64(receipt.at("revision"),"revision"),
            OrdinalDay{detail::i64(receipt.at("day"),"day")},detail::text(receipt.at("cause"),"cause",256),
            detail::u64(receipt.at("reservation_id"),"reservation_id"),Cents{detail::i64(receipt.at("cash_delta_cents"),"cash_delta_cents")}});
    }
    const auto& random=value.at("random_streams");
    if (!random.is_object()||random.size()!=core::stream_count) detail::invalid("random_streams","Expected all seven named streams");
    for (std::size_t index=0;index<core::stream_count;++index) {
        const auto name=core::stream_names[index];
        if (!random.contains(name)) detail::invalid("random_streams","A named stream is missing");
        result.random[index]=detail::u64(random.at(name),"random_stream");
    }
    return result;
}
Json strings(const std::vector<std::string>& values) {return Json(values);}
std::vector<std::string> parse_strings(const Json& value,std::size_t maximum=maximum_records) {
    detail::array(value,"strings",maximum);std::vector<std::string> result;
    for (const auto& item:value) result.push_back(detail::text(item,"string",4096));
    return result;
}
Json world_json(const WorldState& world) {
    Json facts=Json::object();for (const auto& [name,value]:world.facts) facts[name]=scalar(value);
    Json enabled=nullptr;if (world.enabled_flows) enabled=strings(*world.enabled_flows);
    return Json{{"simulation",simulation_json(world.simulation)},{"company_id",world.company_id},
        {"world_id",world.world_id},{"state_id",world.state_id},{"campaign_start",world.campaign_start.value()},
        {"scope_created",world.scope_created.value()},{"root_seed",world.root_seed},{"enabled_flows",std::move(enabled)},
        {"facts",std::move(facts)},{"counterparty_cash_cents",world.counterparty_cash.value()},
        {"decisions",strings(world.decisions)},{"negotiations",strings(world.negotiations)}};
}
WorldState parse_world(const Json& value) {
    detail::fields(value,"world",{"simulation","company_id","world_id","state_id","campaign_start","scope_created",
                                   "root_seed","enabled_flows","facts","counterparty_cash_cents","decisions","negotiations"});
    WorldState result;
    result.simulation=parse_simulation(value.at("simulation"));
    result.company_id=detail::text(value.at("company_id"),"company_id",128);
    result.world_id=detail::text(value.at("world_id"),"world_id",128);
    result.state_id=detail::text(value.at("state_id"),"state_id",128);
    result.campaign_start=OrdinalDay{detail::i64(value.at("campaign_start"),"campaign_start")};
    result.scope_created=OrdinalDay{detail::i64(value.at("scope_created"),"scope_created")};
    result.root_seed=detail::u64(value.at("root_seed"),"root_seed");
    if (!value.at("enabled_flows").is_null()) result.enabled_flows=parse_strings(value.at("enabled_flows"),maximum_activations);
    result.counterparty_cash=Cents{detail::i64(value.at("counterparty_cash_cents"),"counterparty_cash_cents")};
    result.decisions=parse_strings(value.at("decisions"));result.negotiations=parse_strings(value.at("negotiations"));
    const auto& facts=value.at("facts");
    if (!facts.is_object()||facts.size()>4096) detail::invalid("facts","Expected bounded registered fact map");
    for (const auto& [name,item]:facts.items()) {
        if (name.size()>128) detail::invalid("fact","Fact ID is oversized");
        result.facts.emplace(name,parse_scalar(item));
    }
    return result;
}
Json frame_json(const ContinuationFrame& frame) {
    return Json{{"kind",enumeration(frame.kind,frame_names)},{"flow_id",frame.flow_id},{"sequence_node",frame.sequence_node},
        {"next_child",frame.next_child},{"return_node",frame.return_node},{"binding",binding(frame.binding)}};
}
ContinuationFrame parse_frame(const Json& value) {
    detail::fields(value,"frame",{"kind","flow_id","sequence_node","next_child","return_node","binding"});
    return {parse_enum<FrameKind>(value.at("kind"),frame_names),detail::text(value.at("flow_id"),"flow_id",128),
        detail::text(value.at("sequence_node"),"sequence_node",128,true),
        static_cast<std::size_t>(detail::u64(value.at("next_child"),"next_child",512)),
        detail::text(value.at("return_node"),"return_node",128,true),parse_binding(value.at("binding"))};
}
Json runtime_json(const RuntimeState& state) {
    Json activations=Json::array(),lifecycles=Json::array(),modifiers=Json::array(),receipts=Json::array(),presentations=Json::array(),random_resolutions=Json::array(),duration_states=Json::array();
    for (const auto& activation:state.activations) {
        Json frames=Json::array();for (const auto& frame:activation.frames) frames.push_back(frame_json(frame));
        activations.push_back({{"id",activation.id},{"definition_id",activation.definition_id},{"definition_revision",activation.definition_revision},
            {"started_on",activation.started_on.value()},
            {"binding",binding(activation.binding)},{"current_flow",activation.current_flow},{"current_node",activation.current_node},
            {"frames",std::move(frames)},{"status",enumeration(activation.status,activation_names)},
            {"due",optional_day(activation.due)},{"wait_started",optional_day(activation.wait_started)},
            {"presentation_id",optional_id(activation.presentation_id)},
            {"node_visits",activation.node_visits},{"exiting",activation.exiting}});
    }
    for (const auto& lifecycle:state.lifecycles) lifecycles.push_back({{"definition_id",lifecycle.definition_id},
        {"binding",binding(lifecycle.binding)},{"occurrences",lifecycle.occurrences},{"was_eligible",lifecycle.was_eligible},
        {"last_checked",optional_day(lifecycle.last_checked)},{"last_started",optional_day(lifecycle.last_started)},
        {"first_due",optional_day(lifecycle.first_due)},{"calendar_day",lifecycle.calendar_day},{"interval_index",lifecycle.interval_index},
        {"active_activation",optional_id(lifecycle.active_activation)}});
    for (const auto& modifier:state.modifiers) modifiers.push_back({{"activation_id",modifier.activation_id},
        {"binding",binding(modifier.binding)},{"key",modifier.key},{"field",modifier.field},{"add_basis_points",modifier.add_basis_points}});
    for (const auto& receipt:state.receipts) receipts.push_back({{"id",receipt.id},{"activation_id",receipt.activation_id},
        {"revision",receipt.revision},{"day",receipt.day.value()},{"flow_id",receipt.flow_id},{"node_id",receipt.node_id},
        {"effect_index",receipt.effect_index},{"operation",receipt.operation},{"target",binding(receipt.target)},
        {"before",scalar(receipt.before)},{"after",scalar(receipt.after)},{"counterparty_delta_cents",receipt.counterparty_delta.value()}});
    for (const auto& presentation:state.presentations) {
        Json choices=Json::array();for (const auto& choice:presentation.choices) choices.push_back({{"id",choice.id},{"label_key",choice.label_key}});
        presentations.push_back({{"id",presentation.id},{"activation_id",presentation.activation_id},{"day",presentation.day.value()},
            {"kind",enumeration(presentation.kind,presentation_names)},{"flow_id",presentation.flow_id},{"node_id",presentation.node_id},
            {"title_key",presentation.title_key},{"text_key",presentation.text_key},{"choices",std::move(choices)},
            {"resolved",presentation.resolved},{"selected_choice",presentation.selected_choice}});
    }
    for (const auto& resolution:state.random_resolutions) random_resolutions.push_back({
        {"activation_id",resolution.activation_id},{"node_visit",resolution.node_visit},{"day",resolution.day.value()},
        {"flow_id",resolution.flow_id},{"node_id",resolution.node_id},{"branch_id",resolution.branch_id},
        {"sample",resolution.sample},{"stream_before",resolution.stream_before},{"stream_after",resolution.stream_after}});
    for (const auto& duration:state.duration_states) duration_states.push_back({
        {"definition_id",duration.definition_id},{"definition_revision",duration.definition_revision},
        {"predicate_id",duration.predicate_id},{"binding",binding(duration.binding)},
        {"observed_on",duration.observed_on.value()},{"true_since",optional_day(duration.true_since)}});
    return Json{{"next_activation_id",state.next_activation_id},{"next_receipt_id",state.next_receipt_id},
        {"next_presentation_id",state.next_presentation_id},{"prepared_day",optional_day(state.prepared_day)},
        {"activations",std::move(activations)},{"lifecycles",std::move(lifecycles)},{"modifiers",std::move(modifiers)},
        {"receipts",std::move(receipts)},{"presentations",std::move(presentations)},
        {"random_resolutions",std::move(random_resolutions)},{"narrative_origin",optional_id(state.narrative_origin)},
        {"duration_states",std::move(duration_states)},{"duration_sampled_day",optional_day(state.duration_sampled_day)}};
}
bool uses_duration(const content::Condition& condition) {
    return condition.kind==content::ConditionKind::holds_for_days ||
        std::any_of(condition.children.begin(),condition.children.end(),uses_duration);
}
RuntimeState parse_runtime(const Json& value,const CompiledProgram& program) {
    if (!value.is_object()) detail::invalid("runtime","Expected object");
    if (value.contains("duration_states")!=value.contains("duration_sampled_day"))
        detail::invalid("duration_states","Duration history and its sampled-day cursor must be present together");
    // Check the prior format-1 shapes without copying the potentially large
    // record arrays. The additive duration field is validated separately below.
    Json prior_shape=Json::object();
    for (const auto& [name,ignored]:value.items()) {
        (void)ignored;
        if (name!="duration_states" && name!="duration_sampled_day") prior_shape[name]=nullptr;
    }
    if (!value.contains("duration_states")) {
        for (const auto& [definition_id,definition]:program.collection.definitions) {
            (void)definition_id;
            const auto* flow=std::get_if<content::FlowDefinition>(&definition.data);
            if (flow && (uses_duration(flow->trigger) ||
                (flow->lifecycle.exit_when && uses_duration(*flow->lifecycle.exit_when))))
                detail::invalid("duration_states","Duration-bearing content requires explicit predicate history");
        }
    }
    // Format 1's earlier deterministic checkpoints have no stochastic history.
    // Permit the paired additive fields to be absent only for deterministic
    // content, while all other shape checks stay strict.
    if (value.contains("random_resolutions"))
        detail::fields(prior_shape,"runtime",{"next_activation_id","next_receipt_id","next_presentation_id","prepared_day",
            "activations","lifecycles","modifiers","receipts","presentations","random_resolutions","narrative_origin"});
    else {
        for (const auto& [definition_id,definition]:program.collection.definitions) {
            (void)definition_id;
            const auto* flow=std::get_if<content::FlowDefinition>(&definition.data);
            if (flow && std::any_of(flow->nodes.begin(),flow->nodes.end(),[](const auto& entry) {
                return entry.second.kind==content::NodeKind::random;
            })) detail::invalid("random_resolutions","Random-bearing content requires explicit resolution history");
        }
        detail::fields(prior_shape,"runtime",{"next_activation_id","next_receipt_id","next_presentation_id","prepared_day",
            "activations","lifecycles","modifiers","receipts","presentations"});
    }
    RuntimeState result;
    result.next_activation_id=detail::u64(value.at("next_activation_id"),"next_activation_id");
    result.next_receipt_id=detail::u64(value.at("next_receipt_id"),"next_receipt_id");
    result.next_presentation_id=detail::u64(value.at("next_presentation_id"),"next_presentation_id");
    result.prepared_day=parse_day(value.at("prepared_day"));
    const auto& activations=value.at("activations");detail::array(activations,"activations",maximum_activations);
    for (const auto& item:activations) {
        detail::fields(item,"activation",{"id","definition_id","definition_revision","started_on","binding","current_flow","current_node",
            "frames","status","due","wait_started","presentation_id","node_visits","exiting"});
        Activation activation;
        activation.id=detail::u64(item.at("id"),"id");activation.definition_id=detail::text(item.at("definition_id"),"definition_id",128);
        activation.definition_revision=static_cast<std::uint32_t>(detail::u64(item.at("definition_revision"),"definition_revision",max_u32));
        activation.started_on=OrdinalDay{detail::i64(item.at("started_on"),"started_on")};
        activation.binding=parse_binding(item.at("binding"));activation.current_flow=detail::text(item.at("current_flow"),"current_flow",128);
        activation.current_node=detail::text(item.at("current_node"),"current_node",128,true);
        activation.status=parse_enum<ActivationStatus>(item.at("status"),activation_names);
        activation.due=parse_day(item.at("due"));activation.wait_started=parse_day(item.at("wait_started"));
        activation.presentation_id=parse_id(item.at("presentation_id"));
        activation.node_visits=static_cast<std::uint32_t>(detail::u64(item.at("node_visits"),"node_visits",max_u32));
        activation.exiting=detail::boolean(item.at("exiting"),"exiting");
        const auto& frames=item.at("frames");detail::array(frames,"frames",maximum_continuation_frames);
        for (const auto& frame:frames) activation.frames.push_back(parse_frame(frame));
        result.activations.push_back(std::move(activation));
    }
    const auto& lifecycles=value.at("lifecycles");detail::array(lifecycles,"lifecycles",maximum_activations);
    for (const auto& item:lifecycles) {
        detail::fields(item,"lifecycle",{"definition_id","binding","occurrences","was_eligible","last_checked","last_started",
            "first_due","calendar_day","interval_index","active_activation"});
        result.lifecycles.push_back({detail::text(item.at("definition_id"),"definition_id",128),parse_binding(item.at("binding")),
            static_cast<std::uint32_t>(detail::u64(item.at("occurrences"),"occurrences",max_u32)),detail::boolean(item.at("was_eligible"),"was_eligible"),
            parse_day(item.at("last_checked")),parse_day(item.at("last_started")),parse_day(item.at("first_due")),
            static_cast<unsigned>(detail::u64(item.at("calendar_day"),"calendar_day",31)),
            detail::u64(item.at("interval_index"),"interval_index"),parse_id(item.at("active_activation"))});
    }
    const auto& modifiers=value.at("modifiers");detail::array(modifiers,"modifiers",maximum_records);
    for (const auto& item:modifiers) {
        detail::fields(item,"modifier",{"activation_id","binding","key","field","add_basis_points"});
        result.modifiers.push_back({detail::u64(item.at("activation_id"),"activation_id"),parse_binding(item.at("binding")),
            detail::text(item.at("key"),"key",128),detail::text(item.at("field"),"field",128),detail::i64(item.at("add_basis_points"),"add_basis_points")});
    }
    const auto& receipts=value.at("receipts");detail::array(receipts,"receipts",maximum_records);
    for (const auto& item:receipts) {
        detail::fields(item,"effect_receipt",{"id","activation_id","revision","day","flow_id","node_id","effect_index",
            "operation","target","before","after","counterparty_delta_cents"});
        result.receipts.push_back({detail::u64(item.at("id"),"id"),detail::u64(item.at("activation_id"),"activation_id"),
            detail::u64(item.at("revision"),"revision"),OrdinalDay{detail::i64(item.at("day"),"day")},
            detail::text(item.at("flow_id"),"flow_id",128),detail::text(item.at("node_id"),"node_id",128),
            static_cast<std::size_t>(detail::u64(item.at("effect_index"),"effect_index",512)),
            detail::text(item.at("operation"),"operation",128),parse_binding(item.at("target")),parse_scalar(item.at("before")),
            parse_scalar(item.at("after")),Cents{detail::i64(item.at("counterparty_delta_cents"),"counterparty_delta_cents")}});
    }
    const auto& presentations=value.at("presentations");detail::array(presentations,"presentations",maximum_records);
    for (const auto& item:presentations) {
        detail::fields(item,"presentation",{"id","activation_id","day","kind","flow_id","node_id","title_key","text_key",
            "choices","resolved","selected_choice"});
        Presentation presentation;
        presentation.id=detail::u64(item.at("id"),"id");presentation.activation_id=detail::u64(item.at("activation_id"),"activation_id");
        presentation.day=OrdinalDay{detail::i64(item.at("day"),"day")};presentation.kind=parse_enum<PresentationKind>(item.at("kind"),presentation_names);
        presentation.flow_id=detail::text(item.at("flow_id"),"flow_id",128);presentation.node_id=detail::text(item.at("node_id"),"node_id",128);
        presentation.title_key=detail::text(item.at("title_key"),"title_key",256,true);presentation.text_key=detail::text(item.at("text_key"),"text_key",256,true);
        presentation.resolved=detail::boolean(item.at("resolved"),"resolved");presentation.selected_choice=detail::text(item.at("selected_choice"),"selected_choice",128,true);
        const auto& choices=item.at("choices");detail::array(choices,"choices",512);
        for (const auto& choice:choices) {
            detail::fields(choice,"choice",{"id","label_key"});
            presentation.choices.push_back({detail::text(choice.at("id"),"id",128),detail::text(choice.at("label_key"),"label_key",256)});
        }
        result.presentations.push_back(std::move(presentation));
    }
    if (value.contains("random_resolutions")) {
        result.narrative_origin=parse_id(value.at("narrative_origin"));
        const auto& resolutions=value.at("random_resolutions");
        detail::array(resolutions,"random_resolutions",maximum_records);
        for (const auto& item:resolutions) {
            detail::fields(item,"random_resolution",{"activation_id","node_visit","day","flow_id","node_id",
                "branch_id","sample","stream_before","stream_after"});
            result.random_resolutions.push_back({detail::u64(item.at("activation_id"),"activation_id"),
                static_cast<std::uint32_t>(detail::u64(item.at("node_visit"),"node_visit",max_u32)),
                OrdinalDay{detail::i64(item.at("day"),"day")},detail::text(item.at("flow_id"),"flow_id",128),
                detail::text(item.at("node_id"),"node_id",128),detail::text(item.at("branch_id"),"branch_id",128),
                detail::u64(item.at("sample"),"sample"),detail::u64(item.at("stream_before"),"stream_before"),
                detail::u64(item.at("stream_after"),"stream_after")});
        }
    }
    if (value.contains("duration_states")) {
        result.duration_sampled_day=parse_day(value.at("duration_sampled_day"));
        const auto& durations=value.at("duration_states");
        detail::array(durations,"duration_states",maximum_records);
        for (const auto& item:durations) {
            detail::fields(item,"duration_state",{"definition_id","definition_revision","predicate_id","binding","observed_on","true_since"});
            result.duration_states.push_back({detail::text(item.at("definition_id"),"definition_id",128),
                static_cast<std::uint32_t>(detail::u64(item.at("definition_revision"),"definition_revision",max_u32)),
                detail::text(item.at("predicate_id"),"predicate_id",128),parse_binding(item.at("binding")),
                OrdinalDay{detail::i64(item.at("observed_on"),"observed_on")},parse_day(item.at("true_since"))});
        }
    }
    return result;
}
Json identity_json(const SaveIdentity& identity) {
    return Json{{"save_format",identity.save_format},{"simulation_rules",identity.simulation_rules},{"content_schema",identity.content_schema},
        {"game_version",identity.game_version},{"content_version",identity.content_version},{"content_hash",identity.content_hash}};
}
SaveIdentity parse_identity(const Json& value) {
    detail::fields(value,"identity",{"save_format","simulation_rules","content_schema","game_version","content_version","content_hash"});
    return {static_cast<std::uint32_t>(detail::u64(value.at("save_format"),"save_format",max_u32)),
        static_cast<std::uint32_t>(detail::u64(value.at("simulation_rules"),"simulation_rules",max_u32)),
        static_cast<std::uint32_t>(detail::u64(value.at("content_schema"),"content_schema",max_u32)),
        detail::text(value.at("game_version"),"game_version",256),detail::text(value.at("content_version"),"content_version",256),
        detail::text(value.at("content_hash"),"content_hash",64)};
}
void check_identity(const SaveIdentity& actual,const SaveIdentity& expected) {
    if (actual.save_format>expected.save_format) throw PersistenceError("future_save_format","Unsupported future save; original file was preserved");
    if (actual.save_format!=expected.save_format) throw PersistenceError("save_format","No migration exists for this save format; original was preserved");
    if (actual.simulation_rules!=expected.simulation_rules) throw PersistenceError("simulation_rules","Simulation rules do not match; no migration was guessed");
    if (actual.content_schema!=expected.content_schema) throw PersistenceError("content_schema","Content schema does not match; no migration was guessed");
    if (actual.game_version!=expected.game_version) throw PersistenceError("game_version","This development save requires its exact game version");
    if (actual.content_version!=expected.content_version || actual.content_hash!=expected.content_hash)
        throw PersistenceError("content_identity","Saved definition collection differs from the loaded collection");
}
void limits_valid(SaveLimits limits) {
    if (limits.maximum_bytes==0 || limits.maximum_bytes>64U*1024U*1024U)
        throw PersistenceError("limit","Save byte bound must be between one byte and 64 MiB");
    if (limits.maximum_json_events==0 || limits.maximum_json_events>8'000'000)
        throw PersistenceError("limit","Save JSON event bound must be between one and eight million");
}
std::filesystem::path ring_path(const std::filesystem::path& directory,std::size_t index) {
    return directory/("autosave-"+std::to_string(index)+".pgpsave");
}
}

SaveIdentity save_identity(const runtime::CompiledProgram& program) {
    const auto canonical=runtime::compile_collection(program.collection,program.registry).canonical_content_bytes;
    if (canonical!=program.canonical_content_bytes || canonical.empty())
        throw PersistenceError("program_identity","Compiled program was modified without recompilation");
    const auto& build=core::build_identity();
    if (program.collection.schema!=build.content_schema)
        throw PersistenceError("content_schema","Compiled schema is unsupported by this build");
    return {build.save_format,build.simulation_rules,build.content_schema,std::string(build.game_version),
        program.collection.content_version,sha256(canonical)};
}
std::string encode_save(const runtime::CompiledProgram& program,const runtime::Checkpoint& checkpoint,
                        std::uint64_t autosave_sequence,SaveLimits limits) {
    limits_valid(limits);
    try {
        runtime::validate_checkpoint(program,checkpoint);
        const auto identity=save_identity(program);
        Json body{{"format","project-glowing-pancake.save"},{"identity",identity_json(identity)},
            {"autosave_sequence",autosave_sequence},
            {"checkpoint",Json{{"world",world_json(checkpoint.world)},{"runtime",runtime_json(checkpoint.runtime)}}}};
        body["sha256"]=sha256(body.dump());
        auto bytes=body.dump()+"\n";
        if (bytes.size()>limits.maximum_bytes) throw PersistenceError("oversized","Encoded save exceeds its configured byte bound");
        // Never commit bytes that this same configured decoder would reject on
        // UTF-8/depth/event budgets, even near the combined record-count caps.
        (void)detail::parse(bytes,limits.maximum_bytes,limits.maximum_json_events);
        return bytes;
    } catch (const PersistenceError&) {throw;}
    catch (const std::exception& error) {throw PersistenceError("invalid_state",error.what());}
}
LoadedSave decode_save(const runtime::CompiledProgram& program,std::string_view bytes,SaveLimits limits) {
    limits_valid(limits);
    try {
        auto body=detail::parse(bytes,limits.maximum_bytes,limits.maximum_json_events);
        detail::fields(body,"save",{"format","identity","autosave_sequence","checkpoint","sha256"});
        if (detail::text(body.at("format"),"format",64)!="project-glowing-pancake.save")
            throw PersistenceError("format","This is not a Project Glowing Pancake save");
        auto identity=parse_identity(body.at("identity"));check_identity(identity,save_identity(program));
        const auto digest=detail::text(body.at("sha256"),"sha256",64);
        body.erase("sha256");
        if (digest!=sha256(body.dump())) throw PersistenceError("integrity","Save checksum does not match; use its recovery copy or another autosave");
        const auto sequence=detail::u64(body.at("autosave_sequence"),"autosave_sequence");
        const auto& state=body.at("checkpoint");detail::fields(state,"checkpoint",{"world","runtime"});
        runtime::Checkpoint checkpoint{parse_world(state.at("world")),parse_runtime(state.at("runtime"),program)};
        runtime::validate_checkpoint(program,checkpoint);
        return {std::move(identity),std::move(checkpoint),sequence};
    } catch (const PersistenceError&) {throw;}
    catch (const std::exception& error) {throw PersistenceError("invalid_state",error.what());}
}
void save_file(const runtime::CompiledProgram& program,const runtime::Checkpoint& checkpoint,
               const std::filesystem::path& path,std::uint64_t autosave_sequence,SaveLimits limits) {
    if (path.extension()!=".pgpsave") throw PersistenceError("extension","Save/export filenames must end in .pgpsave");
    const auto bytes=encode_save(program,checkpoint,autosave_sequence,limits);
    atomic_write(path,bytes,[&](auto prior){(void)decode_save(program,prior,limits);},limits.maximum_bytes);
}
LoadedSave load_file(const runtime::CompiledProgram& program,const std::filesystem::path& path,SaveLimits limits) {
    limits_valid(limits);return decode_save(program,read_bounded(path,limits.maximum_bytes),limits);
}
AutosaveRing::AutosaveRing(std::filesystem::path directory):directory_(std::move(directory)) {
    if (directory_.empty()) throw PersistenceError("path","Autosaves need an explicit local directory");
}
std::optional<std::filesystem::path> AutosaveRing::write(const runtime::CompiledProgram& program,
    const runtime::Checkpoint& checkpoint,AutosaveReason reason,bool enabled,SaveLimits limits) const {
    if (!enabled) return std::nullopt;
    if (reason==AutosaveReason::monthly_boundary &&
        (core::from_ordinal(checkpoint.world.simulation.date).day!=1 ||
         (checkpoint.world.simulation.phase!=core::DayPhase::work_ready &&
          checkpoint.world.simulation.phase!=core::DayPhase::awaiting_choice && checkpoint.world.simulation.phase!=core::DayPhase::finished)))
        throw PersistenceError("autosave_boundary","Monthly autosaves require the first morning, including a suspended opening choice, or completed final month");
    if (reason==AutosaveReason::campaign_end && checkpoint.world.simulation.phase!=core::DayPhase::finished)
        throw PersistenceError("autosave_boundary","Campaign-end autosaves require a completed campaign");
    if (reason!=AutosaveReason::monthly_boundary && reason!=AutosaveReason::before_irreversible_commitment && reason!=AutosaveReason::campaign_end)
        throw PersistenceError("autosave_reason","Unsupported autosave reason");
    std::uint64_t generation=0;
    for (std::size_t index=0;index<3;++index) {
        auto path=ring_path(directory_,index);
        for (int candidate=0;candidate<2;++candidate) {
            if (candidate==1) path+=".bak";
            std::error_code error;
            if (std::filesystem::exists(path,error)) {
                try {
                    const auto prior=load_file(program,path,limits);
                    generation=std::max(generation,prior.autosave_sequence);
                } catch (const PersistenceError&) {
                    throw PersistenceError("autosave_recovery_required",
                        "An autosave or recovery copy is invalid or incompatible and was preserved. "
                        "Load another valid copy, then choose a fresh autosave directory or repair the preserved file before retrying.");
                }
            } else if (error) throw PersistenceError("io","Cannot inspect local autosave ring");
        }
    }
    if (generation==std::numeric_limits<std::uint64_t>::max()) throw PersistenceError("autosave_limit","Autosave generation range exhausted");
    ++generation;
    const auto target=ring_path(directory_,static_cast<std::size_t>((generation-1)%3));
    save_file(program,checkpoint,target,generation,limits);return target;
}
LoadedSave AutosaveRing::latest(const runtime::CompiledProgram& program,SaveLimits limits) const {
    std::optional<LoadedSave> latest_save;
    for (std::size_t index=0;index<3;++index) {
        auto path=ring_path(directory_,index);
        for (int candidate=0;candidate<2;++candidate) {
            if (candidate==1) path+=".bak";
            try {
                auto loaded=load_file(program,path,limits);
                if (!latest_save || loaded.autosave_sequence>latest_save->autosave_sequence) latest_save=std::move(loaded);
            } catch (const PersistenceError&) {} // Preserve invalid files and try another recovery candidate.
        }
    }
    if (!latest_save) throw PersistenceError("no_valid_autosave","No valid matching autosave or recovery copy was found; existing files were preserved");
    return std::move(*latest_save);
}
}
