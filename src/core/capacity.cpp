#include <pgp/core/capacity.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* detail) { throw std::invalid_argument(detail); }
void text_valid(std::string_view value, std::size_t maximum = 128) {
    if (value.empty() || value.size() > maximum || std::any_of(value.begin(), value.end(),
        [](unsigned char c) { return c < 32 || c == 127; })) invalid("Capacity text must be nonempty, bounded and contain no control characters");
}
OrdinalDay current_day(const CapacityState& state) {
    return state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
}
void require_day(const CapacityState& state, OrdinalDay day) {
    if (day != current_day(state)) invalid("Capacity mutation must name the current unclosed day");
}
const CapacitySource& source(const CapacityState& state, std::string_view id) {
    const auto found = std::find_if(state.sources.begin(), state.sources.end(),
        [id](const auto& row) { return row.id == id; });
    if (found == state.sources.end()) invalid("Capacity source does not exist");
    return *found;
}
CapacityProject& project(CapacityState& state, std::string_view id) {
    const auto found = std::find_if(state.projects.begin(), state.projects.end(),
        [id](const auto& row) { return row.id == id; });
    if (found == state.projects.end()) invalid("Capacity project does not exist");
    return *found;
}
std::int64_t demand(const CapacityProject& row) {
    return row.kind == ProjectKind::serving ? row.requested.value() :
        std::min(row.requested.value(), checked_subtract(row.total_work.value(), row.completed_work.value()));
}
template<class Operation> void mutate(CapacityState& state, Operation operation) {
    validate_capacity(state);
    auto candidate = state;
    operation(candidate);
    validate_capacity(candidate);
    state = std::move(candidate);
}
struct Claim {
    std::size_t project_index;
    std::int64_t requested;
    std::uint64_t quantum;
    std::int64_t assigned = 0;
    std::uint64_t lost_whole = 0;
    std::uint64_t fraction = 0;
};
std::int64_t allocate(std::vector<Claim>& claims, std::int64_t budget, const CapacityState& state) {
    std::int64_t total = 0;
    for (const auto& claim : claims) total = checked_add(total, claim.requested);
    if (total <= budget) {
        for (auto& claim : claims) claim.assigned = claim.requested;
        return total;
    }
    std::int64_t used = 0;
    for (auto& claim : claims) {
        const auto share = mul_div_floor(static_cast<std::uint64_t>(claim.requested),
            static_cast<std::uint64_t>(budget), static_cast<std::uint64_t>(total));
        const auto assigned = (share.quotient / claim.quantum) * claim.quantum;
        claim.assigned = static_cast<std::int64_t>(assigned);
        claim.lost_whole = share.quotient - assigned;
        claim.fraction = share.remainder;
        used = checked_add(used, claim.assigned);
    }
    std::sort(claims.begin(), claims.end(), [&](const auto& a, const auto& b) {
        if (a.lost_whole != b.lost_whole) return a.lost_whole > b.lost_whole;
        if (a.fraction != b.fraction) return a.fraction > b.fraction;
        return state.projects[a.project_index].id < state.projects[b.project_index].id;
    });
    auto left = checked_subtract(budget, used);
    // Integer work uses largest remainders. Serving uses whole useful tasks;
    // first restore one task in quota-loss order, then place any fragments
    // stranded by a larger task into still-feasible demand in that same order.
    for (auto& claim : claims) {
        if (claim.requested - claim.assigned >= static_cast<std::int64_t>(claim.quantum) &&
            static_cast<std::uint64_t>(left) >= claim.quantum) {
            claim.assigned = checked_add(claim.assigned, static_cast<std::int64_t>(claim.quantum));
            left = checked_subtract(left, static_cast<std::int64_t>(claim.quantum));
        }
    }
    for (auto& claim : claims) {
        const auto extra = std::min(static_cast<std::uint64_t>(claim.requested - claim.assigned),
                                    (static_cast<std::uint64_t>(left) / claim.quantum) * claim.quantum);
        claim.assigned = checked_add(claim.assigned, static_cast<std::int64_t>(extra));
        left = checked_subtract(left, static_cast<std::int64_t>(extra));
    }
    return checked_subtract(budget, left);
}
} // namespace

