#pragma once

#include "json_support.hpp"
#include <pgp/core/lab.hpp>

namespace pgp::persistence::detail {
inline Json lab_profile_json(const core::LabProfile& profile) {
    auto result=Json::array();
    for (std::size_t i=0;i<core::stored_lab_domains(profile);++i) result.push_back(profile[i].value());
    return result;
}
inline core::LabProfile parse_lab_profile(const Json& value) {
    array(value,"lab/profile",core::lab_domain_count);
    if (value.size()<core::introductory_lab_domains) invalid("lab/profile","Missing introductory task domains");
    auto result=core::empty_lab_profile;
    for (std::size_t i=0;i<value.size();++i) result[i]=core::BasisPoints{i64(value[i],"lab/profile")};
    return result;
}
} // namespace pgp::persistence::detail
