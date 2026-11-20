#include <pgp/runtime/scenario.hpp>
#include <pgp/core/person_name.hpp>
#include <algorithm>
#include <pgp/runtime/career.hpp>
#include <set>

namespace pgp::runtime {
namespace {
const content::CampaignGenerationRules* generation(const content::ScenarioDefinition& scenario, const WorldState& world) {
    if (!world.founding || world.founding->mode != "campaign" || !scenario.founding || !scenario.founding->generation)
        return nullptr;
    return &*scenario.founding->generation;
}

// Separate local draws by stable record ID. Editing one offer never consumes another's draws.
core::SplitMix64 record_random(std::uint64_t seed, std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char c : id) hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    return core::SplitMix64{seed ^ hash};
}

unsigned percentage(core::SplitMix64& random, unsigned spread) {
    return 100 - spread + static_cast<unsigned>(random.bounded(2 * spread + 1));
}

core::Cents vary(core::Cents amount, unsigned percent) {
    return core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(amount.value()), percent, 100).quotient};
}

void generate_dataset(core::DataOffer& data, const WorldState& world, unsigned spread) {
    auto random = record_random(world.root_seed, data.id);
    const auto volume = percentage(random, spread);
    const auto scale = [volume](std::uint64_t units) {
        return std::clamp<std::uint64_t>(core::mul_div_floor(units, volume, 100).quotient, 1, 1'000'000'000'000ULL);
    };
    data.unique_units = scale(data.unique_units);
    if (data.raw_units) data.raw_units = scale(data.raw_units);
    data.price = vary(data.price, percentage(random, spread));
}

void publish_collection(core::DataOffer& holding, const core::LabCatalog& catalog, const WorldState& world,
                        const content::CampaignGenerationRules& rules) {
    // The publisher supplies its own subject archive and commissions original material.
    // Scenario references continue to address the subject even when the edition changes.
    std::vector<const core::DataAcquisitionOffer*> commissions;
    for (const auto& source : catalog.acquisition)
        if (source.source && source.source->transfers_ip) commissions.push_back(&source);
    if (commissions.empty()) return;
    auto random = record_random(world.root_seed, holding.id);
    const auto& original = *commissions[random.bounded(commissions.size())];
    core::LabState publisher;
    publisher.research_stream_origin = random.next();
    publisher.owner = rules.company_prefixes[random.bounded(rules.company_prefixes.size())] + " " +
                      rules.company_suffixes[random.bounded(rules.company_suffixes.size())];
    core::DataAcquisitionOffer archive;
    archive.dataset = holding;
    archive.source = core::CollectionSource{holding.quality, true};
    publisher.catalog.acquisition = {archive, original};
    auto edition = core::collection_terms(publisher,"collected.mix.publisher",{holding.id,original.dataset.id},"Publisher edition").dataset;
    edition.name = holding.name + " · " + publisher.owner;
    edition.id = holding.id;
    edition.source_path = holding.source_path;
    edition.supplier_or_publisher = publisher.owner;
    edition.provenance = holding.name + " + " + original.dataset.name;
    edition.underlying_history = holding.provenance;
    edition.collection_method = "Publisher archive and commissioned originals";
    edition.rights = holding.rights;
    if (!edition.open_weight_release && holding.open_weight_release)
        edition.rights += " Commissioned material restricts weight redistribution.";
    if (!holding.price.value()) edition.price = core::Cents{0};
    edition.unique_units = std::min<std::uint64_t>(edition.unique_units,1'000'000'000'000ULL);
    const auto archive_raw = std::clamp<std::uint64_t>(holding.raw_units,holding.unique_units,1'000'000'000'000ULL);
    edition.raw_units = archive_raw + original.dataset.unique_units;
    edition.duplication_bp = core::BasisPoints{core::mul_div_floor(edition.raw_units - edition.unique_units,10000,edition.raw_units).quotient};
    holding = std::move(edition);
}
}

