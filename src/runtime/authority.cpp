#include "authority_internal.hpp"

namespace pgp::runtime::authority_detail {
void pay(WorldState& w,Cents fee,std::string key,std::string cause) {
    auto& s=w.simulation;if(!s.accounts) invalid("Institutional work requires company accounts.");
    auto cash=s.cash;for(const auto& r:s.reservations) cash=checked_unit_subtract(cash,r.reserved);
    if(fee>cash) invalid("The disclosed institutional fee exceeds unreserved cash.");
    record_cash(*s.accounts,s.date,Cents{-fee.value()},Account::expenses,std::move(cause),std::move(key));s.cash=accounts_report(*s.accounts).cash;
}
void reserve_review(WorldState& w,std::string id,std::string source,OrdinalDay ends,MilliCUPerDay rate,std::uint32_t staff) {
    auto& s=w.simulation;
    if(!s.capacity || !s.accounts || ends<=s.date || ends>s.end_date)
        invalid("Choose available institutional work and a response date within the campaign.");
    const auto* supply=named(s.capacity->sources,source);if(!supply) invalid("Choose an existing compute source for institutional work.");
    CapacityProject project;project.id=std::move(id);project.source_id=std::move(source);project.kind=ProjectKind::research;
    project.starts_on=s.date;project.requested=rate;project.staff=staff;project.interconnect=supply->interconnect;
    project.total_work=integrate(rate,Days{ends.value()-s.date.value()});
    project.cause="Funded institutional administration and review";
    reserve_project(*s.capacity,s.date,std::move(project));
}
bool release_mandate_staff(WorldState& w,AuthorityMandate& m) {
    if(m.staff_released_on) return false;
    const auto close=closing_day(w,m);if(!close || *close>w.simulation.date) return false;
    auto& s=w.simulation;const auto* project=named(s.capacity->projects,m.project);
    if(!project) invalid("Institutional staffing lost its operating project.");
    if(project->status==ProjectStatus::active) cancel_project(*s.capacity,s.date,m.project);
    end_recurring(*s.accounts,payroll_id(m.id),s.date);
    remove_staff_capacity(*s.capacity,s.date,departure_id(m.id),authority_offer(w.authority->catalog,m.offer).staff);
    m.staff_released_on=s.date;return true;
}
AuthorityMandate application(const WorldState& w,const RequestInstitutionalMandate& request,OrdinalDay day,std::uint64_t id,
    std::uint64_t saved_deployment,bool historic) {
    if(!w.authority || !w.society || !w.simulation.deployments || !w.simulation.capacity || !w.simulation.market)
        invalid("This scenario has no institutional authority path.");
    const auto& a=*w.authority;const auto& offer=authority_offer(a.catalog,request.offer);
    const auto region=society_region_index(w.society->catalog,request.region);
    const auto& testimony=numbered(a.testimonies,request.testimony);
    const auto* report=society_evidence(w.simulation,testimony.report,0,day);
    const auto& deployments=w.simulation.deployments->records;
    const auto* deployment=historic
        ? (saved_deployment && saved_deployment<=deployments.size()?&deployments[static_cast<std::size_t>(saved_deployment-1)]:nullptr)
        : deployed(w.simulation,request.service,day);
    const auto* previous=historic?deployed(w.simulation,request.service,OrdinalDay{day.value()-1}):nullptr;
    const auto* product=named(w.simulation.market->products,request.service);
    if(testimony.day>day || !report || report->lower[0]<offer.evidence_floor || !deployment ||
        deployment->service_id!=request.service || deployment->deployed_on>day || (previous && deployment<previous) ||
        deployment->evidence_id!=testimony.report ||
        report->model_id!=deployment->model_id || !public_service_scope(w,*deployment,request.region) ||
        !product || (!historic && !product->available)) invalid("Disclose and deploy an independent report meeting this role's evidence threshold in the selected region.");
    const auto* month=known_month(w,day);
    if(!month || month->outcomes[region].player_public<offer.public_work)
        invalid("This role requires the disclosed amount of actual public-service work in the last settled regional month.");
    std::size_t active=0;
    for(const auto& prior:a.mandates) if(prior.id<id && prior.applied_on<=day) {
        if(!closing_day(w,prior) || *closing_day(w,prior)>day) ++active;
        if(prior.region==request.region && authority_offer(a.catalog,prior.offer).kind==offer.kind &&
            (day<advance_days(prior.applied_on,a.catalog.reapplication_delay) || !closing_day(w,prior) || *closing_day(w,prior)>day))
            invalid("A role in this region is already committed or remains within its disclosed reapplication interval.");
    }
    if(active>=authority_active_limit || id>authority_record_limit) invalid("The institutional commitment allowance is full.");
    const auto* supply=named(w.simulation.capacity->sources,request.source);
    if(!supply || (supply->available_from && *supply->available_from>day)) invalid("Choose an existing compute source available on the application date.");
    AuthorityMandate m;m.id=id;m.offer=request.offer;m.region=request.region;m.service=request.service;m.source=request.source;m.site=request.site;
    m.testimony=request.testimony;m.promise=request.promise;m.applied_on=day;m.decision_on=advance_days(day,offer.lead);
    m.ends_before=advance_days(m.decision_on,offer.term);m.project=operation_id(id);
    m.deployment=static_cast<std::uint64_t>(deployment-deployments.data())+1;
    if(m.ends_before>w.simulation.end_date) invalid("The institution's full funded term must fit inside this campaign.");
    if(offer.kind==AuthorityKind::standards) {
        for(const auto& round:w.society->rounds) if(round.ballot_on<=day &&
            std::any_of(round.members.begin(),round.members.end(),[&](const auto& row){return row.region==request.region;})) m.forum_round=round.id;
        if(m.forum_round==0) invalid("This standards role needs recorded participation by the regional member in an actual forum ballot.");
    }
    if(offer.kind==AuthorityKind::delegation) {
        const auto& promise=numbered(a.promises,request.promise);
        if(promise.kind!=PublicPromiseKind::public_work || promise.region!=request.region || !promise.met_on || *promise.met_on>day)
            invalid("Institutional delegation requires a fulfilled public-service commitment in this region.");
    } else if(request.promise!=0) invalid("Only institutional delegation binds a prior public-service commitment.");
    if(offer.kind==AuthorityKind::infrastructure) {
        const auto* site=w.simulation.infrastructure?named(w.simulation.infrastructure->sites,request.site):nullptr;
        const auto hardware=w.simulation.infrastructure?std::find_if(w.simulation.infrastructure->hardware.begin(),w.simulation.infrastructure->hardware.end(),
            [&](const auto& row){return row.source_id==request.source && row.site_id==request.site && row.commissioned_on && *row.commissioned_on<=day && (!row.retired_on || *row.retired_on>day);}):std::vector<HardwareOrder>::const_iterator{};
        if(request.region!="region.us" || !site || !site->commissioned_on || *site->commissioned_on>day ||
            (site->closed_on && *site->closed_on<=day) || supply->kind!=CapacitySourceKind::owned ||
            hardware==w.simulation.infrastructure->hardware.end()) invalid("Critical infrastructure rights require the company's actually commissioned US site and owned source.");
    } else if(!request.site.empty()) invalid("A site is bound only to the critical infrastructure role.");
    return m;
}
}

