#include <pgp/content/market_setup.hpp>
#include <pgp/content/collection.hpp>
#include <pgp/content/deployment_risk.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::content {
namespace {
void shape(const MarketSetup& setup, core::OrdinalDay starts_on) {
    if (setup.offers.size() > core::market_product_limit || setup.segments.size() > core::market_segment_limit ||
        setup.contracts.size() > core::market_contract_limit)
        throw std::invalid_argument("Market setup exceeds its catalog bounds");
    for (const auto& offer : setup.offers) {
        const auto& row = offer.serving;
        if (row.id != offer.product.capacity_project_id || row.source_id != "lease.initial" ||
            row.kind != core::ProjectKind::serving || row.status != core::ProjectStatus::active ||
            row.starts_on != starts_on || row.stopped_on || row.total_work.value() != 0 ||
            row.completed_work.value() != 0 || row.useful_tasks != 0 || row.cause != "Market service: " + offer.product.id ||
            core::required_inference(row.tasks_per_day, row.inference_milli_CU_per_task).value() != row.requested.value())
            throw std::invalid_argument("Market setup contains incompatible serving state");
    }
    for (const auto& contract : setup.contracts)
        if (contract.terms.accepted_on != starts_on || contract.terms.status != core::ContractStatus::pending ||
            contract.terms.started_on || contract.terms.stopped_on)
            throw std::invalid_argument("Market setup contains previously consumed contract state");
    for (const auto& segment : setup.segments)
        if (segment.preferences.size() > core::market_product_limit)
            throw std::invalid_argument("Market preference count exceeds its catalog bound");
}
}
std::vector<core::ServiceDefinition> service_definitions(const MarketSetup& setup) {
    auto result=setup.offers;
    for (auto& offer:result) {
        offer.preferences.clear();
        for (const auto& segment:setup.segments) for (const auto& pref:segment.preferences)
            if (pref.product_id==offer.product.id) offer.preferences.push_back({segment.id,pref});
    }
    core::validate_service_catalog(result);
    return result;
}
void apply_market_setup(const MarketSetup& setup, core::MarketState& market,
                        core::AccountsState& accounts, core::CapacityState& capacity,
                        core::OrdinalDay starts_on) {
    shape(setup, starts_on);
    auto next_market = market;
    auto next_accounts = accounts;
    auto next_capacity = capacity;
    for (const auto& offer : setup.offers) {
        core::reserve_project(next_capacity, starts_on, offer.serving);
        if (offer.serving.monthly_support_cost.value() > 0)
            core::add_recurring(next_accounts, {"support/" + offer.serving.id, offer.serving.monthly_support_cost,
                starts_on, std::nullopt, "Serving support: " + offer.serving.id});
        core::add_market_product(next_market, next_capacity, offer.product);
    }
    for (const auto& segment : setup.segments) core::add_market_segment(next_market, segment);
    for (const auto& contract : setup.contracts) {
        core::accept_contract(next_market, next_capacity, starts_on, contract.terms);
        if (contract.start_immediately) core::start_contract(next_market, next_capacity, starts_on, contract.terms.id);
    }
    core::validate_market_links(next_market, next_accounts, next_capacity);
    market = std::move(next_market); accounts = std::move(next_accounts); capacity = std::move(next_capacity);
}
std::string canonical_market_setup_bytes(const MarketSetup& setup, core::OrdinalDay starts_on) {
    shape(setup, starts_on);
    using Json = nlohmann::json;
    Json offers = Json::array(), segments = Json::array(), contracts = Json::array();
    std::vector<const MarketOfferDefinition*> ordered_offers;
    for (const auto& offer : setup.offers) ordered_offers.push_back(&offer);
    std::sort(ordered_offers.begin(), ordered_offers.end(), [](auto a, auto b) { return a->product.id < b->product.id; });
    for (const auto* offer : ordered_offers) {
        const auto& row = offer->serving;
        offers.push_back({{"id",offer->product.id},{"capacity_project_id",row.id},
            {"price_cents_per_task",offer->product.price_per_task.value()},{"available",offer->product.available},
            {"tasks_per_day",row.tasks_per_day},{"inference_milli_CU_per_task",row.inference_milli_CU_per_task.value()},
            {"staff",row.staff},{"minimum_memory_mib",row.minimum_memory_mib},{"interconnect",row.interconnect},
            {"monthly_support_cost_cents",row.monthly_support_cost.value()}});
        if (!offer->required_recipe.empty()) offers.back()["required_recipe"]=offer->required_recipe;
        if (!offer->workload_domain.empty()) offers.back()["workload_domain"]=offer->workload_domain;
        if (offer->incident_risk) offers.back()["incident_risk"]=Json::parse(canonical_deployment_risk_rules(*offer->incident_risk));
        if (offer->product.channel != core::ProductChannel::legacy) {
            offers.back()["channel"] = core::channel_key(offer->product.channel);
            offers.back()["monthly_migration_bp"] = offer->product.monthly_migration.value();
        }
        if (!offer->required_research.empty()) offers.back()["required_research"] = offer->required_research;
        if (std::any_of(offer->required.begin(),offer->required.end(),[](auto value){return value.value()>0;})) {
            auto& encoded=offers.back();
            const auto profile=[](const core::LabProfile& values) {
                auto result=Json::array();
                for (std::size_t i=0;i<core::stored_lab_domains(values);++i) result.push_back(values[i].value());
                return result;
            };
            encoded["required_bp"]=profile(offer->required);
            encoded["review_credit_bp"]=profile(offer->review_credit);
            encoded["review_staff"]=offer->review_staff;
            encoded["review_monthly_cost_cents"]=offer->review_monthly_cost.value();
            encoded["review_tasks_per_day"]=offer->review_tasks_per_day;
        }
    }
    std::vector<const core::MarketSegment*> ordered_segments;
    for (const auto& segment : setup.segments) ordered_segments.push_back(&segment);
    std::sort(ordered_segments.begin(), ordered_segments.end(), [](auto a, auto b) { return a->id < b->id; });
    for (const auto* row : ordered_segments) {
        auto preferences = row->preferences;
        std::sort(preferences.begin(), preferences.end(), [](const auto& a, const auto& b) { return a.product_id < b.product_id; });
        Json prefs = Json::array();
        for (const auto& preference : preferences) prefs.push_back({{"product_id",preference.product_id},
            {"utility",preference.utility},{"willingness_to_pay_cents",preference.willingness_to_pay.value()},
            {"switching_cost",preference.switching_cost},{"eligible",preference.eligible}});
        segments.push_back({{"id",row->id},{"daily_tasks",row->daily_tasks},{"daily_budget_cents",row->daily_budget.value()},
            {"outside_utility",row->outside_utility},{"reservation_threshold",row->reservation_threshold},
            {"maximum_monthly_migration_basis_points",row->maximum_monthly_migration.value()},
            {"preferences",std::move(prefs)}});
        if (!row->region_id.empty()) {
            auto& encoded = segments.back();
            encoded["region"] = row->region_id;
            encoded["customer_compute_milli_CU_per_day"] = row->customer_compute.value();
            encoded["customer_memory_mib"] = row->customer_memory_mib;
            encoded["customer_interconnect"] = row->customer_interconnect;
            encoded["requires_local"] = row->requires_local;
        }
    }
    std::vector<const StartingContractDefinition*> ordered_contracts;
    for (const auto& contract : setup.contracts) ordered_contracts.push_back(&contract);
    std::sort(ordered_contracts.begin(), ordered_contracts.end(), [](auto a, auto b) { return a->terms.id < b->terms.id; });
    for (const auto* contract : ordered_contracts) {
        const auto& row = contract->terms;
        contracts.push_back({{"id",row.id},{"segment_id",row.segment_id},{"product_id",row.product_id},
            {"tasks_per_day",row.tasks_per_day},{"monthly_fee_cents",row.monthly_fee.value()},
            {"usage_price_cents",row.usage_price.value()},{"service_credit_basis_points",row.service_credit_basis_points.value()},
            {"missing_task_penalty_cents",row.missing_task_penalty.value()},{"missed_start_penalty_cents",row.missed_start_penalty.value()},
            {"cancellation_penalty_cents",row.cancellation_penalty.value()},
            {"start_by",core::iso_date(row.start_by)},{"ends_before",core::iso_date(row.ends_before)},
            {"cause",row.cause},{"start_immediately",contract->start_immediately}});
    }
    return Json{{"offers",std::move(offers)},{"segments",std::move(segments)},{"contracts",std::move(contracts)}}.dump();
}

