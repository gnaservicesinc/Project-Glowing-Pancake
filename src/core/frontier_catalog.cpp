#include "frontier_internal.hpp"

#include <set>

namespace pgp::core {
using namespace frontier_detail;

std::string_view frontier_key(FrontierTechnology kind) {
    constexpr std::array<std::string_view,4> names{"data", "research", "recipe", "hardware"};
    return names.at(static_cast<std::size_t>(kind));
}
std::string_view frontier_key(FrontierTrialKind kind) {
    constexpr std::array<std::string_view,3> names{"assessment", "experiment", "replication"};
    return names.at(static_cast<std::size_t>(kind));
}

namespace frontier_detail {
bool text_valid(std::string_view text, std::size_t maximum) {
    return !text.empty() && text.size() <= maximum &&
        std::none_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 32 || ch == 127; });
}
bool condition_met(const FutureConditions& actual, const FutureConditions& minimum) {
    for (std::size_t i = 0; i < actual.size(); ++i)
        if (actual[i] < minimum[i]) return false;
    return true;
}
FrontierScores model_scores(const FrontierCatalog& catalog, const FuturePath& path, const LabCatalog& lab, const LabModel& model,std::span<const GeneratedDataset> generated,std::span<const DataAcquisition> acquired) {
    FrontierScores result{};
    const auto branch_ceiling = static_cast<std::uint32_t>(path.ceiling.value());
    const auto task_capability = model_capability(lab,model,generated,acquired);
    for (std::size_t i = 0; i < introductory_lab_domains; ++i)
        result[i] = std::min(branch_ceiling, static_cast<std::uint32_t>(task_capability[i].value()));
    const auto* profile = named(catalog.recipes, model.recipe_id);
    if (!profile) return result;
    const auto& recipe = require_named(lab.recipes, model.recipe_id);
    std::array<std::uint64_t, frontier_extra_count> effective{};
    for (const auto& id : model.data_ids) if (const auto* corpus = named(catalog.corpora, id)) {
        const auto& source = require_named(lab.data, id);
        for (std::size_t i = 0; i < effective.size(); ++i) {
            const std::uint64_t factors[]{source.unique_units, static_cast<std::uint64_t>(source.quality.value()), corpus->coverage[i]};
            effective[i] += mul_div_round_half_up(factors, 100'000'000);
        }
    }
    const auto work = static_cast<std::uint64_t>(model.consumed_work.value());
    const auto compute = mul_div_floor(work, 10000, work + static_cast<std::uint64_t>(recipe.compute_scale.value())).quotient;
    for (std::size_t i = 0; i < effective.size(); ++i) {
        if (effective[i] == 0) continue;
        const auto data = mul_div_floor(effective[i], 10000, effective[i] + profile->data_scale).quotient;
        const auto gain = static_cast<std::uint32_t>(mul_div_floor(profile->gain[i], std::min(compute, data), 10000).quotient);
        result[i + introductory_lab_domains] = std::min({branch_ceiling, profile->ceiling[i], profile->baseline[i] + gain});
    }
    return result;
}
bool meets_agi(const FrontierScores& scores) {
    return std::all_of(scores.begin(),scores.begin()+frontier_domain_count,[](auto score){return score>=frontier_agi_domain;}) &&
        scores[6]>=frontier_agi_transfer && scores[7]>=frontier_agi_reliability && scores[8]>=frontier_agi_workflows;
}
bool meets_asi(const FrontierScores& scores) {
    return std::all_of(scores.begin(),scores.begin()+frontier_domain_count,[](auto score){return score>=frontier_asi_domain;}) &&
        scores[9]>=frontier_asi_novelty;
}
}

namespace {
template<class Rows> void identities(const Rows& rows) {
    std::set<std::string_view> ids;
    for (const auto& row : rows)
        if (!text_valid(row.id, 64) || !ids.insert(row.id).second) invalid("Frontier definitions need distinct bounded identities.");
}
bool valid_scores(const auto& scores) {
    return std::all_of(scores.begin(), scores.end(), [](auto value) { return value <= 10000; });
}
void validate_terms(const FrontierWorkTerms& work) {
    if (work.fee.value() <= 0 || work.fee.value() > 1'000'000'000 || work.work.value() <= 0 ||
        work.work.value() > 1'000'000'000'000 || work.minimum_days.value() < 3 || work.minimum_days.value() > 180 ||
        work.minimum_staff < 1 || work.minimum_staff > 16)
        invalid("Frontier evidence needs finite paid work, staff and a real observation window.");
}
const FrontierGate* gate(const FrontierCatalog& catalog, FrontierTechnology kind, std::string_view id) {
    const auto row = std::find_if(catalog.gates.begin(), catalog.gates.end(),
        [&](const auto& entry) { return entry.kind == kind && entry.target == id; });
    return row == catalog.gates.end() ? nullptr : &*row;
}
}

