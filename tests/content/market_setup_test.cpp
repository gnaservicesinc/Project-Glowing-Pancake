#include <pgp/content/collection.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace pgp;
namespace {
using Json = nlohmann::json;
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
Json& scenario(Json& source) {
    for (auto& definition : source.at("definitions"))
        if (definition.at("kind") == "scenario" && definition.contains("market")) return definition;
    throw std::runtime_error("Fixture has no market-bearing scenario");
}
content::ScenarioDefinition& scenario(content::Collection& source, const std::string& id) {
    return std::get<content::ScenarioDefinition>(source.definitions.at(id).data);
}
void rejects(const Json& source, const std::function<void(Json&)>& change) {
    auto modified = source;
    change(modified);
    bool refused = false;
    try { (void)content::load_collection(modified.dump(), content::foundation_registry()); }
    catch (const content::ValidationError& error) {
        refused = true;
        check(!error.code().empty() && !error.path().empty(), "Rejected market content lacks a reason and location");
    }
    check(refused, "Invalid authored market setup was accepted");
}
void typed_rejects(const content::Collection& source, const std::string& id,
                   const std::function<void(content::MarketSetup&)>& change) {
    auto modified = source;
    change(*scenario(modified, id).market);
    bool refused = false;
    try { (void)runtime::compile_collection(modified, content::foundation_registry()); }
    catch (const runtime::CompileError& error) {
        refused = true;
        check(!error.code().empty() && !error.path().empty(), "Rejected typed market state lacks a compiler location");
    }
    check(refused, "Compiler normalized invented/invalid typed market state into valid authored content");
}
runtime::CompiledProgram compile(const Json& source) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source.dump(), registry), registry);
}
void loading_and_identity(const Json& source, const std::string& id) {
    const auto registry = content::foundation_registry();
    const auto collection = content::load_collection(source.dump(), registry);
    const auto program = runtime::compile_collection(collection, registry);
    const auto normalized = content::load_collection(program.canonical_content_bytes, registry);
    const auto recompiled = runtime::compile_collection(normalized, registry);
    check(program.canonical_content_bytes == recompiled.canonical_content_bytes,
          "Typed market content did not retain exact canonical bytes through compiler/loader roundtrip");
    const auto identity = persistence::save_identity(program);
    check(identity.content_hash == persistence::sha256(program.canonical_content_bytes) &&
          identity.content_hash == persistence::save_identity(recompiled).content_hash,
          "Market setup was omitted from content identity or changed on typed normalization");
    auto initial = runtime::begin_scenario(program, id, "US-CA");
    const auto& definition = std::get<content::ScenarioDefinition>(program.collection.definitions.at(id).data);
    const auto& setup = *definition.market;
    const auto& state = initial.world.simulation;
    check(state.market && state.capacity && state.accounts,
          "Authored scenario failed to initialize all three business owners");
    check(state.market->products.size() == setup.offers.size() && state.market->segments.size() == setup.segments.size() &&
          state.market->contracts.size() == setup.contracts.size(),
          "Scenario creation lost or duplicated authored products, segments or contracts");
    for (const auto& offer : setup.offers) {
        const auto project = std::find_if(state.capacity->projects.begin(), state.capacity->projects.end(),
            [&](const auto& row) { return row.id == offer.product.capacity_project_id; });
        check(project != state.capacity->projects.end() && project->requested == offer.serving.requested &&
              project->inference_milli_CU_per_task == offer.serving.inference_milli_CU_per_task,
              "Authored offer did not retain its exact shared inference profile");
    }
    for (const auto& authored : setup.contracts) {
        const auto contract = std::find_if(state.market->contracts.begin(), state.market->contracts.end(),
            [&](const auto& row) { return row.id == authored.terms.id; });
        check(contract != state.market->contracts.end() && contract->status == (authored.start_immediately ?
              core::ContractStatus::active : core::ContractStatus::pending),
              "Scenario did not apply authored pending/immediate contract start semantics");
    }
    const auto bytes = persistence::encode_save(program, initial);
    check(persistence::decode_save(program, bytes).checkpoint == initial,
          "Authored market/profile/contract setup did not survive exact save/reload");
    auto changed = collection;
    auto& utility = scenario(changed, id).market->segments.front().preferences.front().utility;
    utility = utility == core::market_utility_limit ? utility - 1 : utility + 1;
    check(persistence::save_identity(runtime::compile_collection(changed, registry)).content_hash != identity.content_hash,
          "Semantic market preference edits failed to change runtime content identity");
}
void order_independence(const Json& source, const std::string& id) {
    auto expanded = source;
    auto& setup = scenario(expanded).at("market");
    auto offer = setup.at("offers").front();
    offer["id"] = "product.secondary";
    offer["capacity_project_id"] = "serve.secondary";
    setup.at("offers").push_back(offer);
    auto preference = setup.at("segments").front().at("preferences").front();
    preference["product_id"] = "product.secondary";
    setup.at("segments").front().at("preferences").push_back(preference);
    auto segment = setup.at("segments").front();
    segment["id"] = "segment.secondary";
    setup.at("segments").push_back(segment);
    const auto original = compile(expanded);
    std::reverse(setup.at("offers").begin(), setup.at("offers").end());
    std::reverse(setup.at("segments").begin(), setup.at("segments").end());
    for (auto& row : setup.at("segments")) std::reverse(row.at("preferences").begin(), row.at("preferences").end());
    const auto reversed = compile(expanded);
    check(original.canonical_content_bytes == reversed.canonical_content_bytes,
          "Market catalog/preference order changed the canonical content identity");
    check(runtime::begin_scenario(original, id, "US-CA") == runtime::begin_scenario(reversed, id, "US-CA"),
          "Equivalent content order produced different initial market/resource/account state");
}
void malformed_content(const Json& source, const content::ScenarioDefinition& definition) {
    rejects(source, [](auto& value) { scenario(value)["market"]["script"] = "unsupported"; });
    rejects(source, [](auto& value) { scenario(value)["market"]["offers"][0]["price_cents_per_task"] = 1.5; });
    rejects(source, [](auto& value) { scenario(value)["market"]["offers"][0]["inference_milli_CU_per_task"] = 0; });
    rejects(source, [](auto& value) { scenario(value)["market"]["offers"][0]["tasks_per_day"] = core::market_task_limit; });
    rejects(source, [](auto& value) { scenario(value)["market"]["offers"][0]["staff"] = 1'000'000; });
    rejects(source, [](auto& value) { scenario(value)["market"]["offers"][0]["minimum_memory_mib"] = 1; });
    rejects(source, [](auto& value) { scenario(value)["market"]["segments"][0]["preferences"][0]["product_id"] = "product.missing"; });
    rejects(source, [](auto& value) { scenario(value)["market"]["contracts"][0]["segment_id"] = "segment.missing"; });
    rejects(source, [](auto& value) {
        scenario(value)["market"]["segments"][0]["daily_budget_cents"] = 0;
        scenario(value)["market"]["contracts"][0]["monthly_fee_cents"] = 1;
    });
    rejects(source, [&](auto& value) {
        scenario(value)["market"]["contracts"][0]["start_by"] =
            core::iso_date(core::OrdinalDay{core::checked_subtract(definition.starts_on.value(), 1)});
    });
    rejects(source, [](auto& value) { scenario(value)["market"]["contracts"][0]["ends_before"] = scenario(value).at("starts_on"); });
    rejects(source, [&](auto& value) {
        scenario(value)["market"]["contracts"][0]["ends_before"] = core::iso_date(core::advance_days(definition.ends_before, core::Days{1}));
    });
    rejects(source, [](auto& value) {
        scenario(value)["market"]["offers"][0]["available"] = false;
        scenario(value)["market"]["contracts"][0]["start_immediately"] = true;
    });
    rejects(source, [](auto& value) {
        auto duplicate = scenario(value)["market"]["offers"][0];
        scenario(value)["market"]["offers"].push_back(duplicate);
    });
}
void malformed_typed(const content::Collection& collection, const std::string& id) {
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().serving.completed_work = core::MilliCU{1}; });
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().serving.status = core::ProjectStatus::cancelled; });
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().serving.source_id = "source.missing"; });
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().serving.requested = core::MilliCUPerDay{1}; });
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().product.capacity_project_id = "profile.missing"; });
    typed_rejects(collection, id, [](auto& setup) { setup.offers.front().serving.cause = "Different hidden cause"; });
    typed_rejects(collection, id, [](auto& setup) { setup.contracts.front().terms.started_on = setup.contracts.front().terms.accepted_on; });
    typed_rejects(collection, id, [](auto& setup) { setup.segments.front().preferences.front().utility = core::market_utility_limit + 1; });
}
void failed_setup_is_atomic(const content::ScenarioDefinition& definition) {
    auto setup = *definition.market;
    auto failing_offer = setup.offers.front();
    failing_offer.product.id = "product.invalid";
    failing_offer.product.capacity_project_id = "serve.invalid";
    failing_offer.serving.id = failing_offer.product.capacity_project_id;
    failing_offer.serving.cause = "Market service: " + failing_offer.product.id;
    failing_offer.serving.staff = definition.team_people + 1;
    setup.offers.push_back(std::move(failing_offer));
    auto market = core::opening_market(definition.starts_on);
    auto accounts = core::opening_accounts(definition.cash, definition.starts_on);
    auto capacity = core::opening_capacity({{"lease.initial", "cloud.pool.initial", "company.player", "supplier.compute.initial",
        core::CapacitySourceKind::leased, definition.leased_capacity, definition.leased_capacity, 0, "general", definition.lease_energy_included}},
        definition.team_people, definition.starts_on);
    const auto before_market = market;
    const auto before_accounts = accounts;
    const auto before_capacity = capacity;
    bool refused = false;
    try { content::apply_market_setup(setup, market, accounts, capacity, definition.starts_on); }
    catch (const std::exception&) { refused = true; }
    check(refused && market == before_market && accounts == before_accounts && capacity == before_capacity,
          "A later invalid offer left partial market products, support obligations or capacity reservations");
}
} // namespace
int main() {
    try {
        std::ifstream input(PGP_MARKET_CONTENT_FIXTURE_PATH, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open market content fixture");
        std::ostringstream bytes; bytes << input.rdbuf();
        auto source = Json::parse(bytes.str());
        const auto id = scenario(source).at("id").get<std::string>();
        auto collection = content::load_collection(source.dump(), content::foundation_registry());
        const auto& definition = scenario(collection, id);
        check(definition.market && !definition.market->offers.empty() && !definition.market->segments.empty() &&
              !definition.market->contracts.empty(), "Market fixture must exercise offers, segments and contracts");
        loading_and_identity(source, id);
        order_independence(source, id);
        malformed_content(source, definition);
        malformed_typed(collection, id);
        failed_setup_is_atomic(definition);
        std::cout << checks << " market setup content/compiler checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
