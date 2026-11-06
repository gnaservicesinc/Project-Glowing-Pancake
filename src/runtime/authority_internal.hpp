#pragma once
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/society.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::runtime::authority_detail {
using namespace core;
[[noreturn]] inline void invalid(std::string message) {throw std::invalid_argument(std::move(message));}
template<class Rows> const auto* named(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});return it==rows.end()?nullptr:&*it;
}
template<class Rows> const auto& numbered(const Rows& rows,std::uint64_t id) {
    if(id==0 || id>rows.size() || rows[static_cast<std::size_t>(id-1)].id!=id) invalid("Choose an existing institutional record.");
    return rows[static_cast<std::size_t>(id-1)];
}
inline std::uint32_t ratio(std::uint64_t part,std::uint64_t all) {
    return all?static_cast<std::uint32_t>(std::min<std::uint64_t>(10000,mul_div_floor(part,10000,all).quotient)):0;
}
[[nodiscard]] const SocietyMonth* known_month(const WorldState&,OrdinalDay);
[[nodiscard]] const DeploymentRecord* deployed(const SimulationState&,std::string_view,OrdinalDay);
[[nodiscard]] std::uint64_t public_work(const WorldState&,std::string_view region,std::string_view service,OrdinalDay first,OrdinalDay ends);
[[nodiscard]] std::uint64_t admin_work(const WorldState&,std::string_view project,OrdinalDay first,OrdinalDay ends);
[[nodiscard]] std::optional<OrdinalDay> closing_day(const WorldState&,const AuthorityMandate&);
[[nodiscard]] bool operative(const WorldState&,const AuthorityMandate&,OrdinalDay);
[[nodiscard]] std::uint32_t resistance(const WorldState&,std::size_t region,OrdinalDay);
[[nodiscard]] InstitutionalReview review(const WorldState&,const AuthorityMandate&,OrdinalDay);
[[nodiscard]] std::optional<OrdinalDay> promise_met(const WorldState&,const PublicPromise&,OrdinalDay through);
[[nodiscard]] std::optional<OrdinalDay> publication_day(const WorldState&,std::string_view,OrdinalDay);
[[nodiscard]] OrdinalDay case_due(const PublicCase&,const AuthorityCatalog&);
[[nodiscard]] std::array<std::uint32_t,society_regions> next_resistance(const WorldState&,OrdinalDay);
[[nodiscard]] AuthorityMandate application(const WorldState&,const RequestInstitutionalMandate&,OrdinalDay,std::uint64_t,
    std::uint64_t saved_deployment=0,bool historic=false);
void pay(WorldState&,Cents,std::string key,std::string cause);
void reserve_review(WorldState&,std::string id,std::string source,OrdinalDay ends,Cents monthly,MilliCUPerDay,std::uint32_t,bool hire);
bool release_mandate_staff(WorldState&,AuthorityMandate&);
[[nodiscard]] std::string operation_id(std::uint64_t);
[[nodiscard]] std::string hire_id(std::uint64_t);
[[nodiscard]] std::string departure_id(std::uint64_t);
void replay_prepare(WorldState&,std::optional<OrdinalDay> settled);
}