void validate_frontier_catalog(const FrontierCatalog& catalog) {
    if (!text_valid(catalog.name) || !text_valid(catalog.assumptions, 1200) ||
        catalog.branches_after < to_ordinal({2027,1,1}) || catalog.paths.empty() || catalog.paths.size() > 8 ||
        catalog.gates.size() > 64 || catalog.corpora.empty() || catalog.corpora.size() > 16 ||
        catalog.recipes.empty() || catalog.recipes.size() > 16 || catalog.minimum_field_tasks_per_day == 0 ||
        catalog.minimum_field_tasks_per_day > 1000 || catalog.integration_loss.value() > 500 ||
        !text_valid(catalog.refresh_research,64) || catalog.refresh_review_age.value()<30 || catalog.refresh_review_age.value()>730)
        invalid("Frontier scenarios need bounded future paths, technologies and independent protocols.");
    identities(catalog.paths);
    identities(catalog.corpora);
    identities(catalog.recipes);
    identities(catalog.panels);
    identities(catalog.experiments);
    for (const auto& path : catalog.paths) {
        if (!text_valid(path.name) || !text_valid(path.description, 600) || !valid_scores(path.maximum) ||
            !condition_met(path.maximum,path.minimum) || path.ceiling.value() < 5000 ||
            (catalog.asi_route ? path.ceiling.value() < 9500 : path.ceiling.value() >= 9000))
            invalid("The future branch ceiling or seeded condition range conflicts with the scenario's declared route.");
    }
    std::set<std::pair<FrontierTechnology,std::string_view>> gates;
    for (const auto& row : catalog.gates) {
        (void)frontier_key(row.kind);
        if (!text_valid(row.target, 64) || !valid_scores(row.minimum) || !gates.emplace(row.kind,row.target).second)
            invalid("A future technology needs one bounded conditions gate.");
    }
    for (const auto& corpus : catalog.corpora)
        if (!valid_scores(corpus.coverage)) invalid("Frontier dataset coverage must be between zero and 100 percent.");
    for (const auto& recipe : catalog.recipes) {
        if (recipe.data_scale == 0 || recipe.data_scale > 1'000'000'000'000 || !valid_scores(recipe.baseline) ||
            !valid_scores(recipe.gain) || !valid_scores(recipe.ceiling))
            invalid("Frontier recipe resources and learning limits are invalid.");
        for (std::size_t i=0; i<recipe.baseline.size(); ++i)
            if (recipe.baseline[i] >= 7500 || recipe.baseline[i] > recipe.ceiling[i])
                invalid("A frontier recipe must earn its qualifying capability through actual training.");
    }
    std::set<std::string_view> holdouts, examiners, research;
    for (const auto& panel : catalog.panels) {
        validate_terms(panel.terms);
        if (!text_valid(panel.name) || !text_valid(panel.reviewer) || !text_valid(panel.holdout) ||
            !holdouts.insert(panel.holdout).second || !examiners.insert(panel.reviewer).second ||
            panel.margin.value() < 1 || panel.margin.value() > 500)
            invalid("Each independent assessment needs a different reviewer, held-out panel and uncertainty margin.");
    }
    for (const auto& experiment : catalog.experiments) {
        validate_terms(experiment.pilot);
        validate_terms(experiment.replication);
        if (!text_valid(experiment.name) || !text_valid(experiment.reviewer) || !text_valid(experiment.verification, 256) ||
            !text_valid(experiment.research,64) || !research.insert(experiment.research).second ||
            !examiners.insert(experiment.reviewer).second || experiment.baseline.value() == 0 ||
            experiment.required_gain.value() == 0 || experiment.margin.value() == 0 ||
            experiment.possible_gain.value()*9500/10000 - experiment.margin.value() < experiment.required_gain.value() ||
            experiment.baseline.value() + experiment.possible_gain.value() > 10000)
            invalid("Each independent replication needs distinct research, review and a bounded improvement over its benchmark.");
    }
}