MilliCU required_inference(std::uint64_t tasks, MilliCU cost_per_task) {
    if (cost_per_task.value() <= 0) invalid("Inference cost per useful task must be positive");
    return MilliCU{mul_div_floor(tasks, static_cast<std::uint64_t>(cost_per_task.value()), 1).quotient};
}
std::uint64_t maximum_tasks(MilliCU available, MilliCU cost_per_task) {
    if (cost_per_task.value() <= 0) invalid("Inference cost per useful task must be positive");
    return static_cast<std::uint64_t>(available.value() / cost_per_task.value());
}
CapacityReport capacity_report(const CapacityState& state) {
    CapacityReport report;
    report.available_staff = state.available_staff;
    for (const auto& row : state.sources) {
        report.nominal = checked_unit_add(report.nominal, row.nominal);
        report.effective = checked_unit_add(report.effective, row.effective);
    }
    for (const auto& row : state.projects) {
        report.completed_work = checked_unit_add(report.completed_work, row.completed_work);
        if (row.useful_tasks > std::numeric_limits<std::uint64_t>::max() - report.useful_tasks)
            throw std::overflow_error("Useful task aggregate overflow");
        report.useful_tasks += row.useful_tasks;
        if (row.status != ProjectStatus::active) continue;
        report.reserved = checked_unit_add(report.reserved, MilliCUPerDay{demand(row)});
        if (row.kind == ProjectKind::serving)
            report.protected_serving = checked_unit_add(report.protected_serving, row.requested);
        if (row.staff > std::numeric_limits<std::uint32_t>::max() - report.assigned_staff)
            throw std::overflow_error("Assigned staff aggregate overflow");
        report.assigned_staff += row.staff;
    }
    return report;
}
void validate_capacity(const CapacityState& state) {
    text_valid(state.owner); (void)from_ordinal(state.starts_on);
    if (state.available_staff > 1000000 || state.sources.size() > capacity_source_limit ||
        state.projects.size() > capacity_project_limit || state.days.size() > capacity_day_limit)
        invalid("Capacity state exceeds its deterministic bounds");
    if (state.closed_through) {
        (void)from_ordinal(*state.closed_through);
        if (*state.closed_through < state.starts_on || state.days.size() !=
            static_cast<std::uint64_t>(state.closed_through->value() - state.starts_on.value() + 1))
            invalid("Capacity close cursor does not match complete daily history");
    } else if (!state.days.empty()) invalid("Capacity history has no close cursor");
    const auto latest = current_day(state);
    std::unordered_set<std::string_view> source_ids, physical_ids;
    source_ids.reserve(state.sources.size());
    physical_ids.reserve(state.sources.size());
    for (const auto& row : state.sources) {
        text_valid(row.id, 100); text_valid(row.physical_id); text_valid(row.owner); text_valid(row.provider); text_valid(row.interconnect);
        if (!source_ids.insert(row.id).second || !physical_ids.insert(row.physical_id).second || row.owner != state.owner ||
            row.nominal.value() <= 0 || row.effective > row.nominal ||
            (row.kind != CapacitySourceKind::leased && row.kind != CapacitySourceKind::owned) ||
            (row.kind == CapacitySourceKind::owned && row.provider != row.owner) ||
            (row.kind == CapacitySourceKind::leased && row.provider == row.owner))
            invalid("Capacity source has duplicate ownership, impossible output or an invalid provider");
    }
    std::unordered_map<std::string_view, const CapacityProject*> projects;
    std::unordered_map<std::string_view, std::int64_t> source_reservations;
    projects.reserve(state.projects.size());
    source_reservations.reserve(state.sources.size());
    for (const auto& row : state.projects) {
        text_valid(row.id, 100); text_valid(row.source_id, 100); text_valid(row.interconnect); text_valid(row.cause, 256);
        (void)from_ordinal(row.starts_on);
        const auto& supply = source(state, row.source_id);
        if (!projects.emplace(row.id, &row).second || row.starts_on < state.starts_on || row.starts_on > latest ||
            row.requested.value() <= 0 || row.staff == 0 || row.staff > state.available_staff ||
            row.minimum_memory_mib > supply.memory_mib || row.interconnect != supply.interconnect || row.monthly_support_cost.value() < 0)
            invalid("Project identity, date, staff or source compatibility is invalid");
        switch (row.kind) {
        case ProjectKind::serving:
            if (row.tasks_per_day == 0 || row.inference_milli_CU_per_task.value() <= 0 || row.total_work.value() != 0 ||
                required_inference(row.tasks_per_day, row.inference_milli_CU_per_task).value() != row.requested.value() ||
                required_inference(row.useful_tasks, row.inference_milli_CU_per_task) != row.completed_work ||
                row.status == ProjectStatus::completed)
                invalid("Serving project has an invalid checked useful-task profile");
            break;
        case ProjectKind::training: case ProjectKind::research: case ProjectKind::evaluation:
            if (row.total_work.value() <= 0 || row.completed_work > row.total_work || row.tasks_per_day != 0 ||
                row.inference_milli_CU_per_task.value() != 0 || row.useful_tasks != 0 || row.monthly_support_cost.value() != 0)
                invalid("Finite project has invalid work or serving-only fields");
            break;
        default: invalid("Unknown project kind");
        }
        switch (row.status) {
        case ProjectStatus::active:
            if (row.stopped_on || (row.kind != ProjectKind::serving && row.completed_work == row.total_work))
                invalid("Active project has a stop cursor or already completed its goal");
            source_reservations[row.source_id] = checked_add(source_reservations[row.source_id], demand(row));
            break;
        case ProjectStatus::completed:
            if (!row.stopped_on || row.kind == ProjectKind::serving || row.completed_work != row.total_work)
                invalid("Completed project has no exact finite completion");
            break;
        case ProjectStatus::cancelled:
            if (!row.stopped_on) invalid("Cancelled project has no cancellation cursor");
            break;
        default: invalid("Unknown project status");
        }
        if (row.stopped_on && (*row.stopped_on < row.starts_on || *row.stopped_on > latest))
            invalid("Project stop date is outside its valid lifetime");
    }
    for (const auto& [id, reserved] : source_reservations)
        if (reserved > source(state, id).nominal.value()) invalid("Project commitments oversubscribe nominal source capacity");
    const auto report = capacity_report(state);
    if (report.assigned_staff > state.available_staff) invalid("Project commitments oversubscribe staff");
    struct Totals { MilliCU work{0}; std::uint64_t tasks = 0; std::size_t days = 0; std::optional<OrdinalDay> last_day; };
    std::unordered_map<std::string_view, Totals> totals;
    totals.reserve(state.projects.size());
    std::unordered_map<std::string_view, MilliCU> source_use;
    source_use.reserve(state.sources.size());
    std::size_t work_records = 0;
    for (std::size_t day_index = 0; day_index < state.days.size(); ++day_index) {
        const auto& day = state.days[day_index];
        if (day.day.value() != checked_add(state.starts_on.value(), static_cast<std::int64_t>(day_index)) ||
            day.sources.size() != state.sources.size() || day.projects.size() > state.projects.size())
            invalid("Daily capacity history is incomplete or out of order");
        work_records += day.projects.size();
        if (work_records > capacity_work_record_limit) invalid("Capacity work history exceeds its deterministic bound");
        source_use.clear();
        std::string_view previous;
        std::uint64_t staff_used = 0;
        for (const auto& used : day.projects) {
            if (!previous.empty() && used.project_id <= previous) invalid("Daily project records must have unique sorted identities");
            previous = used.project_id;
            const auto found = projects.find(used.project_id);
            if (found == projects.end()) invalid("Daily work refers to an unknown project");
            const auto& row = *found->second;
            auto& total = totals[row.id];
            if (day.day < row.starts_on || (row.stopped_on && (day.day > *row.stopped_on ||
                (row.status == ProjectStatus::cancelled && day.day == *row.stopped_on))) ||
                (total.last_day ? advance_days(*total.last_day, Days{1}) != day.day : day.day != row.starts_on) ||
                used.assigned_staff != row.staff || used.work.value() > row.requested.value())
                invalid("Daily work occurs outside its project lifetime, rate or staff assignment");
            total.last_day = day.day; ++total.days;
            total.work = checked_unit_add(total.work, used.work);
            if (used.useful_tasks > std::numeric_limits<std::uint64_t>::max() - total.tasks)
                throw std::overflow_error("Project useful task history overflow");
            total.tasks += used.useful_tasks;
            if (row.kind == ProjectKind::serving) {
                if (used.useful_tasks > row.tasks_per_day || used.protected_tasks > used.useful_tasks ||
                    required_inference(used.useful_tasks, row.inference_milli_CU_per_task) != used.work)
                    invalid("Serving history credits work without its exact inference cost");
            } else {
                if (used.useful_tasks != 0 || used.protected_tasks != 0 || total.work > row.total_work)
                    invalid("Finite work history exceeds its goal or invents useful serving tasks");
                if (total.work == row.total_work && (row.status != ProjectStatus::completed || !row.stopped_on || *row.stopped_on != day.day))
                    invalid("Finite work must complete on the exact day its goal is reached");
            }
            auto [supply, fresh] = source_use.try_emplace(row.source_id, MilliCU{0});
            (void)fresh;
            supply->second = checked_unit_add(supply->second, used.work);
            staff_used += used.assigned_staff;
        }
        if (staff_used > state.available_staff) invalid("Daily assignments oversubscribe staff");
        previous = {};
        for (const auto& used : day.sources) {
            if (!previous.empty() && used.source_id <= previous) invalid("Daily source records must have unique sorted identities");
            previous = used.source_id;
            const auto& supply = source(state, used.source_id);
            const auto recorded = source_use.find(used.source_id);
            const auto actual = recorded == source_use.end() ? MilliCU{0} : recorded->second;
            if (used.effective > supply.nominal || used.used.value() > used.effective.value() || actual != used.used)
                invalid("Daily work exceeds effective capacity or differs from source consumption");
        }
    }
    for (const auto& row : state.projects) {
        const auto found = totals.find(row.id);
        const Totals empty;
        const auto& total = found == totals.end() ? empty : found->second;
        auto through = state.closed_through;
        if (row.stopped_on) {
            auto stop = *row.stopped_on;
            if (row.status == ProjectStatus::cancelled) stop = OrdinalDay{checked_subtract(stop.value(), 1)};
            if (!through || stop < *through) through = stop;
        }
        const auto expected_days = through && *through >= row.starts_on ?
            static_cast<std::uint64_t>(through->value() - row.starts_on.value() + 1) : 0;
        if (total.work != row.completed_work || total.tasks != row.useful_tasks || total.days != expected_days ||
            (row.status == ProjectStatus::completed && (!total.last_day || *total.last_day != *row.stopped_on)))
            invalid("Project totals or lifecycle cursor differ from complete committed work history");
    }
}
CapacityState opening_capacity(std::vector<CapacitySource> sources, std::uint32_t staff, OrdinalDay starts_on, std::string owner) {
    CapacityState state;
    state.owner = std::move(owner); state.starts_on = starts_on; state.available_staff = staff;
    state.sources = std::move(sources);
    std::sort(state.sources.begin(), state.sources.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    validate_capacity(state);
    return state;
}
void reserve_project(CapacityState& state, OrdinalDay day, CapacityProject row) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        if (row.starts_on != day || row.status != ProjectStatus::active || row.stopped_on ||
            row.completed_work.value() != 0 || row.useful_tasks != 0)
            invalid("A new project must start today without invented work or prior lifecycle state");
        const auto source_id = row.source_id;
        candidate.projects.push_back(std::move(row));
        std::int64_t reserved = 0;
        for (const auto& project_row : candidate.projects)
            if (project_row.status == ProjectStatus::active && project_row.source_id == source_id)
                reserved = checked_add(reserved, demand(project_row));
        if (reserved > source(candidate, source_id).effective.value())
            invalid("A new project cannot exceed currently effective source capacity");
        std::sort(candidate.projects.begin(), candidate.projects.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void cancel_project(CapacityState& state, OrdinalDay day, std::string_view project_id) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        auto& row = project(candidate, project_id);
        if (row.status != ProjectStatus::active) invalid("Only an active project can be cancelled");
        row.status = ProjectStatus::cancelled; row.stopped_on = day;
    });
}
void update_effective_capacity(CapacityState& state, OrdinalDay day, std::string_view source_id, MilliCUPerDay effective) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        const auto found = std::find_if(candidate.sources.begin(), candidate.sources.end(),
            [source_id](const auto& row) { return row.id == source_id; });
        if (found == candidate.sources.end() || effective > found->nominal) invalid("Effective output must fit an existing source's nominal capacity");
        found->effective = effective;
    });
}
namespace {
CapacityDay preview_capacity_day_unchecked(const CapacityState& state, OrdinalDay day,
                                           const std::vector<ServingRequest>& serving) {
    require_day(state, day);
    if (serving.size() > capacity_project_limit) invalid("Daily serving plan exceeds its record bound");
    std::unordered_map<std::string_view, ServingRequest> requests;
    requests.reserve(serving.size());
    for (const auto& request : serving) {
        const auto found = std::find_if(state.projects.begin(), state.projects.end(),
            [&](const auto& row) { return row.id == request.project_id; });
        if (found == state.projects.end() || found->kind != ProjectKind::serving || found->status != ProjectStatus::active ||
            request.protected_tasks > found->tasks_per_day ||
            request.additional_tasks > found->tasks_per_day - request.protected_tasks ||
            !requests.emplace(request.project_id, request).second)
            invalid("Daily serving request has an invalid project, task count or duplicate identity");
    }
    CapacityDay record; record.day = day;
    std::vector<std::int64_t> allocations(state.projects.size(), 0), protected_work(state.projects.size(), 0);
    for (const auto& supply : state.sources) {
        std::vector<Claim> priority, discretionary, other;
        for (std::size_t i = 0; i < state.projects.size(); ++i) {
            const auto& row = state.projects[i];
            if (row.status != ProjectStatus::active || row.source_id != supply.id) continue;
            if (row.kind != ProjectKind::serving) { other.push_back({i, demand(row), 1}); continue; }
            const auto found = requests.find(row.id);
            if (found == requests.end()) continue;
            const auto quantum = static_cast<std::uint64_t>(row.inference_milli_CU_per_task.value());
            priority.push_back({i, required_inference(found->second.protected_tasks, row.inference_milli_CU_per_task).value(), quantum});
            discretionary.push_back({i, required_inference(found->second.additional_tasks, row.inference_milli_CU_per_task).value(), quantum});
        }
        const auto priority_used = allocate(priority, supply.effective.value(), state);
        const auto extra_used = allocate(discretionary, checked_subtract(supply.effective.value(), priority_used), state);
        const auto serving_used = checked_add(priority_used, extra_used);
        const auto other_used = allocate(other, checked_subtract(supply.effective.value(), serving_used), state);
        for (const auto& claim : priority) {
            protected_work[claim.project_index] = claim.assigned;
            allocations[claim.project_index] = claim.assigned;
        }
        for (const auto& claim : discretionary)
            allocations[claim.project_index] = checked_add(allocations[claim.project_index], claim.assigned);
        for (const auto& claim : other) allocations[claim.project_index] = claim.assigned;
        record.sources.push_back({supply.id, supply.effective, MilliCU{checked_add(serving_used, other_used)}});
    }
    for (std::size_t i = 0; i < state.projects.size(); ++i) {
        const auto& row = state.projects[i];
        if (row.status != ProjectStatus::active) continue;
        const bool serves = row.kind == ProjectKind::serving;
        const auto tasks = serves ? maximum_tasks(MilliCU{allocations[i]}, row.inference_milli_CU_per_task) : 0;
        const auto priority = serves ? maximum_tasks(MilliCU{protected_work[i]}, row.inference_milli_CU_per_task) : 0;
        record.projects.push_back({row.id, MilliCU{allocations[i]}, tasks, row.staff, priority});
    }
    std::sort(record.sources.begin(), record.sources.end(), [](const auto& a, const auto& b) { return a.source_id < b.source_id; });
    std::sort(record.projects.begin(), record.projects.end(), [](const auto& a, const auto& b) { return a.project_id < b.project_id; });
    return record;
}
} // namespace

