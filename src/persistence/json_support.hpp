#pragma once

#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>

namespace pgp::persistence::detail {
using Json=nlohmann::json;
[[noreturn]] inline void invalid(std::string_view path,std::string detail) {
    throw PersistenceError("invalid_state",std::string(path)+": "+detail);
}
inline void fields(const Json& value,std::string_view path,std::initializer_list<std::string_view> names) {
    if (!value.is_object()) invalid(path,"Expected object");
    if (value.size()!=names.size()) invalid(path,"Missing or unsupported fields");
    for (auto name:names) if (!value.contains(name)) invalid(path,"Missing or unsupported fields");
}
inline std::string text(const Json& value,std::string_view path,std::size_t maximum=4096,bool empty=false) {
    if (!value.is_string()) invalid(path,"Expected UTF-8 string");
    auto result=value.get<std::string>();
    if ((!empty&&result.empty())||result.size()>maximum||result.find('\0')!=std::string::npos)
        invalid(path,"String is empty, oversized or contains NUL");
    return result;
}
inline std::uint64_t u64(const Json& value,std::string_view path,
                         std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max()) {
    if (!value.is_number_integer() || (!value.is_number_unsigned()&&value.get<std::int64_t>()<0))
        invalid(path,"Expected unsigned integer token");
    const auto result=value.get<std::uint64_t>();
    if (result>maximum) invalid(path,"Unsigned field exceeds its bound");
    return result;
}
inline std::int64_t i64(const Json& value,std::string_view path) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned()&&value.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
        invalid(path,"Expected signed 64-bit integer token");
    return value.get<std::int64_t>();
}
inline bool boolean(const Json& value,std::string_view path) {
    if (!value.is_boolean()) invalid(path,"Expected boolean without numeric coercion");
    return value.get<bool>();
}
inline void array(const Json& value,std::string_view path,std::size_t maximum) {
    if (!value.is_array()||value.size()>maximum) invalid(path,"Array exceeds its count bound or has wrong type");
}
inline Json parse(std::string_view bytes,std::size_t maximum_bytes,std::size_t maximum_events=2'000'000) {
    if (bytes.size()>maximum_bytes) throw PersistenceError("oversized","JSON exceeds its configured byte bound");
    std::vector<std::set<std::string>> keys;
    std::size_t tokens=0;
    const auto callback=[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>64 || ++tokens>maximum_events) throw PersistenceError("budget","JSON depth/token budget exceeded");
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
            throw PersistenceError("duplicate_key","Duplicate JSON object key");
        else if (event==Json::parse_event_t::value&&value.is_number_float())
            throw PersistenceError("integer_token","Fractional/exponent authoritative values are forbidden");
        return true;
    };
    try {return Json::parse(bytes.begin(),bytes.end(),callback,true,false);}
    catch (const Json::exception& error) {throw PersistenceError("json",error.what());}
}
}
