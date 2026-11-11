#include "headquarters_json.hpp"
#include <limits>
#include <type_traits>

namespace pgp::persistence::detail {
namespace {
template<class T,class F> void visit(T& r,F&& f) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::HQRoom>) {f("x",r.x);f("y",r.y);f("width",r.width);f("depth",r.depth);f("passage",r.passage,true);}
    else if constexpr(std::is_same_v<V,core::HQPremises>) {
        f("id",r.id);f("kind",r.kind);f("market",r.market);f("plan",r.plan);f("lessor",r.lessor);f("address",r.address);f("width",r.width);f("depth",r.depth);f("rooms",r.rooms);
        f("rent",r.rent);f("deposit",r.deposit);f("entered_on",r.entered_on);f("left_on",r.left_on);f("construction",r.construction);
    } else if constexpr(std::is_same_v<V,core::HQEquipment>) {
        f("id",r.id);f("offer",r.offer);f("person",r.person);f("ordered_on",r.ordered_on);f("delivered_on",r.delivered_on);f("paid",r.paid);f("inherited",r.inherited);f("powered",r.powered);f("rotated",r.rotated);f("premises",r.premises);f("mount",r.mount);f("x",r.x);f("y",r.y);
    } else if constexpr(std::is_same_v<V,core::HQLand>) {f("id",r.id);f("market",r.market);f("plan",r.plan);f("purchased_on",r.purchased_on);f("paid",r.paid);}
    else if constexpr(std::is_same_v<V,core::HQConstruction>) {
        f("id",r.id);f("land",r.land);f("contractor",r.contractor);f("width",r.width);f("depth",r.depth);f("rooms",r.rooms);f("signed_on",r.signed_on);f("promised_on",r.promised_on);f("finish_on",r.finish_on);
        f("quote",r.quote);f("prepaid",r.prepaid);f("actual",r.actual);f("delay_announced",r.delay_announced);f("completed_on",r.completed_on);
    } else if constexpr(std::is_same_v<V,core::HQNotice>) {f("day",r.day);f("text",r.text);}
    else if constexpr(std::is_same_v<V,core::HeadquartersState>) {f("stream",r.stream);f("energy_wh",r.energy_wh);f("electricity_remainder",r.electricity_remainder);f("electricity_cost",r.electricity_cost);f("metered_through",r.metered_through);f("premises",r.premises);f("items",r.items);f("land",r.land);f("construction",r.construction);f("notices",r.notices);f("home_family",r.home_family);f("home_market",r.home_market);}
}
[[noreturn]] void bad() {throw PersistenceError("headquarters","Invalid headquarters ownership, floor plan or contract record.");}
template<class T> T read(const Json& j) {
    if constexpr(std::is_same_v<T,std::string>) {if(!j.is_string()) bad();auto v=j.get<std::string>();if(v.size()>1024 || v.find('\0')!=v.npos) bad();return v;}
    else if constexpr(std::is_same_v<T,bool>) {if(!j.is_boolean()) bad();return j.get<bool>();}
    else if constexpr(std::is_integral_v<T>) {
        if constexpr(std::is_unsigned_v<T>) return static_cast<T>(u64(j,"headquarters integer",std::numeric_limits<T>::max()));
        else {const auto v=i64(j,"headquarters integer");if(v<std::numeric_limits<T>::min() || v>std::numeric_limits<T>::max()) bad();return static_cast<T>(v);}
    } else if constexpr(requires(T v){v.has_value();}) {if(j.is_null()) return {};return T{read<typename T::value_type>(j)};}
    else if constexpr(requires(T v){v.value();}) {return T{read<std::int64_t>(j)};}
    else if constexpr(requires(T v){v.push_back(typename T::value_type{});}) {if(!j.is_array() || j.size()>128) bad();T values;for(const auto& row:j) values.push_back(read<typename T::value_type>(row));return values;}
    else {if(!j.is_object()) bad();T r;std::size_t count=0;visit(r,[&](const char* name,auto& value,bool optional=false){if(!j.contains(name)) {if(optional)return;bad();}try {value=read<std::remove_cvref_t<decltype(value)>>(j.at(name));} catch(const PersistenceError& e) {throw PersistenceError("headquarters",std::string{name}+": "+e.what());}++count;});if(j.size()!=count) bad();return r;}
}
template<class T> Json write(const T& v) {
    if constexpr(std::is_same_v<T,std::string> || std::is_integral_v<T>) return v;
    else if constexpr(requires{v.has_value();}) return v?write(*v):Json(nullptr);
    else if constexpr(requires{v.value();}) return v.value();
    else if constexpr(requires{v.begin();v.end();}) {auto j=Json::array();for(const auto& r:v) j.push_back(write(r));return j;}
    else {auto j=Json::object();visit(v,[&](const char* name,const auto& value,bool optional=false){
        if constexpr(std::is_same_v<std::remove_cvref_t<decltype(value)>,bool>) {if(optional && !value)return;}
        j[name]=write(value);
    });return j;}
}
}
Json headquarters_json(const core::HeadquartersState& h) {return write(h);}
core::HeadquartersState parse_headquarters(const Json& j) {return read<core::HeadquartersState>(j);}
}
