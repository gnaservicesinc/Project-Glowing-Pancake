#include <pgp/core/lab.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

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
const DataAcquisition* acquired_data(std::span<const DataAcquisition> rows,std::string_view id) {
    const auto row=std::ranges::find_if(rows,[&](const auto& value){return value.offer_id==id && value.output.has_value();});
    return row==rows.end()?nullptr:&*row;
}
const DataOffer& lookup_data(const LabCatalog& catalog,std::span<const GeneratedDataset> generated,
                             std::span<const DataAcquisition> acquired,std::string_view id) {
    const auto authored=std::ranges::find(catalog.data,id,&DataOffer::id);
    if(authored!=catalog.data.end()) return *authored;
    const auto job=std::ranges::find(generated,id,&GeneratedDataset::id);
    if(job!=generated.end() && job->output) return *job->output;
    if(const auto* collection=acquired_data(acquired,id)) return *collection->output;
    invalid("Choose a catalog dataset or wait for the data job to deliver its corpus");
}
bool licensed(const LabState& state, std::string_view id) {
    return std::any_of(state.licenses.begin(), state.licenses.end(), [&](const auto& row) { return row.offer_id == id; }) ||
        std::ranges::any_of(state.generated,[&](const auto& row) { return row.id==id && row.output.has_value(); }) ||
        acquired_data(state.acquisitions,id);
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
        (find(capacity.projects, model.project_id).status == ProjectStatus::active || find(capacity.projects, model.project_id).status == ProjectStatus::paused)))
        invalid("Clone, evaluate or continue from an immutable checkpoint. Save a checkpoint of running or paused training first");
}
std::unordered_map<std::string_view,MilliCU> checkpoint_work(const LabState& state,const CapacityState& capacity) {
    std::unordered_map<std::string_view,MilliCU> result,prefix;
    std::unordered_map<std::string_view,const LabModel*> models;
    for(const auto& model:state.models) models.emplace(model.id,&model);
    struct Query {const LabModel* model;const LabModel* parent;};
    std::vector<Query> queries;
    for(const auto& model:state.models) if(model.checkpoint_cursor) {
        const auto parent=models.find(model.parent_id);
        if(parent==models.end() || !model.project_id.empty() || model.consumed_work.value()==0 || model.created_on<capacity.starts_on ||
            *model.checkpoint_cursor>capacity.days.size() || *model.checkpoint_cursor!=static_cast<std::uint64_t>(model.created_on.value()-capacity.starts_on.value()))
            invalid("Checkpoint has no immutable parent snapshot or actual settled-day boundary");
        queries.push_back({&model,parent->second});
        if(!parent->second->project_id.empty()) prefix.try_emplace(parent->second->project_id,MilliCU{0});
    }
    std::sort(queries.begin(),queries.end(),[](const auto& a,const auto& b) {
        return std::tie(*a.model->checkpoint_cursor,a.model->id)<std::tie(*b.model->checkpoint_cursor,b.model->id);
    });
    std::size_t cursor=0;
    for(const auto& query:queries) {
        while(cursor<*query.model->checkpoint_cursor) {
            for(const auto& used:capacity.days[cursor].projects) if(const auto it=prefix.find(used.project_id);it!=prefix.end())
                it->second=checked_unit_add(it->second,used.work);
            ++cursor;
        }
        result.emplace(query.model->id,query.parent->project_id.empty()?query.parent->consumed_work:
            checked_unit_add(query.parent->inherited_work,prefix.at(query.parent->project_id)));
    }
    return result;
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
    for (const auto& id : ids) (void)lab_data(state,id);
    return ids;
}
std::array<std::uint64_t, lab_domain_count> effective_data(const LabCatalog& catalog, const LabModel& model,std::span<const GeneratedDataset> generated,
        std::span<const DataAcquisition> acquired) {
    std::array<std::uint64_t, lab_domain_count> result{};
    for (const auto& id : model.data_ids) {
        const auto& offer = lookup_data(catalog,generated,acquired,id);
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
            contamination = std::max(contamination, lab_data(state,id).contamination.value());
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

const DataOffer& lab_data(const LabState& state,std::string_view id) {
    return lookup_data(state.catalog,state.generated,state.acquisitions,id);
}

std::optional<OrdinalDay> data_available_on(const LabState& state,const CapacityState& capacity,std::string_view id) {
    const auto license=std::ranges::find(state.licenses,id,&DataLicense::offer_id);
    if(license!=state.licenses.end()) return license->acquired_on;
    const auto generated=std::ranges::find(state.generated,id,&GeneratedDataset::id);
    const auto* acquired=acquired_data(state.acquisitions,id);
    std::string_view project;
    if(generated!=state.generated.end() && generated->output) project=generated->project_id;
    else if(acquired) project=acquired->project_id;
    else return std::nullopt;
    const auto& work=find(capacity.projects,project);
    if(work.status!=ProjectStatus::completed || !work.stopped_on) return std::nullopt;
    return advance_days(*work.stopped_on,Days{1});
}

void validate_lab_catalog(const LabCatalog& catalog) {
    validate_synthetic_catalog(catalog);
    validate_acquisition_catalog(catalog);
    identities(catalog.data, lab_catalog_limit); identities(catalog.research, lab_catalog_limit);
    identities(catalog.recipes, lab_catalog_limit); identities(catalog.evaluations, lab_catalog_limit);
    if (catalog.data.empty() || catalog.recipes.empty() || catalog.evaluations.empty()) invalid("Lab catalog is incomplete");
    for (const auto& offer : catalog.data) {
        label(offer.name); label(offer.provenance); label(offer.rights);
        if (offer.price.value() < 0 || offer.unique_units == 0 || offer.unique_units > 1'000'000'000'000ULL)
            invalid("Data price or unique unit count is outside its bound");
        if (offer.open_weight_release && !offer.commercial_use)
            invalid("Research-only data cannot authorize open weight release");
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
    if(catalog.creative_research.size()>lab_catalog_limit || (!catalog.creative_research.empty() && !catalog.research_workplaces)) invalid("Creative research needs equipped named teams.");
    for(const auto& [id,r]:catalog.creative_research) {
        (void)find(catalog.research,id);
        if(r.earliest_year<2015 || r.earliest_year>2200 || r.difficulty<1 || r.difficulty>1000 || r.coordination_bonus_bp>2500) invalid("Creative research rules exceed their bounds.");
    }
    if(catalog.research_workplaces) {
        if(catalog.research_workplaces->size()>lab_catalog_limit) invalid("Too many research workplace requirements");
        for(const auto& [id,required]:*catalog.research_workplaces) {
            label(id);
            if(required.empty() || required.size()>8 || (!catalog.research.empty() && std::ranges::none_of(catalog.research,[&](const auto& node){return node.id==id;})))
                invalid("A workplace requirement needs equipment and a known research topic");
            std::set<std::string_view> seen;
            for(const auto& equipment:required) {label(equipment);if(!seen.insert(equipment).second) invalid("Research equipment requirements must be distinct");}
        }
    }
    for (const auto& node : catalog.research) {
        references(node.all_of); references(node.any_of);
        if (node.cash.value() < 0 || (catalog.research_workplaces && node.cash.value()!=0) || node.staff_days.value() == 0 || node.staff_days.value() > 1'000'000 ||
            node.repeat_limit<1 || node.repeat_limit>12 ||
            node.experiment_work.value() == 0 || node.experiment_work.value() > 1'000'000'000'000LL)
            invalid("Research requires bounded positive staff-days and experiment work");
        if (node.setback) {
            const auto& risk = *node.setback;
            (void)find(catalog.research, risk.safeguard);
            label(risk.finding);
            if (risk.safeguard == node.id || risk.failure_chance.value() == 0 ||
                risk.retry_credit.value() == 0 || risk.retry_credit.value() > 2500)
                invalid("A research setback needs another topic as a safeguard and a positive, bounded retry credit");
        }
        visit(node, 0);
    }
    for (const auto& recipe : catalog.recipes) {
        label(recipe.name); label(recipe.interconnect); references(recipe.requires_research);
        if (recipe.compute_scale.value() == 0 || recipe.compute_scale.value() > 1'000'000'000'000LL ||
            recipe.minimum_work.value() == 0 || recipe.maximum_work < recipe.minimum_work ||
            recipe.maximum_work.value() > 1'000'000'000'000LL || recipe.inference_per_task.value() == 0)
            invalid("Recipe has an invalid finite compute or inference bound");
        for (std::size_t j = 0; j < lab_domain_count; ++j) {
            if (j>=introductory_lab_domains && recipe.data_scale[j]==0 && recipe.baseline[j].value()==0 &&
                recipe.gain[j].value()==0 && recipe.ceiling[j].value()==0 && recipe.fit[j].value()==0) continue;
            if (recipe.data_scale[j]==0 || recipe.data_scale[j]>1'000'000'000'000ULL ||
                recipe.ceiling[j]<recipe.baseline[j] || (j>=introductory_lab_domains && recipe.ceiling[j].value()==0))
                invalid("Recipe data scales and capability ceilings are invalid");
        }
    }
    for (const auto& plan : catalog.evaluations) {
        label(plan.name); references(plan.requires_research);
        if (plan.work.value() == 0 || plan.work.value() > 1'000'000'000'000LL || plan.cash.value() < 0 ||
            std::all_of(plan.coverage.begin(), plan.coverage.end(), [](auto bp) { return bp.value() == 0; }))
            invalid("Evaluation needs finite work and at least one covered subgroup");
    }
}

LabProfile model_capability(const LabCatalog& catalog, const LabModel& model,
        std::span<const GeneratedDataset> generated,std::span<const DataAcquisition> acquired) {
    const auto& recipe = find(catalog.recipes, model.recipe_id);
    const auto units = effective_data(catalog,model,generated,acquired);
    const auto compute = saturation(static_cast<std::uint64_t>(model.consumed_work.value()),
                                    static_cast<std::uint64_t>(recipe.compute_scale.value()));
    LabProfile result = empty_lab_profile;
    for (std::size_t j = 0; j < lab_domain_count; ++j) {
        if (recipe.data_scale[j]==0) continue;
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
    const auto profile = model_capability(state.catalog,model,state.generated,state.acquisitions);
    const auto units = effective_data(state.catalog,model,state.generated,state.acquisitions);
    const auto compute = saturation(static_cast<std::uint64_t>(model.consumed_work.value()), static_cast<std::uint64_t>(recipe.compute_scale.value()));
    for (std::size_t j = 0; j < lab_domain_count; ++j) {
        if (recipe.data_scale[j]==0) continue;
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
namespace {
BasisPoints insight_on(const LabState& state,std::string_view id,OrdinalDay day) {
    for (const auto& row:state.insights) if (row.research_id==id && row.observed_on<=day) return row.credit;
    return BasisPoints{0};
}
MilliCU insight_work(MilliCU work,BasisPoints credit) {
    const auto result=mul_div_floor(static_cast<std::uint64_t>(work.value()),static_cast<std::uint64_t>(10000-credit.value()),10000);
    return MilliCU{checked_add(static_cast<std::int64_t>(result.quotient),result.remainder>0?1:0)};
}
MilliCU research_work(const LabResearch& node, MilliCUPerDay rate, std::uint32_t staff,
                      BasisPoints public_credit, BasisPoints retry_credit) {
    const auto days = node.staff_days.value() / staff + (node.staff_days.value() % staff != 0 ? 1 : 0);
    const auto staff_floor = integrate(rate, Days{days});
    if (!node.setback)
        return insight_work(std::max(node.experiment_work, staff_floor), public_credit);
    return std::max(staff_floor, insight_work(node.experiment_work, std::max(public_credit, retry_credit)));
}
bool experiment_failed(const LabState& state, const CapacityState& capacity, const LabResearch& node,
                       const CapacityProject& work, bool has_result) {
    if (work.status != ProjectStatus::completed || !node.setback || has_result ||
        knowledge_on(state, capacity, node.setback->safeguard, work.starts_on)) return false;
    // One stable sample for the first settled result of this topic. Cancelled attempts
    // and evaluations cannot change it. The stream origin is already saved.
    std::uint64_t key = 14695981039346656037ULL;
    for (const char byte : node.id) key = (key ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
    SplitMix64 draw{state.research_stream_origin ^ key};
    return draw.bounded(10'000) < static_cast<std::uint64_t>(node.setback->failure_chance.value());
}
}
ResearchPacing research_pacing(const LabState& state,const CapacityState& capacity,std::string_view id,OrdinalDay day) {
    const auto& node=find(state.catalog.research,id);
    ResearchPacing pacing;
    pacing.prerequisite_groups=static_cast<std::uint32_t>(node.all_of.size())+(node.any_of.empty()?0U:1U);
    for(const auto& required:node.all_of) if(!knowledge_on(state,capacity,required,day)) ++pacing.missing_groups;
    if(!node.any_of.empty() && std::ranges::none_of(node.any_of,[&](const auto& required){return knowledge_on(state,capacity,required,day);}))
        ++pacing.missing_groups;
    const auto rule=state.catalog.creative_research.find(std::string{id});
    if(rule==state.catalog.creative_research.end()) {
        pacing.threshold=pacing.missing_groups?UINT32_MAX:0;
        return pacing;
    }
    const auto& r=rule->second;
    pacing.expected_offset=static_cast<std::uint32_t>(std::max(0,r.earliest_year-from_ordinal(state.starts_on).year));
    pacing.early_multiplier=static_cast<std::uint32_t>(std::max(1,r.earliest_year-from_ordinal(day).year));
    if(pacing.missing_groups) {
        const auto base=static_cast<std::uint64_t>(pacing.expected_offset)+r.difficulty;
        const auto scaled=base*pacing.early_multiplier*pacing.missing_groups;
        pacing.threshold=static_cast<std::uint32_t>((scaled+pacing.prerequisite_groups-1)/pacing.prerequisite_groups);
    }
    return pacing;
}
std::uint32_t research_creativity_threshold(const LabState& state,const CapacityState& capacity,std::string_view id,OrdinalDay day) {
    return research_pacing(state,capacity,id,day).threshold;
}
std::uint32_t coordinated_staff_days(const LabCatalog& catalog,const LabResearch& node,std::uint32_t compatibility,BasisPoints cooperation) {
    const auto found=catalog.creative_research.find(node.id);
    const std::uint64_t bonus=found==catalog.creative_research.end()?0:static_cast<std::uint64_t>(found->second.coordination_bonus_bp)*compatibility/10000;
    const auto numerator=static_cast<std::uint64_t>(node.staff_days.value())*10000;
    const auto coordinated=(numerator+10000+bonus-1)/(10000+bonus);
    return static_cast<std::uint32_t>(insight_work(MilliCU{static_cast<std::int64_t>(coordinated)},cooperation).value());
}
void start_research(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                    std::string_view research_id, const LabAllocation& allocation, Cents protected_cash,Cents sponsored_fee,std::uint32_t creativity,std::uint32_t compatibility_bp,std::optional<ResearchCooperation> cooperation) {
    require_day(state, day);
    const auto& node = find(state.catalog.research, research_id);
    if (sponsored_fee.value()<0 || sponsored_fee>node.cash) invalid("A research sponsor cannot pay more than the fee");
    if(creativity>1024 || compatibility_bp>10000) invalid("Research team characteristics exceed their bounds.");
    const auto threshold=research_creativity_threshold(state,capacity,research_id,day);
    if(threshold>creativity) invalid("Complete the prerequisites or assign a team with enough distinct traits.");
    std::size_t attempt = 1;
    std::uint32_t completed_runs = 0;
    bool has_failure = false;
    for (const auto& run : state.research) if (run.research_id == research_id) {
        const auto status = find(capacity.projects,run.project_id).status;
        if (status == ProjectStatus::active || status==ProjectStatus::paused) invalid("This research is already unfinished; resume or cancel its existing run");
        if (run.completed) ++completed_runs;
        has_failure = has_failure || run.failed;
        ++attempt;
    }
    if (completed_runs>=node.repeat_limit) invalid("This topic has reached its limit for successful research results");
    if (state.research.size() >= lab_record_limit) invalid("Research run record limit was reached");
    if (allocation.staff == 0) invalid("Research needs assigned staff");
    const auto credit=insight_on(state,research_id,day);
    const auto retry_credit = node.setback && has_failure && completed_runs == 0 ? node.setback->retry_credit : BasisPoints{0};
    const bool grounded=state.catalog.research_workplaces.has_value();
    if(grounded && allocation.researchers.size()!=allocation.staff) invalid("Choose the people who will do the research");
    const auto cooperative=cooperation?cooperation->credit:BasisPoints{0};
    if(cooperation && (!grounded || cooperative.value()<=0 || cooperative.value()>2500 || cooperation->agreement.empty() || cooperation->agreement.size()>64 || cooperation->partner==state.owner || cooperation->partner.empty()))invalid("Research cooperation needs a named partner and bounded agreement credit.");
    const auto work = grounded?insight_work(node.experiment_work,std::max({credit,retry_credit,cooperative})):
        research_work(node, allocation.daily_compute, allocation.staff, credit, retry_credit);
    auto row = project("lab/research/" + node.id + "/" + std::to_string(attempt), ProjectKind::research, day, allocation, work);
    if(grounded) row.required_staff_days=coordinated_staff_days(state.catalog,node,compatibility_bp,cooperative);
    reserve_project(capacity, day, row);
    charge(accounts, day, checked_unit_subtract(node.cash,sponsored_fee), protected_cash, row.id);
    state.research.push_back({node.id, row.id, false,credit,sponsored_fee,false,retry_credit,allocation.researchers,creativity,compatibility_bp,std::move(cooperation)});
}
void publish_research(LabState& state,const CapacityState& capacity,OrdinalDay day,std::string_view id) {
    require_day(state,day);
    if (!knowledge_on(state,capacity,id,day) || std::any_of(state.publications.begin(),state.publications.end(),[&](const auto& row){return row.research_id==id;}) ||
        state.publications.size()>=lab_catalog_limit) invalid("Publish a completed, previously private research result");
    state.publications.push_back({std::string(id),day});
}
void acquire_research_insight(LabState& state,OrdinalDay day,ResearchInsight insight) {
    require_day(state,day);(void)find(state.catalog.research,insight.research_id);label(insight.publisher);
    if (insight.publisher==state.owner || insight.published_on>=day || insight.observed_on!=day || insight.credit.value()<=0 || insight.credit.value()>2500 ||
        state.insights.size()>=lab_catalog_limit || std::any_of(state.insights.begin(),state.insights.end(),[&](const auto& row){return row.research_id==insight.research_id;}))
        invalid("A public research insight must have a distinct publisher, prior publication and one bounded adoption credit");
    state.insights.push_back(std::move(insight));
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
    model.capability = model_capability(state.catalog,model,state.generated,state.acquisitions);
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
    model.inherited_work = model.consumed_work;model.checkpoint_cursor.reset();
    state.models.push_back(std::move(model));
}
void checkpoint_model(LabState& state,const CapacityState& capacity,OrdinalDay day,std::string_view source_id,std::string id) {
    require_day(state,day);label(id);
    if(state.models.size()>=lab_record_limit || std::any_of(state.models.begin(),state.models.end(),[&](const auto& row){return row.id==id;})) invalid("Checkpoint identity is already used or the model register is full");
    auto model=find(state.models,source_id);
    if(model.consumed_work.value()==0) invalid("Complete some training work before saving a model checkpoint");
    model.parent_id=model.id;model.id=std::move(id);model.project_id.clear();model.created_on=day;
    model.inherited_work=model.consumed_work;model.checkpoint_cursor=capacity.days.size();
    state.models.push_back(std::move(model));
}
void start_evaluation(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                      std::string id, std::string_view model_id, std::string_view plan_id,
                      const LabAllocation& allocation, Cents protected_cash, std::string specialist_id) {
    require_day(state, day); label(id);
    if (state.evaluations.size() >= lab_record_limit || std::any_of(state.evaluations.begin(), state.evaluations.end(),
        [&](const auto& row) { return row.id == id; })) invalid("Evaluation identity is already used or the record limit was reached");
    const auto& model = find(state.models, model_id); frozen(model, capacity);
    const auto& plan = find(state.catalog.evaluations, plan_id);
    if (!requires_met(state, plan.requires_research)) invalid("Evaluation methodology research is not complete");
    auto row = project("lab/evaluate/" + id, ProjectKind::evaluation, day, allocation,
        evaluation_work(capacity,day,plan.work,specialist_id));
    row.specialist_id=std::move(specialist_id);
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
    close_synthetic_data(state,capacity,day);
    for (auto& run : state.research) if (!run.completed && !run.failed) {
        const auto& work = find(capacity.projects, run.project_id);
        if (work.status != ProjectStatus::completed) continue;
        const auto& node = find(state.catalog.research, run.research_id);
        const bool has_result = node.setback && std::ranges::any_of(state.research, [&](const auto& prior) {
            return prior.research_id == node.id && (prior.completed || prior.failed);
        });
        run.failed = experiment_failed(state, capacity, node, work, has_result);
        run.completed = !run.failed;
    }
    for (auto& model : state.models) if (!model.project_id.empty()) {
        model.consumed_work = checked_unit_add(model.inherited_work, find(capacity.projects, model.project_id).completed_work);
        model.capability = model_capability(state.catalog,model,state.generated,state.acquisitions);
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
    if (state.publications.size()>lab_catalog_limit || state.insights.size()>lab_catalog_limit) invalid("Research publication/insight history exceeds its bound");
    std::set<std::string_view> insight_ids,publication_ids;
    for (const auto& row:state.insights) {
        (void)find(state.catalog.research,row.research_id);label(row.publisher);
        if (!insight_ids.insert(row.research_id).second || row.publisher==state.owner || row.observed_on<state.starts_on || row.observed_on>today ||
            row.published_on>=row.observed_on || row.credit.value()<=0 || row.credit.value()>2500) invalid("Research insight has invalid provenance, adoption date or stacked credit");
    }
    for (const auto& row:state.publications)
        if (!publication_ids.insert(row.research_id).second || row.day>today || !knowledge_on(state,capacity,row.research_id,row.day)) invalid("Publication lacks a unique actual completed result");
    identities(state.models, lab_record_limit); identities(state.evaluations, lab_record_limit);
    std::set<std::string_view> licenses, projects, prior_models;
    struct TopicHistory {
        std::size_t attempts = 0, successes = 0;
        bool failed = false;
        const CapacityProject* latest = nullptr;
    };
    std::map<std::string_view, TopicHistory> history;
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
        auto& prior = history[node.id];
        if (prior.successes >= node.repeat_limit) invalid("Research exceeded its authored repetition limit");
        if (prior.latest && (!prior.latest->stopped_on || *prior.latest->stopped_on > work.starts_on ||
            (prior.latest->status == ProjectStatus::completed && *prior.latest->stopped_on == work.starts_on)))
            invalid("Research retry precedes the settled end of its previous attempt");
        const auto attempt = ++prior.attempts;
        const auto credit=insight_on(state,node.id,work.starts_on);
        const auto retry = node.setback && prior.failed && prior.successes == 0 ? node.setback->retry_credit : BasisPoints{0};
        const bool grounded=state.catalog.research_workplaces.has_value();
        const auto cooperative=run.cooperation?run.cooperation->credit:BasisPoints{0};
        if(run.cooperation && (!grounded || cooperative.value()<=0 || cooperative.value()>2500 || run.cooperation->agreement.empty() || run.cooperation->agreement.size()>64 || run.cooperation->partner.empty() || run.cooperation->partner==state.owner))invalid("Research cooperation lost its bounded partner agreement.");
        const auto minimum = grounded?insight_work(node.experiment_work,std::max({credit,retry,cooperative})):
            research_work(node, work.requested, std::max(work.staff, 1U), credit, retry);
        if(work.required_staff_days!=(grounded?coordinated_staff_days(state.catalog,node,run.compatibility_bp,cooperative):0) ||
            (grounded && (run.researchers.size()!=work.staff || run.researchers.size()>32 ||
                std::set<std::string_view>(run.researchers.begin(),run.researchers.end()).size()!=run.researchers.size())) ||
            (!grounded && !run.researchers.empty()))
            invalid("Research needs its independent labor budget and named researchers");
        const bool failed = experiment_failed(state, capacity, node, work, prior.failed || prior.successes != 0);
        if (!projects.insert(work.id).second || work.id != "lab/research/" + node.id + "/" + std::to_string(attempt) ||
            work.kind != ProjectKind::research || work.staff == 0 || run.failed != failed ||
            run.completed != (work.status == ProjectStatus::completed && !failed) ||
            run.insight_credit != credit || run.retry_credit != retry || work.total_work < minimum ||
            ((grounded || node.setback || credit.value()>0) && work.total_work != minimum))
            invalid("Research progress or outcome differs from its settled work and accepted experiment terms");
        prior.latest = &work;
        prior.failed = prior.failed || run.failed;
        if (run.completed) ++prior.successes;
        if (run.sponsored_fee.value()<0 || run.sponsored_fee>node.cash) invalid("Sponsored research exceeds the authored fee");
        const auto cash=checked_unit_subtract(node.cash,run.sponsored_fee);
        payment(work.id,cash,work.starts_on);
        if (deep && cash.value()==0 && run.sponsored_fee.value()>0 &&
            std::ranges::any_of(accounts.postings,[&](const auto& posting){return posting.key==work.id;}))
            invalid("A fully sponsored fee also has a cash posting");
        if(run.creativity>1024 || run.compatibility_bp>10000 || run.creativity<research_creativity_threshold(state,capacity,node.id,work.starts_on))
            invalid("Research began without prerequisites or its recorded creative team threshold.");
    }
    validate_synthetic_data(state,capacity,accounts,deep);
    for (const auto& row:state.generated)
        if (!projects.insert(row.project_id).second) invalid("Synthetic data duplicated a lab project owner");
    for (const auto& row:state.acquisitions)
        if (!projects.insert(row.project_id).second) invalid("Data collection duplicated a Lab project owner");
    const auto snapshots=deep?checkpoint_work(state,capacity):std::unordered_map<std::string_view,MilliCU>{};
    for (const auto& model : state.models) {
        const auto& recipe = find(state.catalog.recipes, model.recipe_id);
        if (model.created_on < state.starts_on || model.created_on > today || model.data_ids.empty() || model.data_ids.size() > lab_catalog_limit ||
            !std::is_sorted(model.data_ids.begin(), model.data_ids.end()) || std::adjacent_find(model.data_ids.begin(), model.data_ids.end()) != model.data_ids.end())
            invalid("Model creation or unique data mixture is invalid");
        for (const auto& id : model.data_ids) {
            const auto available=data_available_on(state,capacity,id);
            if(!available || *available>model.created_on)
                invalid("Model used data before its actual license acquisition or delivery");
        }
        for (const auto& id : recipe.requires_research)
            if (!knowledge_on(state,capacity,id,model.created_on)) invalid("Model used research before it completed");
        if (!model.parent_id.empty()) {
            if (!prior_models.contains(model.parent_id)) invalid("Model lineage is cyclic or references a later model");
            const auto& parent = find(state.models, model.parent_id);
            if (model.recipe_id != parent.recipe_id || model.data_ids != parent.data_ids || parent.created_on > model.created_on)
                invalid("Model lineage changed its recipe, data rights or creation order");
            if(model.checkpoint_cursor) {
                if(!model.project_id.empty() || model.consumed_work.value()==0 || *model.checkpoint_cursor>capacity.days.size() ||
                    *model.checkpoint_cursor!=static_cast<std::uint64_t>(model.created_on.value()-capacity.starts_on.value()) ||
                    model.inherited_work<parent.inherited_work || model.inherited_work>parent.consumed_work ||
                    (deep && model.inherited_work!=snapshots.at(model.id))) invalid("Checkpoint inherited work outside its actual settled snapshot");
            } else {
                if(model.inherited_work!=parent.consumed_work) invalid("Model lineage added free inherited work");
                if (!parent.project_id.empty()) {
                    const auto& work = find(capacity.projects,parent.project_id);
                    if (!work.stopped_on || *work.stopped_on > model.created_on) invalid("Model inherited an unfinished checkpoint");
                }
            }
        } else if (model.inherited_work.value() != 0 || model.checkpoint_cursor) invalid("New model invented inherited work or a checkpoint origin");
        MilliCU expected = model.inherited_work;
        if (!model.project_id.empty()) {
            const auto& work = find(capacity.projects, model.project_id);
            if (work.id != "lab/train/" + model.id || work.kind != ProjectKind::training || work.starts_on != model.created_on ||
                work.total_work < recipe.minimum_work || work.total_work > recipe.maximum_work ||
                work.minimum_memory_mib != recipe.minimum_memory_mib || work.interconnect != recipe.interconnect ||
                !projects.insert(work.id).second) invalid("Model has a foreign, incompatible or duplicate training commitment");
            expected = checked_unit_add(expected, work.completed_work);
        } else if (model.parent_id.empty()) invalid("Model has neither training nor a checkpoint parent");
        if (model.consumed_work != expected || model.capability != model_capability(state.catalog,model,state.generated,state.acquisitions))
            invalid("Model capability differs from actual cumulative resources");
        prior_models.insert(model.id);
    }
    std::vector<std::pair<std::pair<OrdinalDay, std::size_t>, const EvaluationEvidence*>> resolved;
    for (std::size_t i = 0; i < state.evaluations.size(); ++i) {
        const auto& evidence = state.evaluations[i];
        const auto& model = find(state.models, evidence.model_id);
        const auto& plan = find(state.catalog.evaluations, evidence.plan_id);
        const auto& work = find(capacity.projects, evidence.project_id);
        if (work.id != "lab/evaluate/" + evidence.id || work.kind != ProjectKind::evaluation ||
            work.total_work != evaluation_work(capacity,work.starts_on,plan.work,work.specialist_id) ||
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
    for (const auto& row:state.generated) if (row.output) {
        result.data_offers.push_back(*row.output);
        const auto& work=find(capacity.projects,row.project_id);
        result.licenses.push_back({row.id,advance_days(*work.stopped_on,Days{1})});
    }
    for (const auto& row:state.acquisitions) if (row.output) {
        result.data_offers.push_back(*row.output);
        const auto& work=find(capacity.projects,row.project_id);
        result.licenses.push_back({row.offer_id,advance_days(*work.stopped_on,Days{1})});
    }
    for (const auto& model : state.models)
        result.models.push_back({model.id, model.recipe_id, model.parent_id, model.consumed_work,
            !model.project_id.empty() && (find(capacity.projects, model.project_id).status == ProjectStatus::active || find(capacity.projects,model.project_id).status==ProjectStatus::paused),
            !model.project_id.empty() && find(capacity.projects,model.project_id).status==ProjectStatus::paused});
    for (const auto& evidence : state.evaluations) {
        const auto& plan = find(state.catalog.evaluations, evidence.plan_id);
        result.evidence.push_back({evidence.id, evidence.model_id, evidence.plan_id, evidence.completed,
            evidence.measured, evidence.lower, evidence.upper, plan.coverage, evidence.findings, plan.independent});
    }
    return result;
}
} // namespace pgp::core
