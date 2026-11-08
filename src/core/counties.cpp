#include <pgp/core/counties.hpp>
#include <pgp/core/random.hpp>
#include <pgp/core/checked_arithmetic.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
bool same_month(OrdinalDay a,OrdinalDay b) {const auto x=from_ordinal(a),y=from_ordinal(b);return x.year==y.year && x.month==y.month;}
}
void validate_county_catalog(const CountyCatalog& c) {
    if(c.counties.empty() || c.counties.size()>3300 || c.source.empty() || c.source.size()>512 || c.assumptions.empty() || c.assumptions.size()>2048 ||
        c.monthly_jitter>100 || c.development_step>20 || c.population_step>10 || c.regional_weight>10 || c.global_weight>5)
        invalid("County catalog bounds are invalid.");
    std::set<std::string> ids;
    for(const auto& row:c.counties) if(row.id.empty() || row.id.size()>16 || !ids.insert(row.id).second || row.state.size()!=5 ||
        row.name.empty() || row.name.size()>128 || row.character.empty() || row.character.size()>32 || row.connections.size()>128 ||
        row.population<1 || row.population>20000000 || row.development>10000 || row.latitude < -90000000 || row.latitude>90000000 || row.longitude < -180000000 || row.longitude>180000000)
        invalid("County identity, initial population or map coordinates are invalid.");
}
CountyState initialize_counties(const CountyCatalog& c,OrdinalDay day,std::uint64_t seed) {
    validate_county_catalog(c);CountyState result;result.updated_on=day;SplitMix64 rng{seed ^ UINT64_C(0x434f554e54595631)};result.stream=rng.next();
    for(const auto& row:c.counties) result.counties.push_back({row.id,row.population,row.development,5000,0});
    return result;
}
void advance_counties(CountyState& s,const CountyCatalog& c,OrdinalDay day,int regional,int global) {
    if(day<s.updated_on) invalid("County time cannot run backwards.");
    if(same_month(s.updated_on,day)) return;
    if(s.counties.size()!=c.counties.size()) invalid("County records do not match their definitions.");
    SplitMix64 rng{s.stream};
    s.regional_driver=std::clamp(regional,-100,100);s.global_driver=std::clamp(global,-100,100);
    for(std::size_t i=0;i<s.counties.size();++i) {
        auto& row=s.counties[i];
        const auto jitter=static_cast<int>(rng.bounded(c.monthly_jitter*2+1))-static_cast<int>(c.monthly_jitter);
        const auto mean_reversion=(5000-static_cast<int>(row.economy))/80;
        const auto change=jitter+mean_reversion+s.regional_driver*static_cast<int>(c.regional_weight)/10+s.global_driver*static_cast<int>(c.global_weight)/10;
        const auto next=std::clamp(static_cast<int>(row.economy)+change,1000,9000);
        row.last_change=next-static_cast<int>(row.economy);row.economy=static_cast<std::uint32_t>(next);
        const auto direction=row.economy>5200?1:row.economy<4800?-1:0;
        row.development=static_cast<std::uint32_t>(std::clamp(static_cast<int>(row.development)+direction*static_cast<int>(c.development_step),500,9500));
        const auto growth=direction*static_cast<std::int64_t>(c.population_step);
        const auto population=static_cast<std::int64_t>(row.population)+static_cast<std::int64_t>(row.population)*growth/10000;
        row.population=static_cast<std::uint64_t>(std::clamp(population,std::max(INT64_C(1),static_cast<std::int64_t>(c.counties[i].population/2)),static_cast<std::int64_t>(c.counties[i].population*2)));
    }
    s.stream=rng.state();s.updated_on=day;
}
void validate_counties(const CountyState& s,const CountyCatalog& c,OrdinalDay now) {
    if(s.updated_on>now || !same_month(s.updated_on,now) || s.counties.size()!=c.counties.size() ||
        s.regional_driver < -100 || s.regional_driver>100 || s.global_driver < -100 || s.global_driver>100)
        invalid("County state is outside its prepared month or driver bounds.");
    for(std::size_t i=0;i<s.counties.size();++i) {
        const auto& row=s.counties[i];const auto& definition=c.counties[i];
        if(row.id!=definition.id || row.population<std::max(UINT64_C(1),definition.population/2) || row.population>definition.population*2 ||
            row.economy<1000 || row.economy>9000 || row.development<500 || row.development>9500 || row.last_change < -500 || row.last_change>500)
            invalid("County trend or population is outside its bounded game model.");
    }
}
}