CapacityDay preview_capacity_day(const CapacityState& state, OrdinalDay day,
                                 const std::vector<ServingRequest>& serving) {
    validate_capacity(state);
    return preview_capacity_day_unchecked(state, day, serving);
}
void close_capacity_day(CapacityState& state, OrdinalDay day, const std::vector<ServingRequest>& serving, bool validate_io) {
    const auto apply_day = [&](CapacityState& candidate) {
        auto record = preview_capacity_day_unchecked(candidate, day, serving);
        for (std::size_t i = 0; i < candidate.projects.size(); ++i) {
            auto& row = candidate.projects[i];
            if (row.status != ProjectStatus::active) continue;
            const auto found = std::find_if(record.projects.begin(), record.projects.end(), [&](const auto& used) { return used.project_id == row.id; });
            const auto work = found->work;
            const auto tasks = found->useful_tasks;
            if (tasks > std::numeric_limits<std::uint64_t>::max() - row.useful_tasks)
                throw std::overflow_error("Project useful task total overflow");
            row.completed_work = checked_unit_add(row.completed_work, work); row.useful_tasks += tasks;
            if (row.kind != ProjectKind::serving && row.completed_work == row.total_work) {
                row.status = ProjectStatus::completed; row.stopped_on = day;
            }
        }
        candidate.days.push_back(std::move(record)); candidate.closed_through = day;
    };
    if (validate_io) mutate(state, apply_day);
    else apply_day(state);
}
void close_capacity_day(CapacityState& state, OrdinalDay day, bool validate_io) {
    std::vector<ServingRequest> serving;
    for (const auto& row : state.projects)
        if (row.status == ProjectStatus::active && row.kind == ProjectKind::serving)
            serving.push_back({row.id, 0, row.tasks_per_day});
    close_capacity_day(state, day, serving, validate_io);
}
} // namespace pgp::core
