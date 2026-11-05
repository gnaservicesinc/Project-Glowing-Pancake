#include <pgp/core/lab.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void label(std::string_view value) {
    if (value.empty() || value.size() > 256 ||
        std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        invalid("Lab labels must be short, nonempty and contain no control characters");
}
template<class Rows> const auto& find(const Rows& rows, std::string_view id) {
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& value) { return value.id == id; });
    if (row == rows.end()) invalid("Lab catalog or record reference does not exist");
    return *row;
}
template<class Rows> void identities(const Rows& rows, std::size_t bound) {
    if (rows.size() > bound) invalid("Lab collection exceeds its count bound");
    std::set<std::string_view> ids;
    for (const auto& row : rows) {
        label(row.id);
        if (!ids.insert(row.id).second) invalid("Duplicate lab record identity");
    }
}
bool licensed(const LabState& state, std::string_view id) {
    return std::any_of(state.licenses.begin(), state.licenses.end(), [&](const auto& row) { return row.offer_id == id; });
}
bool completed(const LabState& state, std::string_view id) {
    return std::any_of(state.research.begin(), state.research.end(), [&](const auto& row) {
        return row.research_id == id && row.completed;
    });
}
bool requires_met(const LabState& state, const std::vector<std::string>& required) {
    return std::all_of(required.begin(), required.end(), [&](const auto& id) { return completed(state, id); });
}
bool knowledge_on(const LabState& state, const CapacityState& capacity, std::string_view id, OrdinalDay day) {
    return std::any_of(state.research.begin(), state.research.end(), [&](const auto& row) {
        if (row.research_id != id || !row.completed) return false;
        const auto& work = find(capacity.projects, row.project_id);
        return work.stopped_on && *work.stopped_on < day;
    });
}
void require_day(const LabState& state, OrdinalDay day) {
    if (day != (state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on))
        invalid("Lab operation must name the current unclosed day");
}
void charge(AccountsState& accounts, OrdinalDay day, Cents amount, Cents protected_cash, const std::string& key) {
    if (amount.value() < 0 || amount > checked_unit_subtract(accounts_report(accounts).cash, protected_cash))
        invalid("Lab purchase would spend reserved or unavailable cash");
    if (amount.value() > 0) record_cash(accounts, day, Cents{-amount.value()}, Account::expenses, key, key);
}
void frozen(const LabModel& model, const CapacityState& capacity) {
    if (model.consumed_work.value() == 0 || (!model.project_id.empty() &&
        find(capacity.projects, model.project_id).status == ProjectStatus::active))
        invalid("Checkpoint or evaluate a model after stopping or completing its current training");
}
CapacityProject project(std::string id, ProjectKind kind, OrdinalDay day,
                         const LabAllocation& allocation, MilliCU work) {
    label(id);
    if (allocation.daily_compute.value() == 0 || allocation.staff == 0 || work.value() == 0)
        invalid("Lab work needs positive staff, daily compute and a finite work budget");
    CapacityProject row;
    row.id = std::move(id); row.source_id = allocation.source_id; row.kind = kind;
    row.starts_on = day; row.requested = allocation.daily_compute; row.staff = allocation.staff;
    row.total_work = work; row.cause = "Lab work: " + row.id;
    return row;
}
std::vector<std::string> data_union(const LabState& state, const TrainingRequest& request) {
    auto ids = request.data_ids;
    if (ids.size() > lab_catalog_limit) invalid("Dataset mixture exceeds its source bound");
    std::set<std::string> unique;
    for (const auto& id : ids)
        if (!unique.insert(id).second) invalid("A dataset cannot contribute twice to one mixture");
    if (!request.parent_id.empty()) {
        const auto& parent = find(state.models, request.parent_id);
        if (parent.recipe_id != request.recipe_id) invalid("Checkpoint continuation must retain its recipe");
        for (const auto& id : ids)
            if (std::find(parent.data_ids.begin(), parent.data_ids.end(), id) == parent.data_ids.end())
                invalid("Checkpoint continuation retains its original data; train a fresh candidate to learn a new mixture");
        ids.insert(ids.end(), parent.data_ids.begin(), parent.data_ids.end());
    }
    std::sort(ids.begin(), ids.end()); ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids.empty()) invalid("A training mixture needs at least one data source");
    for (const auto& id : ids) (void)find(state.catalog.data, id);
    return ids;
}
std::array<std::uint64_t, lab_domain_count> effective_data(const LabCatalog& catalog, const LabModel& model) {
    std::array<std::uint64_t, lab_domain_count> result{};
    for (const auto& id : model.data_ids) {
        const auto& offer = find(catalog.data, id);
        for (std::size_t j = 0; j < lab_domain_count; ++j) {
            const std::uint64_t factors[]{offer.unique_units, static_cast<std::uint64_t>(offer.quality.value()),
                static_cast<std::uint64_t>(offer.relevance[j].value()), static_cast<std::uint64_t>(offer.coverage[j].value())};
            const auto contribution = mul_div_round_half_up(factors, 1'000'000'000'000ULL);
            // Authored per-source units are bounded to 10^12, at most 64 sources.
            result[j] += contribution;
        }
    }
    return result;
}
std::uint64_t saturation(std::uint64_t amount, std::uint64_t scale) {
    if (scale == 0 || amount > static_cast<std::uint64_t>(INT64_MAX) - scale)
        invalid("Invalid saturation scale or resource sum");
    return mul_div_floor(10000, amount, amount + scale).quotient;
}
void resolve_evidence(const LabState& state, EvaluationEvidence& evidence, SplitMix64& stream) {
    const auto& model = find(state.models, evidence.model_id);
    const auto& plan = find(state.catalog.evaluations, evidence.plan_id);
    std::int64_t contamination = 0;
    if (!plan.independent)
        for (const auto& id : model.data_ids)
            contamination = std::max(contamination, find(state.catalog.data, id).contamination.value());
    evidence.stream_before = stream.state();
    evidence.findings.clear();
    evidence.measured = evidence.lower = evidence.upper = empty_lab_profile;
    for (std::size_t j = 0; j < lab_domain_count; ++j) {
        if (plan.coverage[j].value() == 0) continue;
        const auto radius = plan.uncertainty.value();
        const auto draw = stream.bounded(static_cast<std::uint64_t>(2 * radius + 1));
        const auto noise = static_cast<std::int64_t>(draw) - radius;
        const auto observed = std::clamp(model.capability[j].value() + noise + contamination, INT64_C(0), INT64_C(10000));
        evidence.measured[j] = BasisPoints{observed};
        evidence.lower[j] = BasisPoints{std::max(INT64_C(0), observed - radius)};
        evidence.upper[j] = BasisPoints{std::min(INT64_C(10000), observed + radius)};
        if (evidence.upper[j] < plan.required[j])
            evidence.findings.push_back(std::string(lab_domain_names[j]) +
                ": observed performance falls below the promised task requirement; add targeted data and retrain, use human review, or narrow the scope.");
    }
    evidence.stream_after = stream.state(); evidence.completed = true;
}
} // namespace

