#include "heavyweight_json.hpp"
#include <array>
namespace pgp::persistence::detail {
namespace {
using namespace runtime;using core::OrdinalDay;
Json optional(const std::optional<OrdinalDay>& value) {return value?Json(value->value()):Json(nullptr);}
OrdinalDay day(const Json& value) {return OrdinalDay{i64(value,"heavyweight/day")};}
std::optional<OrdinalDay> maybe(const Json& value) {return value.is_null()?std::nullopt:std::optional{day(value)};}
template<class Enum,std::size_t Count> Enum parse_enum(const Json& value,const std::array<std::string_view,Count>& names) {
    const auto name=text(value,"heavyweight/enum",32);const auto it=std::find(names.begin(),names.end(),name);
    if (it==names.end()) invalid("heavyweight/enum","Unsupported state or offer kind");return static_cast<Enum>(it-names.begin());
}
constexpr std::array<std::string_view,4> phases{"watch","mobilize","compete","consolidate"};
constexpr std::array<std::string_view,2> kinds{"publication","pilot"};
constexpr std::array<std::string_view,7> statuses{"pending","active","paid","declined","expired","cancelled","failed"};
}
Json heavyweight_json(const HeavyweightState& state) {
    Json reviews=Json::array(),transitions=Json::array();
    for (const auto& row:state.reviews) {
        Json components=Json::array();for (const auto score:row.components) components.push_back(score.value());
        reviews.push_back({{"day",row.day.value()},{"components",std::move(components)},{"score",row.score.value()},{"revenue",row.substituted_revenue.value()},{"tasks",row.delivered_tasks},{"demand",row.customer_demand},{"services",row.public_services},{"rounds",row.disclosed_rounds}});
    }
    for (const auto& row:state.transitions) transitions.push_back({{"day",row.day.value()},{"phase",to_string(row.phase)},{"cause",row.cause},{"due",optional(row.due)}});
    return {{"phase",to_string(state.phase)},{"next_review",state.next_review.value()},{"high_reviews",state.high_reviews},{"low_reviews",state.low_reviews},{"warned_on",optional(state.warned_on)},{"mobilized_on",optional(state.mobilized_on)},
        {"ready_on",optional(state.ready_on)},{"retired_on",optional(state.retired_on)},{"offers_issued",state.offers_issued},{"reviews",std::move(reviews)},{"transitions",std::move(transitions)}};
}
HeavyweightState parse_heavyweight(const Json& j) {
    fields(j,"heavyweight",{"phase","next_review","high_reviews","low_reviews","warned_on","mobilized_on","ready_on","retired_on","offers_issued","reviews","transitions"});HeavyweightState state;
    state.phase=parse_enum<HeavyweightPhase>(j.at("phase"),phases);state.next_review=day(j.at("next_review"));state.high_reviews=static_cast<std::uint32_t>(u64(j.at("high_reviews"),"high_reviews",8192));state.low_reviews=static_cast<std::uint32_t>(u64(j.at("low_reviews"),"low_reviews",8192));
    state.warned_on=maybe(j.at("warned_on"));state.mobilized_on=maybe(j.at("mobilized_on"));state.ready_on=maybe(j.at("ready_on"));state.retired_on=maybe(j.at("retired_on"));state.offers_issued=boolean(j.at("offers_issued"),"offers_issued");
    array(j.at("reviews"),"heavyweight/reviews",8192);
    for (const auto& row:j.at("reviews")) {
        fields(row,"heavyweight/review",{"day","components","score","revenue","tasks","demand","services","rounds"});array(row.at("components"),"heavyweight/components",4);if (row.at("components").size()!=4) invalid("heavyweight/components","Expected four public pressure components");
        PressureReview review;review.day=day(row.at("day"));for (std::size_t i=0;i<4;++i) review.components[i]=core::BasisPoints{i64(row.at("components")[i],"component")};
        review.score=core::BasisPoints{i64(row.at("score"),"score")};review.substituted_revenue=core::Cents{i64(row.at("revenue"),"revenue")};review.delivered_tasks=u64(row.at("tasks"),"tasks");review.customer_demand=u64(row.at("demand"),"demand");
        review.public_services=static_cast<std::uint32_t>(u64(row.at("services"),"services",64));review.disclosed_rounds=static_cast<std::uint32_t>(u64(row.at("rounds"),"rounds",256));state.reviews.push_back(std::move(review));
    }
    array(j.at("transitions"),"heavyweight/transitions",512);
    for (const auto& row:j.at("transitions")) {fields(row,"heavyweight/transition",{"day","phase","cause","due"});state.transitions.push_back({day(row.at("day")),parse_enum<HeavyweightPhase>(row.at("phase"),phases),text(row.at("cause"),"cause",128),maybe(row.at("due"))});}
    return state;
}
Json competitive_offers_json(const std::vector<CompetitiveOffer>& offers) {
    Json result=Json::array();for (const auto& row:offers) {result.push_back({{"id",row.id},{"company",row.company_id},{"subject",row.subject},{"kind",to_string(row.kind)},{"status",to_string(row.status)},
        {"issued_on",row.issued_on.value()},{"respond_by",row.respond_by.value()},{"accepted_on",optional(row.accepted_on)},{"due_on",optional(row.due_on)},{"resolved_on",optional(row.resolved_on)},{"last_delivery_day",optional(row.last_delivery_day)},
        {"amount",row.amount.value()},{"reservation",row.reservation_id},{"required_tasks",row.required_tasks},{"delivered_tasks",row.delivered_tasks}});}
    return result;
}
std::vector<CompetitiveOffer> parse_competitive_offers(const Json& rows) {
    array(rows,"competitive/offers",40);std::vector<CompetitiveOffer> result;
    for (const auto& j:rows) {
        fields(j,"competitive/offer",{"id","company","subject","kind","status","issued_on","respond_by","accepted_on","due_on","resolved_on","last_delivery_day","amount","reservation","required_tasks","delivered_tasks"});CompetitiveOffer row;
        row.id=text(j.at("id"),"id",128);row.company_id=text(j.at("company"),"company",64);row.subject=text(j.at("subject"),"subject",128);row.kind=parse_enum<CompetitiveOfferKind>(j.at("kind"),kinds);row.status=parse_enum<CompetitiveOfferStatus>(j.at("status"),statuses);
        row.issued_on=day(j.at("issued_on"));row.respond_by=day(j.at("respond_by"));row.accepted_on=maybe(j.at("accepted_on"));row.due_on=maybe(j.at("due_on"));row.resolved_on=maybe(j.at("resolved_on"));row.last_delivery_day=maybe(j.at("last_delivery_day"));
        row.amount=core::Cents{i64(j.at("amount"),"amount")};row.reservation_id=u64(j.at("reservation"),"reservation");row.required_tasks=u64(j.at("required_tasks"),"required_tasks");row.delivered_tasks=u64(j.at("delivered_tasks"),"delivered_tasks");result.push_back(std::move(row));
    }
    return result;
}
}
