#include "capacity_json.hpp"
#include "history_blocks.hpp"
#include <pgp/core/deployment_risk.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace pgp::persistence::detail {
namespace {
constexpr std::array<std::string_view,3> source_names{"leased","owned","imported"};
constexpr std::array<std::string_view,8> project_names{"training","research","evaluation","serving","replication","data_generation","incident_response","data_acquisition"};
constexpr std::array<std::string_view,4> status_names{"active","completed","cancelled","paused"};
template<class Enum,std::size_t Size> Json enum_json(Enum value,const std::array<std::string_view,Size>& names) {
    const auto index=static_cast<std::size_t>(value);
    if (index>=names.size()) invalid("capacity/enum","Unsupported enum value");
    return names[index];
}
template<class Enum,std::size_t Size> Enum parse_enum(const Json& value,const std::array<std::string_view,Size>& names) {
    const auto name=text(value,"capacity/enum",32);
    const auto found=std::find(names.begin(),names.end(),name);
    if (found==names.end()) invalid("capacity/enum","Unsupported enum value");
    return static_cast<Enum>(found-names.begin());
}
Json optional_day(const std::optional<core::OrdinalDay>& value) {
    return value?Json(value->value()):Json(nullptr);
}
core::OrdinalDay day(const Json& value) { return core::OrdinalDay{i64(value,"capacity/day")}; }
std::optional<core::OrdinalDay> parse_optional_day(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{day(value)};
}
std::string label(const Json& value) { return text(value,"capacity/text",256); }
core::MilliCU work(const Json& value) { return core::MilliCU{i64(value,"capacity/milli_CU")}; }
core::MilliCUPerDay rate(const Json& value) { return core::MilliCUPerDay{i64(value,"capacity/milli_CU_per_day")}; }
std::uint32_t staff(const Json& value) {
    return static_cast<std::uint32_t>(u64(value,"capacity/staff",std::numeric_limits<std::uint32_t>::max()));
}
}