void validate_lab_catalog(const LabCatalog& catalog) {
    identities(catalog.data, lab_catalog_limit); identities(catalog.research, lab_catalog_limit);
    identities(catalog.recipes, lab_catalog_limit); identities(catalog.evaluations, lab_catalog_limit);
    if (catalog.data.empty() || catalog.recipes.empty() || catalog.evaluations.empty()) invalid("Lab catalog is incomplete");
    for (const auto& offer : catalog.data) {
        label(offer.name); label(offer.provenance); label(offer.rights);
        if (offer.price.value() < 0 || offer.unique_units == 0 || offer.unique_units > 1'000'000'000'000ULL)
            invalid("Data price or unique unit count is outside its bound");
    }
    auto references = [&](const auto& ids) {
        if (ids.size() > lab_catalog_limit) invalid("Research reference bound exceeded");
        std::set<std::string_view> unique;
        for (const auto& id : ids) {
            (void)find(catalog.research, id);
            if (!unique.insert(id).second) invalid("Duplicate research prerequisite");
        }
    };
    std::map<std::string, unsigned> marks;
    std::function<void(const LabResearch&, std::size_t)> visit = [&](const auto& node, std::size_t depth) {
        if (depth > lab_catalog_limit || marks[node.id] == 1) invalid("Research graph contains a cycle");
        if (marks[node.id] == 2) return;
        marks[node.id] = 1;
        for (const auto* edges : {&node.all_of, &node.any_of})
            for (const auto& edge : *edges) visit(find(catalog.research, edge), depth + 1);
        marks[node.id] = 2;
    };
    for (const auto& node : catalog.research) {
        references(node.all_of); references(node.any_of);
        if (node.cash.value() < 0 || node.staff_days.value() == 0 || node.staff_days.value() > 1'000'000 ||
            node.experiment_work.value() == 0 || node.experiment_work.value() > 1'000'000'000'000LL)
            invalid("Research requires bounded positive staff-days and experiment work");
        visit(node, 0);
    }
    for (const auto& recipe : catalog.recipes) {
        label(recipe.name); label(recipe.interconnect); references(recipe.requires_research);
        if (recipe.compute_scale.value() == 0 || recipe.compute_scale.value() > 1'000'000'000'000LL ||
            recipe.minimum_work.value() == 0 || recipe.maximum_work < recipe.minimum_work ||
            recipe.maximum_work.value() > 1'000'000'000'000LL || recipe.inference_per_task.value() == 0)
            invalid("Recipe has an invalid finite compute or inference bound");
        for (std::size_t j = 0; j < lab_domain_count; ++j)
            if (recipe.data_scale[j] == 0 || recipe.data_scale[j] > 1'000'000'000'000ULL || recipe.ceiling[j] < recipe.baseline[j])
                invalid("Recipe data scales and capability ceilings are invalid");
    }
    for (const auto& plan : catalog.evaluations) {
        label(plan.name); references(plan.requires_research);
        if (plan.work.value() == 0 || plan.work.value() > 1'000'000'000'000LL || plan.cash.value() < 0 ||
            std::all_of(plan.coverage.begin(), plan.coverage.end(), [](auto bp) { return bp.value() == 0; }))
            invalid("Evaluation needs finite work and at least one covered subgroup");
    }
}

