#pragma once
#include <pgp/runtime/runtime.hpp>
#include <pgp/runtime/romance.hpp>
namespace pgp::runtime::romance_detail {
struct Person {
    std::string id,gender,orientation,state,city;
    const core::RomanticProfile* romantic=nullptr;
    bool employed=false;
};
using People=std::map<std::string,Person,std::less<>>;
[[nodiscard]] People roster(const WorldState&);
[[nodiscard]] std::string social_key(const WorldState&,std::string_view);
[[nodiscard]] int friendship(const WorldState&,std::string_view,std::string_view);
[[nodiscard]] std::string distance(const Person&,const Person&);
[[nodiscard]] std::string other(const core::RomanceBond&,std::string_view);
[[nodiscard]] std::string blocked(const WorldState&,const People&,const RomanceRequest&);
}