Json capacity_json(const core::CapacityState& state, bool packed) {
    Json sources=Json::array(),projects=Json::array();
    HistoryRows days{packed};
    for (const auto& source:state.sources) {
        sources.push_back({
        {"id",source.id},{"physical_id",source.physical_id},{"owner",source.owner},{"provider",source.provider},
        {"kind",enum_json(source.kind,source_names)},
        {"nominal_milli_CU_per_day",source.nominal.value()},{"effective_milli_CU_per_day",source.effective.value()},
        {"memory_mib",source.memory_mib},{"interconnect",source.interconnect},{"energy_included",source.energy_included},
            {"available_from",optional_day(source.available_from)}});
        if (source.wholesale_reserved.value() != 0)
            sources.back()["wholesale_reserved_milli_CU_per_day"] = source.wholesale_reserved.value();
        if (source.replica_reserved.value()!=0)
            sources.back()["replica_reserved_milli_CU_per_day"]=source.replica_reserved.value();
    }
    for (const auto& project:state.projects) {
        projects.push_back({
        {"id",project.id},{"source_id",project.source_id},{"kind",enum_json(project.kind,project_names)},
        {"status",enum_json(project.status,status_names)},{"starts_on",project.starts_on.value()},
        {"stopped_on",optional_day(project.stopped_on)},{"requested_milli_CU_per_day",project.requested.value()},
        {"staff",project.staff},{"minimum_memory_mib",project.minimum_memory_mib},{"interconnect",project.interconnect},
        {"total_work_milli_CU",project.total_work.value()},{"completed_work_milli_CU",project.completed_work.value()},
        {"tasks_per_day",project.tasks_per_day},{"inference_milli_CU_per_task",project.inference_milli_CU_per_task.value()},
        {"useful_tasks",project.useful_tasks},{"monthly_support_cost_cents",project.monthly_support_cost.value()},
            {"cause",project.cause}});
        if(!project.pauses.empty()) {
            projects.back()["pauses"]=Json::array();
            for(const auto& pause:project.pauses) projects.back()["pauses"].push_back({{"starts_on",pause.starts_on.value()},{"ends_before",optional_day(pause.ends_before)}});
        }
        if (!project.disruptions.empty()) {
            projects.back()["disruptions"]=Json::array();
            for (const auto& disruption:project.disruptions) projects.back()["disruptions"].push_back({{"starts_on",disruption.starts_on.value()},{"ends_before",optional_day(disruption.ends_before)}});
        }
        if (project.customer_inference) projects.back()["customer_inference"] = true;
    }
    const core::CapacityDay* prior = nullptr;
    for (const auto& closed:state.days) {
        if (packed && prior && prior->sources == closed.sources && prior->projects == closed.projects) {
            days.repeat();
            continue;
        }
        prior = &closed;
        Json source_usage=Json::array(),project_work=Json::array();
        for (const auto& source:closed.sources) {
            source_usage.push_back({{"source_id",source.source_id},{"effective_milli_CU_per_day",source.effective.value()},
                                    {"used_milli_CU",source.used.value()}});
            if (source.wholesale_available.value() != 0 || source.wholesale_used.value() != 0) {
                source_usage.back()["wholesale_available_milli_CU_per_day"] = source.wholesale_available.value();
                source_usage.back()["wholesale_used_milli_CU"] = source.wholesale_used.value();
            }
            if (source.replica_available.value()!=0)
                source_usage.back()["replica_available_milli_CU_per_day"]=source.replica_available.value();
        }
        for (const auto& project:closed.projects) {
            project_work.push_back({{"project_id",project.project_id},{"work_milli_CU",project.work.value()},
                {"useful_tasks",project.useful_tasks},{"assigned_staff",project.assigned_staff},
                {"protected_tasks",project.protected_tasks}});
            if (project.failed_tasks>0) project_work.back()["failed_tasks"]=project.failed_tasks;
        }
        Json daily{{"sources",std::move(source_usage)},{"projects",std::move(project_work)}};
        if (!packed) daily["day"] = closed.day.value();
        days.push(std::move(daily));
    }
    Json staff_additions=Json::array(),staff_departures=Json::array();
    for (const auto& row:state.staff_additions) staff_additions.push_back({{"id",row.id},{"day",row.day.value()},{"people",row.people}});
    for (const auto& row:state.staff_departures) staff_departures.push_back({{"id",row.id},{"day",row.day.value()},{"people",row.people}});
    return {{"staff_additions",std::move(staff_additions)},{"staff_departures",std::move(staff_departures)},{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",optional_day(state.closed_through)},{"available_staff",state.available_staff},
        {"sources",std::move(sources)},{"projects",std::move(projects)},{"days",days.finish()}};
}

core::CapacityState parse_capacity(const Json& value, bool packed) {
    fields(value,"capacity",{"owner","starts_on","closed_through","available_staff","sources","projects","days"},{"staff_additions","staff_departures"});
    core::CapacityState result;
    if (value.contains("staff_departures")) {
        array(value.at("staff_departures"),"capacity/staff_departures",core::capacity_staff_change_limit);
        for (const auto& row:value.at("staff_departures")) {fields(row,"capacity/staff_departure",{"id","day","people"});result.staff_departures.push_back({label(row.at("id")),day(row.at("day")),staff(row.at("people"))});}
    }
    if (value.contains("staff_additions")) {
        array(value.at("staff_additions"),"capacity/staff_additions",core::capacity_staff_change_limit);
        for (const auto& row:value.at("staff_additions")) {fields(row,"capacity/staff_addition",{"id","day","people"});result.staff_additions.push_back({label(row.at("id")),day(row.at("day")),staff(row.at("people"))});}
    }
    result.owner=label(value.at("owner"));
    result.starts_on=day(value.at("starts_on"));
    result.closed_through=parse_optional_day(value.at("closed_through"));
    result.available_staff=staff(value.at("available_staff"));
    const auto& sources=value.at("sources");array(sources,"capacity/sources",core::capacity_source_limit);
    for (const auto& source:sources) {
        fields(source,"capacity/source",{"id","physical_id","owner","provider","kind","nominal_milli_CU_per_day",
            "effective_milli_CU_per_day","memory_mib","interconnect","energy_included"},{"available_from","wholesale_reserved_milli_CU_per_day","replica_reserved_milli_CU_per_day"});
        result.sources.push_back({label(source.at("id")),label(source.at("physical_id")),label(source.at("owner")),
            label(source.at("provider")),parse_enum<core::CapacitySourceKind>(source.at("kind"),source_names),
            rate(source.at("nominal_milli_CU_per_day")),rate(source.at("effective_milli_CU_per_day")),
            u64(source.at("memory_mib"),"capacity/source/memory_mib"),label(source.at("interconnect")),
            boolean(source.at("energy_included"),"capacity/source/energy_included"),
            source.contains("available_from")?parse_optional_day(source.at("available_from")):std::nullopt,
            source.contains("wholesale_reserved_milli_CU_per_day") ? rate(source.at("wholesale_reserved_milli_CU_per_day")) : core::MilliCUPerDay{0},
            source.contains("replica_reserved_milli_CU_per_day") ? rate(source.at("replica_reserved_milli_CU_per_day")) : core::MilliCUPerDay{0}});
    }
    const auto& projects=value.at("projects");array(projects,"capacity/projects",core::capacity_project_limit);
    for (const auto& project:projects) {
        fields(project,"capacity/project",{"id","source_id","kind","status","starts_on","stopped_on","requested_milli_CU_per_day",
            "staff","minimum_memory_mib","interconnect","total_work_milli_CU","completed_work_milli_CU","tasks_per_day",
            "inference_milli_CU_per_task","useful_tasks","monthly_support_cost_cents","cause"},{"customer_inference","pauses","disruptions"});
        result.projects.push_back({label(project.at("id")),text(project.at("source_id"),"capacity/source_id",100,true),
            parse_enum<core::ProjectKind>(project.at("kind"),project_names),
            parse_enum<core::ProjectStatus>(project.at("status"),status_names),day(project.at("starts_on")),
            parse_optional_day(project.at("stopped_on")),rate(project.at("requested_milli_CU_per_day")),staff(project.at("staff")),
            u64(project.at("minimum_memory_mib"),"capacity/project/minimum_memory_mib"),label(project.at("interconnect")),
            work(project.at("total_work_milli_CU")),work(project.at("completed_work_milli_CU")),
            u64(project.at("tasks_per_day"),"capacity/project/tasks_per_day"),work(project.at("inference_milli_CU_per_task")),
            u64(project.at("useful_tasks"),"capacity/project/useful_tasks"),
            core::Cents{i64(project.at("monthly_support_cost_cents"),"capacity/project/monthly_support_cost_cents")},
            label(project.at("cause")), project.contains("customer_inference") &&
                boolean(project.at("customer_inference"),"capacity/customer_inference")});
        if (project.contains("disruptions")) {
            array(project.at("disruptions"),"capacity/project/disruptions",core::deployment_incident_limit);
            for (const auto& row:project.at("disruptions")) {
                fields(row,"capacity/project/disruption",{"starts_on","ends_before"});
                result.projects.back().disruptions.push_back({day(row.at("starts_on")),parse_optional_day(row.at("ends_before"))});
            }
        }
        if(project.contains("pauses")) {
            array(project.at("pauses"),"capacity/project/pauses",128);
            for(const auto& row:project.at("pauses")) {
                fields(row,"capacity/project/pause",{"starts_on","ends_before"});
                result.projects.back().pauses.push_back({day(row.at("starts_on")),parse_optional_day(row.at("ends_before"))});
            }
        }
    }
    std::size_t project_records=0;
    const auto count_work = [&](std::size_t added) {
        if (added > core::capacity_work_record_limit - project_records)
            invalid("capacity/day/projects", "Project work history exceeds its total record bound");
        project_records += added;
    };
    read_history_rows(value.at("days"), "capacity/days", core::capacity_day_limit, packed, [&](const Json& closed, bool repeat) {
        const auto closed_day = packed ? core::advance_days(result.starts_on, core::Days{result.days.size()}) : day(closed.at("day"));
        if (repeat) {
            auto row = result.days.back();
            row.day = closed_day;
            count_work(row.projects.size());
            result.days.push_back(std::move(row));
            return;
        }
        if (packed) fields(closed,"capacity/closed_day",{"sources","projects"});
        else fields(closed,"capacity/closed_day",{"day","sources","projects"});
        core::CapacityDay row;
        row.day = closed_day;
        const auto& source_usage=closed.at("sources");array(source_usage,"capacity/day/sources",core::capacity_source_limit);
        for (const auto& source:source_usage) {
            fields(source,"capacity/source_usage",{"source_id","effective_milli_CU_per_day","used_milli_CU"},
                {"wholesale_available_milli_CU_per_day","wholesale_used_milli_CU","replica_available_milli_CU_per_day"});
            row.sources.push_back({label(source.at("source_id")),rate(source.at("effective_milli_CU_per_day")),work(source.at("used_milli_CU")),
                source.contains("wholesale_available_milli_CU_per_day") ? rate(source.at("wholesale_available_milli_CU_per_day")) : core::MilliCUPerDay{0},
                source.contains("wholesale_used_milli_CU") ? work(source.at("wholesale_used_milli_CU")) : core::MilliCU{0},
                source.contains("replica_available_milli_CU_per_day") ? rate(source.at("replica_available_milli_CU_per_day")) : core::MilliCUPerDay{0}});
        }
        const auto& project_work=closed.at("projects");array(project_work,"capacity/day/projects",core::capacity_project_limit);
        count_work(project_work.size());
        for (const auto& project:project_work) {
            fields(project,"capacity/project_work",{"project_id","work_milli_CU","useful_tasks","assigned_staff"},
                {"protected_tasks","failed_tasks"});
            row.projects.push_back({label(project.at("project_id")),work(project.at("work_milli_CU")),
                u64(project.at("useful_tasks"),"capacity/project_work/useful_tasks"),staff(project.at("assigned_staff")),
                project.contains("protected_tasks")?u64(project.at("protected_tasks"),"capacity/project_work/protected_tasks"):0,
                project.contains("failed_tasks")?u64(project.at("failed_tasks"),"capacity/project_work/failed_tasks"):0});
        }
        if (!result.days.empty()) row.share_unchanged(result.days.back());
        result.days.push_back(std::move(row));
    });
    return result;
}
}
