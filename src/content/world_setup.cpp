#include <pgp/content/world_setup.hpp>
#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
Json parse(std::string_view bytes) {
    if (bytes.size()>maximum_document_bytes) invalid("World setup exceeds its byte bound");
    std::size_t tokens=0; std::vector<std::set<std::string>> keys;
    return Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>16 || ++tokens>32768) invalid("World setup exceeds its JSON budget");
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid("Duplicate world setup key");
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid("World setup uses integer tokens only");
        return true;
    });
}
void fields(const Json& row,std::initializer_list<std::string_view> names) {
    if (!row.is_object() || row.size()!=names.size()) invalid("World setup has missing or unsupported fields");
    for (auto name:names) if (!row.contains(name)) invalid("World setup has missing or unsupported fields");
}
std::string text(const Json& value,bool empty=false) {
    if (!value.is_string()) invalid("World setup needs a label");
    auto result=value.get<std::string>();
    if ((!empty && result.empty()) || result.size()>128 || std::any_of(result.begin(),result.end(),[](unsigned char c){return c<32 || c==127;}))
        invalid("World setup label is empty, oversized or contains control characters");
    return result;
}
std::uint64_t unsigned_number(const Json& value) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>()<0)) invalid("World setup requires nonnegative integer tokens");
    return value.get<std::uint64_t>();
}
std::uint64_t number(const Json& value) {
    const auto result=unsigned_number(value);
    if (result>static_cast<std::uint64_t>(INT64_MAX)) invalid("World setup integer exceeds its signed unit bound");
    return result;
}
core::OrdinalDay date(const Json& value) { return core::parse_iso_date(text(value)); }
void array(const Json& value,std::size_t maximum) {
    if (!value.is_array() || value.size()>maximum) invalid("World setup count exceeds its bound");
}
}
std::vector<core::BusinessOffer> load_business_offer_catalog(std::string_view bytes) {
    const auto doc=parse(bytes); array(doc,32); std::vector<core::BusinessOffer> result;
    for (auto row:doc) {
        if (!row.contains("renewable")) row["renewable"]=false;
        fields(row,{"id","name","kind","available_from","available_before","term_days","setup_cost_cents","monthly_amount_cents",
            "capacity_milli_CU_per_day","segment_id","product_id","tasks_per_day","renewable"});
        const auto kind=text(row.at("kind"));
        if (kind!="compute_lease" && kind!="recovery_pilot" && kind!="enterprise_contract") invalid("Unsupported business offer kind");
        const auto type=kind=="compute_lease"?core::BusinessOfferKind::compute_lease:
            kind=="recovery_pilot"?core::BusinessOfferKind::recovery_pilot:core::BusinessOfferKind::enterprise_contract;
        result.push_back({text(row.at("id")),text(row.at("name")),type,
            date(row.at("available_from")),date(row.at("available_before")),core::Days{number(row.at("term_days"))},
            core::Cents{number(row.at("setup_cost_cents"))},core::Cents{number(row.at("monthly_amount_cents"))},
            core::MilliCUPerDay{number(row.at("capacity_milli_CU_per_day"))},text(row.at("segment_id"),true),text(row.at("product_id"),true),number(row.at("tasks_per_day"))});
        if (!row.at("renewable").is_boolean()) invalid("Renewable flag must be boolean");
        result.back().renewable=row.at("renewable").get<bool>();
        if (!valid_id(result.back().id)) invalid("Business offer needs a stable content ID");
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_business_offer_catalog(result); return result;
}
std::string canonical_business_offer_catalog(const std::vector<core::BusinessOffer>& offers) {
    Json result=Json::array();
    for (const auto& row:offers) result.push_back({{"id",row.id},{"name",row.name},{"kind",row.kind==core::BusinessOfferKind::compute_lease?"compute_lease":
        row.kind==core::BusinessOfferKind::recovery_pilot?"recovery_pilot":"enterprise_contract"},
        {"available_from",core::iso_date(row.available_from)},{"available_before",core::iso_date(row.available_before)},
        {"term_days",row.term.value()},{"setup_cost_cents",row.setup_cost.value()},{"monthly_amount_cents",row.monthly_amount.value()},
        {"capacity_milli_CU_per_day",row.capacity.value()},{"segment_id",row.segment_id},{"product_id",row.product_id},{"tasks_per_day",row.tasks_per_day},{"renewable",row.renewable}});
    return result.dump();
}
std::vector<RivalDefinition> load_rival_definitions(std::string_view bytes) {
    const auto doc=parse(bytes); array(doc,32); std::vector<RivalDefinition> result; std::set<std::string> ids;
    for (auto row:doc) {
        std::optional<RivalPlanning> planning;
        if (row.contains("planning")) {
            const auto& source=row.at("planning");
            fields(source,{"group","region","leader","slot","reserve_floor_cents","price_floor_cents","price_ceiling_cents","runway_months","publish_results","priorities","research","recipes","services"});
            RivalPlanning value;value.group=text(source.at("group"));value.region=text(source.at("region"));value.leader=text(source.at("leader"));
            const auto slot=number(source.at("slot"));
            if ((value.group!="domestic" && value.group!="international" && value.group!="heavyweight") || slot==0 || slot>(value.group=="heavyweight"?4U:8U)) invalid("Rival planning group/slot is unsupported");
            const std::set<std::string> regions{"US","Canada","Europe","China","India","EastAsiaOceania","LatinAmerica","AfricaMiddleEast"};
            if (!regions.contains(value.region) || (((value.group=="domestic")!=(value.region=="US")) && value.group!="heavyweight")) invalid("Rival planning region/group is inconsistent");
            value.slot=static_cast<std::uint32_t>(slot);value.reserve_floor=core::Cents{number(source.at("reserve_floor_cents"))};
            value.price_floor=core::Cents{number(source.at("price_floor_cents"))};value.price_ceiling=core::Cents{number(source.at("price_ceiling_cents"))};
            if (value.price_floor.value()==0 || value.price_floor>value.price_ceiling || value.price_ceiling.value()>core::market_price_limit) invalid("Rival price policy exceeds actual market bounds");
            const auto runway=number(source.at("runway_months"));if (runway==0 || runway>12 || !source.at("publish_results").is_boolean()) invalid("Rival horizon/publication policy is invalid");
            value.runway_months=static_cast<std::uint32_t>(runway);value.publish_results=source.at("publish_results").get<bool>();
            array(source.at("priorities"),10);if (source.at("priorities").size()!=10) invalid("A rival needs ten bounded action priorities");
            for (std::size_t i=0;i<10;++i) {const auto priority=number(source.at("priorities").at(i));if (priority>1000) invalid("Rival priority exceeds its bound");value.priorities[i]=static_cast<std::uint32_t>(priority);}
            const auto ids=[&](const char* name,std::size_t maximum) {
                const auto& rows=source.at(name);array(rows,maximum);std::vector<std::string> result_ids;std::set<std::string> unique;
                for (const auto& entry:rows) {auto id=text(entry);if (!valid_id(id) || !unique.insert(id).second) invalid("Rival planning needs unique registered IDs");result_ids.push_back(std::move(id));}
                return result_ids;
            };
            value.research=ids("research",64);value.recipes=ids("recipes",2);value.services=ids("services",2);
            if (value.recipes.empty() || value.services.empty()) invalid("A planning profile needs recipes and deployable services");
            planning=std::move(value);row.erase("planning");
        }
        fields(row,{"id","name","seed","cash_cents","monthly_payroll_cents","monthly_lease_cents","capacity_milli_CU_per_day",
            "training_milli_CU_per_day","staff","start_after_days","warning_lead_days","player_deployment_threshold","recipe_id","data_id","training_work_milli_CU"});
        const auto staff=number(row.at("staff")), threshold=number(row.at("player_deployment_threshold"));
        if (staff==0 || staff>1000 || threshold>1024) invalid("Rival staff or trigger count exceeds its bound");
        result.push_back({text(row.at("id")),text(row.at("name")),unsigned_number(row.at("seed")),core::Cents{number(row.at("cash_cents"))},
            core::Cents{number(row.at("monthly_payroll_cents"))},core::Cents{number(row.at("monthly_lease_cents"))},
            core::MilliCUPerDay{number(row.at("capacity_milli_CU_per_day"))},core::MilliCUPerDay{number(row.at("training_milli_CU_per_day"))},
            static_cast<std::uint32_t>(staff),core::Days{number(row.at("start_after_days"))},core::Days{number(row.at("warning_lead_days"))},
            static_cast<std::uint32_t>(threshold),text(row.at("recipe_id")),text(row.at("data_id")),core::MilliCU{number(row.at("training_work_milli_CU"))}});
        const auto& rival=result.back();
        result.back().planning=std::move(planning);
        if (!valid_id(rival.id) || rival.id.size()>64 || !ids.insert(rival.id).second || rival.cash.value()==0 ||
            rival.monthly_payroll.value()==0 || rival.monthly_lease.value()==0 || rival.capacity.value()==0 || rival.training_rate.value()==0 ||
            rival.cash.value()>1'000'000'000'000LL || rival.monthly_payroll.value()>1'000'000'000'000LL ||
            rival.monthly_lease.value()>1'000'000'000'000LL || rival.capacity.value()>1'000'000'000 ||
            rival.training_rate>rival.capacity || rival.start_after.value()>(rival.planning?36524:1096) || rival.warning_lead.value()>365 ||
            (rival.planning && rival.planning->reserve_floor>rival.cash) ||
            (rival.player_deployment_threshold==0 && rival.warning_lead.value()!=0) ||
            ((rival.player_deployment_threshold>0)!=(rival.warning_lead.value()>=90))) invalid("Rival budget, warning or resource terms are invalid");
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;}); return result;
}
std::string canonical_rival_definitions(const std::vector<RivalDefinition>& rivals) {
    Json result=Json::array();
    for (const auto& row:rivals) {
        Json value{{"id",row.id},{"name",row.name},{"seed",row.seed},{"cash_cents",row.cash.value()},
        {"monthly_payroll_cents",row.monthly_payroll.value()},{"monthly_lease_cents",row.monthly_lease.value()},
        {"capacity_milli_CU_per_day",row.capacity.value()},{"training_milli_CU_per_day",row.training_rate.value()},{"staff",row.staff},
        {"start_after_days",row.start_after.value()},{"warning_lead_days",row.warning_lead.value()},{"player_deployment_threshold",row.player_deployment_threshold},
        {"recipe_id",row.recipe_id},{"data_id",row.data_id},{"training_work_milli_CU",row.training_work.value()}};
        if (row.planning) {const auto& plan=*row.planning;value["planning"]={{"group",plan.group},{"region",plan.region},{"leader",plan.leader},{"slot",plan.slot},
            {"reserve_floor_cents",plan.reserve_floor.value()},{"price_floor_cents",plan.price_floor.value()},{"price_ceiling_cents",plan.price_ceiling.value()},
            {"runway_months",plan.runway_months},{"publish_results",plan.publish_results},
            {"priorities",plan.priorities},{"research",plan.research},{"recipes",plan.recipes},{"services",plan.services}};}
        result.push_back(std::move(value));
    }
    return result.dump();
}
CompetitionSetup load_competition_setup(std::string_view bytes) {
    const auto row=parse(bytes);fields(row,{"domestic","international","heavyweights","catchup_cash_cents","catchup_acquired_bp","publication_delay_days","publication_credit_bp","estimate_error"});
    const auto domestic=number(row.at("domestic")),international=number(row.at("international")),heavy=number(row.at("heavyweights")),error=number(row.at("estimate_error"));
    CompetitionSetup setup;
    if (domestic>8 || international>8 || heavy>4 || error>100) invalid("Rival roster/estimate settings exceed tested bounds");
    setup.domestic=static_cast<std::uint32_t>(domestic);setup.international=static_cast<std::uint32_t>(international);setup.heavyweights=static_cast<std::uint32_t>(heavy);
    setup.estimate_error=static_cast<std::uint32_t>(error);setup.catchup_cash=core::Cents{number(row.at("catchup_cash_cents"))};
    setup.catchup_acquired=core::BasisPoints{number(row.at("catchup_acquired_bp"))};
    setup.publication_delay=core::Days{number(row.at("publication_delay_days"))};setup.publication_credit=core::BasisPoints{number(row.at("publication_credit_bp"))};
    if (setup.catchup_cash.value()>100'000'000 || setup.catchup_acquired.value()==0 || setup.catchup_acquired.value()>5000 || setup.publication_delay.value()<7 || setup.publication_delay.value()>365 || setup.publication_credit.value()>2500)
        invalid("Catch-up capital or public research diffusion exceeds its disclosed bound");
    return setup;
}
std::string canonical_competition_setup(const CompetitionSetup& setup) {
    return Json{{"domestic",setup.domestic},{"international",setup.international},{"heavyweights",setup.heavyweights},{"catchup_cash_cents",setup.catchup_cash.value()},{"catchup_acquired_bp",setup.catchup_acquired.value()},
        {"publication_delay_days",setup.publication_delay.value()},{"publication_credit_bp",setup.publication_credit.value()},{"estimate_error",setup.estimate_error}}.dump();
}
bool selected_rival(const RivalDefinition& definition,const CompetitionSetup& setup) {
    if (!definition.planning) return false;
    const auto& row=*definition.planning;
    return row.slot<=(row.group=="domestic"?setup.domestic:row.group=="international"?setup.international:setup.heavyweights);
}
}
