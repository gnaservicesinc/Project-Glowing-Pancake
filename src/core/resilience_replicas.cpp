#include "resilience_internal.hpp"
namespace pgp::core {
using namespace resilience_detail;
namespace {
const MarketProduct& product(const SimulationState& s,std::string_view id) {
    if (!s.market || !s.deployments || !s.lab) invalid("Replication requires a qualified hosted service");
    return find(s.market->products,id);
}
const CapacityProject& current_project(const SimulationState& s,const ServiceReplica& row) {
    return find(s.capacity->projects,product(s,row.service_id).capacity_project_id);
}
}
namespace resilience_detail {
std::string replica_description(const SimulationState& s,const ServiceReplica& row,std::string_view destination) {
    return "Replicated service "+row.service_id+" switched to its funded rack in "+site(s,site_for_source(s,destination)).state_id+".";
}
void refresh_replica_reservations(SimulationState& s) {
    for (auto& source:s.capacity->sources) source.replica_reserved=MilliCUPerDay{0};
    for (const auto& row:risks(s).replicas) if (!row.stopped_on) {
        const auto& current=current_project(s,row);
        const auto standby=current.source_id==row.primary_source?row.backup_source:row.primary_source;
        for (auto& source:s.capacity->sources) if (source.id==standby) {
            source.replica_reserved=checked_unit_add(source.replica_reserved,row.reservation);
            if (checked_unit_add(source.replica_reserved,source.wholesale_reserved)>source.nominal)
                invalid("Standby replication and wholesale oversold the same physical capacity");
        }
    }
}
}
void replicate_service(SimulationState& s,const ReplicateService& request,Cents protected_cash) {
    if (s.deployments && pending_rollout(*s.deployments,request.service_id))
        invalid("Promote or discard the staged release before adding geographic replication");
    auto& r=risks(s);
    const auto& live=product(s,request.service_id);
    if (live.retirement) invalid("A service with a published retirement date cannot add a geographic replica");
    const auto& primary=find(s.capacity->projects,live.capacity_project_id);
    const auto& spare=capacity_source(s,request.backup_source);
    const auto primary_site=site_for_source(s,primary.source_id);
    const auto backup_site=site_for_source(s,spare.id);
    const auto& terms=r.catalog.replication;
    if (primary.kind!=ProjectKind::serving || primary.status!=ProjectStatus::active || primary.customer_inference ||
        capacity_source(s,primary.source_id).kind!=CapacitySourceKind::owned || spare.kind!=CapacitySourceKind::owned ||
        site(s,primary_site).state_id==site(s,backup_site).state_id || primary.minimum_memory_mib>spare.memory_mib ||
        primary.interconnect!=spare.interconnect || r.replicas.size()>=resilience_replica_limit ||
        std::any_of(r.replicas.begin(),r.replicas.end(),[&](const auto& row){return row.service_id==request.service_id && !row.stopped_on;}) ||
        checked_unit_add(primary.requested,terms.synchronization)>free_capacity(s,spare))
        invalid("Replication needs a compatible spare rack in a different state with capacity for the full service and synchronization");
    const auto original=std::find_if(s.deployments->records.rbegin(),s.deployments->records.rend(),[&](const auto& row){return row.project_id==primary.id;});
    if (original==s.deployments->records.rend()) invalid("Replication requires measured deployment evidence");
    const auto id="replica."+std::to_string(r.replicas.size()+1);
    CapacityProject copy;
    copy.id="sync/"+id;copy.source_id=spare.id;copy.kind=ProjectKind::replication;copy.starts_on=s.date;
    copy.requested=terms.synchronization;copy.staff=terms.staff;copy.minimum_memory_mib=primary.minimum_memory_mib;
    copy.interconnect=primary.interconnect;copy.monthly_support_cost=terms.monthly;copy.cause="Geographic service synchronization";
    spend(s,terms.setup,protected_cash,"resilience.replica/"+id,"Geographic service replication setup");
    // Copy the source strings before the bounded project vector can grow.
    ServiceReplica replica{id,request.service_id,primary.source_id,spare.id,copy.id,original->model_id,
        original->evidence_id,original->human_review,s.date,{},{},primary.requested};
    replica.initial_deployment=static_cast<std::uint64_t>(std::distance(original,s.deployments->records.rend()));
    replica.last_deployment=replica.initial_deployment;
    reserve_project(*s.capacity,s.date,copy);
    add_recurring(*s.accounts,{"support/"+copy.id,terms.monthly,s.date,{},"Serving support: "+copy.id});
    r.replicas.push_back(std::move(replica));refresh_replica_reservations(s);
}
void stop_service_replica(SimulationState& s,std::string_view id) {
    auto& r=risks(s);
    auto row=std::find_if(r.replicas.begin(),r.replicas.end(),[&](const auto& entry){return entry.id==id;});
    if (row==r.replicas.end() || row->stopped_on) invalid("This replica is unavailable or already stopped");
    cancel_project(*s.capacity,s.date,row->sync_project);
    end_recurring(*s.accounts,"support/"+row->sync_project,s.date);
    row->stopped_on=s.date;refresh_replica_reservations(s);
}
void prepare_service_replicas(SimulationState& s) {
    if (!s.resilience) return;
    auto& r=risks(s);
    refresh_replica_reservations(s);
    for (auto& row:r.replicas) if (!row.stopped_on) {
        const auto& copy=find(s.capacity->projects,row.sync_project);
        if (!row.ready_on && copy.completed_work>=r.catalog.replication.initial_copy) row.ready_on=s.date;
        if (!row.ready_on) continue;
        const auto& current=current_project(s,row);
        const auto destination=current.source_id==row.primary_source?row.backup_source:row.primary_source;
        auto& alternate=const_cast<CapacitySource&>(capacity_source(s,destination));
        const auto usable=company_capacity(capacity_source(s,current.source_id));
        if (destination==row.backup_source && usable>=row.reservation) continue;
        alternate.replica_reserved=checked_unit_subtract(alternate.replica_reserved,row.reservation);
        const bool fits=free_capacity(s,alternate)>=row.reservation;
        if (!fits) {alternate.replica_reserved=checked_unit_add(alternate.replica_reserved,row.reservation);continue;}
        deploy_model(*s.deployments,*s.lab,*s.accounts,*s.capacity,*s.market,s.date,
            {row.service_id,row.model_id,row.evidence_id,destination,row.human_review});
        row.last_deployment=s.deployments->records.size();
        refresh_replica_reservations(s);
        alert(s,"route/"+row.id+"/"+std::to_string(s.deployments->records.size()),site_for_source(s,destination),s.date,
            replica_description(s,row,destination));
    }
}
}