std::string canonical_regional_market_bytes(const RegionalMarkets& catalog) {
    using Json = nlohmann::json;
    Json regions = Json::array(), plans = Json::array();
    for (const auto& region : catalog.regions)
        regions.push_back({{"id",region.id},{"name",region.name},{"description",region.description}});
    for (const auto& plan : catalog.wholesale) {
        plans.push_back({{"id",plan.id},{"name",plan.name},{"capacity_milli_CU_per_day",plan.capacity.value()},
            {"minimum_memory_mib",plan.minimum_memory_mib},{"interconnect",plan.interconnect},
            {"term_days",plan.term.value()},{"daily_fee_cents",plan.daily_fee.value()},
            {"energy_included",plan.energy_included},{"availability_bp",plan.availability.value()}});
    }
    return Json{{"regions",std::move(regions)},{"wholesale",std::move(plans)},
        {"products",Json::parse(canonical_market_setup_bytes(catalog.products, core::OrdinalDay{0}))}}.dump();
}

const RegionalMarkets* regional_markets(const Collection& collection, const ScenarioDefinition& scenario) {
    if (scenario.regional_markets.empty()) return nullptr;
    const auto* definition = collection.find(scenario.regional_markets);
    const auto* catalog = definition ? std::get_if<RegionalMarkets>(&definition->data) : nullptr;
    if (!catalog) throw std::invalid_argument("Scenario refers to an unknown regional market catalog");
    return catalog;
}
const core::ResilienceCatalog* resilience_catalog(const Collection& collection,const ScenarioDefinition& scenario) {
    if (scenario.resilience.empty()) return nullptr;
    const auto* definition=collection.find(scenario.resilience);
    const auto* catalog=definition?std::get_if<core::ResilienceCatalog>(&definition->data):nullptr;
    if (!catalog) throw std::invalid_argument("Scenario refers to an unknown site resilience catalogue");
    return catalog;
}

