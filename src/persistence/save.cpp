#include "counties_json.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/core/build_identity.hpp>
#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/objectives.hpp>
#include "json_support.hpp"
#include "accounts_json.hpp"
#include "capacity_json.hpp"
#include "market_json.hpp"
#include "liquidity_json.hpp"
#include "lab_json.hpp"
#include "deployment_json.hpp"
#include "management_json.hpp"
#include "infrastructure_json.hpp"
#include "resilience_json.hpp"
#include "civic_json.hpp"
#include "objectives_json.hpp"
#include "mergers_json.hpp"
#include "competition_history_json.hpp"
#include "heavyweight_json.hpp"
#include "history_blocks.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace pgp::persistence {
namespace {
using detail::Json;
using core::OrdinalDay;
using core::Cents;
using namespace runtime;
constexpr auto max_u32=std::numeric_limits<std::uint32_t>::max();
constexpr std::string_view packed_history_format = "history-blocks-v3";
bool archive_history(const core::SimulationState& state) {
    return state.market && state.market->days.size() > 365;
}
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
ScopeBinding parse_binding(const Json& value, bool member=false) {
    detail::fields(value,"binding",{"scope","id"});
    const auto scope=detail::text(value.at("scope"),"binding/scope",16);
    auto kind=content::Scope::world;
    if (member && scope=="project") kind=content::Scope::project;
    else if (member && scope=="contract") kind=content::Scope::contract;
    else if (member && scope=="capacity_source") kind=content::Scope::capacity_source;
    else if (scope=="company") kind=content::Scope::company;
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
constexpr std::array<std::string_view,5> phase_names{"opening","awaiting_choice","work_ready","finished","closed"};
constexpr std::array<std::string_view,3> reservation_names{"active","completed","cancelled"};
constexpr std::array<std::string_view,5> activation_names{"ready","delayed","awaiting_choice","continuous_active","completed"};
constexpr std::array<std::string_view,2> frame_names{"sequence","subflow"};
constexpr std::array<std::string_view,3> presentation_names{"news","dialogue","result"};
Json committed_source_json(const CommittedSource& source) {
    return {{"fact_id",source.fact_id},{"subject",binding(source.subject)},{"settled_on",source.settled_on.value()},
        {"unit",content::unit_name(source.unit)},{"result",scalar(source.result)},{"cause",source.cause}};
}
CommittedSource parse_committed_source(const Json& value) {
    detail::fields(value,"committed_source",{"fact_id","subject","settled_on","unit","result","cause"});
    const auto name=detail::text(value.at("unit"),"unit",64);
    for (unsigned index=0;index<=static_cast<unsigned>(content::Unit::count);++index) {
        const auto unit=static_cast<content::Unit>(index);
        if (content::unit_name(unit)==name)
            return {detail::text(value.at("fact_id"),"fact_id",128),parse_binding(value.at("subject"),true),
                OrdinalDay{detail::i64(value.at("settled_on"),"settled_on")},unit,
                parse_scalar(value.at("result")),detail::text(value.at("cause"),"cause",1024)};
    }
    detail::invalid("committed_source/unit","Unsupported fact unit");
}
Json business_offers_json(const core::BusinessOffersState& state) {
    Json accepted=Json::array(),renewals=Json::array();
    for (const auto& row:state.accepted) accepted.push_back({{"offer_id",row.offer_id},{"accepted_on",row.accepted_on.value()},{"expires_before",row.expires_before.value()}});
    for (const auto& row:state.renewals) renewals.push_back({{"offer_id",row.offer_id},{"renewed_on",row.renewed_on.value()},{"expires_before",row.expires_before.value()}});
    return {{"catalog",Json::parse(content::canonical_business_offer_catalog(state.catalog))},{"accepted",std::move(accepted)},{"renewals",std::move(renewals)}};
}
core::BusinessOffersState parse_business_offers(const Json& value) {
    detail::fields(value,"business_offers",{"catalog","accepted"},{"renewals"});
    core::BusinessOffersState result;result.catalog=content::load_business_offer_catalog(value.at("catalog").dump());
    detail::array(value.at("accepted"),"business_offers/accepted",32);
    for (const auto& row:value.at("accepted")) {
        detail::fields(row,"offer_acceptance",{"offer_id","accepted_on","expires_before"});
        result.accepted.push_back({detail::text(row.at("offer_id"),"business_offer/id",64),OrdinalDay{detail::i64(row.at("accepted_on"),"business_offer/accepted_on")},OrdinalDay{detail::i64(row.at("expires_before"),"business_offer/expires_before")}});
    }
    if (value.contains("renewals")) {
        detail::array(value.at("renewals"),"business_offers/renewals",64);
        for (const auto& row:value.at("renewals")) {
            detail::fields(row,"lease_renewal",{"offer_id","renewed_on","expires_before"});
            result.renewals.push_back({detail::text(row.at("offer_id"),"business_offer/id",64),OrdinalDay{detail::i64(row.at("renewed_on"),"lease_renewal/day")},OrdinalDay{detail::i64(row.at("expires_before"),"lease_renewal/expiry")}});
        }
    }
    return result;
}
struct ReceiptCursor {
    std::uint64_t id = 0;
    std::uint64_t revision = 0;
    OrdinalDay day{0};
};
Json packed_receipt(const core::TransactionReceipt& receipt, ReceiptCursor& cursor) {
    const auto id = receipt.id - std::exchange(cursor.id, receipt.id);
    // Modulo deltas preserve the full unsigned revision range and older
    // histories whose receipt revisions are not monotone.
    const auto revision = receipt.revision - std::exchange(cursor.revision, receipt.revision);
    const auto day = core::checked_subtract(receipt.day.value(), std::exchange(cursor.day, receipt.day).value());
    return Json::array({id, revision, day, receipt.cause, receipt.reservation_id, receipt.cash_delta.value()});
}
core::TransactionReceipt parse_packed_receipt(const Json& row, ReceiptCursor& cursor) {
    detail::array(row, "receipt", 6);
    if (row.size() != 6) detail::invalid("receipt", "Packed receipt has missing columns");
    const auto id = detail::u64(row.at(0), "receipt/id_delta", std::numeric_limits<std::uint64_t>::max() - cursor.id);
    if (id == 0) detail::invalid("receipt/id_delta", "Receipt IDs must increase");
    cursor.id += id;
    cursor.revision += detail::u64(row.at(1), "receipt/revision_delta");
    cursor.day = OrdinalDay{core::checked_add(cursor.day.value(), detail::i64(row.at(2), "receipt/day_delta"))};
    return {cursor.id, cursor.revision, cursor.day, detail::text(row.at(3), "receipt/cause", 256),
        detail::u64(row.at(4), "receipt/reservation_id"), Cents{detail::i64(row.at(5), "receipt/cash_delta_cents")}};
}
Json simulation_json(const core::SimulationState& state, bool packed) {
    Json reservations=Json::array(),opening=Json::array(),random=Json::object();
    detail::HistoryRows receipts{packed};
    for (const auto& row:state.reservations) reservations.push_back({{"id",row.id},{"name",row.name},
        {"reserved_cents",row.reserved.value()},{"consumed_cents",row.consumed.value()},
        {"reversible",row.reversible},{"status",enumeration(row.status,reservation_names)}});
    for (const auto& action:state.opening_actions) opening.push_back({{"id",action.id},{"due",action.due.value()},
        {"reservation_id",action.reservation_id},{"amount_cents",action.amount.value()},
        {"requires_choice",action.requires_choice},{"cause",action.cause}});
    ReceiptCursor cursor;
    for (const auto& receipt:state.receipts) receipts.push(packed ? packed_receipt(receipt, cursor) : Json{{"id",receipt.id},{"revision",receipt.revision},
        {"day",receipt.day.value()},{"cause",receipt.cause},{"reservation_id",receipt.reservation_id},
        {"cash_delta_cents",receipt.cash_delta.value()}});
    const auto streams=state.deployments && state.deployments->risk?core::stream_count:static_cast<std::size_t>(core::RandomStream::risk);
    for (std::size_t index=0;index<streams;++index) random[core::stream_names[index]]=state.random[index];
    Json result{{"revision",state.revision},{"date",state.date.value()},{"end_date",state.end_date.value()},
        {"phase",enumeration(state.phase,phase_names)},{"paused",state.paused},{"cash_cents",state.cash.value()},
        {"next_reservation_id",state.next_reservation_id},{"next_receipt_id",state.next_receipt_id},
        {"reservations",std::move(reservations)},{"opening_actions",std::move(opening)},
        {"opening_cursor",state.opening_cursor},{"pending_choice",optional_id(state.pending_choice)},
        {"receipts",receipts.finish()},{"random_streams",std::move(random)},
        {"accounting_mode",state.accounts?"business":"foundation"},
        {"accounts",state.accounts?detail::accounts_json(*state.accounts, packed):Json(nullptr)},
        {"capacity_mode",state.capacity?"business":"foundation"},
        {"capacity",state.capacity?detail::capacity_json(*state.capacity, packed):Json(nullptr)},
        {"market_mode",state.market?"business":"foundation"},
        {"market",state.market?detail::market_json(*state.market, packed):Json(nullptr)},
        {"liquidity_mode",state.liquidity?"business":"foundation"},
        {"liquidity",state.liquidity?detail::liquidity_json(*state.liquidity):Json(nullptr)},
        {"lab_mode",state.lab?"training":"foundation"},
        {"lab",state.lab?detail::lab_json(*state.lab):Json(nullptr)},
        {"management_mode",state.management?"bounded":"foundation"},
        {"management",state.management?detail::management_json(*state.management):Json(nullptr)},
        {"infrastructure_mode",state.infrastructure?"domestic":"foundation"},
        {"infrastructure",state.infrastructure?detail::infrastructure_json(*state.infrastructure):Json(nullptr)},
        {"resilience",state.resilience?detail::resilience_json(*state.resilience):Json(nullptr)},
        {"deployment_mode",state.deployments?"qualified":"foundation"},
        {"deployments",state.deployments?detail::deployment_json(*state.deployments):Json(nullptr)},
        {"business_offers_mode",state.business_offers?"catalog":"foundation"},
        {"business_offers",state.business_offers?business_offers_json(*state.business_offers):Json(nullptr)}};
    if (state.civic) result["civic"] = detail::civic_json(*state.civic);
    if (state.frontier) result["frontier"] = detail::frontier_json(*state.frontier);
    if (state.succession) result["succession"] = detail::succession_json(*state.succession);
    return result;
}
// The typed readers enforce field and storage bounds. Deep reconciliation
// runs after all world and runtime evidence has been restored.
core::SimulationState parse_simulation(const Json& value, bool packed = false, std::uint64_t seed = 0) {
    if (!value.is_object()) detail::invalid("simulation","Expected object");
    Json prior_shape=Json::object();
    for (const auto& [name,ignored]:value.items()) {
        (void)ignored;
        if (name!="accounts" && name!="accounting_mode" && name!="capacity" && name!="capacity_mode" &&
            name!="market" && name!="market_mode" && name!="liquidity" && name!="liquidity_mode" && name!="lab" && name!="lab_mode" &&
            name!="business_offers" && name!="business_offers_mode" && name!="deployment_mode" && name!="deployments" && name!="management_mode" && name!="management" && name!="infrastructure_mode" && name!="infrastructure" && name!="resilience" && name!="civic" && name!="frontier" && name!="succession")
            prior_shape[name]=nullptr;
    }
    detail::fields(prior_shape,"simulation",{"revision","date","end_date","phase","paused","cash_cents",
        "next_reservation_id","next_receipt_id","reservations","opening_actions","opening_cursor",
        "pending_choice","receipts","random_streams"});
    core::SimulationState result;
    result.random=core::RandomStreams{seed}.snapshot();
    result.revision=detail::u64(value.at("revision"),"revision");
    result.date=OrdinalDay{detail::i64(value.at("date"),"date")};
    result.end_date=OrdinalDay{detail::i64(value.at("end_date"),"end_date")};
    result.phase=parse_enum<core::DayPhase>(value.at("phase"),phase_names);
    result.paused=detail::boolean(value.at("paused"),"paused");
    result.cash=Cents{detail::i64(value.at("cash_cents"),"cash_cents")};
    if (value.contains("infrastructure_mode")!=value.contains("infrastructure")) detail::invalid("infrastructure","Infrastructure mode/state must appear together");
    if (value.contains("infrastructure_mode")) {
        const auto mode=detail::text(value.at("infrastructure_mode"),"infrastructure_mode",16);
        if (mode=="domestic") result.infrastructure=detail::parse_infrastructure(value.at("infrastructure"));
        else if (mode!="foundation" || !value.at("infrastructure").is_null()) detail::invalid("infrastructure","Infrastructure mode differs from its state");
    }
    if (value.contains("resilience") && !value.at("resilience").is_null()) result.resilience=detail::parse_resilience(value.at("resilience"));
    if (value.contains("civic")) result.civic = detail::parse_civic(value.at("civic"));
    if (value.contains("frontier")) result.frontier = detail::parse_frontier(value.at("frontier"));
    if (value.contains("succession")) result.succession = detail::parse_succession(value.at("succession"));
    if (value.contains("management_mode")!=value.contains("management")) detail::invalid("management","Management mode/state must appear together");
    if (value.contains("management_mode")) {
        const auto mode=detail::text(value.at("management_mode"),"management_mode",16);
        if (mode=="bounded") result.management=detail::parse_management(value.at("management"));
        else if (mode!="foundation" || !value.at("management").is_null()) detail::invalid("management","Management mode differs from its state");
    }
    if (value.contains("deployment_mode")!=value.contains("deployments")) detail::invalid("deployment","Deployment mode and state must be present together");
    if (value.contains("deployment_mode")) {
        const auto mode=detail::text(value.at("deployment_mode"),"deployment_mode",16);
        if (mode=="qualified") result.deployments=detail::parse_deployment(value.at("deployments"));
        else if (mode!="foundation" || !value.at("deployments").is_null()) detail::invalid("deployment","Deployment mode differs from its state");
    }
    if (value.contains("business_offers")!=value.contains("business_offers_mode")) detail::invalid("business_offers","Offer mode and state must be present together");
    if (value.contains("business_offers_mode")) {
        const auto mode=detail::text(value.at("business_offers_mode"),"business_offers_mode",16);
        if (mode=="catalog") result.business_offers=parse_business_offers(value.at("business_offers"));
        else if (mode!="foundation" || !value.at("business_offers").is_null()) detail::invalid("business_offers","Offer mode differs from its state");
    }
    if (value.contains("lab") != value.contains("lab_mode")) detail::invalid("lab_mode","Lab mode and state must be present together");
    if (value.contains("lab_mode")) {
        const auto mode=detail::text(value.at("lab_mode"),"lab_mode",16);
        if (mode=="training") result.lab=detail::parse_lab(value.at("lab"));
        else if (mode!="foundation" || !value.at("lab").is_null()) detail::invalid("lab_mode","Lab mode differs from its state");
    }
    // Only the original shape predating both fields is a legacy foundation
    // checkpoint. New business saves cannot silently lose their ledger.
    if (value.contains("accounts") != value.contains("accounting_mode"))
        detail::invalid("simulation/accounting_mode","Accounting mode and accounts must be present together");
    if (value.contains("accounting_mode")) {
        const auto mode=detail::text(value.at("accounting_mode"),"simulation/accounting_mode",16);
        if (mode=="business") result.accounts=detail::parse_accounts(value.at("accounts"), packed);
        else if (mode!="foundation" || !value.at("accounts").is_null())
            detail::invalid("simulation/accounting_mode","Accounting mode does not match its ledger");
    }
    // Capacity was absent from original foundation and accounts-only
    // checkpoints. An old shape keeps its explicit absence without free sources
    // or reconstructed staff/project history.
    if (value.contains("capacity") != value.contains("capacity_mode"))
        detail::invalid("simulation/capacity_mode","Capacity mode and state must be present together");
    if (value.contains("capacity_mode")) {
        const auto mode=detail::text(value.at("capacity_mode"),"simulation/capacity_mode",16);
        if (mode=="business") result.capacity=detail::parse_capacity(value.at("capacity"), packed);
        else if (mode!="foundation" || !value.at("capacity").is_null())
            detail::invalid("simulation/capacity_mode","Capacity mode does not match its state");
    }
    // Older engineering and accounts/capacity checkpoints did not own a market.
    // Keep that explicit absence; never reconstruct contracts or prior revenue.
    if (value.contains("market") != value.contains("market_mode"))
        detail::invalid("simulation/market_mode","Market mode and state must be present together");
    if (value.contains("market_mode")) {
        const auto mode=detail::text(value.at("market_mode"),"simulation/market_mode",16);
        if (mode=="business") result.market=detail::parse_market(value.at("market"), packed);
        else if (mode!="foundation" || !value.at("market").is_null())
            detail::invalid("simulation/market_mode","Market mode does not match its state");
    }
    // Legacy checkpoints retain the explicit absence of liquidity policy. Do
    // not manufacture a warned workout or reset an existing business deadline.
    if (value.contains("liquidity") != value.contains("liquidity_mode"))
        detail::invalid("simulation/liquidity_mode","Liquidity mode and state must be present together");
    if (value.contains("liquidity_mode")) {
        const auto mode=detail::text(value.at("liquidity_mode"),"simulation/liquidity_mode",16);
        if (mode=="business") result.liquidity=detail::parse_liquidity(value.at("liquidity"));
        else if (mode!="foundation" || !value.at("liquidity").is_null())
            detail::invalid("simulation/liquidity_mode","Liquidity mode does not match its state");
    }
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
    const auto& receipts=value.at("receipts");
    const bool packed_receipts = packed && !receipts.is_array();
    ReceiptCursor cursor;
    detail::read_history_rows(receipts, "receipts", core::simulation_receipt_limit, packed_receipts, [&](const Json& receipt) {
        if (packed_receipts) {
            result.receipts.push_back(parse_packed_receipt(receipt, cursor));
            return;
        }
        detail::fields(receipt,"receipt",{"id","revision","day","cause","reservation_id","cash_delta_cents"});
        result.receipts.push_back({detail::u64(receipt.at("id"),"id"),detail::u64(receipt.at("revision"),"revision"),
            OrdinalDay{detail::i64(receipt.at("day"),"day")},detail::text(receipt.at("cause"),"cause",256),
            detail::u64(receipt.at("reservation_id"),"reservation_id"),Cents{detail::i64(receipt.at("cash_delta_cents"),"cash_delta_cents")}});
    });
    const auto& random=value.at("random_streams");
    detail::fields(random,"random_streams",{"research","rivals","markets","geography","politics","hazards","narrative"},{"risk"});
    const bool risk_enabled=result.deployments && result.deployments->risk.has_value();
    if (random.contains("risk")!=risk_enabled)
        detail::invalid("random_streams","Deployment risk requires its explicit owned stream; legacy states retain seven streams");
    const auto streams=risk_enabled?core::stream_count:static_cast<std::size_t>(core::RandomStream::risk);
    for (std::size_t index=0;index<streams;++index) {
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
Json wholesale_json(const std::vector<WholesaleContract>& contracts) {
    Json result=Json::array();
    for (const auto& contract:contracts) {
        Json days=Json::array();
        for (const auto& row:contract.days) days.push_back(Json::array({
            row.available.value(),row.used.value(),row.service_credit.value(),row.electricity.value(),row.seller_posting,row.buyer_posting}));
        result.push_back({{"id",contract.id},{"plan",contract.plan_id},{"source",contract.source_id},{"buyer",contract.buyer_id},
            {"starts_on",contract.starts_on.value()},{"ends_before",contract.ends_before.value()},{"terminated_on",optional_day(contract.terminated_on)},
            {"reservation",contract.reservation_id},{"daily_ceiling_cents",contract.daily_ceiling.value()},{"days",std::move(days)}});
    }
    return result;
}
std::vector<WholesaleContract> parse_wholesale(const Json& value) {
    detail::array(value,"wholesale",wholesale_contract_limit);
    std::vector<WholesaleContract> result;
    for (const auto& contract:value) {
        detail::fields(contract,"wholesale/contract",{"id","plan","source","buyer","starts_on","ends_before",
            "terminated_on","reservation","daily_ceiling_cents","days"});
        WholesaleContract row{detail::text(contract.at("id"),"wholesale/id",64),detail::text(contract.at("plan"),"wholesale/plan",64),
            detail::text(contract.at("source"),"wholesale/source",128),detail::text(contract.at("buyer"),"wholesale/buyer",64),
            OrdinalDay{detail::i64(contract.at("starts_on"),"wholesale/starts_on")},OrdinalDay{detail::i64(contract.at("ends_before"),"wholesale/ends_before")},
            parse_day(contract.at("terminated_on")),detail::u64(contract.at("reservation"),"wholesale/reservation"),
            Cents{detail::i64(contract.at("daily_ceiling_cents"),"wholesale/daily_ceiling_cents")},{}};
        const auto& days=contract.at("days");
        detail::array(days,"wholesale/days",wholesale_day_limit);
        for (const auto& day:days) {
            detail::array(day,"wholesale/day",6);
            if (day.size()!=6) detail::invalid("wholesale/day","A settlement needs all six fields");
            row.days.push_back({core::MilliCUPerDay{detail::i64(day.at(0),"wholesale/available")},core::MilliCU{detail::i64(day.at(1),"wholesale/used")},
                Cents{detail::i64(day.at(2),"wholesale/credit")},Cents{detail::i64(day.at(3),"wholesale/electricity")},
                detail::u64(day.at(4),"wholesale/seller_posting"),detail::u64(day.at(5),"wholesale/buyer_posting")});
        }
        result.push_back(std::move(row));
    }
    return result;
}
struct DecisionCursor {
    OrdinalDay day{0};
    std::uint64_t stream = 0;
};
Json packed_decision(const RivalDecision& decision, DecisionCursor& cursor) {
    const auto gap = core::checked_subtract(decision.day.value(), std::exchange(cursor.day, decision.day).value());
    const auto stream_gap = decision.stream_before - cursor.stream;
    cursor.stream = decision.stream_after;
    const auto& observation = decision.observation;
    return Json::array({gap, decision.action, decision.subject, decision.explanation, decision.accepted,
        stream_gap, observation.public_providers, observation.public_price.value(),
        observation.demand_lower, observation.demand_upper});
}
RivalDecision parse_packed_decision(const Json& row, DecisionCursor& cursor) {
    detail::array(row, "competition/decision", 10);
    if (row.size() != 10) detail::invalid("competition/decision", "Missing packed decision columns");
    cursor.day = OrdinalDay{core::checked_add(cursor.day.value(), detail::i64(row.at(0), "competition/decision/day_gap"))};
    const auto before = cursor.stream + detail::u64(row.at(5), "competition/decision/stream_gap");
    core::SplitMix64 draw{before};
    (void)draw.next();
    cursor.stream = draw.state();
    const auto providers = static_cast<std::uint32_t>(detail::u64(row.at(6), "competition/decision/providers", 21));
    RivalObservation observation{cursor.day, providers, Cents{detail::i64(row.at(7), "competition/decision/price")},
        detail::u64(row.at(8), "competition/decision/demand_lower"), detail::u64(row.at(9), "competition/decision/demand_upper")};
    return {cursor.day, detail::text(row.at(1), "competition/decision/action", 32),
        detail::text(row.at(2), "competition/decision/subject", 256, true),
        detail::text(row.at(3), "competition/decision/explanation", 1024),
        detail::boolean(row.at(4), "competition/decision/accepted"), before, cursor.stream, observation};
}
Json competition_json(const CompetitionState& state, bool packed_world) {
    Json companies=Json::array(),notices=Json::array();
    detail::CompetitionHistoryBudget history_budget;
    for (const auto& row:state.companies) {
        const bool packed = row.planning && archive_history(row.simulation);
        Json company{{"definition_id",row.definition_id},{"simulation",simulation_json(row.simulation, packed)},
            {"attempted",row.attempted},{"warned_on",optional_day(row.warned_on)},{"published_on",optional_day(row.published_on)}};
        if (row.planning) {
            if (packed) company["simulation_storage"] = packed_history_format;
            else {
                company["simulation"]=detail::compact_competition_history(std::move(company.at("simulation")),history_budget);
                company["simulation_storage"]="history-rows-v2";
            }
            detail::HistoryRows decisions{packed};
            DecisionCursor cursor;
            for (const auto& decision:row.planning->decisions) {
                if (packed) {
                    decisions.push(packed_decision(decision, cursor));
                    continue;
                }
                const auto& observation=decision.observation;
                decisions.push({{"day",decision.day.value()},{"action",decision.action},{"subject",decision.subject},{"explanation",decision.explanation},
                    {"accepted",decision.accepted},{"stream_before",decision.stream_before},{"stream_after",decision.stream_after},
                    {"observation",{{"day",observation.day.value()},{"providers",observation.public_providers},{"price",observation.public_price.value()},
                        {"demand_lower",observation.demand_lower},{"demand_upper",observation.demand_upper}}}});
            }
            company["planning"]={{"entered_on",optional_day(row.planning->entered_on)},{"next_review",row.planning->next_review.value()},{"decisions",decisions.finish()}};
        }
        if (!row.parent_company.empty()) company["parent_company"]=row.parent_company;
        if (row.response) company["response"]=detail::heavyweight_json(*row.response);
        companies.push_back(std::move(company));
    }
    for (const auto& row:state.notices) {
        Json value{{"company_id",row.company_id},{"kind",row.kind},{"day",row.day.value()},{"response_due",optional_day(row.response_due)}};
        if (!row.subject.empty()) value["subject"]=row.subject;notices.push_back(std::move(value));
    }
    Json result{{"companies",std::move(companies)},{"notices",std::move(notices)}};
    if (state.setup) {
        result["setup"]=Json::parse(content::canonical_competition_setup(*state.setup));
        result["offers"]=detail::competitive_offers_json(state.offers);
        if (!state.acquisitions.empty()) result["acquisitions"]=detail::acquisitions_json(state.acquisitions);
        if (!state.wholesale.empty()) result["wholesale"]=wholesale_json(state.wholesale);
        result["releases"]=Json::array();for (const auto& row:state.releases) result["releases"].push_back({{"company",row.company_id},{"product",row.product_id},{"day",row.day.value()}});
        detail::HistoryRows adoptions{packed_world};
        for (const auto& row:state.adoptions) {
            Json shares=Json::array();for (const auto& share:row.shares) shares.push_back({{"product",share.product_id},{"share",share.share.value()}});
            adoptions.push({{"day",row.reviewed_on.value()},{"segment",row.segment_id},{"observed",row.observed_tasks},{"shares",std::move(shares)}});
        }
        result["adoptions"] = adoptions.finish();
    }
    return result;
}
CompetitionState parse_competition(const Json& value, bool packed_world, const CompiledProgram& program, std::uint64_t root_seed) {
    detail::fields(value,"competition",{"companies","notices"},{"setup","releases","adoptions","offers","acquisitions","wholesale"}); CompetitionState result;
    if (value.contains("acquisitions")) result.acquisitions=detail::parse_acquisitions(value.at("acquisitions"));
    if (value.contains("offers")) result.offers=detail::parse_competitive_offers(value.at("offers"));
    if (value.contains("wholesale")) result.wholesale=parse_wholesale(value.at("wholesale"));
    if (value.contains("setup")) {
        if (!value.contains("releases") || !value.contains("adoptions")) detail::invalid("competition","Planned competition lost public market state");
        result.setup=content::load_competition_setup(value.at("setup").dump());
        detail::array(value.at("releases"),"competition/releases",core::allocation_product_limit);
        for (const auto& row:value.at("releases")) {
            detail::fields(row,"competition/release",{"company","product","day"});
            result.releases.push_back({detail::text(row.at("company"),"company",128),detail::text(row.at("product"),"product",128),OrdinalDay{detail::i64(row.at("day"),"day")}});
        }
        const auto& adoptions = value.at("adoptions");
        detail::read_history_rows(adoptions, "competition/adoptions", core::market_record_limit,
                                  packed_world && !adoptions.is_array(), [&](const Json& row) {
            detail::fields(row,"competition/adoption",{"day","segment","observed","shares"});
            core::MarketAdoption adoption{OrdinalDay{detail::i64(row.at("day"),"day")},detail::text(row.at("segment"),"segment",128),detail::u64(row.at("observed"),"observed"),{}};
            detail::array(row.at("shares"),"competition/shares",core::allocation_product_limit+1);
            for (const auto& share:row.at("shares")) {
                detail::fields(share,"competition/share",{"product","share"});
                adoption.shares.push_back({detail::text(share.at("product"),"product",256),core::BasisPoints{detail::i64(share.at("share"),"share")}});
            }
            result.adoptions.push_back(std::move(adoption));
        });
    } else if (value.contains("releases") || value.contains("adoptions") || value.contains("offers") ||
        value.contains("acquisitions") || value.contains("wholesale")) detail::invalid("competition","Public industry state has no authored setup");
    detail::array(value.at("companies"),"competition/companies",32); detail::array(value.at("notices"),"competition/notices",result.setup?4096:128);
    detail::CompetitionHistoryBudget history_budget;
    for (const auto& row:value.at("companies")) {
        detail::fields(row,"competition/company",{"definition_id","simulation","attempted","warned_on","published_on"},{"planning","simulation_storage","response","parent_company"});
        const auto format = row.contains("simulation_storage") ? detail::text(row.at("simulation_storage"), "simulation_storage", 32) : std::string{};
        const bool packed = format == packed_history_format;
        if (!format.empty() && !packed && format!="history-rows-v1" && format!="history-rows-v2")
            detail::invalid("competition/history","Unsupported history storage format; original save was preserved");
        const auto& source = row.at("simulation");
        const auto definition_id=detail::text(row.at("definition_id"),"competition/definition_id",64);
        const auto& definition=rival_definition(program,definition_id);
        const auto seed=result.setup?root_seed^definition.seed:definition.seed;
        auto simulation = format.empty() || packed ? parse_simulation(source, packed, seed) :
            parse_simulation(detail::expand_competition_history(source, history_budget, format=="history-rows-v2"), false, seed);
        if (packed && (!row.contains("planning") || !archive_history(simulation)))
            detail::invalid("competition/history", "Packed rival history requires a planned company with settled days");
        RivalCompany company{definition_id,std::move(simulation),
            detail::boolean(row.at("attempted"),"competition/attempted"),parse_day(row.at("warned_on")),parse_day(row.at("published_on"))};
        if (row.contains("planning")) {
            const auto& plan=row.at("planning");detail::fields(plan,"competition/plan",{"entered_on","next_review","decisions"});
            RivalPlanState next{parse_day(plan.at("entered_on")),OrdinalDay{detail::i64(plan.at("next_review"),"next_review")},{}};
            DecisionCursor cursor;
            detail::read_history_rows(plan.at("decisions"), "competition/decisions", rival_decision_limit, packed, [&](const Json& decision) {
                if (packed && decision.is_array()) {
                    next.decisions.push_back(parse_packed_decision(decision, cursor));
                    return;
                }
                detail::fields(decision,"competition/decision",{"day","action","subject","explanation","accepted","stream_before","stream_after","observation"});
                const auto& observation=decision.at("observation");detail::fields(observation,"competition/observation",{"day","providers","price","demand_lower","demand_upper"});
                const auto providers=detail::u64(observation.at("providers"),"providers");if (providers>21) detail::invalid("providers","Public provider count exceeds its bound");
                RivalObservation observed{OrdinalDay{detail::i64(observation.at("day"),"day")},static_cast<std::uint32_t>(providers),Cents{detail::i64(observation.at("price"),"price")},
                    detail::u64(observation.at("demand_lower"),"demand_lower"),detail::u64(observation.at("demand_upper"),"demand_upper")};
                next.decisions.push_back({OrdinalDay{detail::i64(decision.at("day"),"day")},detail::text(decision.at("action"),"action",32),detail::text(decision.at("subject"),"subject",256,true),
                    detail::text(decision.at("explanation"),"explanation",1024),detail::boolean(decision.at("accepted"),"accepted"),detail::u64(decision.at("stream_before"),"stream_before"),
                    detail::u64(decision.at("stream_after"),"stream_after"),observed});
                cursor.day = next.decisions.back().day;
                cursor.stream = next.decisions.back().stream_after;
            });
            company.planning=std::move(next);
        }
        if (row.contains("parent_company")) company.parent_company=detail::text(row.at("parent_company"),"competition/parent",128);
        if (row.contains("response")) {
            if (!result.setup || !company.planning || !value.contains("offers")) detail::invalid("heavyweight","Response-bearing saves require an authored planner and explicit funded offers, including an empty list");
            company.response=detail::parse_heavyweight(row.at("response"));
        }
        result.companies.push_back(std::move(company));
    }
    for (const auto& row:value.at("notices")) {
        detail::fields(row,"competition/notice",{"company_id","kind","day","response_due"},{"subject"});
        result.notices.push_back({detail::text(row.at("company_id"),"competition/company_id",64),detail::text(row.at("kind"),"competition/kind",32),
            OrdinalDay{detail::i64(row.at("day"),"competition/day")},parse_day(row.at("response_due")),row.contains("subject")?detail::text(row.at("subject"),"subject",128):std::string{}});
    }
    return result;
}
Json world_json(const WorldState& world) {
    Json facts=Json::object();for (const auto& [name,value]:world.facts) facts[name]=scalar(value);
    Json variables=Json::array();
    for (const auto& variable:world.variables) variables.push_back({
        {"binding",binding(variable.binding)},{"variable_id",variable.variable_id},{"value",scalar(variable.value)}});
    Json enabled=nullptr;if (world.enabled_flows) enabled=strings(*world.enabled_flows);
    const bool packed = world.objectives && archive_history(world.simulation);
    auto simulation=simulation_json(world.simulation, packed);
    Json result{{"simulation",std::move(simulation)},{"company_id",world.company_id},
        {"world_id",world.world_id},{"state_id",world.state_id},{"campaign_start",world.campaign_start.value()},
        {"scope_created",world.scope_created.value()},{"root_seed",world.root_seed},{"enabled_flows",std::move(enabled)},
        {"facts",std::move(facts)},{"counterparty_cash_cents",world.counterparty_cash.value()},
        {"decisions",strings(world.decisions)},{"negotiations",strings(world.negotiations)},{"variables",std::move(variables)},
        {"competition_mode",world.competition?(world.competition->setup?"planned":"bounded"):"foundation"},
        {"competition",world.competition?competition_json(*world.competition, packed):Json(nullptr)}};
    if(packed) result["simulation_storage"] = packed_history_format;
    if (!world.scenario_id.empty()) result["scenario_id"]=world.scenario_id;
    if (world.society) result["society"]=detail::society_json(*world.society);
    if (world.authority) result["authority"]=detail::authority_json(*world.authority);
    if (world.objectives) result["objectives"]=detail::objectives_json(*world.objectives);
    if (!world.difficulty.empty()) result["difficulty"] = world.difficulty;
    if (!world.background.empty()) result["background"] = world.background;
    if (world.counties) result["counties"]=detail::counties_json(*world.counties);
    if (world.founding) result["founding"]=Json::parse(content::canonical_founding_selection(*world.founding));
    if (world.identity) result["identity"] = {{"company_name",world.identity->company_name},
        {"founder_name",world.identity->founder_name},{"founder_pronouns",world.identity->founder_pronouns}};
    return result;
}
WorldState parse_world(Json& value,const CompiledProgram& program) {
    if (!value.is_object()) detail::invalid("world","Expected object");
    Json prior_shape=Json::object();
    for (const auto& [name,ignored]:value.items()) {
        (void)ignored;
        if (name!="variables" && name!="competition" && name!="competition_mode" && name!="scenario_id" && name!="society" &&
            name!="authority" && name!="objectives" && name!="simulation_storage" && name!="identity" && name!="background" && name!="difficulty" && name!="founding" && name!="counties") prior_shape[name]=nullptr;
    }
    detail::fields(prior_shape,"world",{"simulation","company_id","world_id","state_id","campaign_start","scope_created",
                                   "root_seed","enabled_flows","facts","counterparty_cash_cents","decisions","negotiations"});
    if (!value.contains("variables") && std::any_of(program.registry.facts().begin(),program.registry.facts().end(),[](const auto& entry) {
        return entry.second.default_value.has_value();
    })) detail::invalid("variables","Registered variables require an explicit value collection");
    WorldState result;
    if(value.contains("counties")) result.counties=detail::parse_counties(value.at("counties"));
    if(value.contains("founding")) result.founding=content::load_founding_selection(value.at("founding").dump());
    result.root_seed=detail::u64(value.at("root_seed"),"root_seed");
    if (value.contains("difficulty")) result.difficulty=detail::text(value.at("difficulty"),"world/difficulty",128);
    if (value.contains("background")) result.background=detail::text(value.at("background"),"world/background",128);
    if (value.contains("identity")) {
        const auto& identity = value.at("identity");
        detail::fields(identity,"world/identity",{"company_name","founder_name","founder_pronouns"});
        result.identity = CampaignIdentity{detail::text(identity.at("company_name"),"company_name",128),
            detail::text(identity.at("founder_name"),"founder_name",128),
            detail::text(identity.at("founder_pronouns"),"founder_pronouns",64,true)};
    }
    const auto format = value.contains("simulation_storage") ?
        detail::text(value.at("simulation_storage"), "world/simulation_storage", 32) : std::string{};
    const bool packed = format == packed_history_format;
    if (value.contains("society")) result.society=detail::parse_society(value.at("society"));
    if (value.contains("authority")) result.authority=detail::parse_authority(value.at("authority"));
    if (value.contains("objectives")) result.objectives=detail::parse_objectives(value.at("objectives"));
    if (value.contains("scenario_id")) result.scenario_id=detail::text(value.at("scenario_id"),"scenario_id",128);
    if (value.contains("competition")!=value.contains("competition_mode")) detail::invalid("competition","Competition mode and state must be present together");
    if (value.contains("competition_mode")) {
        const auto mode=detail::text(value.at("competition_mode"),"competition_mode",16);
        if (mode=="bounded" || mode=="planned") {
            result.competition=parse_competition(value.at("competition"), packed, program, result.root_seed);
            if ((mode=="planned")!=result.competition->setup.has_value()) detail::invalid("competition","Competition mode differs from its authored planner setup");
        }
        else if (mode!="foundation" || !value.at("competition").is_null()) detail::invalid("competition","Competition mode differs from its state");
    }
    if(!format.empty()) {
        if(!result.objectives || (format != "history-rows-v2" && !packed))
            detail::invalid("world/simulation_storage","Unsupported compact century history");
        if (packed) result.simulation = parse_simulation(value.at("simulation"), true, result.root_seed);
        else {
            detail::CompetitionHistoryBudget history_budget;
            result.simulation=parse_simulation(detail::expand_competition_history(std::move(value.at("simulation")), history_budget, true), false, result.root_seed);
        }
        if(!archive_history(result.simulation))
            detail::invalid("world/simulation_storage","The compact history lacks the required settled days");
    } else result.simulation=parse_simulation(value.at("simulation"), false, result.root_seed);
    result.company_id=detail::text(value.at("company_id"),"company_id",128);
    result.world_id=detail::text(value.at("world_id"),"world_id",128);
    result.state_id=detail::text(value.at("state_id"),"state_id",128);
    result.campaign_start=OrdinalDay{detail::i64(value.at("campaign_start"),"campaign_start")};
    result.scope_created=OrdinalDay{detail::i64(value.at("scope_created"),"scope_created")};
    if (!value.at("enabled_flows").is_null()) result.enabled_flows=parse_strings(value.at("enabled_flows"),maximum_activations);
    result.counterparty_cash=Cents{detail::i64(value.at("counterparty_cash_cents"),"counterparty_cash_cents")};
    result.decisions=parse_strings(value.at("decisions"));result.negotiations=parse_strings(value.at("negotiations"));
    const auto& facts=value.at("facts");
    if (!facts.is_object()||facts.size()>4096) detail::invalid("facts","Expected bounded registered fact map");
    for (const auto& [name,item]:facts.items()) {
        if (name.size()>128) detail::invalid("fact","Fact ID is oversized");
        result.facts.emplace(name,parse_scalar(item));
    }
    if (value.contains("variables")) {
        const auto& variables=value.at("variables");
        detail::array(variables,"variables",maximum_records);
        for (const auto& item:variables) {
            detail::fields(item,"variable",{"binding","variable_id","value"});
            result.variables.push_back({parse_binding(item.at("binding")),
                detail::text(item.at("variable_id"),"variable_id",128),parse_scalar(item.at("value"))});
        }
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
        if (activation.committed_source) activations.back()["committed_source"]=committed_source_json(*activation.committed_source);
    }
    for (const auto& lifecycle:state.lifecycles) {
        lifecycles.push_back({{"definition_id",lifecycle.definition_id},
            {"binding",binding(lifecycle.binding)},{"occurrences",lifecycle.occurrences},{"was_eligible",lifecycle.was_eligible},
            {"last_checked",optional_day(lifecycle.last_checked)},{"last_started",optional_day(lifecycle.last_started)},
            {"first_due",optional_day(lifecycle.first_due)},{"calendar_day",lifecycle.calendar_day},{"interval_index",lifecycle.interval_index},
            {"active_activation",optional_id(lifecycle.active_activation)}});
        if (lifecycle.committed_cursor) lifecycles.back()["committed_cursor"]=*lifecycle.committed_cursor;
    }
    for (const auto& modifier:state.modifiers) modifiers.push_back({{"activation_id",modifier.activation_id},
        {"binding",binding(modifier.binding)},{"key",modifier.key},{"field",modifier.field},{"add_basis_points",modifier.add_basis_points}});
    for (const auto& receipt:state.receipts) {
        receipts.push_back({{"id",receipt.id},{"activation_id",receipt.activation_id},
        {"revision",receipt.revision},{"day",receipt.day.value()},{"flow_id",receipt.flow_id},{"node_id",receipt.node_id},
        {"effect_index",receipt.effect_index},{"operation",receipt.operation},{"target",binding(receipt.target)},
        {"before",scalar(receipt.before)},{"after",scalar(receipt.after)},{"counterparty_delta_cents",receipt.counterparty_delta.value()}});
        if (!receipt.bound_variable.empty()) receipts.back()["bound_variable"]=receipt.bound_variable;
        if (!receipt.group_calls.empty()) receipts.back()["group_calls"]=receipt.group_calls;
    }
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
    Json body{{"next_activation_id",state.next_activation_id},{"next_receipt_id",state.next_receipt_id},
        {"next_presentation_id",state.next_presentation_id},{"prepared_day",optional_day(state.prepared_day)},
        {"activations",std::move(activations)},{"lifecycles",std::move(lifecycles)},{"modifiers",std::move(modifiers)},
        {"receipts",std::move(receipts)},{"presentations",std::move(presentations)},
        {"random_resolutions",std::move(random_resolutions)},{"narrative_origin",optional_id(state.narrative_origin)},
        {"duration_states",std::move(duration_states)},{"duration_sampled_day",optional_day(state.duration_sampled_day)}};
    if (!state.incident_origins.empty()) {
        auto origins=Json::object();
        for (const auto& [stream,origin]:state.incident_origins)
            origins[core::stream_names.at(static_cast<std::size_t>(stream))]=origin;
        body["incident_origins"]=std::move(origins);
    }
    return body;
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
            "activations","lifecycles","modifiers","receipts","presentations","random_resolutions","narrative_origin"},{"incident_origins"});
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
    const auto& activations=value.at("activations");detail::array(activations,"activations",maximum_activation_history);
    for (const auto& item:activations) {
        detail::fields(item,"activation",{"id","definition_id","definition_revision","started_on","binding","current_flow","current_node",
            "frames","status","due","wait_started","presentation_id","node_visits","exiting"},{"committed_source"});
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
        if (item.contains("committed_source")) activation.committed_source=parse_committed_source(item.at("committed_source"));
        const auto& frames=item.at("frames");detail::array(frames,"frames",maximum_continuation_frames);
        for (const auto& frame:frames) activation.frames.push_back(parse_frame(frame));
        result.activations.push_back(std::move(activation));
    }
    const auto& lifecycles=value.at("lifecycles");detail::array(lifecycles,"lifecycles",maximum_activations);
    for (const auto& item:lifecycles) {
        detail::fields(item,"lifecycle",{"definition_id","binding","occurrences","was_eligible","last_checked","last_started",
            "first_due","calendar_day","interval_index","active_activation"},{"committed_cursor"});
        result.lifecycles.push_back({detail::text(item.at("definition_id"),"definition_id",128),parse_binding(item.at("binding")),
            static_cast<std::uint32_t>(detail::u64(item.at("occurrences"),"occurrences",max_u32)),detail::boolean(item.at("was_eligible"),"was_eligible"),
            parse_day(item.at("last_checked")),parse_day(item.at("last_started")),parse_day(item.at("first_due")),
            static_cast<unsigned>(detail::u64(item.at("calendar_day"),"calendar_day",31)),
            detail::u64(item.at("interval_index"),"interval_index"),parse_id(item.at("active_activation"))});
        if (item.contains("committed_cursor"))
            result.lifecycles.back().committed_cursor=static_cast<std::uint32_t>(detail::u64(item.at("committed_cursor"),"committed_cursor",max_u32));
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
            "operation","target","before","after","counterparty_delta_cents"},{"bound_variable","group_calls"});
        result.receipts.push_back({detail::u64(item.at("id"),"id"),detail::u64(item.at("activation_id"),"activation_id"),
            detail::u64(item.at("revision"),"revision"),OrdinalDay{detail::i64(item.at("day"),"day")},
            detail::text(item.at("flow_id"),"flow_id",128),detail::text(item.at("node_id"),"node_id",128),
            static_cast<std::size_t>(detail::u64(item.at("effect_index"),"effect_index",512)),
            detail::text(item.at("operation"),"operation",128),parse_binding(item.at("target")),parse_scalar(item.at("before")),
            parse_scalar(item.at("after")),Cents{detail::i64(item.at("counterparty_delta_cents"),"counterparty_delta_cents")}});
        if (item.contains("bound_variable")) result.receipts.back().bound_variable=detail::text(item.at("bound_variable"),"bound_variable",128);
        if (item.contains("group_calls")) result.receipts.back().group_calls=parse_strings(item.at("group_calls"),runtime::maximum_continuation_frames);
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
        if (value.contains("incident_origins")) {
            const auto& origins=value.at("incident_origins");
            if (!origins.is_object() || origins.empty() || origins.size()>2)
                detail::invalid("incident_origins","Expected nonempty bounded stream origins");
            for (const auto& [name,origin]:origins.items()) {
                const auto stream=content::random_branch_stream(name);
                if (!stream || !content::is_incident_stream(*stream))
                    detail::invalid("incident_origins","Unknown incident stream");
                result.incident_origins.emplace(*stream,detail::u64(origin,"incident_origin"));
            }
        }
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
        throw PersistenceError("content_identity","This save requires its original content revision or a reviewed migration copy.");
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
namespace {
std::string encode_save_impl(const runtime::CompiledProgram& program,const runtime::Checkpoint& checkpoint,
                             std::uint64_t autosave_sequence,SaveLimits limits,const SaveIdentity* checked_identity) {
    limits_valid(limits);
    try {
        runtime::validate_checkpoint(program,checkpoint);
        const auto identity=checked_identity?*checked_identity:save_identity(program);
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
LoadedSave decode_save_impl(const runtime::CompiledProgram& program,std::string_view bytes,SaveLimits limits,const SaveIdentity* checked_identity) {
    limits_valid(limits);
    try {
        auto body=detail::parse(bytes,limits.maximum_bytes,limits.maximum_json_events);
        detail::fields(body,"save",{"format","identity","autosave_sequence","checkpoint","sha256"});
        if (detail::text(body.at("format"),"format",64)!="project-glowing-pancake.save")
            throw PersistenceError("format","This is not a Project Glowing Pancake save");
        auto identity=parse_identity(body.at("identity"));check_identity(identity,checked_identity?*checked_identity:save_identity(program));
        const auto digest=detail::text(body.at("sha256"),"sha256",64);
        body.erase("sha256");
        if (digest!=sha256(body.dump())) throw PersistenceError("integrity","Save checksum does not match; use its recovery copy or another autosave");
        const auto sequence=detail::u64(body.at("autosave_sequence"),"autosave_sequence");
        auto& state=body.at("checkpoint");detail::fields(state,"checkpoint",{"world","runtime"});
        runtime::Checkpoint checkpoint{parse_world(state.at("world"),program),parse_runtime(state.at("runtime"),program)};
        runtime::validate_checkpoint(program,checkpoint);
        return {std::move(identity),std::move(checkpoint),sequence};
    } catch (const PersistenceError&) {throw;}
    catch (const std::exception& error) {throw PersistenceError("invalid_state",error.what());}
}
}
std::string encode_save(const runtime::CompiledProgram& program,const runtime::Checkpoint& checkpoint,
                        std::uint64_t autosave_sequence,SaveLimits limits) {
    return encode_save_impl(program,checkpoint,autosave_sequence,limits,nullptr);
}
LoadedSave decode_save(const runtime::CompiledProgram& program,std::string_view bytes,SaveLimits limits) {
    return decode_save_impl(program,bytes,limits,nullptr);
}
namespace {
SaveIdentity revision_identity(const runtime::CompiledProgram& program, std::string_view game_version) {
    if (game_version.empty() || game_version.size() > 256)
        throw PersistenceError("game_version", "A bounded finalized game version is required for revision review");
    auto identity = save_identity(program);
    identity.game_version = game_version;
    return identity;
}
}
LoadedSave decode_revision_save(const runtime::CompiledProgram& program,
    std::string_view game_version, std::string_view bytes, SaveLimits limits) {
    const auto identity = revision_identity(program, game_version);
    return decode_save_impl(program, bytes, limits, &identity);
}
std::string encode_revision_save(const runtime::CompiledProgram& program,
    std::string_view game_version, const runtime::Checkpoint& checkpoint,
    std::uint64_t autosave_sequence, SaveLimits limits) {
    const auto identity = revision_identity(program, game_version);
    return encode_save_impl(program, checkpoint, autosave_sequence, limits, &identity);
}
void validate_save_roundtrips(const runtime::CompiledProgram& program,std::span<const runtime::Checkpoint> checkpoints,SaveLimits limits) {
    limits_valid(limits);
    const auto identity=save_identity(program);
    for (const auto& checkpoint:checkpoints) {
        const auto bytes=encode_save_impl(program,checkpoint,0,limits,&identity);
        if (decode_save_impl(program,bytes,limits,&identity).checkpoint!=checkpoint)
            throw PersistenceError("roundtrip","A prepared scenario did not roundtrip through the actual save validator.");
    }
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
    if (reason==AutosaveReason::campaign_end && checkpoint.world.simulation.phase!=core::DayPhase::finished &&
        checkpoint.world.simulation.phase!=core::DayPhase::closed && !runtime::objective_time_stopped(checkpoint.world))
        throw PersistenceError("autosave_boundary","Campaign-end autosaves require a recorded campaign ending");
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