LabProfile model_capability(const LabCatalog& catalog, const LabModel& model) {
    const auto& recipe = find(catalog.recipes, model.recipe_id);
    const auto units = effective_data(catalog, model);
    const auto compute = saturation(static_cast<std::uint64_t>(model.consumed_work.value()),
                                    static_cast<std::uint64_t>(recipe.compute_scale.value()));
    LabProfile result = empty_lab_profile;
    for (std::size_t j = 0; j < lab_domain_count; ++j) {
        const auto bottleneck = std::min(compute, saturation(units[j], recipe.data_scale[j]));
        const std::uint64_t factors[]{static_cast<std::uint64_t>(recipe.gain[j].value()), bottleneck,
                                      static_cast<std::uint64_t>(recipe.fit[j].value())};
        const auto gain = mul_div_round_half_up(factors, 100'000'000);
        result[j] = BasisPoints{std::min(recipe.ceiling[j].value(), checked_add(recipe.baseline[j].value(), static_cast<std::int64_t>(gain)))};
    }
    return result;
}

TrainingPreview preview_training(const LabState& state, const TrainingRequest& request) {
    const auto& recipe = find(state.catalog.recipes, request.recipe_id);
    if (request.daily_compute.value() == 0 || request.staff == 0 || request.work < recipe.minimum_work || request.work > recipe.maximum_work)
        invalid("Training allocation is outside the recipe's finite work limits");
    LabModel model; model.recipe_id = request.recipe_id; model.data_ids = data_union(state, request);
    model.consumed_work = request.work;
    if (!request.parent_id.empty()) model.consumed_work = checked_unit_add(model.consumed_work, find(state.models, request.parent_id).consumed_work);
    TrainingPreview result;
    const auto quotient = request.work.value() / request.daily_compute.value();
    result.minimum_days = Days{checked_add(quotient, request.work.value() % request.daily_compute.value() != 0 ? 1 : 0)};
    result.licensed = true;
    for (const auto& id : model.data_ids) if (!licensed(state, id)) {
        result.licensed = false;
        result.unowned_data_cost = checked_unit_add(result.unowned_data_cost, find(state.catalog.data, id).price);
    }
    result.prerequisites_met = requires_met(state, recipe.requires_research);
    const auto profile = model_capability(state.catalog, model);
    const auto units = effective_data(state.catalog, model);
    const auto compute = saturation(static_cast<std::uint64_t>(model.consumed_work.value()), static_cast<std::uint64_t>(recipe.compute_scale.value()));
    for (std::size_t j = 0; j < lab_domain_count; ++j) {
        result.lower[j] = BasisPoints{std::max(INT64_C(0), profile[j].value() - recipe.preview_uncertainty.value())};
        result.upper[j] = BasisPoints{std::min(INT64_C(10000), profile[j].value() + recipe.preview_uncertainty.value())};
        result.bottlenecks[j] = saturation(units[j], recipe.data_scale[j]) < compute ? "data coverage" : "training compute";
    }
    result.explanation = "Estimated interval, not held-out evidence. Time assumes full compute availability. Existing lease costs continue; data licenses are paid once.";
    return result;
}