MarketSetup scenario_market_setup(const Collection& collection, const ScenarioDefinition& scenario) {
    if (!scenario.market) throw std::invalid_argument("A regional scenario needs an opening market");
    auto setup = *scenario.market;
    const auto* catalog = regional_markets(collection, scenario);
    if (!catalog) return setup;
    for (auto& base : setup.segments) {
        const auto regional = std::find_if(catalog->products.segments.begin(), catalog->products.segments.end(),
            [&](const auto& row) { return row.id == base.id; });
        if (regional == catalog->products.segments.end())
            throw std::invalid_argument("An opening market segment has no regional profile");
        base.region_id = regional->region_id;
        base.customer_compute = regional->customer_compute;
        base.customer_memory_mib = regional->customer_memory_mib;
        base.customer_interconnect = regional->customer_interconnect;
        base.requires_local = regional->requires_local;
    }
    for (auto segment : catalog->products.segments) {
        if (std::any_of(setup.segments.begin(), setup.segments.end(), [&](const auto& base) { return base.id == segment.id; })) continue;
        segment.preferences.clear();
        setup.segments.push_back(std::move(segment));
    }
    std::sort(setup.segments.begin(), setup.segments.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    return setup;
}

std::vector<core::ServiceDefinition> scenario_services(const Collection& collection, const ScenarioDefinition& scenario) {
    if (!scenario.service_catalog) return {};
    auto services = service_definitions(*scenario.service_catalog);
    if (const auto* catalog = regional_markets(collection, scenario)) {
        auto additions = service_definitions(catalog->products);
        for (auto& service : additions) {
            service.serving.starts_on = scenario.starts_on;
            services.push_back(std::move(service));
        }
        core::validate_service_catalog(services);
    }
    return services;
}
}