namespace pgp::runtime {
using namespace authority_detail;
namespace {
void open_case(AuthorityState& a,PublicCaseKind kind,std::uint64_t subject,std::string region,OrdinalDay day) {
    if(std::any_of(a.cases.begin(),a.cases.end(),[&](const auto& c){return c.kind==kind && c.subject==subject && !c.resolved_on && !c.disclosed_on;})) return;
    if(a.cases.size()>=authority_case_limit) invalid("The public case history reached its bound.");
    PublicCase c;c.id=a.cases.size()+1;c.kind=kind;c.subject=subject;c.region=std::move(region);c.opened_on=day;
    c.deadline=advance_days(day,a.catalog.response_window);a.cases.push_back(std::move(c));
}
bool cured(const WorldState& w,const PublicCase& c,OrdinalDay day) {
    const auto& a=*w.authority;
    if(c.kind==PublicCaseKind::promise) {
        const auto& p=numbered(a.promises,c.subject);return p.met_on && *p.met_on<=day;
    }
    const auto& m=numbered(a.mandates,c.subject);
    if(m.withdrawn_on && *m.withdrawn_on<=day) return true;
    const InstitutionalReview* latest=nullptr;for(const auto& r:m.reviews) if(r.day<=day) latest=&r;
    if(!latest || latest->day<=c.opened_on || latest->operation<a.catalog.minimum_operation) return false;
    return c.kind!=PublicCaseKind::resistance || resistance(w,society_region_index(w.society->catalog,m.region),day)<a.catalog.resistance_threshold.value();
}
bool stop_review(WorldState& w,std::string_view project) {
    if(project.empty()) return false;
    const auto* work=named(w.simulation.capacity->projects,project);
    if(!work || work->status!=ProjectStatus::active) return false;
    cancel_project(*w.simulation.capacity,w.simulation.date,project);
    return true;
}
}
namespace {
bool prepare_impl(WorldState& w,std::optional<OrdinalDay> settled,bool proof) {
    if(!w.authority) return false;auto& a=*w.authority;const auto day=w.simulation.date;
    const bool next=a.prepared_on!=day;
    if(next && (!a.prepared_on || (!proof && a.prepared_on->value()+1!=day.value()) ||
        !settled || settled->value()+1!=day.value() || day<*a.prepared_on))
        invalid("Institutional preparation cannot skip an actual settled morning.");
    bool resources=false;
    const auto release=[&](AuthorityMandate& m) {
        if(!proof) return release_mandate_staff(w,m);
        if(!m.staff_released_on && closing_day(w,m) && *closing_day(w,m)<=day) m.staff_released_on=day;
        return false;
    };
    for(auto& p:a.promises) if(!p.met_on && !p.closed_on) {
        if(p.kind==PublicPromiseKind::publication) {
            if(const auto date=publication_day(w,p.subject,next?OrdinalDay{day.value()-1}:day)) {p.met_on=*date;p.progress=1;}
        } else if(next && settled && *settled>=p.made_on) {
            p.progress+=public_work(w,p.region,p.subject,std::max(p.made_on,*a.prepared_on),day);
            if(p.progress>=p.target) p.met_on=day;
        }
        if(!p.met_on && day>=p.due_on) open_case(a,PublicCaseKind::promise,p.id,p.region,p.due_on);
    }
    bool monthly=false;
    if(next && settled) {const auto civil=from_ordinal(*settled);monthly=civil.day==days_in_month(civil.year,civil.month);}
    for(auto& m:a.mandates) {
        const bool closing=closing_day(w,m) && *closing_day(w,m)<=day;
        const bool decision=m.reviews.empty() && day==m.decision_on;
        if((decision && !closing) || (monthly && !m.reviews.empty() && operative(w,m,*settled) && m.reviews.back().day<day)) {
            const auto r=review(w,m,day);m.reviews.push_back(r);
            if(r.approved && !closing && r.operation<a.catalog.minimum_operation) open_case(a,PublicCaseKind::delivery,m.id,m.region,day);
        }
        resources=release(m)||resources;
    }
    if(monthly) {
        auto record=authority_snapshot(w,*settled);record.resistance=next_resistance(w,*settled);
        for(std::size_t i=0;i<society_regions;++i) {
            record.hard[i]=0;
            for(const auto& m:a.mandates) if(m.region==a.catalog.regions[i].id && operative(w,m,*settled) && authority_offer(a.catalog,m.offer).kind==AuthorityKind::coercive) {
                const auto points=m.reviews.empty()?0:m.reviews.back().score.value();
                record.hard[i]=std::max(record.hard[i],static_cast<std::uint32_t>(std::max<std::int64_t>(0,points-record.resistance[i]/2)));
            }
        }
        std::uint64_t covered_population=0,covered_output=0,population=0,output=0;
        for(std::size_t i=0;i<society_regions;++i) {
            const auto& r=w.society->catalog.regions[i];population+=r.population_weight;output+=r.output_weight;
            if(std::max(record.soft[i],record.hard[i])>=6000) {covered_population+=r.population_weight;covered_output+=r.output_weight;}
        }
        record.population_coverage=ratio(covered_population,population);record.output_coverage=ratio(covered_output,output);
        if(a.months.size()>=society_month_limit) invalid("The institutional monthly history is full.");
        a.months.push_back(record);
        for(const auto& m:a.mandates) if(operative(w,m,day) && authority_offer(a.catalog,m.offer).kind==AuthorityKind::coercive &&
            resistance(w,society_region_index(w.society->catalog,m.region),day)>=a.catalog.resistance_threshold.value())
            open_case(a,PublicCaseKind::resistance,m.id,m.region,day);
    }
    for(auto& c:a.cases) if(!c.resolved_on && !c.disclosed_on) {
        if(c.kind!=PublicCaseKind::promise && day>=advance_days(c.opened_on,a.catalog.concern_period) && day<case_due(c,a.catalog)) {
            auto& m=a.mandates[static_cast<std::size_t>(c.subject-1)];
            if(operative(w,m,day) && m.reviews.back().day<day && m.reviews.size()<64) {
                const auto candidate=review(w,m,day);
                if(candidate.operation>=a.catalog.minimum_operation && (c.kind!=PublicCaseKind::resistance ||
                    resistance(w,society_region_index(w.society->catalog,m.region),day)<a.catalog.resistance_threshold.value())) m.reviews.push_back(candidate);
            }
        }
        if(cured(w,c,day)) {c.resolved_on=day;c.acknowledged=true;}
        else if(day>=case_due(c,a.catalog)) {
            c.disclosed_on=day;c.acknowledged=false;
            if(c.kind==PublicCaseKind::promise) a.promises[static_cast<std::size_t>(c.subject-1)].closed_on=day;
            else {
                auto& m=a.mandates[static_cast<std::size_t>(c.subject-1)];m.revoked_on=day;
                resources=release(m)||resources;
            }
        } else if(next && day==advance_days(c.opened_on,a.catalog.concern_period)) c.acknowledged=false;
        if(!proof && (c.resolved_on || c.disclosed_on || w.simulation.phase==DayPhase::closed || w.simulation.phase==DayPhase::finished))
            resources=stop_review(w,c.remedy_project)||resources;
    }
    if(!proof && w.simulation.management && w.simulation.management->governance)
        for(const auto& c:w.simulation.management->governance->disputes)
            if(c.resolved_on==day) resources=stop_review(w,c.review_project)||resources;
    a.prepared_on=day;return resources;
}
}
bool prepare_authority(WorldState& w,std::optional<OrdinalDay> settled) {return prepare_impl(w,settled,false);}
namespace authority_detail {
void replay_prepare(WorldState& w,std::optional<OrdinalDay> settled) {(void)prepare_impl(w,settled,true);}
}
bool submit_authority_action(WorldState& w,const CommandAction& action) {
    std::string region;
    if(const auto* mandate=std::get_if<RequestInstitutionalMandate>(&action)) region=mandate->region;
    else if(const auto* promise=std::get_if<MakePublicPromise>(&action)) region=promise->region;
    if(!region.empty()) if(const auto block=regional_contact_block(w.simulation,region);!block.empty()) throw std::invalid_argument(block);
    const bool own=std::holds_alternative<SubmitPublicTestimony>(action) || std::holds_alternative<MakePublicPromise>(action) ||
        std::holds_alternative<RequestInstitutionalMandate>(action) || std::holds_alternative<NegotiatePublicCase>(action) ||
        std::holds_alternative<NegotiateExecutiveDispute>(action) || std::holds_alternative<AcknowledgePublicCase>(action) ||
        std::holds_alternative<RelinquishInstitutionalMandate>(action) || std::holds_alternative<DirectRegionalProcurement>(action);
    if(!own) return false;
    if(!w.authority) invalid("This scenario has no public institutions workspace.");
    if(w.simulation.phase!=DayPhase::work_ready) invalid("Institutional actions require an active prepared campaign.");
    auto& a=*w.authority;const auto day=w.simulation.date;
    if(const auto* request=std::get_if<SubmitPublicTestimony>(&action)) {
        if(!society_evidence(w.simulation,request->report,0,day)) invalid("Public testimony needs an actually completed independent evaluation.");
        auto found=std::find_if(a.testimonies.begin(),a.testimonies.end(),[&](const auto& row){return row.report==request->report;});
        if(found==a.testimonies.end()) {
            if(a.testimonies.size()>=authority_record_limit) invalid("The public testimony register is full.");
            a.testimonies.push_back({a.testimonies.size()+1,request->report,day});found=std::prev(a.testimonies.end());
        } else if(request->case_id==0) invalid("This independent report is already publicly disclosed.");
        if(request->case_id) {
            (void)numbered(a.cases,request->case_id);auto& c=a.cases[static_cast<std::size_t>(request->case_id-1)];
            if(c.resolved_on || c.disclosed_on || c.testimony) invalid("Choose an unresolved case that has not already cited testimony.");
            c.testimony=found->id;c.cited_on=day;
        }
    } else if(const auto* request=std::get_if<MakePublicPromise>(&action)) {
        if(a.promises.size()>=authority_record_limit) invalid("The public commitment register is full.");
        std::size_t active=0;for(const auto& p:a.promises) {
            if(!p.met_on && !p.closed_on) ++active;
            if(p.kind==request->kind && p.region==request->region && p.subject==request->subject && day<advance_days(p.made_on,a.catalog.reapplication_delay))
                invalid("A commitment for this subject remains within its disclosed repeat interval.");
        }
        if(active>=8) invalid("Finish or resolve an existing public commitment before making another.");
        PublicPromise p;p.id=a.promises.size()+1;p.kind=request->kind;p.region=request->region;p.subject=request->subject;p.made_on=day;
        p.due_on=advance_days(day,a.catalog.promise_window);if(p.due_on>=w.simulation.end_date) invalid("The public commitment must fit before the campaign ends.");
        if(p.kind==PublicPromiseKind::publication) {
            if(!p.region.empty() || !w.simulation.lab || publication_day(w,p.subject,day) ||
                std::none_of(w.simulation.lab->research.begin(),w.simulation.lab->research.end(),[&](const auto& run){return run.research_id==p.subject;}))
                invalid("Promise an actual research project that has not already been published.");
            p.target=1;
        } else {
            (void)society_region_index(w.society->catalog,p.region);
            const auto* deployment=deployed(w.simulation,p.subject,day);
            const auto* product=named(w.simulation.market->products,p.subject);
            if(!deployment || !product || !product->available || !public_service_scope(w,*deployment,p.region))
                invalid("Choose an operating service with public customers in this region.");
            p.deployment=static_cast<std::uint64_t>(deployment-w.simulation.deployments->records.data())+1;
            p.target=a.catalog.promise_tasks;
        }
        a.promises.push_back(std::move(p));
    } else if(const auto* request=std::get_if<RequestInstitutionalMandate>(&action)) {
        auto m=application(w,*request,day,a.mandates.size()+1);const auto& offer=authority_offer(a.catalog,m.offer);
        pay(w,offer.setup,"authority.setup/"+std::to_string(m.id),"Institutional staffing and application: "+offer.name);
        add_staff_capacity(*w.simulation.capacity,day,hire_id(m.id),offer.staff);
        reserve_review(w,m.project,m.source,m.ends_before,offer.compute,offer.staff);
        add_recurring(*w.simulation.accounts,{payroll_id(m.id),offer.monthly,day,{},"Institutional contract team: "+offer.name});
        a.mandates.push_back(std::move(m));
    } else if(const auto* request=std::get_if<NegotiatePublicCase>(&action)) {
        (void)numbered(a.cases,request->id);auto& c=a.cases[static_cast<std::size_t>(request->id-1)];
        if(c.negotiated_on || c.resolved_on || c.disclosed_on || day<advance_days(c.opened_on,a.catalog.concern_period) || day>=c.deadline || c.testimony==0)
            invalid("Cite independent testimony and negotiate once during the formal response window.");
        const auto end=advance_days(c.deadline,a.catalog.extension);const auto project="authority.remedy/"+std::to_string(c.id);
        pay(w,a.catalog.remedy_fee,"authority.remedy.fee/"+std::to_string(c.id),"Funded public response review");
        reserve_review(w,project,request->source,end,a.catalog.remedy_compute,a.catalog.remedy_staff);
        c.negotiated_on=day;c.remedy_project=project;c.acknowledged=true;
    } else if(const auto* request=std::get_if<NegotiateExecutiveDispute>(&action)) {
        auto& s=w.simulation;
        if(!s.management || !s.management->governance) invalid("Appoint a negotiating executive before reviewing a private commitment.");
        const auto* c=named(s.management->governance->disputes,request->id);
        if(!c || c->kind!="publication" || c->status!=CorporateDisputeStatus::formal || c->negotiated_on || day>=c->deadline)
            invalid("Choose one formal publication dispute within its response window.");
        const auto end=advance_days(c->deadline,s.management->catalog.governance->response_window);
        const auto project="authority.executive/"+c->id;
        pay(w,a.catalog.remedy_fee,"authority.executive.fee/"+c->id,"Funded executive response negotiation");
        reserve_review(w,project,request->source,end,a.catalog.remedy_compute,a.catalog.remedy_staff);
        extend_executive_response(s,request->id,project);a.executive_negotiations.push_back({request->id,project,day});
    } else if(const auto* request=std::get_if<AcknowledgePublicCase>(&action)) {
        (void)numbered(a.cases,request->id);auto& c=a.cases[static_cast<std::size_t>(request->id-1)];
        if(c.acknowledged) invalid("This public notice is already acknowledged.");c.acknowledged=true;
    } else if(const auto* request=std::get_if<RelinquishInstitutionalMandate>(&action)) {
        (void)numbered(a.mandates,request->id);auto& m=a.mandates[static_cast<std::size_t>(request->id-1)];
        if(m.withdrawn_on || m.revoked_on || day>=m.ends_before) invalid("This institutional agreement is already closed.");
        m.withdrawn_on=day;(void)release_mandate_staff(w,m);
    } else if(const auto* request=std::get_if<DirectRegionalProcurement>(&action)) {
        (void)numbered(a.mandates,request->id);auto& m=a.mandates[static_cast<std::size_t>(request->id-1)];
        if(!operative(w,m,day) || m.directives.size()>=64 || (m.directives.empty() && !request->enabled) || (!m.directives.empty() &&
            (day<advance_days(m.directives.back().day,a.catalog.directive_spacing) || m.directives.back().enabled==request->enabled)))
            invalid("Use an operating agreement and wait for its published directive-change interval.");
        m.directives.push_back({day,request->enabled});
    }
    (void)prepare_authority(w);return true;
}
}