void buy_data(LabState& state, AccountsState& accounts, OrdinalDay day, std::string_view offer_id, Cents protected_cash) {
    require_day(state, day);
    const auto& offer = find(state.catalog.data, offer_id);
    if (licensed(state, offer_id)) invalid("This delivered data license is already owned");
    charge(accounts, day, offer.price, protected_cash, "lab/data/" + offer.id);
    state.licenses.push_back({offer.id, day});
}
void start_research(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                    std::string_view research_id, const LabAllocation& allocation, Cents protected_cash) {
    require_day(state, day);
    const auto& node = find(state.catalog.research, research_id);
    if (!requires_met(state, node.all_of) || (!node.any_of.empty() &&
        std::none_of(node.any_of.begin(), node.any_of.end(), [&](const auto& id) { return completed(state, id); })))
        invalid("Research prerequisites are not complete");
    std::size_t attempt = 1;
    for (const auto& run : state.research) if (run.research_id == research_id) {
        if (find(capacity.projects, run.project_id).status != ProjectStatus::cancelled)
            invalid("This research is already active or completed");
        ++attempt;
    }
    if (state.research.size() >= lab_record_limit) invalid("Research run record limit was reached");
    if (allocation.staff == 0) invalid("Research needs assigned staff");
    const auto days = node.staff_days.value() / allocation.staff + (node.staff_days.value() % allocation.staff != 0 ? 1 : 0);
    // The experiment budget covers both knowledge work and the authored minimum
    // experiment. Outages delay the same experiment, never grant free research.
    const auto work = std::max(node.experiment_work, integrate(allocation.daily_compute, Days{days}));
    auto row = project("lab/research/" + node.id + "/" + std::to_string(attempt), ProjectKind::research, day, allocation, work);
    reserve_project(capacity, day, row);
    charge(accounts, day, node.cash, protected_cash, row.id);
    state.research.push_back({node.id, row.id, false});
}
void start_training(LabState& state, CapacityState& capacity, OrdinalDay day, const TrainingRequest& request) {
    require_day(state, day); label(request.model_id);
    if (state.models.size() >= lab_record_limit || std::any_of(state.models.begin(), state.models.end(),
        [&](const auto& row) { return row.id == request.model_id; })) invalid("Model identity is already used or the record limit was reached");
    const auto preview = preview_training(state, request);
    if (!preview.licensed || !preview.prerequisites_met) invalid("Acquire the data licenses and complete recipe research before training");
    const auto& recipe = find(state.catalog.recipes, request.recipe_id);
    LabModel model; model.id = request.model_id; model.recipe_id = request.recipe_id; model.parent_id = request.parent_id;
    model.data_ids = data_union(state, request); model.created_on = day;
    if (!model.parent_id.empty()) {
        const auto& parent = find(state.models, model.parent_id); frozen(parent, capacity);
        model.inherited_work = parent.consumed_work;
    }
    model.consumed_work = model.inherited_work;
    model.capability = model_capability(state.catalog, model);
    auto row = project("lab/train/" + model.id, ProjectKind::training, day,
        {request.source_id, request.daily_compute, request.staff}, request.work);
    row.minimum_memory_mib = recipe.minimum_memory_mib; row.interconnect = recipe.interconnect;
    model.project_id = row.id;
    reserve_project(capacity, day, std::move(row));
    state.models.push_back(std::move(model));
}
void clone_model(LabState& state, const CapacityState& capacity, OrdinalDay day, std::string_view source_id, std::string id) {
    require_day(state, day); label(id);
    if (state.models.size() >= lab_record_limit || std::any_of(state.models.begin(), state.models.end(),
        [&](const auto& row) { return row.id == id; })) invalid("Model identity is already used or the record limit was reached");
    auto model = find(state.models, source_id); frozen(model, capacity);
    model.parent_id = model.id; model.id = std::move(id); model.project_id.clear(); model.created_on = day;
    model.inherited_work = model.consumed_work;
    state.models.push_back(std::move(model));
}
void start_evaluation(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                      std::string id, std::string_view model_id, std::string_view plan_id,
                      const LabAllocation& allocation, Cents protected_cash) {
    require_day(state, day); label(id);
    if (state.evaluations.size() >= lab_record_limit || std::any_of(state.evaluations.begin(), state.evaluations.end(),
        [&](const auto& row) { return row.id == id; })) invalid("Evaluation identity is already used or the record limit was reached");
    const auto& model = find(state.models, model_id); frozen(model, capacity);
    const auto& plan = find(state.catalog.evaluations, plan_id);
    if (!requires_met(state, plan.requires_research)) invalid("Evaluation methodology research is not complete");
    auto row = project("lab/evaluate/" + id, ProjectKind::evaluation, day, allocation, plan.work);
    reserve_project(capacity, day, row);
    charge(accounts, day, plan.cash, protected_cash, "lab/evaluate/" + id);
    EvaluationEvidence evidence;
    evidence.id = std::move(id); evidence.model_id = std::string(model_id); evidence.plan_id = std::string(plan_id);
    evidence.project_id = row.id; evidence.model_work = model.consumed_work;
    state.evaluations.push_back(std::move(evidence));
}
void close_lab_day(LabState& state, const CapacityState& capacity, OrdinalDay day, RandomSnapshot& random) {
    require_day(state, day);
    if (capacity.closed_through != day) invalid("Lab outcomes require settled actual capacity work");
    for (auto& run : state.research)
        run.completed = find(capacity.projects, run.project_id).status == ProjectStatus::completed;
    for (auto& model : state.models) if (!model.project_id.empty()) {
        model.consumed_work = checked_unit_add(model.inherited_work, find(capacity.projects, model.project_id).completed_work);
        model.capability = model_capability(state.catalog, model);
    }
    auto streams = RandomStreams::restore(random);
    for (auto& evidence : state.evaluations)
        if (!evidence.completed && find(capacity.projects, evidence.project_id).status == ProjectStatus::completed)
            resolve_evidence(state, evidence, streams.stream(RandomStream::research));
    random = streams.snapshot(); state.closed_through = day;
}

