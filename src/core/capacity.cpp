#include <pgp/core/capacity.hpp>

#include <algorithm>
#include <limits>
#include <iterator>
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
    return row.kind == ProjectKind::serving || row.kind==ProjectKind::replication ? row.requested.value() :
        std::min(row.requested.value(), checked_subtract(row.total_work.value(), row.completed_work.value()));
}
bool finite_goal_reached(const CapacityProject& row) {
    return row.completed_work==row.total_work && row.completed_staff_days==row.required_staff_days;
}
std::uint32_t research_staff_credit(const CapacityProject& row,std::uint64_t staffed_days) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(row.required_staff_days,staffed_days*row.staff));
}
bool in_interval(const std::vector<ProjectPause>& intervals,OrdinalDay day) {
    const auto it=std::upper_bound(intervals.begin(),intervals.end(),day,[](OrdinalDay d,const ProjectPause& p){return d<p.starts_on;});
    if(it==intervals.begin()) return false;
    const auto& pause=*std::prev(it);
    return !pause.ends_before || day<*pause.ends_before;
}
bool paused_on(const CapacityProject& row,OrdinalDay day) { return in_interval(row.pauses,day); }
bool pausable(ProjectKind kind) {
    return kind==ProjectKind::training || kind==ProjectKind::research || kind==ProjectKind::evaluation ||
        kind==ProjectKind::data_generation || kind==ProjectKind::incident_response || kind==ProjectKind::data_acquisition || kind==ProjectKind::software;
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
MilliCUPerDay company_capacity(const CapacitySource& source) {
    return checked_unit_subtract(source.effective, std::min(source.effective,
        checked_unit_add(source.wholesale_reserved,source.replica_reserved)));
}
CapacityReport capacity_report(const CapacityState& state) {
    CapacityReport report;
    report.available_staff = state.available_staff;
    for (const auto& row : state.sources) {
        report.nominal = checked_unit_add(report.nominal, row.nominal);
        report.effective = checked_unit_add(report.effective, company_capacity(row));
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
bool project_disrupted(const CapacityProject& row,OrdinalDay day) { return in_interval(row.disruptions,day); }
BasisPoints staff_output_factor(const CapacityState& state) {
    return state.workload?staff_output_factor(state.workload->rules,state.workload->strain):BasisPoints{10000};
}
void validate_capacity(const CapacityState& state) {
    if(state.workload) validate_staff_workload(state.workload->rules);
    BasisPoints strain{0};
    text_valid(state.owner); (void)from_ordinal(state.starts_on);
    if (state.staff_additions.size()>capacity_staff_change_limit || state.staff_departures.size()>capacity_staff_change_limit) invalid("Staff change history exceeds its bound");
    std::uint64_t added=0;std::set<std::string> staff_ids;
    const auto staff_today=state.closed_through?advance_days(*state.closed_through,Days{1}):state.starts_on;
    for (const auto& row:state.staff_additions) {
        if (row.people==0 || row.people>1'000'000 || row.id.empty() || !staff_ids.insert(row.id).second || row.day<state.starts_on || row.day>staff_today ||
            row.evaluation_credit.value()>2500 || (row.evaluation_credit.value()>0 && row.people!=1))
            invalid("Staff additions require distinct dated hiring commitments");
        added+=row.people;
    }
    std::uint64_t removed=0;
    for (const auto& row:state.staff_departures) {
        if (row.people==0 || row.people>1'000'000 || row.id.empty() || !staff_ids.insert(row.id).second || row.day<state.starts_on || row.day>staff_today ||
            row.evaluation_credit.value()!=0)
            invalid("Staff departures require distinct dated commitments");
        removed+=row.people;(void)available_staff_on(state,row.day);
    }
    if (added>state.available_staff+removed || state.available_staff+removed-added>1'000'000) invalid("Staff changes do not reconcile to a bounded opening team");
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
            (row.kind != CapacitySourceKind::leased && row.kind != CapacitySourceKind::owned && row.kind != CapacitySourceKind::imported) ||
            (row.kind == CapacitySourceKind::owned && row.provider != row.owner) ||
            (row.kind != CapacitySourceKind::owned && row.provider == row.owner) ||
            checked_unit_add(row.wholesale_reserved,row.replica_reserved) > row.nominal ||
            (row.kind!=CapacitySourceKind::owned && row.replica_reserved.value()!=0) ||
            (row.kind != CapacitySourceKind::owned && row.wholesale_reserved.value() != 0) ||
            (row.available_from && (*row.available_from < state.starts_on || *row.available_from > latest)))
            invalid("Capacity source has duplicate ownership, impossible output or an invalid provider");
    }
    std::unordered_map<std::string_view, const CapacityProject*> projects;
    std::unordered_map<std::string_view, std::int64_t> source_reservations;
    projects.reserve(state.projects.size());
    source_reservations.reserve(state.sources.size());
    struct SpecialistUse {std::string_view specialist,id; OrdinalDay start,end;};
    std::vector<SpecialistUse> specialist_use;
    for (const auto& row : state.projects) {
        text_valid(row.id, 100); text_valid(row.interconnect); text_valid(row.cause, 256);
        if (!row.specialist_id.empty()) {
            if (row.kind!=ProjectKind::evaluation) invalid("Evaluation expertise can only be assigned to evaluation work");
            (void)evaluation_work(state,row.starts_on,row.total_work,row.specialist_id);
        }
        (void)from_ordinal(row.starts_on);
        if (row.kind==ProjectKind::software && row.source_id.empty()) {
            if(!row.source_id.empty() || row.requested.value()!=0 || row.total_work.value()!=0 || row.completed_work.value()!=0 || !row.required_staff_days || row.customer_inference)
                invalid("Software requires staff-days and local computers, without reserved model compute");
        } else if (row.customer_inference) {
            if (row.kind != ProjectKind::serving || !row.source_id.empty() || row.requested.value() != 0 ||
                row.completed_work.value() != 0)
                invalid("Customer inference uses a serving team and no company compute");
        } else {
            text_valid(row.source_id, 100);
            const auto& supply = source(state, row.source_id);
            if (row.requested.value() <= 0 || (supply.available_from && row.starts_on < *supply.available_from) ||
                row.minimum_memory_mib > supply.memory_mib || row.interconnect != supply.interconnect)
                invalid("Project demand or source compatibility is invalid");
        }
        if (!projects.emplace(row.id, &row).second || row.starts_on < state.starts_on || row.starts_on > latest ||
            row.staff == 0 || row.staff>1'000'000 || (row.staff > available_staff_on(state,row.starts_on) &&
                !(row.status==ProjectStatus::cancelled && row.stopped_on==row.starts_on && row.completed_work.value()==0)) ||
            row.monthly_support_cost.value() < 0)
            invalid("Project identity, date, staff or support cost is invalid");
        switch (row.kind) {
        case ProjectKind::serving:
            if (row.tasks_per_day == 0 || row.inference_milli_CU_per_task.value() <= 0 || row.total_work.value() != 0 ||
                (!row.customer_inference &&
                    (required_inference(row.tasks_per_day, row.inference_milli_CU_per_task).value() != row.requested.value() ||
                     required_inference(row.useful_tasks, row.inference_milli_CU_per_task) > row.completed_work)) ||
                row.status == ProjectStatus::completed)
                invalid("Serving project has an invalid checked useful-task profile");
            break;
        case ProjectKind::training: case ProjectKind::research: case ProjectKind::evaluation: case ProjectKind::data_generation: case ProjectKind::incident_response: case ProjectKind::data_acquisition: case ProjectKind::management:
            if (row.total_work.value() <= 0 || row.completed_work > row.total_work || row.tasks_per_day != 0 ||
                row.inference_milli_CU_per_task.value() != 0 || row.useful_tasks != 0 || row.monthly_support_cost.value() != 0)
                invalid("Finite project has invalid work or serving-only fields");
            break;
        case ProjectKind::software:
            if(!row.required_staff_days || row.total_work.value()<0 || row.completed_work>row.total_work || (!row.source_id.empty() && row.total_work.value()==0))invalid("Software work requires actual staff-days and any reserved AI compute");
            if(row.tasks_per_day || row.inference_milli_CU_per_task.value() || row.useful_tasks || row.monthly_support_cost.value()) invalid("Software cannot invent inference work");
            break;
        case ProjectKind::replication:
            if (row.total_work.value()!=0 || row.tasks_per_day!=0 || row.inference_milli_CU_per_task.value()!=0 ||
                row.useful_tasks!=0 || row.status==ProjectStatus::completed || row.customer_inference)
                invalid("Replication reserves compute and staff without useful-task credit");
            break;
        default: invalid("Unknown project kind");
        }
        if(row.pauses.size()>128 || (!row.pauses.empty() && !pausable(row.kind)))
            invalid("Only finite lab work supports bounded pause history");
        for(std::size_t i=0;i<row.pauses.size();++i) {
            const auto& pause=row.pauses[i];
            if(pause.starts_on<row.starts_on || pause.starts_on>latest || (row.stopped_on && pause.starts_on>*row.stopped_on) ||
                (pause.ends_before && (*pause.ends_before<pause.starts_on || *pause.ends_before>latest || (row.stopped_on && *pause.ends_before>*row.stopped_on))) ||
                (i && (!row.pauses[i-1].ends_before || pause.starts_on<*row.pauses[i-1].ends_before)) ||
                (!pause.ends_before && i+1!=row.pauses.size())) invalid("Project pause history overlaps or has an invalid date");
        }
        if(row.disruptions.size()>8192 || (!row.disruptions.empty() && row.kind!=ProjectKind::serving))
            invalid("Only serving projects may carry incident containment");
        for(std::size_t i=0;i<row.disruptions.size();++i) {
            const auto& interval=row.disruptions[i];
            if(interval.starts_on<row.starts_on || interval.starts_on>latest ||
                (interval.ends_before && (*interval.ends_before<=interval.starts_on || *interval.ends_before>latest)) ||
                (i && (!row.disruptions[i-1].ends_before || interval.starts_on<=*row.disruptions[i-1].ends_before)))
                invalid("Invalid or overlapping deployment containment intervals");
        }
        if((row.required_staff_days>0 && row.kind!=ProjectKind::research && row.kind!=ProjectKind::software) || row.required_staff_days>1'000'000 ||
            row.completed_staff_days>row.required_staff_days)
            invalid("Only research supports a bounded independent staff-day budget");
        const bool open_pause=!row.pauses.empty() && !row.pauses.back().ends_before;
        if(open_pause!=(row.status==ProjectStatus::paused)) invalid("Project status differs from its open pause interval");
        switch (row.status) {
        case ProjectStatus::active:
            if (row.stopped_on || (row.kind != ProjectKind::serving && row.kind!=ProjectKind::replication && finite_goal_reached(row)))
                invalid("Active project has a stop cursor or already completed its goal");
            if (!row.customer_inference && !row.source_id.empty())
                source_reservations[row.source_id] = checked_add(source_reservations[row.source_id], demand(row));
            break;
        case ProjectStatus::paused:
            if(row.stopped_on || finite_goal_reached(row)) invalid("Paused finite work has a terminal cursor or finished goal");
            break;
        case ProjectStatus::completed:
            if (!row.stopped_on || row.kind == ProjectKind::serving || row.kind==ProjectKind::replication || !finite_goal_reached(row))
                invalid("Completed project has no exact finite completion");
            break;
        case ProjectStatus::cancelled:
            if (!row.stopped_on) invalid("Cancelled project has no cancellation cursor");
            break;
        default: invalid("Unknown project status");
        }
        if (row.stopped_on && (*row.stopped_on < row.starts_on || *row.stopped_on > latest))
            invalid("Project stop date is outside its valid lifetime");
        if (!row.specialist_id.empty()) {
            const auto end=row.stopped_on ?
                advance_days(*row.stopped_on,Days{row.status==ProjectStatus::completed?1:0}) :
                advance_days(latest,Days{1});
            if (end>row.starts_on) specialist_use.push_back({row.specialist_id,row.id,row.starts_on,end});
        }
    }
    std::ranges::sort(specialist_use,[](const auto& a,const auto& b) {
        return std::tie(a.specialist,a.start,a.id)<std::tie(b.specialist,b.start,b.id);
    });
    for (std::size_t i=1;i<specialist_use.size();++i)
        if (specialist_use[i].specialist==specialist_use[i-1].specialist && specialist_use[i].start<specialist_use[i-1].end)
            invalid("A specialist is already committed to an unfinished evaluation, including paused work");
    for (const auto& [id, reserved] : source_reservations) {
        const auto& supply = source(state, id);
        if (reserved > checked_unit_subtract(checked_unit_subtract(supply.nominal,supply.wholesale_reserved),supply.replica_reserved).value())
            invalid("Project and wholesale commitments oversubscribe nominal source capacity");
    }
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
            day.sources.size() != static_cast<std::size_t>(std::count_if(state.sources.begin(),state.sources.end(),[&](const auto& supply) {
                return !supply.available_from || *supply.available_from <= day.day;
            })) || day.projects.size() > state.projects.size())
            invalid("Daily capacity history is incomplete or out of order");
        work_records += day.projects.size();
        if (work_records > capacity_work_record_limit) invalid("Capacity work history exceeds its deterministic bound");
        source_use.clear();
        std::string_view previous;
        std::uint64_t staff_used = 0;
        const auto productivity=state.workload?staff_output_factor(state.workload->rules,strain):BasisPoints{10000};
        for (const auto& used : day.projects) {
            if (!previous.empty() && used.project_id <= previous) invalid("Daily project records must have unique sorted identities");
            previous = used.project_id;
            const auto found = projects.find(used.project_id);
            if (found == projects.end()) invalid("Daily work refers to an unknown project");
            const auto& row = *found->second;
            auto& total = totals[row.id];
            if (day.day < row.starts_on || (row.stopped_on && (day.day > *row.stopped_on ||
                (row.status == ProjectStatus::cancelled && day.day == *row.stopped_on))) ||
                paused_on(row,day.day) ||
                (project_disrupted(row,day.day) && (used.work.value()!=0 || used.useful_tasks!=0 || used.failed_tasks!=0)) ||
                used.assigned_staff != row.staff || used.work.value()>row.requested.value() ||
                (row.kind!=ProjectKind::serving && static_cast<std::uint64_t>(used.work.value())>
                    staffed_output(static_cast<std::uint64_t>(row.requested.value()),productivity)))
                invalid("Daily work occurs outside its project lifetime, rate or staff assignment");
            total.last_day = day.day; ++total.days;
            total.work = checked_unit_add(total.work, used.work);
            if (used.useful_tasks > std::numeric_limits<std::uint64_t>::max() - total.tasks)
                throw std::overflow_error("Project useful task history overflow");
            total.tasks += used.useful_tasks;
            if (row.kind == ProjectKind::serving) {
                const auto staffed_tasks=staffed_output(row.tasks_per_day,productivity);
                if (used.useful_tasks > staffed_tasks || used.protected_tasks > used.useful_tasks ||
                    used.failed_tasks>staffed_tasks-used.useful_tasks || (row.customer_inference && used.failed_tasks>0) ||
                    (!row.customer_inference && required_inference(used.useful_tasks+used.failed_tasks, row.inference_milli_CU_per_task) != used.work))
                    invalid("Serving history credits work without its exact inference cost");
            } else {
                if (used.useful_tasks != 0 || used.protected_tasks != 0 || used.failed_tasks!=0 ||
                    (row.kind!=ProjectKind::replication && total.work > row.total_work))
                    invalid("Finite work history exceeds its goal or invents useful serving tasks");
                if (row.kind!=ProjectKind::replication && total.work == row.total_work &&
                    research_staff_credit(row,total.days)==row.required_staff_days && (row.status != ProjectStatus::completed || !row.stopped_on || *row.stopped_on != day.day))
                    invalid("Finite work must complete on the exact day its goal is reached");
            }
            if (!row.customer_inference && !row.source_id.empty()) {
                auto [supply, fresh] = source_use.try_emplace(row.source_id, MilliCU{0});
                (void)fresh;
                supply->second = checked_unit_add(supply->second, used.work);
            }
            staff_used += used.assigned_staff;
        }
        const auto available=available_staff_on(state,day.day);
        if(staff_used>available) invalid("Daily assignments oversubscribe staff available on that day");
        if(state.workload) strain=next_staff_strain(state.workload->rules,strain,staff_load(staff_used,available));
        previous = {};
        for (const auto& used : day.sources) {
            if (!previous.empty() && used.source_id <= previous) invalid("Daily source records must have unique sorted identities");
            previous = used.source_id;
            const auto& supply = source(state, used.source_id);
            const auto recorded = source_use.find(used.source_id);
            const auto actual = recorded == source_use.end() ? MilliCU{0} : recorded->second;
            if ((supply.available_from && *supply.available_from > day.day) ||
                used.effective > supply.nominal || checked_unit_add(used.wholesale_available,used.replica_available) > used.effective ||
                used.used.value() > checked_unit_subtract(checked_unit_subtract(used.effective,used.wholesale_available),used.replica_available).value() ||
                used.wholesale_used.value() > used.wholesale_available.value() || actual != used.used ||
                (supply.kind != CapacitySourceKind::owned && (used.wholesale_available.value()!=0 || used.replica_available.value()!=0)))
                invalid("Daily work exceeds effective capacity or differs from source consumption");
        }
    }
    if(state.workload && state.workload->strain!=strain)
        invalid("Saved staff strain differs from settled assignments and recovery");
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
        auto expected_days = through && *through >= row.starts_on ?
            static_cast<std::uint64_t>(through->value() - row.starts_on.value() + 1) : 0;
        if(through) for(const auto& pause:row.pauses) {
            const auto end=std::min(pause.ends_before.value_or(advance_days(*through,Days{1})),advance_days(*through,Days{1}));
            if(end>pause.starts_on) expected_days-=static_cast<std::uint64_t>(end.value()-pause.starts_on.value());
        }
        if (total.work != row.completed_work || total.tasks != row.useful_tasks || total.days != expected_days ||
            research_staff_credit(row,total.days)!=row.completed_staff_days ||
            (row.status == ProjectStatus::completed && (!total.last_day || *total.last_day != *row.stopped_on)))
            invalid("Project totals or lifecycle cursor differ from complete committed work history");
    }
}
std::uint32_t available_staff_on(const CapacityState& state,OrdinalDay day) {
    std::uint64_t unavailable=0,available=state.available_staff;
    for (const auto& row:state.staff_additions) if (row.day>day) unavailable+=row.people;
    for (const auto& row:state.staff_departures) if (row.day>day) available+=row.people;
    if (unavailable>available || available-unavailable>1'000'000) invalid("Staff history exceeds the actual team");
    return static_cast<std::uint32_t>(available-unavailable);
}
std::uint32_t opening_staff_count(const CapacityState& state) {
    std::uint64_t result=state.available_staff,added=0;
    for (const auto& row:state.staff_departures) result+=row.people;
    for (const auto& row:state.staff_additions) added+=row.people;
    if (added>result || result-added>1'000'000) invalid("Staff changes do not reconcile to the opening team");
    return static_cast<std::uint32_t>(result-added);
}
void remove_staff_capacity(CapacityState& state,OrdinalDay day,std::string id,std::uint32_t people) {
    if (day!=(state.closed_through?advance_days(*state.closed_through,Days{1}):state.starts_on) || people==0 || people>state.available_staff || state.staff_departures.size()>=capacity_staff_change_limit ||
        capacity_report(state).assigned_staff>state.available_staff-people ||
        std::any_of(state.staff_departures.begin(),state.staff_departures.end(),[&](const auto& row){return row.id==id;}) ||
        std::any_of(state.staff_additions.begin(),state.staff_additions.end(),[&](const auto& row){return row.id==id;}))
        invalid("Staff departure requires unassigned current staff and a unique dated commitment");
    state.available_staff-=people;state.staff_departures.push_back({std::move(id),day,people});
}
void add_staff_capacity(CapacityState& state,OrdinalDay day,std::string id,std::uint32_t people,BasisPoints evaluation_credit) {
    if (day!=(state.closed_through?advance_days(*state.closed_through,Days{1}):state.starts_on) || people==0 || people>1'000'000 ||
        state.available_staff>1'000'000-people || state.staff_additions.size()>=capacity_staff_change_limit ||
        evaluation_credit.value()>2500 || (evaluation_credit.value()>0 && people!=1) ||
        std::any_of(state.staff_additions.begin(),state.staff_additions.end(),[&](const auto& row){return row.id==id;}) ||
        std::any_of(state.staff_departures.begin(),state.staff_departures.end(),[&](const auto& row){return row.id==id;}))
        invalid("Staff addition needs a unique current-day commitment within the team bound");
    state.available_staff+=people;state.staff_additions.push_back({std::move(id),day,people,evaluation_credit});
}
MilliCU evaluation_work(const CapacityState& state,OrdinalDay day,MilliCU work,std::string_view specialist_id) {
    if (specialist_id.empty()) return work;
    const auto hire=std::ranges::find(state.staff_additions,specialist_id,&StaffAddition::id);
    if (hire==state.staff_additions.end() || hire->day>day || hire->evaluation_credit.value()==0 ||
        hire->evaluation_credit.value()>2500 || hire->people!=1)
        invalid("Choose an onboarded evaluation specialist for this experiment");
    const auto result=mul_div_floor(static_cast<std::uint64_t>(work.value()),
        static_cast<std::uint64_t>(10'000-hire->evaluation_credit.value()),10'000);
    return MilliCU{checked_add(static_cast<std::int64_t>(result.quotient),result.remainder>0?1:0)};
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
            row.completed_work.value() != 0 || row.completed_staff_days != 0 || row.useful_tasks != 0 || !row.pauses.empty() || !row.disruptions.empty())
            invalid("A new project must start today without invented work or prior lifecycle state");
        const auto source_id = row.source_id;
        candidate.projects.push_back(std::move(row));
        std::int64_t reserved = 0;
        for (const auto& project_row : candidate.projects)
            if (project_row.status == ProjectStatus::active && project_row.source_id == source_id)
                reserved = checked_add(reserved, demand(project_row));
        if (!source_id.empty() && reserved > company_capacity(source(candidate, source_id)).value())
            invalid("A new project cannot exceed currently effective source capacity");
        std::sort(candidate.projects.begin(), candidate.projects.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    });
}
void add_capacity_source(CapacityState& state, OrdinalDay day, CapacitySource row) {
    mutate(state,[&](auto& candidate) {
        require_day(candidate,day);
        if (row.available_from != day) invalid("A new source must record its actual first availability day");
        candidate.sources.push_back(std::move(row));
        std::sort(candidate.sources.begin(),candidate.sources.end(),[](const auto& a,const auto& b){return a.id < b.id;});
    });
}
void cancel_project(CapacityState& state, OrdinalDay day, std::string_view project_id) {
    mutate(state, [&](auto& candidate) {
        require_day(candidate, day);
        auto& row = project(candidate, project_id);
        if (row.status != ProjectStatus::active && row.status!=ProjectStatus::paused) invalid("Only unfinished work can be cancelled");
        if(row.status==ProjectStatus::paused) row.pauses.back().ends_before=day;
        row.status = ProjectStatus::cancelled; row.stopped_on = day;
    });
}
void pause_project(CapacityState& state,OrdinalDay day,std::string_view id) {
    mutate(state,[&](auto& candidate) {
        require_day(candidate,day);auto& row=project(candidate,id);
        if(row.status!=ProjectStatus::active || !pausable(row.kind) || row.pauses.size()>=128)
            invalid("Pause requires active finite lab work within its pause-history limit");
        row.pauses.push_back({day,{}});row.status=ProjectStatus::paused;
    });
}
void resume_project(CapacityState& state,OrdinalDay day,std::string_view id) {
    mutate(state,[&](auto& candidate) {
        require_day(candidate,day);auto& row=project(candidate,id);
        if(row.status!=ProjectStatus::paused || row.pauses.empty() || row.pauses.back().ends_before) invalid("Resume requires a currently paused lab project");
        row.status=ProjectStatus::active;row.pauses.back().ends_before=day;
        std::int64_t reserved=0;
        for(const auto& other:candidate.projects) if(other.status==ProjectStatus::active && other.source_id==row.source_id) reserved=checked_add(reserved,demand(other));
        if(!row.source_id.empty() && reserved>company_capacity(source(candidate,row.source_id)).value()) invalid("Resume needs the original compute allocation to be available again");
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
    const auto productivity=staff_output_factor(state);
    for (const auto& request : serving) {
        const auto found = std::find_if(state.projects.begin(), state.projects.end(),
            [&](const auto& row) { return row.id == request.project_id; });
        if (found == state.projects.end() || found->kind != ProjectKind::serving || found->status != ProjectStatus::active ||
            (project_disrupted(*found,day) && (request.protected_tasks>0 || request.additional_tasks>0)) ||
            request.protected_tasks > found->tasks_per_day ||
            request.additional_tasks > found->tasks_per_day - request.protected_tasks ||
            request.failed_tasks>request.additional_tasks || (found->customer_inference && request.failed_tasks>0) ||
            requests.contains(request.project_id))
            invalid("Daily serving request has an invalid project, task count or duplicate identity");
        const auto cap=staffed_output(found->tasks_per_day,productivity);
        auto staffed=request;
        staffed.protected_tasks=std::min(staffed.protected_tasks,cap);
        staffed.additional_tasks=std::min(staffed.additional_tasks,cap-staffed.protected_tasks);
        if(staffed.failed_tasks>staffed.additional_tasks)
            invalid("Failed attempts exceed the staffed discretionary serving plan");
        requests.emplace(request.project_id,std::move(staffed));
    }
    CapacityDay record; record.day = day;
    std::vector<std::int64_t> allocations(state.projects.size(), 0), protected_work(state.projects.size(), 0);
    for (const auto& supply : state.sources) {
        std::vector<Claim> priority, discretionary, other;
        for (std::size_t i = 0; i < state.projects.size(); ++i) {
            const auto& row = state.projects[i];
            if (row.status != ProjectStatus::active || row.source_id != supply.id) continue;
            if (row.kind != ProjectKind::serving) {
                const auto cap=staffed_output(static_cast<std::uint64_t>(row.requested.value()),productivity);
                other.push_back({i,std::min(demand(row),static_cast<std::int64_t>(cap)),1});
                continue;
            }
            const auto found = requests.find(row.id);
            if (found == requests.end()) continue;
            const auto quantum = static_cast<std::uint64_t>(row.inference_milli_CU_per_task.value());
            priority.push_back({i, required_inference(found->second.protected_tasks, row.inference_milli_CU_per_task).value(), quantum});
            discretionary.push_back({i, required_inference(found->second.additional_tasks, row.inference_milli_CU_per_task).value(), quantum});
        }
        const auto company_output = company_capacity(supply);
        const auto priority_used = allocate(priority, company_output.value(), state);
        const auto extra_used = allocate(discretionary, checked_subtract(company_output.value(), priority_used), state);
        const auto serving_used = checked_add(priority_used, extra_used);
        const auto other_used = allocate(other, checked_subtract(company_output.value(), serving_used), state);
        for (const auto& claim : priority) {
            protected_work[claim.project_index] = claim.assigned;
            allocations[claim.project_index] = claim.assigned;
        }
        for (const auto& claim : discretionary)
            allocations[claim.project_index] = checked_add(allocations[claim.project_index], claim.assigned);
        for (const auto& claim : other) allocations[claim.project_index] = claim.assigned;
        record.sources.push_back({supply.id, supply.effective, MilliCU{checked_add(serving_used, other_used)},
            std::min(supply.effective,supply.wholesale_reserved),MilliCU{0},
            std::min(checked_unit_subtract(supply.effective,std::min(supply.effective,supply.wholesale_reserved)),supply.replica_reserved)});
    }
    for (std::size_t i = 0; i < state.projects.size(); ++i) {
        const auto& row = state.projects[i];
        if (row.status != ProjectStatus::active) continue;
        if (row.customer_inference) {
            const auto request = requests.find(row.id);
            const auto protected_tasks = request == requests.end() ? 0 : request->second.protected_tasks;
            const auto additional_tasks = request == requests.end() ? 0 : request->second.additional_tasks;
            record.projects.push_back({row.id, MilliCU{0}, protected_tasks + additional_tasks, row.staff, protected_tasks});
            continue;
        }
        const bool serves = row.kind == ProjectKind::serving;
        const auto tasks = serves ? maximum_tasks(MilliCU{allocations[i]}, row.inference_milli_CU_per_task) : 0;
        const auto priority = serves ? maximum_tasks(MilliCU{protected_work[i]}, row.inference_milli_CU_per_task) : 0;
        const auto request=serves?requests.find(row.id):requests.end();
        const auto failed=request==requests.end()?0:request->second.failed_tasks;
        if (failed>0 && tasks-priority!=request->second.additional_tasks)
            invalid("Failed actions require their actual planned inference work");
        record.projects.push_back({row.id, MilliCU{allocations[i]}, tasks-failed, row.staff, priority,failed});
    }
    record.sources.sort([](const auto& a, const auto& b) { return a.source_id < b.source_id; });
    record.projects.sort([](const auto& a, const auto& b) { return a.project_id < b.project_id; });
    return record;
}
} // namespace

CapacityDay preview_capacity_day(const CapacityState& state, OrdinalDay day,
                                 const std::vector<ServingRequest>& serving,bool validate_input) {
    if (validate_input) validate_capacity(state);
    return preview_capacity_day_unchecked(state, day, serving);
}
void close_capacity_day(CapacityState& state, OrdinalDay day, const std::vector<ServingRequest>& serving,
                        bool validate_io, std::span<const WholesaleUse> wholesale) {
    const auto apply_day = [&](CapacityState& candidate) {
        if (candidate.days.size() >= capacity_day_limit) invalid("Capacity history reached its supported day bound");
        auto record = preview_capacity_day_unchecked(candidate, day, serving);
        for (const auto& transfer : wholesale) {
            const auto found = std::find_if(record.sources.begin(), record.sources.end(),
                [&](const auto& row) { return row.source_id == transfer.source_id; });
            if (found == record.sources.end() || transfer.work.value() <= 0 || found->wholesale_used.value() != 0 ||
                transfer.work.value() > found->wholesale_available.value())
                invalid("Wholesale work exceeds its reserved physical source");
            auto usage = *found;
            usage.wholesale_used = transfer.work;
            record.sources.replace(static_cast<std::size_t>(found - record.sources.begin()), std::move(usage));
        }
        for (std::size_t i = 0; i < candidate.projects.size(); ++i) {
            auto& row = candidate.projects[i];
            if (row.status != ProjectStatus::active) continue;
            const auto found = std::find_if(record.projects.begin(), record.projects.end(), [&](const auto& used) { return used.project_id == row.id; });
            const auto work = found->work;
            const auto tasks = found->useful_tasks;
            if (tasks > std::numeric_limits<std::uint64_t>::max() - row.useful_tasks)
                throw std::overflow_error("Project useful task total overflow");
            row.completed_work = checked_unit_add(row.completed_work, work); row.useful_tasks += tasks;
            row.completed_staff_days+=std::min(row.required_staff_days-row.completed_staff_days,found->assigned_staff);
            if (row.kind != ProjectKind::serving && row.kind!=ProjectKind::replication && finite_goal_reached(row)) {
                row.status = ProjectStatus::completed; row.stopped_on = day;
            }
        }
        if(candidate.workload) {
            std::uint64_t assigned=0;
            for(const auto& work:record.projects) assigned+=work.assigned_staff;
            auto& health=*candidate.workload;
            health.strain=next_staff_strain(health.rules,health.strain,staff_load(assigned,candidate.available_staff));
        }
        if (!candidate.days.empty()) record.share_unchanged(candidate.days.back());
        candidate.days.push_back(std::move(record)); candidate.closed_through = day;
    };
    if (validate_io) mutate(state, apply_day);
    else apply_day(state);
}
void close_capacity_day(CapacityState& state, OrdinalDay day, bool validate_io, std::span<const WholesaleUse> wholesale) {
    std::vector<ServingRequest> serving;
    for (const auto& row : state.projects)
        if (row.status == ProjectStatus::active && row.kind == ProjectKind::serving && !row.customer_inference && !project_disrupted(row,day))
            serving.push_back({row.id, 0, row.tasks_per_day});
    close_capacity_day(state, day, serving, validate_io, wholesale);
}
} // namespace pgp::core
