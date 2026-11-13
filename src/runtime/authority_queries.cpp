#include "authority_internal.hpp"
#include <map>
#include <set>
#include <tuple>

namespace pgp::runtime::authority_detail {
const SocietyMonth* known_month(const WorldState& w,OrdinalDay day) {
    if(!w.society) return nullptr;
    const auto it=std::lower_bound(w.society->months.begin(),w.society->months.end(),day,[](const auto& row,OrdinalDay date){return row.month_end<date;});
    return it==w.society->months.begin()?nullptr:&*std::prev(it);
}
const DeploymentRecord* deployed(const SimulationState& s,std::string_view service,OrdinalDay day) {
    if(s.deployments) for(auto it=s.deployments->records.rbegin();it!=s.deployments->records.rend();++it)
        if(it->service_id==service && it->deployed_on<=day) return &*it;
    return nullptr;
}
bool public_service_scope(const WorldState& w,const DeploymentRecord& deployment,std::string_view region) {
    if(!w.society || !w.simulation.market || !w.simulation.capacity ||
        (!deployment.region_id.empty() && deployment.region_id!=region)) return false;
    const auto& market=*w.simulation.market;
    const auto* product=named(market.products,deployment.service_id);
    const auto* work=named(w.simulation.capacity->projects,deployment.project_id);
    if(!product || !work) return false;
    return std::any_of(market.segments.begin(),market.segments.end(),[&](const auto& segment){
        return segment.region_id==region &&
            market_access(*product,segment) &&
            (deployment.weight_audience.empty() || deployment.weight_audience==segment.id) &&
            (!work->customer_inference || (work->minimum_memory_mib<=segment.customer_memory_mib &&
                work->interconnect==segment.customer_interconnect &&
                work->inference_milli_CU_per_task.value()<=segment.customer_compute.value())) &&
            std::ranges::find(w.society->catalog.public_services,segment.id)!=w.society->catalog.public_services.end() &&
            std::any_of(segment.preferences.begin(),segment.preferences.end(),[&](const auto& preference){
                return preference.product_id==deployment.service_id && preference.eligible;
            });
    });
}
std::uint64_t public_work(const WorldState& w,std::string_view region,std::string_view service,OrdinalDay first,OrdinalDay ends) {
    if(!w.society || !w.simulation.market) return 0;
    const auto& market=*w.simulation.market;std::set<std::string_view> groups;
    for(const auto& segment:market.segments) if(segment.region_id==region &&
        std::find(w.society->catalog.public_services.begin(),w.society->catalog.public_services.end(),segment.id)!=w.society->catalog.public_services.end()) groups.insert(segment.id);
    std::uint64_t total=0;
    auto it=std::lower_bound(market.days.begin(),market.days.end(),first,[](const auto& row,OrdinalDay day){return row.day<day;});
    for(;it!=market.days.end() && it->day<ends;++it) {
        for(const auto& row:it->discretionary) if(row.product_id==service && groups.contains(row.segment_id)) total+=row.tasks;
        for(const auto& row:it->contracts) {
            const auto* contract=named(market.contracts,row.contract_id);
            if(contract && contract->product_id==service && groups.contains(contract->segment_id)) total+=row.delivered_tasks;
        }
    }
    return total;
}
std::uint64_t admin_work(const WorldState& w,std::string_view project,OrdinalDay first,OrdinalDay ends) {
    if(!w.simulation.capacity) return 0;
    const auto& days=w.simulation.capacity->days;std::uint64_t total=0;
    auto it=std::lower_bound(days.begin(),days.end(),first,[](const auto& row,OrdinalDay day){return row.day<day;});
    for(;it!=days.end() && it->day<ends;++it) for(const auto& row:it->projects)
        if(row.project_id==project) total+=static_cast<std::uint64_t>(row.work.value());
    return total;
}
std::optional<OrdinalDay> closing_day(const WorldState& w,const AuthorityMandate& m) {
    std::optional<OrdinalDay> result=m.ends_before;
    const auto consider=[&](std::optional<OrdinalDay> day){if(day && (!result || *day<*result)) result=day;};
    consider(m.withdrawn_on);
    if(!m.reviews.empty() && !m.reviews.front().approved) consider(m.decision_on);
    consider(m.revoked_on);
    if(w.simulation.liquidity && !w.simulation.liquidity->workouts.empty())
        consider(w.simulation.liquidity->workouts.back().closed_on);
    if(w.simulation.capacity) if(const auto* project=named(w.simulation.capacity->projects,m.project))
        if(project->status==ProjectStatus::cancelled) consider(project->stopped_on);
    return result;
}
bool operative(const WorldState& w,const AuthorityMandate& m,OrdinalDay day) {
    return !m.reviews.empty() && m.reviews.front().approved && m.decision_on<=day &&
        (!closing_day(w,m) || day<*closing_day(w,m));
}
std::uint32_t resistance(const WorldState& w,std::size_t region,OrdinalDay day) {
    const auto& a=*w.authority;std::uint32_t result=0;OrdinalDay since{w.campaign_start.value()-1};
    const auto it=std::lower_bound(a.months.begin(),a.months.end(),day,[](const auto& row,OrdinalDay date){return row.month_end<date;});
    if(it!=a.months.begin()) {const auto& last=*std::prev(it);result=last.resistance[region];since=last.month_end;}
    for(const auto& m:a.mandates) if(m.applied_on>since && m.applied_on<=day && m.region==a.catalog.regions[region].id &&
        authority_offer(a.catalog,m.offer).kind==AuthorityKind::coercive)
        result=std::min<std::uint32_t>(10000,result+static_cast<std::uint32_t>(a.catalog.initial_resistance.value()));
    return result;
}
InstitutionalReview review(const WorldState& w,const AuthorityMandate& m,OrdinalDay day) {
    const auto& a=*w.authority;const auto& offer=authority_offer(a.catalog,m.offer);
    OrdinalDay from=m.applied_on;
    for(const auto& previous:m.reviews) if(previous.day<day) from=previous.day;
    if(day<=from) invalid("Institutional reviews need a positive observed interval.");
    InstitutionalReview r;r.day=day;
    r.work=admin_work(w,m.project,from,day);
    r.required_work=static_cast<std::uint64_t>(integrate(offer.compute,Days{day.value()-from.value()}).value());
    r.public_work=public_work(w,m.region,m.service,from,day);
    r.required_public=mul_div_round_half_up(offer.public_work,static_cast<std::uint64_t>(day.value()-from.value()),30);
    r.operation=BasisPoints{std::min(ratio(r.work,r.required_work),ratio(r.public_work,r.required_public))};
    const auto* current=deployed(w.simulation,m.service,OrdinalDay{day.value()-1});
    const auto* current_report=current?society_evidence(w.simulation,current->evidence_id,0,OrdinalDay{day.value()-1}):nullptr;
    if(current_report) r.evidence=current_report->lower[0];
    if(r.evidence<offer.evidence_floor) r.operation=BasisPoints{0};
    const auto region=society_region_index(w.society->catalog,m.region);
    const auto& testimony=numbered(a.testimonies,m.testimony);
    const auto* report=society_evidence(w.simulation,testimony.report,0,m.applied_on);
    if(!report) invalid("An institutional application lost its disclosed independent report.");
    const auto reputation=public_identity(w,OrdinalDay{day.value()-1})[4];
    const auto consent=a.catalog.regions[region].consent.value()+(report->lower[0].value()-6000)/2+
        (static_cast<std::int64_t>(reputation)-5000)/4;
    r.consent=BasisPoints{std::clamp<std::int64_t>(consent,0,10000)};
    const bool initial=m.reviews.empty() || m.reviews.front().day>=day;
    r.approved=initial?(r.operation>=a.catalog.minimum_operation &&
        (offer.kind==AuthorityKind::coercive || r.consent>=offer.consent_floor)):m.reviews.front().approved;
    r.score=r.approved?BasisPoints{mul_div_floor(static_cast<std::uint64_t>(offer.points.value()),static_cast<std::uint64_t>(r.operation.value()),10000).quotient}:BasisPoints{0};
    return r;
}
std::optional<OrdinalDay> publication_day(const WorldState& w,std::string_view id,OrdinalDay day) {
    if(w.simulation.lab) for(const auto& p:w.simulation.lab->publications) if(p.research_id==id && p.day<=day) return p.day;
    return {};
}
std::optional<OrdinalDay> promise_met(const WorldState& w,const PublicPromise& p,OrdinalDay through) {
    if(p.kind==PublicPromiseKind::publication) return publication_day(w,p.subject,through);
    if(!w.simulation.market) return {};
    std::uint64_t total=0;
    const auto& days=w.simulation.market->days;
    auto it=std::lower_bound(days.begin(),days.end(),p.made_on,[](const auto& row,OrdinalDay day){return row.day<day;});
    for(;it!=days.end() && it->day<through;++it) {
        total+=public_work(w,p.region,p.subject,it->day,advance_days(it->day,Days{1}));
        if(total>=p.target) return advance_days(it->day,Days{1});
    }
    return {};
}
OrdinalDay case_due(const PublicCase& c,const AuthorityCatalog& catalog) {
    return advance_days(c.deadline,c.negotiated_on?catalog.extension:Days{0});
}
std::array<std::uint32_t,society_regions> next_resistance(const WorldState& w,OrdinalDay month_end) {
    const auto& a=*w.authority;std::array<std::uint32_t,society_regions> result{};
    const auto civil=from_ordinal(month_end);const auto first=std::max(w.campaign_start,to_ordinal({civil.year,civil.month,1}));
    for(std::size_t i=0;i<result.size();++i) {
        auto value=static_cast<std::int64_t>(resistance(w,i,month_end));bool coercive=false,remedied=false;
        for(const auto& m:a.mandates) if(m.region==a.catalog.regions[i].id && authority_offer(a.catalog,m.offer).kind==AuthorityKind::coercive && operative(w,m,month_end)) coercive=true;
        for(const auto& p:a.promises)
            if(p.kind==PublicPromiseKind::public_work && p.region==a.catalog.regions[i].id && p.met_on &&
                *p.met_on>first && *p.met_on<=advance_days(month_end,Days{1})) remedied=true;
        if(coercive) value+=a.catalog.resistance_growth.value();else value-=a.catalog.resistance_decay.value();
        if(remedied) value-=a.catalog.remedy_relief.value();
        result[i]=static_cast<std::uint32_t>(std::clamp<std::int64_t>(value,coercive?1000:0,10000));
    }
    return result;
}
std::string operation_id(std::uint64_t id) {return "authority.operation/"+std::to_string(id);}
std::string hire_id(std::uint64_t id) {return std::string(authority_hire_prefix)+std::to_string(id);}
std::string departure_id(std::uint64_t id) {return "authority.departure/"+std::to_string(id);}
std::string payroll_id(std::uint64_t id) {return "authority.payroll/"+std::to_string(id);}
}