void validate_lab(const LabState& state, const CapacityState& capacity, const AccountsState& accounts,
                  const RandomSnapshot& random, bool deep) {
    if (deep) validate_lab_catalog(state.catalog);
    if (state.owner != capacity.owner || state.owner != accounts.owner || state.starts_on != capacity.starts_on ||
        state.starts_on != accounts.starts_on || state.closed_through != accounts.closed_through ||
        state.closed_through != capacity.closed_through || state.licenses.size() > lab_catalog_limit || state.research.size() > lab_record_limit)
        invalid("Lab ownership, daily cursor or record bound differs from its resources");
    const auto today = state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
    identities(state.models, lab_record_limit); identities(state.evaluations, lab_record_limit);
    std::set<std::string_view> licenses, projects, prior_models;
    std::map<std::string_view, std::size_t> attempts;
    const auto payment = [&](std::string_view key, Cents cash, OrdinalDay day) {
        if (deep && cash.value() > 0 && std::none_of(accounts.postings.begin(), accounts.postings.end(), [&](const auto& posting) {
            return posting.key == key && posting.day == day && posting.credit == Account::cash &&
                posting.debit == Account::expenses && posting.amount == cash;
        })) invalid("Lab commitment is missing its actual payment");
    };
    for (const auto& license : state.licenses) {
        (void)find(state.catalog.data, license.offer_id);
        if (license.acquired_on < state.starts_on || license.acquired_on > today || !licenses.insert(license.offer_id).second)
            invalid("Data license has a duplicate identity or invalid acquisition date");
        const auto& offer = find(state.catalog.data, license.offer_id);
        payment("lab/data/" + offer.id, offer.price, license.acquired_on);
    }
    for (const auto& run : state.research) {
        const auto& node = find(state.catalog.research, run.research_id);
        const auto& work = find(capacity.projects, run.project_id);
        const auto attempt = ++attempts[node.id];
        if (!projects.insert(work.id).second || work.id != "lab/research/" + node.id + "/" + std::to_string(attempt) ||
            work.kind != ProjectKind::research || work.staff == 0 || run.completed != (work.status == ProjectStatus::completed) ||
            work.total_work < node.experiment_work || work.total_work < integrate(work.requested,
                Days{node.staff_days.value() / work.staff + (node.staff_days.value() % work.staff != 0 ? 1 : 0)}))
            invalid("Research progress differs from its finite staff/experiment commitment");
        payment(work.id, node.cash, work.starts_on);
        if (attempt > 1) {
            for (const auto& prior : state.research) {
                if (&prior == &run) break;
                if (prior.research_id == node.id) {
                    const auto& previous = find(capacity.projects, prior.project_id);
                    if (previous.status != ProjectStatus::cancelled || !previous.stopped_on || *previous.stopped_on > work.starts_on)
                        invalid("Research retry precedes cancellation of its previous attempt");
                }
            }
        }
        for (const auto& id : node.all_of) {
            if (!knowledge_on(state,capacity,id,work.starts_on))
                invalid("Research began before its prerequisites completed");
        }
        if (!node.any_of.empty() && std::none_of(state.research.begin(), state.research.end(), [&](const auto& row) {
            return std::find(node.any_of.begin(), node.any_of.end(), row.research_id) != node.any_of.end() && row.completed &&
                *find(capacity.projects, row.project_id).stopped_on < work.starts_on;
        })) invalid("Research alternative prerequisite was not complete at acceptance");
    }
    for (const auto& model : state.models) {
        const auto& recipe = find(state.catalog.recipes, model.recipe_id);
        if (model.created_on < state.starts_on || model.created_on > today || model.data_ids.empty() || model.data_ids.size() > lab_catalog_limit ||
            !std::is_sorted(model.data_ids.begin(), model.data_ids.end()) || std::adjacent_find(model.data_ids.begin(), model.data_ids.end()) != model.data_ids.end())
            invalid("Model creation or unique data mixture is invalid");
        for (const auto& id : model.data_ids)
            if (std::none_of(state.licenses.begin(), state.licenses.end(), [&](const auto& license) {
                return license.offer_id == id && license.acquired_on <= model.created_on;
            })) invalid("Model used data before its actual license acquisition");
        for (const auto& id : recipe.requires_research)
            if (!knowledge_on(state,capacity,id,model.created_on)) invalid("Model used research before it completed");
        if (!model.parent_id.empty()) {
            if (!prior_models.contains(model.parent_id)) invalid("Model lineage is cyclic or references a later model");
            const auto& parent = find(state.models, model.parent_id);
            if (model.inherited_work != parent.consumed_work || model.recipe_id != parent.recipe_id ||
                model.data_ids != parent.data_ids || parent.created_on > model.created_on)
                invalid("Model lineage added free work or discarded inherited data rights");
            if (!parent.project_id.empty()) {
                const auto& work = find(capacity.projects,parent.project_id);
                if (!work.stopped_on || *work.stopped_on > model.created_on) invalid("Model inherited an unfinished checkpoint");
            }
        } else if (model.inherited_work.value() != 0) invalid("New model invented inherited work");
        MilliCU expected = model.inherited_work;
        if (!model.project_id.empty()) {
            const auto& work = find(capacity.projects, model.project_id);
            if (work.id != "lab/train/" + model.id || work.kind != ProjectKind::training || work.starts_on != model.created_on ||
                work.total_work < recipe.minimum_work || work.total_work > recipe.maximum_work ||
                work.minimum_memory_mib != recipe.minimum_memory_mib || work.interconnect != recipe.interconnect ||
                !projects.insert(work.id).second) invalid("Model has a foreign, incompatible or duplicate training commitment");
            expected = checked_unit_add(expected, work.completed_work);
        } else if (model.parent_id.empty()) invalid("Model has neither training nor a checkpoint parent");
        if (model.consumed_work != expected || model.capability != model_capability(state.catalog, model))
            invalid("Model capability differs from actual cumulative resources");
        prior_models.insert(model.id);
    }
    std::vector<std::pair<std::pair<OrdinalDay, std::size_t>, const EvaluationEvidence*>> resolved;
    for (std::size_t i = 0; i < state.evaluations.size(); ++i) {
        const auto& evidence = state.evaluations[i];
        const auto& model = find(state.models, evidence.model_id);
        const auto& plan = find(state.catalog.evaluations, evidence.plan_id);
        const auto& work = find(capacity.projects, evidence.project_id);
        if (work.id != "lab/evaluate/" + evidence.id || work.kind != ProjectKind::evaluation || work.total_work != plan.work ||
            !projects.insert(work.id).second || model.consumed_work != evidence.model_work ||
            model.created_on > work.starts_on || model.consumed_work.value() == 0 ||
            evidence.completed != (work.status == ProjectStatus::completed)) invalid("Evaluation differs from its frozen model or work commitment");
        if (!model.project_id.empty()) {
            const auto& training = find(capacity.projects,model.project_id);
            if (!training.stopped_on || *training.stopped_on > work.starts_on) invalid("Evaluation used an unfinished training run");
        }
        for (const auto& id : plan.requires_research)
            if (!knowledge_on(state,capacity,id,work.starts_on)) invalid("Evaluation methodology was not researched at acceptance");
        payment(work.id,plan.cash,work.starts_on);
        if (evidence.completed) resolved.push_back({{*work.stopped_on, i}, &evidence});
        else if (evidence.measured != empty_lab_profile || evidence.lower != empty_lab_profile || evidence.upper != empty_lab_profile ||
                 !evidence.findings.empty() || evidence.stream_before != 0 || evidence.stream_after != 0)
            invalid("Unfinished evaluation revealed unearned evidence");
    }
    if (deep) {
        for (const auto& work : capacity.projects)
            if (work.id.starts_with("lab/") && !projects.contains(work.id)) invalid("Lab capacity commitment has no owning lab record");
        std::sort(resolved.begin(), resolved.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        SplitMix64 stream{state.research_stream_origin};
        for (const auto& entry : resolved) {
            auto expected = *entry.second;
            resolve_evidence(state, expected, stream);
            if (expected != *entry.second) invalid("Evaluation evidence differs from its saved actual draw");
        }
        if (stream.state() != random[static_cast<std::size_t>(RandomStream::research)])
            invalid("Research stream does not continue the saved evaluation evidence");
    }
}
LabReport lab_report(const LabState& state, const CapacityState& capacity) {
    LabReport result; result.data_offers = state.catalog.data; result.licenses = state.licenses; result.research = state.research;
    for (const auto& model : state.models)
        result.models.push_back({model.id, model.recipe_id, model.parent_id, model.consumed_work,
            !model.project_id.empty() && find(capacity.projects, model.project_id).status == ProjectStatus::active});
    for (const auto& evidence : state.evaluations) {
        const auto& plan = find(state.catalog.evaluations, evidence.plan_id);
        result.evidence.push_back({evidence.id, evidence.model_id, evidence.plan_id, evidence.completed,
            evidence.measured, evidence.lower, evidence.upper, plan.coverage, evidence.findings, plan.independent});
    }
    return result;
}
} // namespace pgp::core
