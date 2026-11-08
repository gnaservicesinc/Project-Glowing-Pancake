#include <pgp/content/county_catalog.hpp>
#include "people_codec.hpp"
#include <set>
namespace pgp::content {
core::CountyCatalog load_county_catalog(std::string_view bytes) {
    if(bytes.size()>2000000) throw std::invalid_argument("County catalog exceeds its byte limit");
    const auto j=nlohmann::json::parse(bytes);
    people_codec::fields(j,{"source","assumptions","monthly_jitter","development_step","population_step","regional_weight","global_weight","counties"});
    core::CountyCatalog c;c.source=j.at("source").get<std::string>();c.assumptions=j.at("assumptions").get<std::string>();
    c.monthly_jitter=people_codec::count(j.at("monthly_jitter"));c.development_step=people_codec::count(j.at("development_step"));c.population_step=people_codec::count(j.at("population_step"));
    c.regional_weight=people_codec::count(j.at("regional_weight"));c.global_weight=people_codec::count(j.at("global_weight"));
    if(!j.at("counties").is_array() || j.at("counties").size()>3300) throw std::invalid_argument("County count out of range");
    for(const auto& row:j.at("counties")) {
        people_codec::fields(row,{"id","state","name","character","connections","population","development","latitude","longitude"});
        const auto pop=people_codec::integer(row.at("population")),dev=people_codec::integer(row.at("development")),lat=people_codec::integer(row.at("latitude")),lon=people_codec::integer(row.at("longitude"));
        if(pop<1 || pop>20000000 || dev<500 || dev>9500 || lat < -90000000 || lat>90000000 || lon < -180000000 || lon>180000000) throw std::invalid_argument("County numeric field out of range");
        c.counties.push_back({row.at("id"),row.at("state"),row.at("name"),row.at("character"),row.at("connections"),static_cast<std::uint64_t>(pop),static_cast<std::uint32_t>(dev),static_cast<int>(lat),static_cast<int>(lon)});
    }
    core::validate_county_catalog(c);return c;
}
std::string canonical_county_catalog(const core::CountyCatalog& c) {
    nlohmann::json counties=nlohmann::json::array();
    for(const auto& row:c.counties) counties.push_back({{"id",row.id},{"state",row.state},{"name",row.name},{"character",row.character},{"connections",row.connections},{"population",row.population},{"development",row.development},{"latitude",row.latitude},{"longitude",row.longitude}});
    return nlohmann::json{{"source",c.source},{"assumptions",c.assumptions},{"monthly_jitter",c.monthly_jitter},{"development_step",c.development_step},{"population_step",c.population_step},{"regional_weight",c.regional_weight},{"global_weight",c.global_weight},{"counties",std::move(counties)}}.dump();
}
}
