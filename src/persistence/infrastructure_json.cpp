#include "infrastructure_json.hpp"
#include <pgp/content/infrastructure_catalog.hpp>
#include <type_traits>
namespace pgp::persistence::detail {
namespace {
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type {};
template<class T,class F> void visit(T& row,F&& f) {
    using V=std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V,core::InfrastructureState>) {
        f("catalog",row.catalog);f("executive_base",row.executive_base);f("closed_through",row.closed_through);f("initial_lease_noticed_on",row.initial_lease_noticed_on);f("initial_lease_ends_before",row.initial_lease_ends_before);
        f("sites",row.sites);f("hardware",row.hardware);f("hires",row.hires);f("appointments",row.appointments);f("notices",row.notices);
    } else if constexpr (std::is_same_v<V,core::OwnedSite>) {
        f("id",row.id);f("name",row.name);f("state_id",row.state_id);f("cooling",row.cooling);f("ordered_on",row.ordered_on);f("built_on",row.built_on);f("grid_on",row.grid_on);f("commissioned_on",row.commissioned_on);f("closed_on",row.closed_on);f("grid_watts",row.grid_watts);
        f("paid_cents",row.paid);f("depreciated_cents",row.depreciated);f("resale_cents",row.resale);f("power",row.power);f("meters",row.meters);
        f("electricity_remainder",row.electricity_remainder);f("water_charge_remainder",row.water_charge_remainder);f("water_remainder",row.water_remainder);f("emissions_remainder",row.emissions_remainder);
        f("last_energy_wh",row.last_energy_wh);f("last_water_ml",row.last_water_ml);f("last_emissions_grams",row.last_emissions_grams);f("last_utility_cost_cents",row.last_utility_cost);
    } else if constexpr (std::is_same_v<V,core::HardwareOrder>) {
        f("id",row.id);f("site_id",row.site_id);f("family_id",row.family_id);f("source_id",row.source_id);f("ordered_on",row.ordered_on);f("delivered_on",row.delivered_on);f("commissioned_on",row.commissioned_on);f("retired_on",row.retired_on);f("paid_cents",row.paid);f("depreciated_cents",row.depreciated);f("resale_cents",row.resale);f("energy_remainder",row.energy_remainder);
    } else if constexpr (std::is_same_v<V,core::SiteOperatorHire>) {
        f("id",row.id);f("site_id",row.site_id);f("people",row.people);f("ordered_on",row.ordered_on);f("starts_on",row.starts_on);
    } else if constexpr (std::is_same_v<V,core::SitePowerLimit>) {f("day",row.day);f("watts",row.watts);}
    else if constexpr (std::is_same_v<V,core::SiteMeterMonth>) {f("month",row.month);f("energy_wh",row.energy_wh);f("water_ml",row.water_ml);f("emissions_grams",row.emissions_grams);f("electricity_cents",row.electricity);f("water_cents",row.water);f("operating_cents",row.operating);}
    else if constexpr (std::is_same_v<V,core::SiteAppointment>) {
        f("id",row.id);f("site_id",row.site_id);f("mode",row.mode);f("booked_on",row.booked_on);f("starts_on",row.starts_on);f("visit_on",row.visit_on);f("ends_before",row.ends_before);f("paid_cents",row.paid);f("cancelled_on",row.cancelled_on);f("inspection",row.inspection);
    } else if constexpr (std::is_same_v<V,core::SiteInspection>) {f("day",row.day);f("built",row.built);f("connected",row.connected);f("delivered_bundles",row.delivered_bundles);f("operators",row.operators);f("effective_milli_CU_per_day",row.effective);f("observed_energy_wh",row.observed_energy_wh);}
    else if constexpr (std::is_same_v<V,core::InfrastructureNotice>) {f("id",row.id);f("kind",row.kind);f("subject",row.subject);f("day",row.day);}
}
template<class T> T decode(const Json& j) {
    if constexpr (std::is_same_v<T,core::InfrastructureCatalog>) return content::load_infrastructure_catalog(j.dump());
    else if constexpr (std::is_same_v<T,std::string>) return text(j,"infrastructure/text",128);
    else if constexpr (std::is_same_v<T,bool>) return boolean(j,"infrastructure/flag");
    else if constexpr (std::is_same_v<T,core::CoolingMode>) {const auto v=text(j,"infrastructure/cooling",16);if (v=="air") return T::air;if (v=="evaporative") return T::evaporative;invalid("infrastructure/cooling","Unsupported cooling mode");}
    else if constexpr (std::is_same_v<T,core::AppointmentMode>) {const auto v=text(j,"infrastructure/appointment_mode",16);if (v=="onsite") return T::onsite;if (v=="remote") return T::remote;if (v=="delegated") return T::delegated;invalid("infrastructure/appointment_mode","Unsupported appointment mode");}
    else if constexpr (std::is_integral_v<T>) {const auto v=u64(j,"infrastructure/count",std::numeric_limits<T>::max());return static_cast<T>(v);}
    else if constexpr (Optional<T>::value) {if (j.is_null()) return {};return decode<typename T::value_type>(j);}
    else if constexpr (requires(T v) {v.value();}) return T{i64(j,"infrastructure/unit")};
    else if constexpr (requires(T v) {v.push_back(typename T::value_type{});}) {
        using Row=typename T::value_type;constexpr std::size_t bound=std::is_same_v<Row,core::OwnedSite>?50:(std::is_same_v<Row,core::HardwareOrder>?200:(std::is_same_v<Row,core::SiteOperatorHire>?400:(std::is_same_v<Row,core::SiteAppointment>?512:(std::is_same_v<Row,core::InfrastructureNotice>?4096:(std::is_same_v<Row,core::SitePowerLimit>?40960:core::retained_campaign_months)))));
        array(j,"infrastructure/records",bound);T rows;for (const auto& row:j) rows.push_back(decode<Row>(row));return rows;
    } else {
        if (!j.is_object()) invalid("infrastructure/record","Expected typed object");T row;std::size_t fields=0;
        visit(row,[&](const char* name,auto& value) {if (!j.contains(name)) invalid("infrastructure/record","Missing field");value=decode<std::remove_cvref_t<decltype(value)>>(j.at(name));++fields;});
        if (fields!=j.size()) invalid("infrastructure/record","Unsupported field");return row;
    }
}
template<class T> Json encode(const T& value) {
    if constexpr (std::is_same_v<T,core::InfrastructureCatalog>) return Json::parse(content::canonical_infrastructure_catalog(value));
    else if constexpr (std::is_same_v<T,std::string> || std::is_integral_v<T>) return value;
    else if constexpr (std::is_same_v<T,core::CoolingMode>) {if (value==T::air) return "air";if (value==T::evaporative) return "evaporative";invalid("infrastructure/cooling","Unknown mode");}
    else if constexpr (std::is_same_v<T,core::AppointmentMode>) {if (value==T::onsite) return "onsite";if (value==T::remote) return "remote";if (value==T::delegated) return "delegated";invalid("infrastructure/appointment","Unknown mode");}
    else if constexpr (Optional<T>::value) return value?encode(*value):Json(nullptr);
    else if constexpr (requires {value.value();}) return value.value();
    else if constexpr (requires {value.begin();value.end();}) {auto j=Json::array();for (const auto& row:value) j.push_back(encode(row));return j;}
    else {auto j=Json::object();visit(value,[&](const char* name,const auto& field) {j[name]=encode(field);});return j;}
}
}
Json infrastructure_json(const core::InfrastructureState& state) {return encode(state);}
core::InfrastructureState parse_infrastructure(const Json& json) {return decode<core::InfrastructureState>(json);}
}