core::LabCatalog campaign_lab_catalog(const content::Collection& collection, const content::ScenarioDefinition& scenario,
                                      const WorldState& world) {
    auto catalog = content::scenario_lab_catalog(collection, scenario);
    if (const auto* rules = generation(scenario, world)) {
        for (auto& offer : catalog.acquisition) {
            generate_dataset(offer.dataset, world, rules->dataset_spread_pct);
            offer.dataset.unique_units = std::min<std::uint64_t>(offer.dataset.unique_units,1'000'000'000);
            offer.dataset.price = std::clamp(offer.dataset.price,core::Cents{offer.source ? 0 : 1},core::Cents{1'000'000'000});
        }
        for (auto& data : catalog.data) {
            generate_dataset(data, world, rules->dataset_spread_pct);
            publish_collection(data,catalog,world,*rules);
        }
    }
    restore_licensed_catalog(catalog, world, world.company_id);
    return catalog;
}

std::optional<core::ManagementCatalog> campaign_management(const content::ScenarioDefinition& scenario, const WorldState& world) {
    auto catalog = content::scenario_management(scenario, world.difficulty, world.founding ? &*world.founding : nullptr);
    if (!generation(scenario, world) || !catalog || !catalog->people || !catalog->governance) return catalog;
    const auto& people = *catalog->people;
    for (auto& candidate : catalog->governance->executives) {
        auto random = record_random(world.root_seed, candidate.id);
        candidate.name = core::draw_name_part(people.first_names, random);
        candidate.name += " " + core::draw_name_part(people.last_names, random);
        const auto skill = [&](unsigned base) {
            const int delta = static_cast<int>(random.bounded(2 * people.skill_jitter + 1)) - static_cast<int>(people.skill_jitter);
            return static_cast<unsigned>(std::clamp(static_cast<int>(base) + delta, 0, 100));
        };
        candidate.technical = skill(candidate.technical);
        candidate.commercial = skill(candidate.commercial);
        candidate.risk_tolerance = skill(candidate.risk_tolerance);
    }
    return catalog;
}

void generate_rival_roster(WorldState& world, const CompiledProgram& program) {
    world.generated_rivals.clear();
    const auto* owner = program.collection.find(world.scenario_id);
    const auto* scenario = owner ? std::get_if<content::ScenarioDefinition>(&owner->data) : nullptr;
    if (!scenario) return;
    const auto* rules = generation(*scenario, world);
    if (!rules) return;
    world.generated_rivals = scenario->rivals;
    const auto combinations = rules->company_prefixes.size() * rules->company_suffixes.size();
    std::set<std::string> names;
    for (auto& rival : world.generated_rivals) {
        if (!rival.planning || career_entry_definition(rival)) continue;
        auto random = record_random(world.root_seed, rival.id);
        rival.seed = random.next();
        const auto first = random.bounded(combinations);
        for (std::size_t i = 0; i <= names.size(); ++i) {
            const auto index = (first + i) % combinations;
            auto name = rules->company_prefixes[index / rules->company_suffixes.size()] + " " +
                        rules->company_suffixes[index % rules->company_suffixes.size()];
            if (i >= combinations) name += " " + std::to_string(i / combinations + 1);
            if (names.insert(name).second) { rival.name = std::move(name); break; }
        }
        if (scenario->management && scenario->management->people) {
            const auto& people = *scenario->management->people;
            rival.planning->leader = core::draw_name_part(people.first_names, random);
            rival.planning->leader += " " + core::draw_name_part(people.last_names, random);
        }
        rival.cash = vary(rival.cash, percentage(random, rules->rival_budget_spread_pct));
        rival.monthly_payroll = vary(rival.monthly_payroll, percentage(random, rules->rival_budget_spread_pct));
        rival.monthly_lease = vary(rival.monthly_lease, percentage(random, rules->rival_budget_spread_pct));
    }
}
}
