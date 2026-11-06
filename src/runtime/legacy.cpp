#include <pgp/runtime/legacy.hpp>
#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/mergers.hpp>
#include <algorithm>

namespace pgp::runtime {
namespace {
using namespace core;
const SimulationState& company(const WorldState& world,std::string_view id) {
    if(id==world.company_id) return world.simulation;
    if(world.competition) for(const auto& row:world.competition->companies) if(row.definition_id==id && row.simulation.accounts) return row.simulation;
    throw std::invalid_argument("The founder legacy needs an entered institution.");
}
BasisPoints holding(const SimulationState& s,std::string_view holder) {
    for(const auto& row:s.accounts->holdings) if(row.holder==holder) return row.shares;
    return BasisPoints{0};
}
Cents equity_book(const WorldState& world,std::string_view id) {
    if(id==world.company_id) return Cents{std::max<std::int64_t>(0,owned_group_report(world).net_assets.value())};
    const auto r=accounts_report(*company(world,id).accounts);
    const auto assets=checked_unit_subtract(checked_unit_add(checked_unit_add(r.cash,r.receivables),r.assets),r.accumulated_depreciation);
    return Cents{std::max<std::int64_t>(0,checked_unit_subtract(checked_unit_subtract(assets,r.payables),r.debt).value())};
}
}
FounderLegacy founder_legacy(const WorldState& world,std::string_view institution) {
    const auto& s=company(world,institution);
    if(!s.management) throw std::invalid_argument("The institution has no founder identity.");
    FounderLegacy result;result.institution=institution;
    result.holder=s.succession?s.succession->founder_holder:s.management->catalog.scoring.founder;
    if(s.succession) {result.cash=s.succession->personal_cash;result.liabilities=succession_catalog(s).personal_liabilities;}
    const auto own=holding(s,result.holder);
    if(own.value()>0) result.positions.push_back({std::string(institution),result.holder,static_cast<std::uint64_t>(own.value()),10000});
    if(world.competition) for(const auto& acquisition:world.competition->acquisitions) if(acquisition.completed_on && acquisition.target==institution) {
        for(const auto& seller:acquisition.settlements) if(seller.holder==result.holder) {
            result.cash=checked_unit_add(result.cash,seller.cash);
            if(seller.shares.value()>0) {
                const auto holder="merger.sellers/"+acquisition.target;
                const auto pool=holding(world.simulation,holder);
                result.positions.push_back({world.company_id,holder,
                    static_cast<std::uint64_t>(pool.value())*static_cast<std::uint64_t>(seller.shares.value()),
                    10000U*static_cast<std::uint64_t>(acquisition.shares.value())});
            }
        }
    }
    for(const auto& position:result.positions) {
        const auto value=equity_book(world,position.company);
        const auto share=mul_div_floor(static_cast<std::uint64_t>(value.value()),position.numerator,position.denominator).quotient;
        result.book_equity=checked_unit_add(result.book_equity,Cents{share});
    }
    result.net_book_wealth=checked_unit_subtract(checked_unit_add(result.book_equity,result.cash),result.liabilities);
    return result;
}
}
