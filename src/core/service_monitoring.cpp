#include <pgp/core/service_monitoring.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
OrdinalDay window_start(OrdinalDay end,Days window) {
    return OrdinalDay{checked_subtract(end.value(),window.value())};
}
ServiceWorkSample sample(const MarketState& market,const ProductCapacityBinding& release,const MarketDemandChange& change,
                         OrdinalDay first,OrdinalDay end,const MarketTrial* trial=nullptr,bool baseline=false) {
    ServiceWorkSample result;
    for (auto day=std::max(first,market.starts_on);day<end;day=advance_days(day,Days{1})) {
        if (baseline && market_workload_shift(market,change.segment_id,day)) continue;
        if (trial ? market_trial(market,release.product_id,day)!=trial :
            market_binding(market,release.product_id,day)!=&release) continue;
        const auto& settled=market.days[static_cast<std::size_t>(day.value()-market.starts_on.value())];
        const auto matches=[&](const auto& row) {
            return row.product_id==release.product_id && row.segment_id==change.segment_id;
        };
        const auto sale=std::ranges::find_if(settled.discretionary,matches);
        if (sale!=settled.discretionary.end()) result.completed+=trial?sale->trial_tasks:sale->tasks-sale->trial_tasks;
        const auto failed=std::ranges::find_if(settled.agent_results,matches);
        if (failed==settled.agent_results.end()) continue;
        const auto& outcome=trial?failed->trial:failed->production;
        result.failures.failed+=outcome.failed;
        result.failures.executed+=outcome.executed;
        result.failures.remedy=checked_unit_add(result.failures.remedy,outcome.remedy);
    }
    return result;
}
std::optional<ServiceWorkloadReview> review(const MarketState& market,const ServiceMonitoringRules& rules,
    const MarketDemandChange& change,std::string_view service,OrdinalDay morning) {
    const auto end=std::min(morning,change.expires_before);
    if (change.shifted_share.value()==0 || end<=change.starts_on || !market.closed_through ||
        end>advance_days(*market.closed_through,Days{1})) return {};
    const auto last=OrdinalDay{checked_subtract(end.value(),1)};
    const auto* release=market_binding(market,service,last);
    if (!release || !release->agent_plan || !release->agent_plan->shifted_failure_rate) return {};
    const auto first=std::max(change.starts_on,window_start(end,rules.window));
    const auto* trial=market_trial(market,service,last);
    ServiceWorkloadReview result{&change,release,trial,
        sample(market,*release,change,window_start(change.starts_on,rules.window),change.starts_on,nullptr,true),
        sample(market,*release,change,first,end),
        trial?sample(market,*release,change,first,end,trial):ServiceWorkSample{},{}};
    if (result.prior.attempts()==0 && result.shifted.attempts()==0 && result.candidate.attempts()==0) return {};
    if (result.prior.attempts()>=rules.minimum_attempts && result.shifted.attempts()>=rules.minimum_attempts) {
        const auto delta=result.prior.completion()->value()-result.shifted.completion()->value();
        result.completion_drop=BasisPoints{std::max<std::int64_t>(0,delta)};
    }
    return result;
}
std::string notice_id(const MarketDemandChange& change,std::string_view service) {
    return "service.workload/"+std::string(service)+"/"+change.id;
}
bool meets_threshold(const ServiceWorkloadReview& review,const ServiceMonitoringRules& rules) {
    return review.completion_drop && *review.completion_drop>=rules.warning_drop;
}
bool stop_at_notice_limit(ManagementState& management,OrdinalDay day) {
    if (management.notices.size()<service_monitoring_notice_limit) return false;
    if (management.notices.size()>=256)
        throw std::invalid_argument("Management history is full and cannot record the monitoring limit");
    management.notices.push_back({std::string(service_monitoring_limit_id),std::string(service_monitoring_limit_notice),
        std::string(service_monitoring_limit_subject),day,{},true,false});
    return true;
}
}
std::optional<BasisPoints> ServiceWorkSample::completion() const {
    if (attempts()==0) return {};
    return BasisPoints{mul_div_floor(completed,10000,attempts()).quotient};
}
void validate_service_monitoring(const ServiceMonitoringRules& rules) {
    if (rules.window.value()<1 || rules.window.value()>14 || rules.minimum_attempts==0 ||
        rules.minimum_attempts>market_task_limit || rules.warning_drop.value()==0)
        throw std::invalid_argument("Service monitoring needs a bounded sample window, attempt count and warning threshold");
}
std::vector<ServiceWorkloadReview> service_workload_reviews(const SimulationState& s,std::string_view service) {
    std::vector<ServiceWorkloadReview> result;
    if (!s.market || !s.management || !s.management->catalog.service_monitoring) return result;
    const auto& rules=*s.management->catalog.service_monitoring;
    for (const auto& change:s.market->demand_changes) {
        if (change.expires_before<window_start(s.date,rules.window)) continue;
        if (auto row=review(*s.market,rules,change,service,s.date)) result.push_back(*row);
    }
    return result;
}
std::optional<ServiceWorkloadReview> service_workload_alert(const SimulationState& s,const ManagementNotice& notice) {
    if (!s.market || !s.management || !s.management->catalog.service_monitoring ||
        notice.kind!=service_workload_notice || !notice.required || notice.deadline) return {};
    const auto binding=std::ranges::find(s.market->capacity_bindings,notice.subject,&ProductCapacityBinding::project_id);
    if (binding==s.market->capacity_bindings.end()) return {};
    const auto& rules=*s.management->catalog.service_monitoring;
    for (const auto& change:s.market->demand_changes) {
        if (notice.day>change.expires_before || notice.id!=notice_id(change,binding->product_id)) continue;
        auto row=review(*s.market,rules,change,binding->product_id,notice.day);
        if (row && row->production==&*binding && meets_threshold(*row,rules)) return row;
    }
    return {};
}
bool service_monitoring_suspended(const SimulationState& s) {
    return s.management && std::ranges::find(s.management->notices,service_monitoring_limit_id,
        &ManagementNotice::id)!=s.management->notices.end();
}
void prepare_service_monitoring(SimulationState& s) {
    if (!s.market || !s.management || !s.management->catalog.service_monitoring) return;
    auto& management=*s.management;
    if (service_monitoring_suspended(s) || stop_at_notice_limit(management,s.date)) return;
    const auto& rules=*management.catalog.service_monitoring;
    for (const auto& change:s.market->demand_changes) {
        if (change.shifted_share.value()==0 || s.date<=change.starts_on || s.date>change.expires_before) continue;
        for (const auto& product:s.market->products) {
            if (!product.available || product.channel!=ProductChannel::agent) continue;
            auto id=notice_id(change,product.id);
            if (std::ranges::find(management.notices,id,&ManagementNotice::id)!=management.notices.end()) continue;
            const auto row=review(*s.market,rules,change,product.id,s.date);
            if (row && meets_threshold(*row,rules)) {
                management.notices.push_back({std::move(id),std::string(service_workload_notice),row->production->project_id,s.date,{},true,false});
                // Reserve the remaining shared history for authored deadlines.
                if (stop_at_notice_limit(management,s.date)) return;
            }
        }
    }
}
} // namespace pgp::core
