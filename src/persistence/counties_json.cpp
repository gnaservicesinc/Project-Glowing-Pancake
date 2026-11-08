#include "counties_json.hpp"
namespace pgp::persistence::detail {
Json counties_json(const core::CountyState& s) {
    Json rows=Json::array();
    for(const auto& row:s.counties) rows.push_back({{"id",row.id},{"population",row.population},{"development",row.development},{"economy",row.economy},{"last_change",row.last_change}});
    return {{"updated_on",s.updated_on.value()},{"stream",s.stream},{"regional_driver",s.regional_driver},{"global_driver",s.global_driver},{"counties",std::move(rows)}};
}
core::CountyState parse_counties(const Json& j) {
    fields(j,"counties",{"updated_on","stream","regional_driver","global_driver","counties"});
    const auto bounded=[&](const Json& value,int limit){const auto n=i64(value,"county integer");if(n < -limit || n>limit) invalid("county","Integer outside its bound");return static_cast<int>(n);};
    core::CountyState s;s.updated_on=core::OrdinalDay{i64(j.at("updated_on"),"updated_on")};s.stream=u64(j.at("stream"),"stream");
    s.regional_driver=bounded(j.at("regional_driver"),100);s.global_driver=bounded(j.at("global_driver"),100);
    array(j.at("counties"),"county records",3300);
    for(const auto& row:j.at("counties")) {
        fields(row,"county",{"id","population","development","economy","last_change"});
        s.counties.push_back({text(row.at("id"),"county id",16),u64(row.at("population"),"population",40000000),
            static_cast<std::uint32_t>(u64(row.at("development"),"development",10000)),static_cast<std::uint32_t>(u64(row.at("economy"),"economy",10000)),bounded(row.at("last_change"),500)});
    }
    return s;
}
}