void validate_frontier_links(const FrontierCatalog& catalog, const LabCatalog& lab, const std::vector<std::string>& hardware) {
    for (const auto& row : catalog.gates) {
        bool exists = false;
        switch (row.kind) {
        case FrontierTechnology::data: exists = named(lab.data,row.target); break;
        case FrontierTechnology::research: exists = named(lab.research,row.target); break;
        case FrontierTechnology::recipe: exists = named(lab.recipes,row.target); break;
        case FrontierTechnology::hardware: exists = std::ranges::find(hardware,row.target) != hardware.end(); break;
        }
        if (!exists) invalid("A frontier technology gate names an unavailable scenario definition.");
    }
    for (const auto& row : catalog.corpora)
        if (!named(lab.data,row.id) || !gate(catalog,FrontierTechnology::data,row.id))
            invalid("A frontier corpus must link to licensed Lab data and its availability gate.");
    for (const auto& row : catalog.recipes)
        if (!named(lab.recipes,row.id) || !gate(catalog,FrontierTechnology::recipe,row.id))
            invalid("A frontier profile must link to a finite Lab recipe and its availability gate.");
    for (const auto& panel : catalog.panels)
        if (named(lab.data,panel.holdout)) invalid("Held-out frontier panels cannot be licensed training datasets.");
    for (const auto& row : catalog.experiments) (void)require_named(lab.research,row.research);
    if (require_named(lab.research,catalog.refresh_research).repeat_limit<2)
        invalid("Mature refresh needs a repeatable method review in the Lab catalog.");

    if (!catalog.asi_route) return;
    // Test the lowest roll of every advertised race path. A seed can slow
    // investment without silently turning a race scenario into a plateau.
    for (const auto& path : catalog.paths) {
        const auto accessible = [&](FrontierTechnology kind, std::string_view id) {
            const auto* requirement = gate(catalog,kind,id);
            return !requirement || condition_met(path.minimum, requirement->minimum);
        };
        std::set<std::string_view> research;
        const auto known = [&](const auto& id) { return research.contains(id); };
        for (std::size_t pass=0; pass<lab.research.size(); ++pass)
            for (const auto& node : lab.research)
                if (accessible(FrontierTechnology::research,node.id) && std::ranges::all_of(node.all_of,known) &&
                    (node.any_of.empty() || std::ranges::any_of(node.any_of,known))) research.insert(node.id);
        if (!std::ranges::all_of(catalog.experiments,[&](const auto& row){return known(row.research);}))
            invalid("An advertised ASI route blocks its independent experiment prerequisites.");
        const auto worst_margin = std::ranges::max_element(catalog.panels,{},&FrontierPanel::margin)->margin;
        bool feasible = false;
        for (const auto& recipe : lab.recipes) {
            if (!named(catalog.recipes,recipe.id) || !accessible(FrontierTechnology::recipe,recipe.id) ||
                !std::ranges::all_of(recipe.requires_research,known)) continue;
            LabModel hypothetical;
            hypothetical.recipe_id = recipe.id;
            hypothetical.consumed_work = recipe.maximum_work;
            for (const auto& data : lab.data) if (data.commercial_use && accessible(FrontierTechnology::data,data.id))
                hypothetical.data_ids.push_back(data.id);
            auto scores = model_scores(catalog,path,lab,hypothetical);
            for (auto& score : scores) score = score > static_cast<std::uint32_t>(worst_margin.value())
                ? score - static_cast<std::uint32_t>(worst_margin.value()) : 0;
            if (meets_asi(scores)) { feasible = true; break; }
        }
        if (!feasible) invalid("An advertised ASI route has no reachable recipe, data and held-out score combination.");
    }
}

FrontierState opening_frontier(const FrontierCatalog& catalog, std::uint64_t seed) {
    validate_frontier_catalog(catalog);
    SplitMix64 stream{seed ^ UINT64_C(0xd6e8feb86659fd93)};
    const auto& path = catalog.paths[static_cast<std::size_t>(stream.bounded(catalog.paths.size()))];
    FrontierState result;
    result.catalog = catalog;
    result.seed = seed;
    result.path = path.id;
    for (std::size_t i=0; i<result.conditions.size(); ++i)
        result.conditions[i] = path.minimum[i] + static_cast<std::uint32_t>(stream.bounded(path.maximum[i]-path.minimum[i]+1));
    return result;
}
const FuturePath& future_path(const FrontierState& state) { return require_named(state.catalog.paths,state.path); }

std::string frontier_technology_block(const SimulationState& state, FrontierTechnology kind, std::string_view id, OrdinalDay day) {
    if (!state.frontier) return {};
    const auto& future = *state.frontier;
    const auto* requirement = gate(future.catalog,kind,id);
    if (!requirement) return {};
    if (day < future.catalog.branches_after)
        return "This technology becomes available to research in the conditional future after " + iso_date(future.catalog.branches_after) + ".";
    for (std::size_t i=0; i<future.conditions.size(); ++i)
        if (future.conditions[i] < requirement->minimum[i])
            return std::string(future_condition_names[i]) + " is below this technology's threshold on the " + future_path(future).name + " path.";
    return {};
}
}