namespace pgp::runtime {
using namespace authority_detail;
std::vector<const DeploymentRecord*> public_service_deployments(const WorldState& w,std::string_view region) {
    std::vector<const DeploymentRecord*> result;
    if(!w.authority || !w.simulation.market) return result;
    for(const auto& product:w.simulation.market->products) if(product.available) {
        const auto* record=deployed(w.simulation,product.id,w.simulation.date);
        if(record && public_service_scope(w,*record,region)) result.push_back(record);
    }
    return result;
}
std::string_view mandate_status(const WorldState& w,const AuthorityMandate& m,OrdinalDay day) {
    const auto closing=closing_day(w,m);
    if(closing && *closing<=day) {
        if(m.revoked_on==closing) return "revoked";
        if(m.withdrawn_on==closing) return "withdrawn";
        if(m.ends_before==closing) return "expired";
    }
    if(m.staff_released_on && *m.staff_released_on<=day && m.reviews.empty()) return "interrupted";
    if(m.reviews.empty() || m.decision_on>day) return "commissioning";
    if(!m.reviews.front().approved) return "declined";
    if(!operative(w,m,day)) return "interrupted";
    return "operating";
}
AuthorityMonth authority_snapshot(const WorldState& w,OrdinalDay day) {
    AuthorityMonth result;result.month_end=day;if(!w.authority) return result;
    const auto& a=*w.authority;
    for(std::size_t i=0;i<society_regions;++i) result.resistance[i]=resistance(w,i,day);
    for(const auto& m:a.mandates) if(operative(w,m,day)) {
        const auto i=society_region_index(w.society->catalog,m.region);const auto& offer=authority_offer(a.catalog,m.offer);
        const InstitutionalReview* latest=nullptr;
        for(const auto& r:m.reviews) if(r.day<=advance_days(day,Days{1})) latest=&r;
        if(!latest) continue;
        const auto score=static_cast<std::uint32_t>(latest->score.value());
        if(offer.kind==AuthorityKind::coercive) result.hard[i]=std::max(result.hard[i],static_cast<std::uint32_t>(std::max<std::int64_t>(0,static_cast<std::int64_t>(score)-result.resistance[i]/2)));
        else result.soft[i]=std::min<std::uint32_t>(10000,result.soft[i]+score);
    }
    std::uint64_t population=0,output=0,p=0,o=0;
    for(std::size_t i=0;i<society_regions;++i) {
        const auto& region=w.society->catalog.regions[i];population+=region.population_weight;output+=region.output_weight;
        if(std::max(result.soft[i],result.hard[i])>=6000) {p+=region.population_weight;o+=region.output_weight;}
    }
    result.population_coverage=ratio(p,population);result.output_coverage=ratio(o,output);return result;
}
std::vector<PublicIdentityEvent> public_identity_events(const WorldState& w,OrdinalDay day) {
    std::vector<PublicIdentityEvent> result;if(!w.authority) return result;const auto& a=*w.authority;
    const auto add=[&](std::size_t effect,OrdinalDay on,std::string subject,std::string cause){if(on<=day) result.push_back({effect,on,std::move(subject),std::move(cause),{}});};
    for(const auto& t:a.testimonies) add(0,t.day,t.report,"Disclosed a completed independent evaluation");
    for(const auto& p:a.promises) if(p.met_on) add(1,*p.met_on,std::to_string(p.id),"Fulfilled a documented public commitment");
    for(const auto& c:a.cases) if(c.disclosed_on) add(c.kind==PublicCaseKind::promise?2:c.kind==PublicCaseKind::resistance?5:6,*c.disclosed_on,std::to_string(c.id),"A documented issue passed its response deadline without an effective remedy");
    if(w.simulation.management && w.simulation.management->governance) for(const auto& c:w.simulation.management->governance->disputes)
        if(c.kind=="publication" && c.status==CorporateDisputeStatus::departed && c.resolved_on) add(3,*c.resolved_on,c.subject,"An executive departed after an unfulfilled publication commitment and failed response path");
    for(const auto& m:a.mandates) if(authority_offer(a.catalog,m.offer).kind==AuthorityKind::coercive) add(4,m.applied_on,m.region,"Funded an abstract coercive institution");
    if(w.simulation.succession && w.simulation.succession->releases) {
        const auto& actor=*w.simulation.succession;const auto& rules=*succession_catalog(w.simulation).departure;
        for(const auto& release:*actor.releases) {
            if(release.day>day) continue;
            const auto& p=actor.plans.at(release.plan-1);const auto& message=departure_message(succession_catalog(w.simulation),release.message);
            const auto subject=leadership_candidate(w.simulation,p.candidate,p.proposed_on).name;
            result.push_back({7,release.day,subject,"Founder announcement: "+message.title,std::array<std::int32_t,society_audiences>{}});
            const auto assessment=departure_assessment(w.simulation,release);
            if(rules.press && assessment=="challenged")
                result.push_back({7,release.day,subject,"Reporters challenged a resignation claim using the company record",rules.press->challenged_effect});
            if(p.asi && succession_catalog(w.simulation).asi) {
                const auto& ai=*succession_catalog(w.simulation).asi;
                result.push_back({7,release.day,subject,"Announced an approved ASI succession",ai.appointment_effect});
                if(p.asi->control<ai.control_scrutiny_below)
                    result.push_back({7,release.day,subject,"Independent control evidence drew scrutiny over the ASI succession",ai.weak_control_effect});
            }
            if(p.status==SuccessionStatus::activated && p.finished_on && *p.finished_on<=day) {
                auto effect=message.completed_effect;
                if(assessment=="challenged" || assessment=="unsubstantiated")
                    for(auto& change:effect) change=std::min(change,0);
                result.push_back({7,std::max(release.day,*p.finished_on),subject,"Completed the publicly announced founder handover",effect});
            }
            const bool stopped=p.status==SuccessionStatus::cancelled || p.status==SuccessionStatus::obsolete;
            const bool late=p.handover_on<=day && (!p.finished_on || *p.finished_on>p.handover_on);
            if((stopped && p.finished_on && *p.finished_on<=day) || late) {
                auto on=late?p.handover_on:*p.finished_on;
                if(stopped && p.finished_on) on=std::min(on,*p.finished_on);
                result.push_back({7,std::max(release.day,on),subject,"The announced founder handover was delayed or cancelled",rules.broken_effect});
            }
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& x,const auto& y){return std::tie(x.day,x.effect,x.subject,x.cause)<std::tie(y.day,y.effect,y.subject,y.cause);});return result;
}
AudienceScores public_identity(const WorldState& w,OrdinalDay day) {
    const auto* known=known_month(w,day);AudienceScores result=known?known->audiences:AudienceScores{5000,5000,5000,5000,5000,5000};
    if(!w.authority) return result;
    std::array<std::int64_t,society_audiences> delta{};const auto memory=w.authority->catalog.identity_memory.value();
    for(const auto& e:public_identity_events(w,day)) {
        const auto age=day.value()-e.day.value();if(age>=memory) continue;
        for(std::size_t i=0;i<delta.size();++i) delta[i]+=static_cast<std::int64_t>((e.direct_effect?(*e.direct_effect)[i]:w.authority->catalog.identity_effects.at(e.effect)[i]))*(memory-age)/memory;
    }
    for(std::size_t i=0;i<result.size();++i) result[i]=static_cast<std::uint32_t>(std::clamp<std::int64_t>(result[i]+delta[i],0,10000));return result;
}
namespace {
struct DeliveryRule {bool sanction=false,procurement=false;};
DeliveryRule regional_delivery_rule(const WorldState& w,const SimulationState& seller,const MarketSegment& segment,OrdinalDay day) {
    const auto& a=*w.authority;
    if(seller.market->owner==w.company_id) {
        const auto earliest=OrdinalDay{day.value()-a.catalog.response_window.value()-a.catalog.extension.value()-a.catalog.sanctions.value()};
        const auto after=std::ranges::upper_bound(a.cases,day,{},&PublicCase::opened_on);
        for(auto it=std::make_reverse_iterator(after);it!=a.cases.rend() && it->opened_on>earliest;++it)
            if(it->kind==PublicCaseKind::resistance && it->region==segment.region_id && it->disclosed_on &&
                *it->disclosed_on<=day && day<advance_days(*it->disclosed_on,a.catalog.sanctions)) return {true,false};
    }
    if(std::ranges::find(w.society->catalog.public_services,segment.id)==w.society->catalog.public_services.end()) return {};
    const auto after=std::ranges::upper_bound(a.mandates,day,{},&AuthorityMandate::applied_on);
    for(auto it=std::make_reverse_iterator(after);it!=a.mandates.rend();++it) {
        if(it->applied_on.value()<=day.value()-authority_maximum_lead_days-authority_maximum_term_days) break;
        if(it->region!=segment.region_id || it->directives.empty() || day>=it->ends_before || !operative(w,*it,day)) continue;
        const auto directive=std::find_if(it->directives.rbegin(),it->directives.rend(),[&](const auto& row){return row.day<=day;});
        if(directive!=it->directives.rend() && directive->enabled) return {false,true};
    }
    return {};
}
bool delivery_allowed(const AuthorityCatalog& catalog,const SimulationState& seller,std::string_view product,OrdinalDay day,
                      DeliveryRule rule,const DeploymentRecord* version=nullptr) {
    if(rule.sanction) return false;
    if(!rule.procurement) return true;
    const auto* deployment=version?version:deployed(seller,product,day);
    const auto* report=deployment?society_evidence(seller,deployment->evidence_id,0,day):nullptr;
    return report && report->lower[0]>=catalog.procurement_floor;
}
}
bool institutional_delivery_allowed(const WorldState& w,const SimulationState& seller,std::string_view product,
                                    std::string_view group,OrdinalDay day,const DeploymentRecord* version) {
    if(!w.authority || !seller.market || (w.authority->mandates.empty() && w.authority->cases.empty())) return true;
    const auto* segment=named(seller.market->segments,group);
    if(!segment) invalid("Institutional delivery lost its customer group.");
    return delivery_allowed(w.authority->catalog,seller,product,day,regional_delivery_rule(w,seller,*segment,day),version);
}
MarketInfluences regional_market_conditions(const WorldState& w,const SimulationState& seller,OrdinalDay day) {
    auto result=society_market_conditions(w,seller,day);
    if(!seller.market) return result;
    if(core::talent_rules(seller))for(const auto& product:seller.market->products) {
        const auto bonus=core::software_market_bonus(seller,product.channel,day);if(bonus.value()==0)continue;
        for(const auto& segment:seller.market->segments) {
            auto it=std::ranges::find_if(result,[&](const auto& row){return row.product_id==product.id && row.segment_id==segment.id;});
            if(it==result.end()){result.push_back({product.id,segment.id,BasisPoints{0},true,true,{},bonus});}
            else it->software_quality=bonus;
        }
    }
    const bool authority=w.authority && (!w.authority->mandates.empty() || !w.authority->cases.empty());
    std::vector<std::pair<const MarketProduct*,const DeploymentRecord*>> trials;
    if (seller.deployments && (authority || w.society)) for (const auto& route:seller.market->trials) {
        if (route.starts_on>day || day>=route.stopped_on.value_or(route.ends_before)) continue;
        const auto* candidate=trial_release(*seller.deployments,route);
        const auto* offer=named(seller.market->products,route.product_id);
        if (!candidate || !offer) invalid("A staged release lost its product or evidence");
        trials.emplace_back(offer,candidate);
    }
    const bool weight_forum=w.society && w.society->catalog.forum.channel==ProductChannel::open_weights;
    std::map<std::string_view,std::vector<const WeightInstallation*>> installations;
    if (seller.deployments && (authority || weight_forum)) for (const auto& copy:seller.market->weight_installations)
        installations[copy.segment_id].push_back(&copy);
    if (!authority && trials.empty() && installations.empty()) return result;
    const auto edge=[&](const std::string& product,const std::string& segment) -> MarketInfluence& {
        auto it=std::find_if(result.begin(),result.end(),[&](const auto& row){return row.product_id==product && row.segment_id==segment;});
        if (it==result.end()) {
            result.push_back({product,segment,BasisPoints{0},true,true});
            it=std::prev(result.end());
        }
        return *it;
    };
    for(const auto& segment:seller.market->segments) {
        const auto rule=authority?regional_delivery_rule(w,seller,segment,day):DeliveryRule{};
        // A signed contract can name a service outside discretionary preferences.
        if (rule.sanction || rule.procurement) for(const auto& product:seller.market->products)
            if(!delivery_allowed(w.authority->catalog,seller,product.id,day,rule)) edge(product.id,segment.id).eligible=false;
        for (const auto& [product,candidate]:trials) {
            if (!forum_allows_release(w.society?&*w.society:nullptr,seller,*product,segment,candidate,day) ||
                (authority && !delivery_allowed(w.authority->catalog,seller,product->id,day,rule,candidate)))
                edge(product->id,segment.id).trial_eligible=false;
        }
        const auto copies=installations.find(segment.id);
        if (copies==installations.end() || (!weight_forum && !rule.sanction && !rule.procurement)) continue;
        for (const auto* copy:copies->second) {
            const auto* product=named(seller.market->products,copy->product_id);
            const auto version=std::ranges::find(seller.deployments->records,copy->project_id,&DeploymentRecord::project_id);
            if (!product || version==seller.deployments->records.end()) invalid("A retained installation lost its qualified publication");
            auto& restrictions=edge(product->id,segment.id).blocked_weight_projects;
            if (!restrictions) restrictions.emplace();
            if (!forum_allows_release(w.society?&*w.society:nullptr,seller,*product,segment,&*version,day) ||
                (authority && !delivery_allowed(w.authority->catalog,seller,product->id,day,rule,&*version)))
                restrictions->push_back(copy->project_id);
        }
    }
    return result;
}
bool required_public_case(const WorldState& w) {
    return w.authority && std::any_of(w.authority->cases.begin(),w.authority->cases.end(),[](const auto& c){return !c.acknowledged;});
}
std::optional<OrdinalDay> next_authority_boundary(const WorldState& w) {
    if(!w.authority) return {};const auto day=w.simulation.date;std::optional<OrdinalDay> result;
    const auto consider=[&](OrdinalDay date){if(date>day && date<w.simulation.end_date && (!result || date<*result)) result=date;};
    const auto civil=from_ordinal(day);consider(to_ordinal({civil.month==12?civil.year+1:civil.year,civil.month==12?1U:civil.month+1,1}));
    for(const auto& p:w.authority->promises) if(!p.met_on) consider(p.due_on);
    for(const auto& m:w.authority->mandates) {consider(m.decision_on);consider(m.ends_before);}
    for(const auto& c:w.authority->cases) if(!c.resolved_on && !c.disclosed_on) {consider(advance_days(c.opened_on,w.authority->catalog.concern_period));consider(case_due(c,w.authority->catalog));}
    return result;
}
}
