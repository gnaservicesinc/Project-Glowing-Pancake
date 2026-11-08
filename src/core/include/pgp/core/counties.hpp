#pragma once
#include <pgp/core/calendar.hpp>
#include <string>
#include <vector>
namespace pgp::core {
struct CountyDefinition {
    std::string id,state,name,character,connections;
    std::uint64_t population=0;
    std::uint32_t development=5000;
    int latitude=0,longitude=0; // millionths of a degree
    auto operator<=>(const CountyDefinition&) const = default;
};
struct CountyCatalog {
    std::string source,assumptions;
    std::vector<CountyDefinition> counties;
    std::uint32_t monthly_jitter=35,development_step=3,population_step=2,regional_weight=3,global_weight=1;
    auto operator<=>(const CountyCatalog&) const = default;
};
struct CountyCondition {
    std::string id;
    std::uint64_t population=0;
    std::uint32_t development=0,economy=5000;
    int last_change=0;
    auto operator<=>(const CountyCondition&) const = default;
};
struct CountyState {
    OrdinalDay updated_on{0};
    std::uint64_t stream=0;
    std::vector<CountyCondition> counties;
    int regional_driver=0,global_driver=0;
    auto operator<=>(const CountyState&) const = default;
};
void validate_county_catalog(const CountyCatalog&);
CountyState initialize_counties(const CountyCatalog&,OrdinalDay,std::uint64_t seed);
void advance_counties(CountyState&,const CountyCatalog&,OrdinalDay,int regional_driver,int global_driver);
void validate_counties(const CountyState&,const CountyCatalog&,OrdinalDay);
}
