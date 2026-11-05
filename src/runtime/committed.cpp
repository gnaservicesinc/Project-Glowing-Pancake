#include "committed.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <tuple>

namespace pgp::runtime::detail {
namespace {
using Sources = std::vector<CommittedSource>;
using content::Scope;
struct Subject { std::string id; std::string cause; };
using Subjects = std::vector<Subject>;

Subjects completed_projects(const WorldState& world, core::OrdinalDay day) {
    Subjects result;
    const auto& capacity = world.simulation.capacity;
    if (capacity && capacity->owner == world.company_id && capacity->closed_through && day <= *capacity->closed_through)
        for (const auto& row : capacity->projects)
            if (row.status == core::ProjectStatus::completed && row.stopped_on == day)
                result.push_back({row.id, row.cause});
    return result;
}
Subjects missed_contracts(const WorldState& world, core::OrdinalDay day) {
    Subjects result;
    const auto& market = world.simulation.market;
    if (market && market->owner == world.company_id && market->closed_through && day <= *market->closed_through)
        for (const auto& row : market->contracts)
            if (row.status == core::ContractStatus::missed && row.stopped_on == day)
                result.push_back({row.id, row.cause});
    return result;
}
struct Provider {
    std::string_view id;
    Scope subject;
    std::string_view result;
    Subjects (*subjects)(const WorldState&, core::OrdinalDay);
};
constexpr Provider providers[]{
    {"capacity.project_completed", Scope::project, "project.completed_milli_cu", completed_projects},
    {"market.contract_missed_start", Scope::contract, "contract.missed_start", missed_contracts},
};
[[noreturn]] void invalid(const char* explanation) { throw std::invalid_argument(explanation); }
const Provider& provider_for(const content::Registry& registry, std::string_view id) {
    const auto found = std::find_if(std::begin(providers), std::end(providers), [id](const auto& provider) { return provider.id == id; });
    const auto* metadata = registry.committed_fact(id);
    if (found == std::end(providers) || !metadata || metadata->owner_scope != Scope::company ||
        metadata->subject_scope != found->subject || metadata->result_fact != found->result)
        invalid("Committed fact has no matching runtime provider");
    return *found;
}
} // namespace

Sources committed_sources(const WorldState& world, const content::Registry& registry, std::string_view fact_id,
                          const ScopeBinding& owner, core::OrdinalDay opening_day) {
    const auto& provider = provider_for(registry, fact_id);
    if (owner.scope != Scope::company || owner.id != world.company_id || opening_day > world.simulation.date)
        invalid("Invalid committed fact owner or opening date");
    if (opening_day <= world.campaign_start) return {};
    const auto settled = core::OrdinalDay{core::checked_subtract(opening_day.value(), 1)};
    auto subjects = provider.subjects(world, settled);
    std::sort(subjects.begin(), subjects.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    const auto& fact = *registry.fact(provider.result);
    Sources result;
    result.reserve(subjects.size());
    for (auto& subject : subjects) {
        ScopeBinding bound{provider.subject, std::move(subject.id)};
        auto value = read_fact(world, fact, bound);
        result.push_back({std::string(fact_id), std::move(bound), settled, fact.unit, std::move(value), std::move(subject.cause)});
    }
    return result;
}

void validate_committed_sources(const CompiledProgram& program, const Checkpoint& checkpoint) {
    const auto& world = checkpoint.world;
    const auto& state = checkpoint.runtime;
    std::map<std::string_view, const content::FlowDefinition*, std::less<>> subscriptions;
    std::set<std::string_view> enabled_sources;
    for (const auto& [id, definition] : program.collection.definitions) {
        const auto* flow = std::get_if<content::FlowDefinition>(&definition.data);
        if (!flow || flow->on_committed.empty()) continue;
        (void)provider_for(program.registry, flow->on_committed);
        if (world.enabled_flows && std::find(world.enabled_flows->begin(), world.enabled_flows->end(), id) == world.enabled_flows->end())
            continue;
        if (!enabled_sources.insert(flow->on_committed).second)
            invalid("Multiple baseline flows subscribe to the same committed fact");
        subscriptions.emplace(id, flow);
    }
    std::map<std::string_view, const LifecycleState*, std::less<>> owners;
    using Key = std::tuple<std::string_view, ScopeBinding, core::OrdinalDay>;
    std::map<Key, Sources> by_opening;
    const auto sources_on = [&](std::string_view fact_id, const ScopeBinding& binding, core::OrdinalDay opening) -> const Sources& {
        const auto [entry, inserted] = by_opening.try_emplace({fact_id, binding, opening});
        if (inserted) entry->second = committed_sources(world, program.registry, fact_id, binding, opening);
        return entry->second;
    };
    for (const auto& row : state.lifecycles) {
        const auto found = subscriptions.find(row.definition_id);
        if (row.committed_cursor.has_value() != (found != subscriptions.end()))
            invalid("Committed fact cursor does not match its subscription");
        if (!row.committed_cursor) continue;
        owners.emplace(row.definition_id, &row);
        if (row.last_checked && *row.last_checked < world.campaign_start)
            invalid("Committed source cursor predates its campaign");
        const auto count = row.last_checked ? sources_on(found->second->on_committed, row.binding, *row.last_checked).size() : 0;
        if (*row.committed_cursor > count || (row.last_started && (!row.last_checked || *row.last_started > *row.last_checked)) ||
            (state.prepared_day && (row.last_checked != state.prepared_day || *row.committed_cursor != count)))
            invalid("Committed source cursor cannot explain the saved opening phase");
    }
    if (state.prepared_day && owners.size() != subscriptions.size())
        invalid("Prepared morning is missing an enabled committed source lifecycle");

    std::set<std::pair<std::string_view, ScopeBinding>> seen;
    std::map<std::string_view, core::OrdinalDay, std::less<>> last_started;
    for (const auto& active : state.activations) {
        const auto found = subscriptions.find(active.definition_id);
        if (active.committed_source.has_value() != (found != subscriptions.end()))
            invalid("Activation has missing or unexpected committed cause");
        if (!active.committed_source) continue;
        const auto& source = *active.committed_source;
        const auto owner = owners.find(active.definition_id);
        if (owner == owners.end() || !owner->second->last_checked || active.started_on > *owner->second->last_checked ||
            !seen.emplace(source.fact_id, source.subject).second)
            invalid("Committed cause has no processed owner or was already consumed");
        const auto& sources = sources_on(found->second->on_committed, active.binding, active.started_on);
        const auto expected = std::lower_bound(sources.begin(), sources.end(), source.subject.id,
            [](const auto& candidate, std::string_view id) { return candidate.subject.id < id; });
        if (expected == sources.end() || *expected != source)
            invalid("Saved committed cause differs from its owning settlement");
        const auto consumed = static_cast<std::size_t>(expected - sources.begin()) + 1;
        const auto cursor = *owner->second->committed_cursor;
        if (active.started_on == owner->second->last_checked &&
            (consumed > cursor || (active.started_on == world.simulation.date &&
                active.status == ActivationStatus::awaiting_choice && consumed != cursor)))
            invalid("Saved committed cause differs from its owning settlement or lies beyond the processed prefix");
        const auto previous = last_started.find(active.definition_id);
        if (previous != last_started.end() && (active.started_on < previous->second ||
            active.started_on.value() - previous->second.value() < found->second->lifecycle.cooldown_days))
            invalid("Committed activation history violates its occurrence cooldown");
        last_started.insert_or_assign(active.definition_id, active.started_on);
    }
    for (const auto& [id, owner] : owners) {
        const auto latest = last_started.find(id);
        if (latest == last_started.end() ? owner->last_started.has_value() : owner->last_started != latest->second)
            invalid("Last committed occurrence differs from retained activations");
    }
}
} // namespace pgp::runtime::detail
