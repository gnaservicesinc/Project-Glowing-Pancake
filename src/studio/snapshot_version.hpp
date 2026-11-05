#pragma once
#include "snapshot_internal.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <regex>

namespace pgp::studio::detail {
struct SnapshotVersion {
    std::array<std::uint32_t,3> core{};
    std::vector<std::string> pre;
};
inline SnapshotVersion snapshot_version(std::string_view value) {
    static const std::regex pattern{R"((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?)"};
    std::cmatch match;
    if (value.empty() || value.size()>256 || !std::regex_match(value.data(),value.data()+value.size(),match,pattern)) snapshot_refuse("version","Use a valid bounded Semantic Version.");
    SnapshotVersion result;
    for (std::size_t i=0;i<3;++i) {
        const auto part=match[i+1].str();std::uint32_t parsed=0;
        const auto conversion=std::from_chars(part.data(),part.data()+part.size(),parsed);
        if (conversion.ec!=std::errc{} || parsed>INT32_MAX) snapshot_refuse("version","Numeric version components exceed the shared CMake contract.");
        result.core[i]=parsed;
    }
    if (match[4].matched) {
        auto pre=match[4].str();std::size_t start=0;
        do {const auto stop=pre.find('.',start);auto part=pre.substr(start,stop==std::string::npos?stop:stop-start);
            if (part.size()>1 && part.front()=='0' && std::all_of(part.begin(),part.end(),[](char c){return c>='0' && c<='9';})) snapshot_refuse("version","Numeric prerelease identifiers cannot have leading zeroes.");
            result.pre.push_back(std::move(part));if (stop==std::string::npos) break;start=stop+1;
        } while (true);
    }
    return result;
}
inline int snapshot_compare(std::string_view a,std::string_view b) {
    const auto left=snapshot_version(a),right=snapshot_version(b);
    if (left.core!=right.core) return left.core>right.core?1:-1;
    if (left.pre.empty() || right.pre.empty()) return left.pre.empty()==right.pre.empty()?0:left.pre.empty()?1:-1;
    for (std::size_t i=0;i<std::min(left.pre.size(),right.pre.size());++i) {
        const auto& x=left.pre[i];const auto& y=right.pre[i];if (x==y) continue;
        const auto numeric=[](const std::string& p){return std::all_of(p.begin(),p.end(),[](char c){return c>='0' && c<='9';});};
        const bool xn=numeric(x),yn=numeric(y);
        if (xn!=yn) return xn?-1:1;
        if (xn && x.size()!=y.size()) return x.size()>y.size()?1:-1;
        return x>y?1:-1;
    }
    return left.pre.size()==right.pre.size()?0:left.pre.size()>right.pre.size()?1:-1;
}
inline std::string snapshot_game_bump(std::string_view value,bool feature) {
    auto v=snapshot_version(value);
    const auto component=feature?1U:2U;
    if (v.core[component]==INT32_MAX) snapshot_refuse("version","The required game version exceeds its numeric bound.");
    ++v.core[component];if (feature) v.core[2]=0;
    return std::to_string(v.core[0])+"."+std::to_string(v.core[1])+"."+std::to_string(v.core[2])+"-dev.1";
}
inline void snapshot_manifest(const SnapshotJson& j) {
    snapshot_fields(j,{"manifest_format","project_id","phase","components","compatibility"});
    if (j.at("manifest_format")!=1 || !j.at("manifest_format").is_number_integer() || j.at("project_id")!="project-glowing-pancake") snapshot_refuse("manifest","Choose this project's supported version manifest.");
    const auto phase=j.at("phase").get<std::string>();
    if (phase!="planning" && phase!="development" && phase!="release-candidate" && phase!="release") snapshot_refuse("manifest","Unsupported project phase.");
    snapshot_fields(j.at("components"),{"game","editor","content"});
    for (const auto& value:j.at("components")) {if (!value.is_string()) snapshot_refuse("manifest","Component versions must be strings.");(void)snapshot_version(value.get<std::string>());}
    snapshot_fields(j.at("compatibility"),{"content_schema","simulation_rules","save_format"});
    for (const auto& value:j.at("compatibility")) if (!value.is_number_integer() || value.is_boolean() || value<1 || value>INT32_MAX) snapshot_refuse("manifest","Compatibility versions must be positive bounded integer tokens.");
}
}
