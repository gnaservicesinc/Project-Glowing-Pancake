#include "authority_internal.hpp"
#include <map>
#include <set>

namespace pgp::runtime {
using namespace authority_detail;
namespace {
void require(bool value,const char* message) {if(!value) invalid(message);}
void project_terms(const WorldState& w,std::string_view id,OrdinalDay start,OrdinalDay end,MilliCUPerDay rate,
    std::uint32_t staff,Cents monthly,std::optional<std::string_view> source={}) {
    const auto* p=named(w.simulation.capacity->projects,id);
    require(p && p->kind==ProjectKind::research && p->starts_on==start && p->requested==rate && p->staff==staff &&
        p->monthly_support_cost==monthly && p->total_work==integrate(rate,Days{end.value()-start.value()}) &&
        (!source || p->source_id==*source),"An institutional agreement lost its exact funded work allocation.");
}
}
void validate_authority_world(const WorldState& w) {
    if(!w.authority) {
        if(w.simulation.management && w.simulation.management->governance)
            for(const auto& c:w.simulation.management->governance->disputes) if(c.negotiated_on)
                invalid("Executive negotiation requires its owning institutional transaction.");
        return;
    }
    const auto& a=*w.authority;const auto day=w.simulation.date;
    validate_authority_catalog(a.catalog);
    require(w.society && w.simulation.capacity && w.simulation.accounts && w.simulation.lab &&
        a.starts_on==w.campaign_start && a.prepared_on==day && a.testimonies.size()<=authority_record_limit &&
        a.promises.size()<=authority_record_limit && a.mandates.size()<=authority_record_limit && a.cases.size()<=authority_case_limit &&
        a.executive_negotiations.size()<=authority_record_limit && a.months.size()<=society_month_limit,
        "Institutional history differs from its owning campaign, prepared morning or bounds.");
    std::map<std::string,const Posting*,std::less<>> expenses;
    for(const auto& p:w.simulation.accounts->postings) if(p.key.starts_with("authority.")) expenses.emplace(p.key,&p);
    const auto paid=[&](std::string key,OrdinalDay on,Cents amount) {
        const auto it=expenses.find(key);
        require(it!=expenses.end() && it->second->day==on && it->second->amount==amount &&
            it->second->debit==Account::expenses && it->second->credit==Account::cash,"An institutional disclosure lost its exact paid expense.");
        expenses.erase(it);
    };
    std::set<std::string> reports,staff_records;
    for(std::size_t i=0;i<a.testimonies.size();++i) {
        const auto& t=a.testimonies[i];require(t.id==i+1 && t.day>=a.starts_on && t.day<=day &&
            (i==0 || a.testimonies[i-1].day<=t.day) && reports.insert(t.report).second &&
            society_evidence(w.simulation,t.report,0,t.day),"A public testimony repeats or precedes its actual independent report.");
    }
    for(std::size_t i=0;i<a.promises.size();++i) {
        const auto& p=a.promises[i];(void)authority_key(p.kind);
        require(p.id==i+1 && p.made_on>=a.starts_on && p.made_on<=day && (i==0 || a.promises[i-1].made_on<=p.made_on) &&
            p.due_on==advance_days(p.made_on,a.catalog.promise_window) && p.due_on<w.simulation.end_date,
            "A public commitment has an invalid identity or response date.");
        if(p.kind==PublicPromiseKind::publication) {
            const auto r=std::find_if(w.simulation.lab->research.begin(),w.simulation.lab->research.end(),[&](const auto& row){return row.research_id==p.subject;});
            const auto* project=r==w.simulation.lab->research.end()?nullptr:named(w.simulation.capacity->projects,r->project_id);
            const auto before=publication_day(w,p.subject,OrdinalDay{p.made_on.value()-1});
            require(p.region.empty() && p.target==1 && project && project->starts_on<=p.made_on && !before,
                "A publication promise lacks an actual unpublished research project.");
        } else {
            (void)society_region_index(w.society->catalog,p.region);
            require(p.target==a.catalog.promise_tasks && deployed(w.simulation,p.subject,p.made_on),
                "A public-service commitment lacks its actual deployed service or authored target.");
        }
        const auto through=p.closed_on?(p.kind==PublicPromiseKind::publication?OrdinalDay{p.closed_on->value()-1}:*p.closed_on):day;
        const auto met=promise_met(w,p,through);
        require(met==p.met_on && (!p.met_on || *p.met_on>=p.made_on),"A fulfilled promise differs from actual dated publication or delivered public work.");
        const auto end=p.met_on?*p.met_on:p.closed_on?*p.closed_on:day;
        const auto progress=p.kind==PublicPromiseKind::publication?(p.met_on?1U:0U):public_work(w,p.region,p.subject,p.made_on,end);
        require(progress==p.progress,"Public commitment progress differs from its actual service receipts.");
        for(std::size_t j=0;j<i;++j) if(a.promises[j].kind==p.kind && a.promises[j].region==p.region && a.promises[j].subject==p.subject)
            require(p.made_on>=advance_days(a.promises[j].made_on,a.catalog.reapplication_delay),"Public commitments bypass their repeat interval.");
    }
    for(std::size_t i=0;i<a.mandates.size();++i) {
        const auto& m=a.mandates[i];require(m.id==i+1 && m.applied_on>=a.starts_on && m.applied_on<=day &&
            (i==0 || a.mandates[i-1].applied_on<=m.applied_on) && m.reviews.size()<=64 && m.directives.size()<=64,
            "An institutional role has an invalid identity, date or bounded history.");
        auto opening=m;opening.withdrawn_on.reset();opening.staff_released_on.reset();opening.revoked_on.reset();opening.reviews.clear();opening.directives.clear();
        const RequestInstitutionalMandate request{m.offer,m.region,m.service,m.source,m.site,m.testimony,m.promise};
        require(application(w,request,m.applied_on,m.id,m.deployment,true)==opening,
            "An institutional application differs from its disclosed evidence, actual public record or decision rights.");
        const auto& offer=authority_offer(a.catalog,m.offer);
        project_terms(w,m.project,m.applied_on,m.ends_before,offer.compute,offer.staff,offer.monthly,m.source);
        const auto* hire=named(w.simulation.capacity->staff_additions,hire_id(m.id));
        require(hire && hire->day==m.applied_on && hire->people==offer.staff,"Institutional staff were not actually hired for their paid work.");
        staff_records.insert(hire_id(m.id));
        const auto* recurring=named(w.simulation.accounts->recurring,"support/"+m.project);
        require(recurring && recurring->starts_on==m.applied_on && recurring->monthly_amount==offer.monthly &&
            recurring->ends_before==m.staff_released_on,"Institutional payroll differs from its real staffing interval.");
        if(m.staff_released_on) {
            const auto* departure=named(w.simulation.capacity->staff_departures,departure_id(m.id));
            require(departure && departure->day==*m.staff_released_on && departure->people==offer.staff,
                "Released institutional staff lack their dated capacity departure.");staff_records.insert(departure_id(m.id));
        }
        if(m.withdrawn_on) require(*m.withdrawn_on>=m.applied_on && *m.withdrawn_on<=day && *m.withdrawn_on<m.ends_before,
            "An institutional withdrawal has no valid action date.");
        for(std::size_t j=0;j<m.directives.size();++j) {
            const auto& directive=m.directives[j];
            require(directive.day<=day && operative(w,m,directive.day) && (j==0 ||
                (directive.day>=advance_days(m.directives[j-1].day,a.catalog.directive_spacing) && directive.enabled!=m.directives[j-1].enabled)),
                "A procurement directive bypassed actual institutional rights or its change interval.");
        }
        paid("authority.setup/"+std::to_string(m.id),m.applied_on,offer.setup);
    }
    for(const auto& change:w.simulation.capacity->staff_additions) if(change.id.starts_with("authority."))
        require(staff_records.contains(change.id),"Institutional staffing contains an orphaned paid hire.");
    for(const auto& change:w.simulation.capacity->staff_departures) if(change.id.starts_with("authority."))
        require(staff_records.contains(change.id),"Institutional staffing contains an orphaned departure.");
    for(std::size_t i=0;i<a.cases.size();++i) {
        const auto& c=a.cases[i];(void)authority_key(c.kind);
        require(c.id==i+1 && c.opened_on>=a.starts_on && c.opened_on<=day && (i==0 || a.cases[i-1].opened_on<=c.opened_on) &&
            c.deadline==advance_days(c.opened_on,a.catalog.response_window) && !(c.resolved_on && c.disclosed_on),
            "A public dispute lacks its concrete cause or warned response window.");
        if(c.kind==PublicCaseKind::promise) (void)numbered(a.promises,c.subject);else (void)numbered(a.mandates,c.subject);
        require((c.testimony!=0)==c.cited_on.has_value(),"A case citation lost its disclosure date.");
        if(c.cited_on) {
            const auto& t=numbered(a.testimonies,c.testimony);
            require(t.day<=*c.cited_on && *c.cited_on>=c.opened_on && *c.cited_on<=day,"Case testimony was unavailable on its actual citation date.");
        }
        if(c.negotiated_on) {
            require(c.cited_on && *c.cited_on<=*c.negotiated_on && *c.negotiated_on>=advance_days(c.opened_on,a.catalog.concern_period) &&
                *c.negotiated_on<c.deadline && *c.negotiated_on<=day && c.remedy_project=="authority.remedy/"+std::to_string(c.id),
                "A public negotiation bypassed its testimony or formal response window.");
            project_terms(w,c.remedy_project,*c.negotiated_on,case_due(c,a.catalog),a.catalog.remedy_compute,a.catalog.remedy_staff,Cents{0});
            paid("authority.remedy.fee/"+std::to_string(c.id),*c.negotiated_on,a.catalog.remedy_fee);
        } else require(c.remedy_project.empty(),"Public response work has no negotiated agreement.");
        if(c.disclosed_on) require(*c.disclosed_on==case_due(c,a.catalog) && *c.disclosed_on<=day,
            "Public disclosure bypassed its final response deadline.");
        if(c.resolved_on) require(*c.resolved_on>=c.opened_on && *c.resolved_on<case_due(c,a.catalog) && *c.resolved_on<=day,
            "A public response was resolved outside its actual remedy window.");
    }
    std::set<std::string> executives;
    for(const auto& n:a.executive_negotiations) {
        require(w.simulation.management && w.simulation.management->governance && executives.insert(n.dispute).second,
            "Executive negotiation repeats or has no company governance owner.");
        const auto* c=named(w.simulation.management->governance->disputes,n.dispute);
        require(c && c->negotiated_on==n.day && c->review_project==n.project && n.project=="authority.executive/"+n.dispute,
            "Executive negotiation differs from the actual private response path.");
        project_terms(w,n.project,n.day,c->deadline,a.catalog.remedy_compute,a.catalog.remedy_staff,Cents{0});
        paid("authority.executive.fee/"+n.dispute,n.day,a.catalog.remedy_fee);
    }
    if(w.simulation.management && w.simulation.management->governance) for(const auto& c:w.simulation.management->governance->disputes)
        if(c.negotiated_on) require(executives.contains(c.id),"A private executive deadline was extended outside its owning transaction.");
    require(expenses.empty(),"An institutional expense has no corresponding agreement or disclosure.");

    // Replay only relevant dates on one copied world. Receipt queries always
    // name historical cutoffs; daily gameplay never performs this audit.
    WorldState replay=w;AuthorityState clean;clean.catalog=a.catalog;clean.starts_on=a.starts_on;clean.prepared_on=a.starts_on;
    replay.authority=std::move(clean);
    std::set<OrdinalDay> dates{a.starts_on,day};
    const auto add=[&](OrdinalDay date){if(date>=a.starts_on && date<=day) dates.insert(date);};
    const auto optional=[&](std::optional<OrdinalDay> date){if(date) add(*date);};
    auto civil=from_ordinal(a.starts_on);
    auto month=to_ordinal({civil.month==12?civil.year+1:civil.year,civil.month==12?1U:civil.month+1,1});
    while(month<=day) {add(month);civil=from_ordinal(month);month=to_ordinal({civil.month==12?civil.year+1:civil.year,civil.month==12?1U:civil.month+1,1});}
    for(const auto& t:a.testimonies) add(t.day);
    for(const auto& p:a.promises) {add(p.made_on);add(p.due_on);optional(p.met_on);optional(p.closed_on);}
    for(const auto& m:a.mandates) {add(m.applied_on);add(m.decision_on);add(m.ends_before);optional(m.withdrawn_on);optional(m.staff_released_on);optional(m.revoked_on);for(const auto& r:m.reviews) add(r.day);}
    for(const auto& c:a.cases) {
        add(c.opened_on);add(advance_days(c.opened_on,a.catalog.concern_period));add(c.deadline);add(case_due(c,a.catalog));
        optional(c.negotiated_on);optional(c.resolved_on);optional(c.disclosed_on);optional(c.cited_on);
        if(c.kind!=PublicCaseKind::promise) for(auto date=advance_days(c.opened_on,a.catalog.concern_period);date<case_due(c,a.catalog) && date<=day;date=advance_days(date,Days{1})) add(date);
    }
    for(const auto& p:w.simulation.lab->publications) add(p.day);
    std::map<OrdinalDay,std::vector<std::size_t>> cancellations;
    for(std::size_t i=0;i<w.simulation.capacity->projects.size();++i) {
        const auto& p=w.simulation.capacity->projects[i];
        if(p.id.starts_with("authority.operation/") && p.status==ProjectStatus::cancelled && p.stopped_on) {add(*p.stopped_on);cancellations[*p.stopped_on].push_back(i);}
    }
    std::size_t testimony_cursor=0,promise_cursor=0,mandate_cursor=0;
    for(const auto date:dates) {
        replay.simulation.date=date;
        bool departure=false;
        if(w.simulation.management && w.simulation.management->governance) for(const auto& app:w.simulation.management->governance->appointments)
            if(app.departed_on==date) departure=true;
        const bool terminal=date==day && (w.simulation.phase==DayPhase::closed || w.simulation.phase==DayPhase::finished);
        if(!departure && !terminal) for(const auto index:cancellations[date]) replay.simulation.capacity->projects[index].status=ProjectStatus::active;
        auto& r=*replay.authority;
        replay_prepare(replay,r.prepared_on==date?std::nullopt:std::optional{OrdinalDay{date.value()-1}});
        while(testimony_cursor<a.testimonies.size() && a.testimonies[testimony_cursor].day==date) r.testimonies.push_back(a.testimonies[testimony_cursor++]);
        while(promise_cursor<a.promises.size() && a.promises[promise_cursor].made_on==date) {
            auto p=a.promises[promise_cursor++];p.met_on.reset();p.closed_on.reset();p.progress=0;r.promises.push_back(std::move(p));
        }
        while(mandate_cursor<a.mandates.size() && a.mandates[mandate_cursor].applied_on==date) {
            auto m=a.mandates[mandate_cursor++];m.withdrawn_on.reset();m.staff_released_on.reset();m.revoked_on.reset();m.reviews.clear();r.mandates.push_back(std::move(m));
        }
        for(std::size_t i=0;i<r.mandates.size();++i) if(a.mandates[i].withdrawn_on==date) r.mandates[i].withdrawn_on=date;
        for(const auto index:cancellations[date]) replay.simulation.capacity->projects[index].status=ProjectStatus::cancelled;
        for(auto& c:r.cases) {
            const auto& original=numbered(a.cases,c.id);
            if(original.cited_on==date) {c.cited_on=date;c.testimony=original.testimony;}
            if(original.negotiated_on==date) {c.negotiated_on=date;c.remedy_project=original.remedy_project;}
        }
        replay_prepare(replay,{});
    }
    auto& expected=*replay.authority;
    require(expected.cases.size()==a.cases.size(),"A public dispute was invented or a concrete consequence was omitted.");
    for(std::size_t i=0;i<expected.cases.size();++i) expected.cases[i].acknowledged=a.cases[i].acknowledged;
    expected.executive_negotiations=a.executive_negotiations;
    require(expected==a,"Institutional authority, public dispute stages, resistance or monthly coverage differ from actual source records.");
}
}
