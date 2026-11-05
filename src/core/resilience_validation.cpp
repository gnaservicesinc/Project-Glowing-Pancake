#include "resilience_internal.hpp"
#include <map>
#include <set>
namespace pgp::core {
using namespace resilience_detail;
namespace {
struct ExpectedPayment {OrdinalDay day{0};Account debit=Account::expenses,credit=Account::cash;Cents amount{0};};
using Payments=std::map<std::string,ExpectedPayment,std::less<>>;
OrdinalDay next_month(OrdinalDay day) {
    const auto date=from_ordinal(day);
    return to_ordinal({date.year+(date.month==12?1:0),date.month==12?1U:date.month+1,1});
}
void expect(Payments& payments,std::string key,OrdinalDay day,Cents amount,Account debit=Account::expenses,Account credit=Account::cash) {
    if (amount.value()>0 && !payments.emplace(std::move(key),ExpectedPayment{day,debit,credit,amount}).second)
        invalid("A resilience transaction has a repeated payment key");
}
std::optional<OrdinalDay> repair_ready(const SimulationState& s,const SiteIncident& row) {
    if (row.repair_cost.value()==0) return row.repair_due<=s.date?std::optional{row.repair_due}:std::nullopt;
    Cents paid{0};
    for (const auto& posting:s.accounts->postings) if (posting.document_id==repair_invoice(row.id) &&
        posting.debit==Account::payables && posting.credit==Account::cash) {
        paid=checked_unit_add(paid,posting.amount);
        if (paid==row.repair_cost) {
            const auto available=posting.key.starts_with("settle/")?advance_days(posting.day,Days{1}):posting.day;
            const auto ready=std::max(available,row.repair_due);
            return ready<=s.date?std::optional{ready}:std::nullopt;
        }
    }
    return {};
}
const CapacityProject& project_on(const SimulationState& s,std::string_view service,OrdinalDay day) {
    const auto& product=find(s.market->products,service);
    auto project=product.capacity_project_id;
    for (const auto& binding:s.market->capacity_bindings)
        if (binding.product_id==service && binding.starts_on<=day) project=binding.project_id;
    return find(s.capacity->projects,project);
}
}
void validate_resilience(const SimulationState& s,bool deep) {
    if (!s.resilience) {
        if (s.capacity && (std::any_of(s.capacity->sources.begin(),s.capacity->sources.end(),[](const auto& row){return row.replica_reserved.value()!=0;}) ||
            std::any_of(s.capacity->projects.begin(),s.capacity->projects.end(),[](const auto& row){return row.kind==ProjectKind::replication;})))
            invalid("Replication capacity has no owning resilience system");
        return;
    }
    const auto& r=risks(s);
    if (r.protections.size()>resilience_protection_limit || r.policies.size()>resilience_policy_limit ||
        r.samples.size()>resilience_sample_limit || r.incidents.size()>resilience_incident_limit ||
        r.replicas.size()>resilience_replica_limit || r.alerts.size()>resilience_alert_limit ||
        r.open_incidents.size()>r.incidents.size() || r.alert_cursor>r.alerts.size() || r.insurer_cash.value()<0)
        invalid("The bounded site resilience record is full");
    if (!deep) return; // Each daily owner appends checked work. Restore replays retained evidence.
    validate_resilience_catalog(r.catalog);
    Payments payments;
    std::map<std::string,SiteAlert,std::less<>> expected_alerts;
    const auto expect_alert=[&](std::string id,std::string_view site_id,OrdinalDay day,std::string cause,bool required=false) {
        const auto hardware=std::find_if(s.infrastructure->hardware.begin(),s.infrastructure->hardware.end(),[&](const auto& row){
            return row.site_id==site_id && row.commissioned_on && *row.commissioned_on<=day;
        });
        if (hardware==s.infrastructure->hardware.end() || !expected_alerts.emplace(id,SiteAlert{id,hardware->source_id,std::move(cause),day,required,false}).second)
            invalid("A resilience outcome has no unique physical source");
    };
    std::set<std::pair<std::string,SiteProtectionKind>> protected_sites;
    for (std::size_t index=0;index<r.protections.size();++index) {
        const auto& row=r.protections[index];const auto& owned=site(s,row.site_id);const auto& terms=protection_terms(r,row);
        if (row.id!="protection."+std::to_string(index+1) || !protected_sites.emplace(row.site_id,terms.kind).second ||
            row.purchased_on<owned.ordered_on || row.purchased_on>s.date || !before_end(row.purchased_on,owned.closed_on) ||
            row.active_from!=advance_days(row.purchased_on,terms.lead))
            invalid("Site protection bypassed installation, date or unique mechanism");
        const auto& upkeep=find(s.accounts->recurring,"resilience/"+row.id);
        const auto end=owned.closed_on?std::optional{std::max(*owned.closed_on,row.active_from)}:std::nullopt;
        if (upkeep.monthly_amount!=terms.monthly || upkeep.starts_on!=row.active_from || upkeep.ends_before!=end || upkeep.cause!="Site resilience upkeep")
            invalid("Site protection lost its paid upkeep commitment");
        expect(payments,"resilience.install/"+row.id,row.purchased_on,terms.setup);
    }
    std::map<std::string,OrdinalDay,std::less<>> policy_ends;
    auto insurer=r.catalog.insurer_capital;
    for (std::size_t index=0;index<r.policies.size();++index) {
        const auto& row=r.policies[index];const auto& owned=site(s,row.site_id);const auto& terms=find(r.catalog.insurance,row.offer_id);
        const auto earlier=policy_ends.find(row.site_id);
        if (row.id!="policy."+std::to_string(index+1) || row.purchased_on<owned.ordered_on || row.purchased_on>s.date ||
            !before_end(row.purchased_on,owned.closed_on) || (earlier!=policy_ends.end() && earlier->second>row.purchased_on) ||
            row.covers_from!=advance_days(row.purchased_on,terms.waiting) || row.ends_before!=advance_days(row.purchased_on,terms.term))
            invalid("Insurance bypassed its waiting period or overlapping fixed term");
        policy_ends.insert_or_assign(row.site_id,row.ends_before);
        insurer=checked_unit_add(insurer,terms.premium);
        expect(payments,"resilience.premium/"+row.id,row.purchased_on,terms.premium);
    }
    std::set<std::string> incident_ids;
    std::optional<std::pair<OrdinalDay,std::uint64_t>> previous_incident;
    for (const auto& row:r.incidents) {
        if (row.sample_index>=r.samples.size() || !incident_ids.insert(row.id).second || row.starts_on>s.date ||
            !before_end(row.starts_on,site(s,row.site_id).closed_on))
            invalid("An incident has no eligible site or sampled cause");
        const auto order=std::pair{row.starts_on,row.sample_index};
        if (previous_incident && *previous_incident>=order) invalid("Site incidents must keep a stable chronological order");
        previous_incident=order;
        auto expected=incident_terms(s,r.samples[static_cast<std::size_t>(row.sample_index)],static_cast<std::size_t>(row.sample_index));
        expected.repaired_on=row.repaired_on;expected.paid_on=row.paid_on;expected.recovery_reported=row.recovery_reported;
        if (row!=expected || (row.repaired_on && (*row.repaired_on<row.repair_due || *row.repaired_on>s.date)) ||
            (row.paid_on && (row.claim.value()==0 || *row.paid_on!=row.claim_due || *row.paid_on>s.date)) ||
            (row.claim.value()>0 && row.claim_due<=s.date && !row.paid_on))
            invalid("Site damage, mitigation or insurance terms differ from their actual cause");
        if (row.paid_on) {
            insurer=checked_unit_subtract(insurer,row.claim);
            expect(payments,"resilience.claim/"+row.id,*row.paid_on,row.claim,Account::cash,Account::expenses);
            expect_alert("insured/"+row.id,row.site_id,*row.paid_on,
                site(s,row.site_id).name+" received the agreed insurance reimbursement. Physical recovery follows the repair schedule.");
        }
        expect_alert(row.id,row.site_id,row.starts_on,incident_description(s,row),true);
        if (row.repaired_on && recovered_on(row)<=s.date)
            expect_alert("restored/"+row.id,row.site_id,recovered_on(row),site(s,row.site_id).name+" completed physical repairs and site data recovery.");
        if (row.repaired_on!=repair_ready(s,row)) invalid("Physical repairs bypassed their real paid invoice or recovery date");
        if (row.repair_cost.value()>0) {
            const auto& invoice=find(s.accounts->invoices,repair_invoice(row.id));
            if (invoice.kind!=InvoiceKind::payable || invoice.amount!=row.repair_cost || invoice.due!=row.repair_due || invoice.cause!="Site incident repair")
                invalid("Incident repair has no matching loss and payable");
            expect(payments,"invoice/"+repair_invoice(row.id),row.starts_on,row.repair_cost,Account::expenses,Account::payables);
        }
    }
    if (insurer!=r.insurer_cash || insurer_available(s).value()<0) invalid("Insurance receipts, protected funds and payouts do not conserve cash");
    std::vector<std::uint64_t> open;
    for (std::size_t index=0;index<r.incidents.size();++index) {
        const auto& incident=r.incidents[index];
        const bool recovered=incident.repaired_on && recovered_on(incident)<=s.date;
        if (incident.recovery_reported!=recovered) invalid("A site's recovery notice disagrees with its actual recovery boundary");
        if (!recovered || (incident.claim.value()>0 && !incident.paid_on)) open.push_back(index);
    }
    if (r.open_incidents!=open) invalid("The active incident queue lost an unpaid claim or unfinished repair");
    std::size_t cursor=0;
    while (cursor<r.alerts.size() && (!r.alerts[cursor].required || r.alerts[cursor].acknowledged)) ++cursor;
    if (cursor!=r.alert_cursor || (r.prepared_on && r.prepared_on!=s.date)) invalid("The site alert or preparation cursor is stale");
    std::map<std::string,MilliCUPerDay,std::less<>> reserves;
    std::set<std::string> sync_projects;
    std::set<std::string> services;
    for (std::size_t index=0;index<r.replicas.size();++index) {
        const auto& row=r.replicas[index];const auto& terms=r.catalog.replication;
        const auto& sync=find(s.capacity->projects,row.sync_project);
        const auto primary_site=site_for_source(s,row.primary_source);
        const auto backup_site=site_for_source(s,row.backup_source);
        if (row.id!="replica."+std::to_string(index+1) || row.sync_project!="sync/"+row.id ||
            !sync_projects.insert(row.sync_project).second || row.starts_on>s.date ||
            (row.stopped_on && (*row.stopped_on<row.starts_on || *row.stopped_on>s.date)) ||
            (row.ready_on && (*row.ready_on<row.starts_on || *row.ready_on>s.date)) ||
            sync.kind!=ProjectKind::replication || sync.source_id!=row.backup_source || sync.starts_on!=row.starts_on ||
            sync.requested!=terms.synchronization || sync.staff!=terms.staff || sync.monthly_support_cost!=terms.monthly ||
            sync.stopped_on!=row.stopped_on || (sync.status==ProjectStatus::active)!=(!row.stopped_on) ||
            site(s,primary_site).state_id==site(s,backup_site).state_id)
            invalid("Replication lost its distinct physical sites, paid copy workload or lifecycle");
        expect(payments,"resilience.replica/"+row.id,row.starts_on,terms.setup);
        if (!row.stopped_on) {
            const auto& live=project_on(s,row.service_id,s.date);
            if (row.last_deployment==0 || row.last_deployment>s.deployments->records.size() ||
                s.deployments->records[static_cast<std::size_t>(row.last_deployment-1)].project_id!=live.id)
                invalid("A replica's current service bypassed its owned deployment transition");
            const auto standby=live.source_id==row.primary_source?row.backup_source:row.primary_source;
            if (!services.insert(row.service_id).second || live.status!=ProjectStatus::active || live.customer_inference ||
                (live.source_id!=row.primary_source && live.source_id!=row.backup_source) || live.requested!=row.reservation)
                invalid("An active replica lost its single qualified service or full reservation");
            auto& total=reserves.try_emplace(standby,MilliCUPerDay{0}).first->second;
            total=checked_unit_add(total,row.reservation);
        }
        if (row.initial_deployment==0 || row.initial_deployment>row.last_deployment || row.last_deployment>s.deployments->records.size())
            invalid("A replica lost its exact ordered deployment boundaries");
        const auto* deployment=&s.deployments->records[static_cast<std::size_t>(row.initial_deployment-1)];
        const auto& original=find(s.capacity->projects,deployment->project_id);
        const auto& spare=capacity_source(s,row.backup_source);
        if (deployment->service_id!=row.service_id || deployment->deployed_on>row.starts_on || deployment->model_id!=row.model_id || deployment->evidence_id!=row.evidence_id ||
            deployment->human_review!=row.human_review || original.requested!=row.reservation || original.source_id!=row.primary_source ||
            spare.kind!=CapacitySourceKind::owned || original.minimum_memory_mib>spare.memory_mib || original.interconnect!=spare.interconnect)
            invalid("The replicated service changed its original evidence or spare-rack compatibility");
        std::optional<OrdinalDay> ready;
        MilliCU copied{0};
        for (const auto& day:s.capacity->days) {
            const auto work=std::find_if(day.projects.begin(),day.projects.end(),[&](const auto& project){return project.project_id==row.sync_project;});
            if (work==day.projects.end()) continue;
            copied=checked_unit_add(copied,work->work);
            if (!ready && copied>=terms.initial_copy) ready=advance_days(day.day,Days{1});
        }
        if (row.ready_on!=ready) invalid("A geographic replica became ready without completed copy work");
        for (std::size_t d=0;d<s.deployments->records.size();++d) {
            const auto& record=s.deployments->records[d];
            if (record.service_id!=row.service_id || d+1<=row.initial_deployment || d+1>row.last_deployment) continue;
            const auto& work=find(s.capacity->projects,record.project_id);
            if (record.deployed_on<row.starts_on || (row.stopped_on && record.deployed_on>*row.stopped_on) ||
                record.model_id!=row.model_id || record.evidence_id!=row.evidence_id || record.human_review!=row.human_review ||
                (work.source_id!=row.primary_source && work.source_id!=row.backup_source) ||
                (work.source_id==row.backup_source && (!ready || record.deployed_on<*ready)))
                invalid("Replication invented a model, skipped evidence or served before its copy was ready");
            expect_alert("route/"+row.id+"/"+std::to_string(d+1),site_for_source(s,work.source_id),record.deployed_on,
                replica_description(s,row,work.source_id));
        }
    }
    for (const auto& source:s.capacity->sources) {
        const auto expected=reserves.find(source.id);
        if (source.replica_reserved!=(expected==reserves.end()?MilliCUPerDay{0}:expected->second))
            invalid("Standby capacity differs from the single owned replica reservation");
    }
    for (const auto& project:s.capacity->projects) if (project.kind==ProjectKind::replication && !sync_projects.contains(project.id))
        invalid("Replication work has no owning service or paid site");
    std::set<std::string> alerts;
    std::optional<OrdinalDay> alert_day;
    for (const auto& row:r.alerts) {
        if (row.id.empty() || !alerts.insert(row.id).second || row.cause.empty() || row.cause.size()>1024 ||
            row.day>s.date || (alert_day && row.day<*alert_day) || row.source_id.empty() || (row.required && !row.acknowledged && !s.paused))
            invalid("A site outcome lost its bounded cause, source or mandatory pause");
        alert_day=row.day;
        (void)capacity_source(s,row.source_id);
        const auto expected=expected_alerts.find(row.id);
        if (expected==expected_alerts.end()) invalid("A site alert has no real incident or qualified replica transition");
        auto actual=row;actual.acknowledged=false;
        if (actual!=expected->second) invalid("A site notice changed its mandatory status, physical cause or date");
        expected_alerts.erase(expected);
    }
    if (!expected_alerts.empty()) invalid("A site incident or settlement lost its causal explanation");
    std::set<std::pair<OrdinalDay,std::string>> actual_samples,expected_samples;
    std::optional<std::pair<OrdinalDay,std::string>> previous_sample;
    for (const auto& row:r.samples) {
        const auto order=std::pair{row.sampled_on,row.site_id};
        if (row.sampled_on>=s.end_date || row.sampled_on>s.date || from_ordinal(row.sampled_on).day!=1 ||
            (previous_sample && *previous_sample>=order) || !actual_samples.insert(order).second ||
            row!=draw_sample(s,row.site_id,row.sampled_on,row.stream_before))
            invalid("A monthly hazard draw changed its saved season, stream or dated protections");
        previous_sample=order;
    }
    for (const auto& owned:s.infrastructure->sites) {
        if (!owned.commissioned_on) continue;
        auto civil=from_ordinal(*owned.commissioned_on);
        auto month=next_month(to_ordinal({civil.year,civil.month,1}));
        for (;month<=s.date && month<s.end_date && before_end(month,owned.closed_on);month=next_month(month)) {
            const auto live=std::any_of(s.infrastructure->hardware.begin(),s.infrastructure->hardware.end(),[&](const auto& hardware){
                return hardware.site_id==owned.id && hardware.commissioned_on && *hardware.commissioned_on<month && before_end(month,hardware.retired_on);
            });
            if (live) expected_samples.emplace(month,owned.id);
        }
    }
    if (actual_samples!=expected_samples) invalid("A site skipped or repeated its monthly exposure sampling");
    for (std::size_t index=0;index<r.samples.size();++index) {
        const auto& sample=r.samples[index];
        const bool due=sample.outcome!=0 && sample.event_on<=s.date && before_end(sample.event_on,site(s,sample.site_id).closed_on);
        if (due!=incident_ids.contains("incident."+std::to_string(index+1))) invalid("A sampled site outcome was lost or applied early");
    }
    for (const auto& day:s.capacity->days) {
        std::map<std::string,MilliCUPerDay,std::less<>> withheld;
        for (const auto& row:r.replicas) if (row.starts_on<=day.day && before_end(day.day,row.stopped_on)) {
            const auto& project=project_on(s,row.service_id,day.day);
            const auto standby=project.source_id==row.primary_source?row.backup_source:row.primary_source;
            auto& amount=withheld.try_emplace(standby,MilliCUPerDay{0}).first->second;
            amount=checked_unit_add(amount,row.reservation);
        }
        for (const auto& source:day.sources) {
            const auto reserved=withheld.find(source.source_id);
            const auto expected=reserved==withheld.end()?MilliCUPerDay{0}:
                std::min(checked_unit_subtract(source.effective,source.wholesale_available),reserved->second);
            if (source.replica_available!=expected) invalid("Historical standby capacity was reused by other company work");
        }
    }
    for (const auto& posting:s.accounts->postings) if (posting.key.starts_with("resilience.") || posting.key.starts_with("invoice/resilience.")) {
        const auto expected=payments.find(posting.key);
        if (expected==payments.end() || posting.day!=expected->second.day || posting.amount!=expected->second.amount ||
            posting.debit!=expected->second.debit || posting.credit!=expected->second.credit)
            invalid("Site protection, losses, premiums and insurance payments fail ledger reconciliation");
        payments.erase(expected);
    }
    if (!payments.empty()) invalid("A site resilience commitment has no actual financial posting");
}
}
