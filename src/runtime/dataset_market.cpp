#include "competition_internal.hpp"
#include <pgp/runtime/runtime.hpp>
#include <algorithm>
#include <set>
#include <tuple>

namespace pgp::runtime {
namespace {
using namespace core;
constexpr std::size_t sale_limit = 128;
constexpr std::string_view license_prefix = "trade.dataset.";

const SimulationState& laboratory(const WorldState& world, std::string_view id) {
    const auto* company = competition_detail::company(world, id);
    if (!company || !company->lab || !company->accounts || !company->capacity)
        throw std::invalid_argument("Choose a company with an operating data lab.");
    return *company;
}

Cents price(const DataOffer& data) {
    // One cent per effective record, quoted once for a nonexclusive training license.
    return Cents{std::max<std::uint64_t>(1, mul_div_floor(data.unique_units,
        static_cast<std::uint64_t>(data.quality.value()), 10000).quotient)};
}

DataOffer licensed_copy(const WorldState& world, const DatasetSale& sale, std::size_t index) {
    const auto& seller = laboratory(world, sale.seller);
    if (!owns_dataset_ip(*seller.lab, sale.dataset))
        throw std::invalid_argument("Only the company that owns the dataset IP can license it.");
    const auto available = data_available_on(*seller.lab,*seller.capacity,sale.dataset);
    if (!available || *available > sale.day) throw std::invalid_argument("The dataset was unavailable on the license date.");
    auto result = lab_data(*seller.lab, sale.dataset);
    result.price = price(result);
    result.id = std::string{license_prefix} + std::to_string(index + 1);
    result.source_path = "licensed";
    result.open_weight_release = false;
    result.parent_dataset_id.clear();
    result.processing_step.clear();
    result.rights = "Nonexclusive training license. The seller retains the dataset IP.";
    result.supplier_or_publisher = sale.seller;
    return result;
}
}
core::Cents dataset_license_quote(const WorldState& world, const CompiledProgram& program, const core::LicenseCompanyDataset& deal) {
    if (!world.competition || !world.competition->setup || world.competition->dataset_sales.size() >= sale_limit || deal.seller == deal.buyer)
        throw std::invalid_argument("Choose two trading companies with room for a new agreement.");
    const auto& seller = laboratory(world,deal.seller);
    const auto& buyer = laboratory(world,deal.buyer);
    if (seller.date != buyer.date || seller.phase == DayPhase::closed || seller.phase == DayPhase::finished ||
        buyer.phase == DayPhase::closed || buyer.phase == DayPhase::finished || buyer.lab->catalog.data.size() >= lab_catalog_limit)
        throw std::invalid_argument("Both companies must be trading on the same day.");
    const auto& sales = world.competition->dataset_sales;
    if (std::ranges::any_of(sales,[&](const auto& row) {
        return row.seller == deal.seller && row.buyer == deal.buyer && row.dataset == deal.dataset;
    })) throw std::invalid_argument("This company already licensed that dataset.");
    const auto copy = licensed_copy(world,{deal.seller,deal.buyer,deal.dataset,seller.date},sales.size());
    auto required = copy.price;
    if (deal.buyer != world.company_id) {
        const auto& rival = rival_definition(world,program,deal.buyer);
        if (!rival.planning || !copy.commercial_use || !copy.coverage[0].value())
            throw std::invalid_argument("The buyer needs commercial training rights and document coverage.");
        required = checked_unit_add(required, rival.planning->reserve_floor);
        for (const auto& bill : buyer.accounts->recurring)
            if (bill.starts_on <= buyer.date && (!bill.ends_before || *bill.ends_before > buyer.date))
                required = checked_unit_add(required, Cents{mul_div_floor(static_cast<std::uint64_t>(bill.monthly_amount.value()),
                    rival.planning->runway_months, 1).quotient});
    }
    if (discretionary_cash(buyer) < required) throw std::invalid_argument("The buyer cannot cover the license and its cash reserve.");
    return copy.price;
}
void license_company_dataset(WorldState& world, const CompiledProgram& program, const core::LicenseCompanyDataset& deal) {
    const auto fee = dataset_license_quote(world,program,deal);
    auto& seller = *competition_detail::company(world,deal.seller);
    auto& buyer = *competition_detail::company(world,deal.buyer);
    auto& sales = world.competition->dataset_sales;
    DatasetSale sale{deal.seller,deal.buyer,deal.dataset,buyer.date};
    const auto copy = licensed_copy(world,sale,sales.size());
    buyer.lab->catalog.data.push_back(copy);
    std::ranges::sort(buyer.lab->catalog.data,{},&DataOffer::id);
    buy_data(*buyer.lab,*buyer.accounts,buyer.date,copy.id,Cents{checked_subtract(buyer.cash.value(),discretionary_cash(buyer).value())});
    record_cash(*seller.accounts,seller.date,fee,Account::revenue,"Dataset license","dataset-sale/"+copy.id);
    buyer.cash = accounts_report(*buyer.accounts).cash;
    seller.cash = accounts_report(*seller.accounts).cash;
    sales.push_back(std::move(sale));
}
void restore_licensed_catalog(core::LabCatalog& catalog, const WorldState& world, std::string_view owner) {
    if (!world.competition || world.competition->dataset_sales.empty()) return;
    std::erase_if(catalog.data,[](const auto& offer) { return offer.id.starts_with(license_prefix); });
    const auto& sales = world.competition->dataset_sales;
    for (std::size_t i=0; i<sales.size(); ++i)
        if (sales[i].buyer == owner) catalog.data.push_back(licensed_copy(world,sales[i],i));
    std::ranges::sort(catalog.data,{},&DataOffer::id);
}
void validate_dataset_sales(const WorldState& world) {
    const auto& sales = world.competition->dataset_sales;
    if (sales.size() > sale_limit || (!sales.empty() && !world.competition->setup))
        throw std::invalid_argument("Invalid dataset market history.");
    std::set<std::tuple<std::string,std::string,std::string>> unique;
    std::set<std::pair<std::string,std::string>> receipts;
    for (std::size_t i=0; i<sales.size(); ++i) {
        const auto& sale = sales[i];
        const auto& buyer = laboratory(world,sale.buyer);
        const auto& seller = laboratory(world,sale.seller);
        const auto copy = licensed_copy(world,sale,i);
        if (sale.buyer == sale.seller || !unique.emplace(sale.seller,sale.buyer,sale.dataset).second ||
            data_available_on(*buyer.lab,*buyer.capacity,copy.id) != sale.day || lab_data(*buyer.lab,copy.id) != copy)
            throw std::invalid_argument("Dataset license does not match its delivered corpus.");
        const auto key = "dataset-sale/"+copy.id;
        const auto posting = std::ranges::find(seller.accounts->postings,key,&Posting::key);
        if (posting == seller.accounts->postings.end() || posting->amount != copy.price || posting->day != sale.day ||
            posting->credit != Account::revenue || posting->debit != Account::cash)
            throw std::invalid_argument("Dataset license lost its seller payment.");
        receipts.emplace(sale.seller,key);
    }
    const auto check_receipts = [&](const SimulationState& state) {
        if (!state.accounts) return;
        for (const auto& posting : state.accounts->postings)
            if (posting.key.starts_with("dataset-sale/") && !receipts.erase({state.accounts->owner,posting.key}))
                throw std::invalid_argument("Dataset revenue has no corresponding sale.");
    };
    check_receipts(world.simulation);
    for (const auto& rival : world.competition->companies) check_receipts(rival.simulation);
}
}
