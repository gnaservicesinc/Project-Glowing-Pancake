#include <pgp/core/management.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==id;});
    if (row==rows.end()) invalid("Management option refers to an unavailable record");
    return *row;
}
ManagementState& management(SimulationState& s) {
    if (!s.management || !s.accounts || !s.capacity || !s.lab || !s.market) invalid("This scenario has no company management policies");
    return *s.management;
}
bool accepted(const std::vector<ManagementAcceptance>& rows,std::string_view id) {
    return std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.offer_id==id;});
}
void notice(ManagementState& state,std::string id,std::string kind,std::string subject,OrdinalDay day,
            std::optional<OrdinalDay> deadline={},bool required=false) {
    if (std::any_of(state.notices.begin(),state.notices.end(),[&](const auto& row){return row.id==id;})) return;
    if (state.notices.size()>=256) invalid("Management notice history is full");
    state.notices.push_back({std::move(id),std::move(kind),std::move(subject),day,deadline,required,false});
}
std::string evaluation_id(const DelegationPolicy& policy,std::uint32_t run) { return "delegated."+policy.id+"."+std::to_string(run); }
void policy_shape(const SimulationState& state,const DelegationPolicy& p) {
    const auto& catalog=state.management->catalog;
    if (p.id.empty() || p.id.size()>32 || p.target_id.empty() || p.budget.value()<=0 || p.reserve_floor.value()<0 ||
        p.monthly_ceiling.value()<0 || p.maximum_runs==0 || p.maximum_runs>catalog.maximum_policy_runs ||
        p.runs>p.maximum_runs || p.committed>p.budget || p.interval<catalog.minimum_evaluation_interval || p.interval.value()>365 ||
        p.created_on<state.accounts->starts_on || p.created_on>state.date || p.next_review<p.created_on)
        invalid("Policy needs finite runs, budget, reserve, interval and valid dated bounds");
    if (p.kind==DelegationKind::evaluation) {
        (void)find(state.lab->models,p.target_id);const auto& plan=find(state.lab->catalog.evaluations,p.plan_id);
        (void)find(state.capacity->sources,p.allocation.source_id);
        if (!plan.independent || p.allocation.daily_compute.value()<=0 || p.allocation.staff==0 || p.allocation.staff>state.capacity->available_staff)
            invalid("Evaluation delegation needs independent methodology and explicit staff/compute ceilings");
    } else if (p.kind==DelegationKind::lease_renewal) {
        if (!state.business_offers) invalid("Renewal requires an accepted lease");
        const auto& offer=find(state.business_offers->catalog,p.target_id);
        if (!offer.renewable || offer.kind!=BusinessOfferKind::compute_lease || p.monthly_ceiling.value()==0 ||
            !p.plan_id.empty() || !p.allocation.source_id.empty() || p.allocation.daily_compute.value()!=0 || p.allocation.staff!=0)
            invalid("Renewal policy needs renewable terms and a monthly ceiling, without evaluation allocations");
        (void)lease_expiry(*state.business_offers,p.target_id);
    } else invalid("Unsupported delegation policy");
}
}
void validate_management_catalog(const ManagementCatalog& c) {
    if (c.default_policy_budget.value()<=0 || c.default_reserve_floor.value()<0 || c.default_monthly_ceiling.value()<=0 ||
        c.default_policy_runs==0 || c.default_policy_runs>c.maximum_policy_runs || c.financing.size()>8 || c.specialists.size()>8 || c.setbacks.size()>8 || c.maximum_policy_runs==0 || c.maximum_policy_runs>12 ||
        c.minimum_evaluation_interval.value()<7 || c.minimum_evaluation_interval.value()>365 || c.scoring.task_target==0 ||
        c.scoring.task_target>market_task_limit*100 || c.scoring.founder.empty() || c.scoring.opening_founder_share.value()==0)
        invalid("Management catalog has invalid bounds or scoring targets");
    std::uint32_t weights=0;for (auto w:c.scoring.weights) {if (w>100) invalid("Slice scoring weight exceeds its bound");weights+=w;}
    if (weights!=100) invalid("Slice score weights must sum to 100");
    std::set<std::string> ids;
    for (const auto& row:c.financing) {
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || !ids.insert(row.id).second ||
            row.proceeds.value()<=0 || row.proceeds.value()>1'000'000'000 || row.term.value()>1096 ||
            (row.term.value()==0 ? (row.acquired.value()==0 || row.acquired.value()>5000 || row.monthly_interest.value()!=0) : row.acquired.value()!=0))
            invalid("Financing offers need bounded disclosed cash, dilution or debt terms");
    }
    for (const auto& row:c.specialists)
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || !ids.insert(row.id).second || row.signing_cost.value()<0 ||
            row.signing_cost.value()>10'000'000 || row.monthly_salary.value()<=0 || row.monthly_salary.value()>10'000'000)
            invalid("Specialist offer needs distinct identity and bounded actual payroll");
    std::map<std::string,OrdinalDay> last;
    for (const auto& row:c.setbacks) {
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || row.source_id.empty() || !ids.insert(row.id).second ||
            row.announced_on>=row.starts_on || row.starts_on>=row.ends_before || row.reduced_capacity.value()==0 ||
            (last.contains(row.source_id) && last.at(row.source_id)>row.announced_on)) invalid("Setbacks need a visible advance warning and nonoverlapping finite source intervals");
        (void)from_ordinal(row.announced_on);(void)from_ordinal(row.ends_before);last.insert_or_assign(row.source_id,row.ends_before);
    }
}
void take_financing(SimulationState& s,std::string_view id) {
    auto& m=management(s);const auto& offer=find(m.catalog.financing,id);
    if (accepted(m.financing,id)) invalid("This financing offer has already been used; terms cannot be rerolled");
    if (offer.term.value()==0) invest_equity(*s.accounts,s.date,"investor/"+offer.id,offer.proceeds,offer.acquired,"financing/"+offer.id);
    else draw_debt(*s.accounts,s.date,{"financing/"+offer.id,offer.proceeds,offer.monthly_interest,s.date,advance_days(s.date,offer.term),0,std::nullopt},"Financing: "+offer.name);
    m.financing.push_back({offer.id,s.date});s.cash=accounts_report(*s.accounts).cash;
}
void hire_specialist(SimulationState& s,std::string_view id,Cents protected_cash) {
    auto& m=management(s);const auto& offer=find(m.catalog.specialists,id);
    if (accepted(m.specialists,id) || offer.signing_cost>checked_unit_subtract(s.cash,protected_cash)) invalid("Specialist was hired already or signing cost exceeds available cash");
    if (offer.signing_cost.value()>0) record_cash(*s.accounts,s.date,Cents{-offer.signing_cost.value()},Account::expenses,"Specialist signing: "+offer.name,"specialist/"+offer.id);
    add_staff_capacity(*s.capacity,s.date,"specialist/"+offer.id,1);
    add_recurring(*s.accounts,{"specialist/"+offer.id,offer.monthly_salary,s.date,std::nullopt,"Specialist salary: "+offer.name});
    m.specialists.push_back({offer.id,s.date});s.cash=accounts_report(*s.accounts).cash;
}
void add_delegation_policy(SimulationState& s,DelegationPolicy p) {
    auto& m=management(s);
    if (m.policies.size()>=8 || std::any_of(m.policies.begin(),m.policies.end(),[&](const auto& row){return row.id==p.id;})) invalid("Use a distinct policy within the eight-policy limit");
    p.created_on=s.date;p.next_review=advance_days(s.date,Days{1});p.runs=0;p.committed=Cents{0};p.blocked.clear();p.enabled=true;
    policy_shape(s,p);
    if (p.kind==DelegationKind::lease_renewal) {
        p.next_review=lease_expiry(*s.business_offers,p.target_id);
        if (p.next_review<=s.date) invalid("Authorize renewal before the expiry morning; an expired lease requires an explicit current-day renewal");
    }
    m.policies.push_back(std::move(p));
}
void set_delegation_enabled(SimulationState& s,std::string_view id,bool enabled) {
    auto& m=management(s);const auto found=std::find_if(m.policies.begin(),m.policies.end(),[&](const auto& p){return p.id==id;});
    if (found==m.policies.end()) invalid("Delegation policy does not exist");
    auto& p=*found;
    if (enabled && (p.runs>=p.maximum_runs || p.committed>=p.budget)) invalid("This policy exhausted its authorized runs or budget; its counters cannot reset");
    p.enabled=enabled;if (enabled) p.blocked.clear();
}
void review_delegation(SimulationState& s,Cents protected_cash) {
    if (!s.management || s.phase!=DayPhase::work_ready || required_management_notice(s) ||
        (s.liquidity && liquidity_report(*s.liquidity).pending_warning) || s.management->reviewed_on==s.date) return;
    s.management->reviewed_on=s.date;
    for (std::size_t i=0;i<s.management->policies.size();++i) {
        const auto& p=s.management->policies[i];
        if (!p.enabled || p.runs>=p.maximum_runs || p.next_review>s.date) continue;
        if (s.management->decisions.size()>=128) {s.management->policies[i].enabled=false;s.management->policies[i].blocked="The finite policy decision history is full.";continue;}
        if (p.kind==DelegationKind::evaluation) {
            const auto& model=find(s.lab->models,p.target_id);
            if (!model.project_id.empty() && find(s.capacity->projects,model.project_id).status==ProjectStatus::active) continue;
            if (p.runs>0) {
                const auto& last=find(s.lab->evaluations,evaluation_id(p,p.runs));
                if (find(s.capacity->projects,last.project_id).status==ProjectStatus::active) continue;
            }
        }
        // Only an actual bounded attempt copies history. Idle daily reviews do
        // not clone accounts/capacity or run a complete load-time replay.
        auto candidate=s;auto& proposed=candidate.management->policies[i];
        try {
            Cents cost{0};std::string result_id;
            if (p.kind==DelegationKind::evaluation) cost=find(s.lab->catalog.evaluations,p.plan_id).cash;
            else {
                const auto& offer=find(s.business_offers->catalog,p.target_id);
                if (offer.monthly_amount>p.monthly_ceiling) invalid("Lease renewal exceeds the authorized monthly ceiling");
                cost=lease_commitment_ceiling(offer);
            }
            if (cost>checked_unit_subtract(p.budget,p.committed) ||
                checked_unit_add(cost,p.reserve_floor)>checked_unit_subtract(s.cash,protected_cash))
                invalid("Delegated commitment exceeds the remaining total budget or cash reserve floor");
            if (p.kind==DelegationKind::evaluation) {
                result_id=evaluation_id(p,p.runs+1);
                start_evaluation(*candidate.lab,*candidate.accounts,*candidate.capacity,s.date,result_id,p.target_id,p.plan_id,p.allocation,protected_cash);
            } else {
                renew_compute_lease(*candidate.business_offers,*candidate.accounts,*candidate.capacity,s.date,p.target_id,protected_cash);
                result_id=std::to_string(candidate.business_offers->renewals.size());
            }
            ++proposed.runs;proposed.committed=checked_unit_add(proposed.committed,cost);
            proposed.next_review=p.kind==DelegationKind::evaluation?advance_days(s.date,p.interval):lease_expiry(*candidate.business_offers,p.target_id);
            if (proposed.runs>=proposed.maximum_runs) proposed.enabled=false;
            candidate.cash=accounts_report(*candidate.accounts).cash;
            candidate.management->decisions.push_back({p.id,result_id,"Authorized action committed within cash, staff and compute bounds.",s.date,cost,true});
            validate_simulation(candidate);s=std::move(candidate);
        } catch (const std::invalid_argument& error) {
            auto& refused=s.management->policies[i];refused.enabled=false;refused.blocked=error.what();
            s.management->decisions.push_back({refused.id,"",refused.blocked,s.date,Cents{0},false});
            notice(*s.management,"policy.blocked/"+refused.id+"/"+std::to_string(s.management->decisions.size()),"policy_blocked",refused.id,s.date);
        }
    }
}
void prepare_management(SimulationState& s) {
    if (!s.management) return;
    auto& m=*s.management;
    for (const auto& setback:m.catalog.setbacks) {
        const auto& source=find(s.capacity->sources,setback.source_id);
        if (s.date==setback.announced_on) notice(m,"setback.announced/"+setback.id,"setback_announced",setback.id,s.date,setback.starts_on);
        if (s.date==setback.starts_on && std::none_of(m.notices.begin(),m.notices.end(),[&](const auto& n){return n.id=="setback.started/"+setback.id;})) {
            update_effective_capacity(*s.capacity,s.date,setback.source_id,setback.reduced_capacity);
            notice(m,"setback.started/"+setback.id,"setback_started",setback.id,s.date,setback.ends_before,true);
        }
        if (s.date==setback.ends_before && std::none_of(m.notices.begin(),m.notices.end(),[&](const auto& n){return n.id=="setback.ended/"+setback.id;})) {
            update_effective_capacity(*s.capacity,s.date,setback.source_id,source.nominal);
            notice(m,"setback.ended/"+setback.id,"setback_ended",setback.id,s.date);
        }
    }
    if (required_management_notice(s)) s.paused=true;
    for (const auto& hired:m.financing) {
        const auto& offer=find(m.catalog.financing,hired.offer_id);
        if (offer.term.value()==0 || find(s.accounts->debts,"financing/"+offer.id).principal.value()==0) continue;
        const auto due=advance_days(hired.day,offer.term);
        if (s.date==OrdinalDay{checked_subtract(due.value(),7)}) notice(m,"finance.due/"+offer.id,"financing_due",offer.id,s.date,due);
    }
    if (s.business_offers) for (const auto& accepted:s.business_offers->accepted) {
        const auto& offer=find(s.business_offers->catalog,accepted.offer_id);
        if (offer.kind!=BusinessOfferKind::compute_lease) continue;
        const auto expiry=lease_expiry(*s.business_offers,offer.id);
        if (s.date==OrdinalDay{checked_subtract(expiry.value(),7)}) notice(m,"lease.due/"+offer.id+"/"+iso_date(expiry),"lease_due",offer.id,s.date,expiry);
    }
}
bool required_management_notice(const SimulationState& s) {
    return s.management && std::any_of(s.management->notices.begin(),s.management->notices.end(),[](const auto& n){return n.required && !n.acknowledged;});
}
void acknowledge_management_notice(SimulationState& s,std::string_view id) {
    auto& m=management(s);const auto found=std::find_if(m.notices.begin(),m.notices.end(),[&](const auto& n){return n.id==id;});
    if (found==m.notices.end()) invalid("Management notice does not exist");
    auto& n=*found;
    if (n.acknowledged) invalid("This management notice was already acknowledged");
    n.acknowledged=true;
}
SliceScore slice_score(const SimulationState& s) {
    SliceScore score;if (!s.management || !s.accounts || !s.market) return score;
    const auto& rubric=s.management->catalog.scoring;const auto report=accounts_report(*s.accounts);
    score.final=s.phase==DayPhase::finished || s.phase==DayPhase::closed;
    score.profit=report.profit;score.delivered_tasks=market_report(*s.market).delivered_tasks;
    const auto holder=std::find_if(s.accounts->holdings.begin(),s.accounts->holdings.end(),[&](const auto& h){return h.holder==rubric.founder;});
    if (holder!=s.accounts->holdings.end()) score.founder_share=holder->shares;
    score.points[0]=s.phase==DayPhase::finished?rubric.weights[0]:0;
    score.points[1]=static_cast<std::uint32_t>(mul_div_floor(std::min(score.delivered_tasks,rubric.task_target),rubric.weights[1],rubric.task_target).quotient);
    score.points[2]=score.profit.value()>0?rubric.weights[2]:0;
    std::array<bool,lab_domain_count> covered{};
    if (s.deployments) for (const auto& d:s.deployments->records) {
        const auto& e=find(s.lab->evaluations,d.evidence_id);const auto& plan=find(s.lab->catalog.evaluations,e.plan_id);
        for (std::size_t i=0;i<lab_domain_count;++i) if (plan.independent && plan.coverage[i].value()>0 && e.lower[i]>=plan.required[i]) covered[i]=true;
    }
    score.points[3]=static_cast<std::uint32_t>(std::count(covered.begin(),covered.end(),true))*rubric.weights[3]/static_cast<std::uint32_t>(lab_domain_count);
    score.points[4]=static_cast<std::uint32_t>(mul_div_floor(static_cast<std::uint64_t>(std::min(score.founder_share,rubric.opening_founder_share).value()),rubric.weights[4],static_cast<std::uint64_t>(rubric.opening_founder_share.value())).quotient);
    for (const auto point:score.points) score.total+=point;
    return score;
}

void validate_management(const SimulationState& s,bool deep) {
    if (!s.management) return;
    if (!s.accounts || !s.capacity || !s.lab || !s.market || !s.deployments) invalid("Management requires the playable company modules");
    const auto& m=*s.management;
    if (m.financing.size()>m.catalog.financing.size() || m.specialists.size()>m.catalog.specialists.size() || m.policies.size()>8 ||
        m.decisions.size()>128 || m.notices.size()>256 || (m.reviewed_on && (*m.reviewed_on<s.accounts->starts_on || *m.reviewed_on>s.date)))
        invalid("Management history exceeds its bounds or daily cursor");
    if (!deep) return;
    validate_management_catalog(m.catalog);
    const auto paid=[&](std::string_view key,OrdinalDay day,Cents amount,Account debit,Account credit) {
        return std::any_of(s.accounts->postings.begin(),s.accounts->postings.end(),[&](const auto& p){return p.key==key && p.day==day && p.amount==amount && p.debit==debit && p.credit==credit;});
    };
    std::set<std::string> ids;
    for (const auto& row:m.financing) {
        const auto& offer=find(m.catalog.financing,row.offer_id);
        if (!ids.insert(row.offer_id).second || row.day<s.accounts->starts_on || row.day>s.date) invalid("Financing acceptance repeats or has an invalid date");
        if (offer.term.value()==0) {
            if (!paid("financing/"+offer.id,row.day,offer.proceeds,Account::cash,Account::equity)) invalid("Financing lacks actual equity proceeds");
            const auto h=std::find_if(s.accounts->holdings.begin(),s.accounts->holdings.end(),[&](const auto& value){return value.holder=="investor/"+offer.id;});
            if (h==s.accounts->holdings.end() || h->reserved_pool) invalid("Financing lacks its actual investor holding");
        } else {
            const auto& debt=find(s.accounts->debts,"financing/"+offer.id);
            if (debt.drawn_on!=row.day || debt.due!=advance_days(row.day,offer.term) || debt.monthly_interest!=offer.monthly_interest ||
                !paid("debt/financing/"+offer.id,row.day,offer.proceeds,Account::cash,Account::debt)) invalid("Financing differs from its real debt/proceeds");
        }
    }
    if (s.capacity->staff_additions.size()!=m.specialists.size()) invalid("Team additions lack their actual hired specialists");
    for (const auto& row:m.specialists) {
        const auto& offer=find(m.catalog.specialists,row.offer_id);
        if (!ids.insert(row.offer_id).second || row.day<s.accounts->starts_on || row.day>s.date) invalid("Specialist acceptance repeats or has an invalid date");
        const auto& added=find(s.capacity->staff_additions,"specialist/"+offer.id);const auto& salary=find(s.accounts->recurring,"specialist/"+offer.id);
        const bool departed=salary.ends_before && std::any_of(s.capacity->staff_departures.begin(),s.capacity->staff_departures.end(),[&](const auto& departure){return departure.day==*salary.ends_before && departure.day>=row.day;});
        if (added.day!=row.day || added.people!=1 || salary.starts_on!=row.day || (salary.ends_before && !departed) || salary.monthly_amount!=offer.monthly_salary ||
            (offer.signing_cost.value()>0 && !paid("specialist/"+offer.id,row.day,offer.signing_cost,Account::expenses,Account::cash)))
            invalid("Specialist lacks its paid signing, dated staff addition or recurring payroll");
    }
    for (const auto& p:m.policies) {
        if (!ids.insert(p.id).second) invalid("Delegation policy identity is duplicated");
        policy_shape(s,p);
        std::uint32_t runs=0;Cents committed{0};std::optional<OrdinalDay> last;
        for (const auto& d:m.decisions) if (d.policy_id==p.id) {
            if (d.day<p.created_on || d.day>s.date || d.explanation.empty() || d.explanation.size()>1024 ||
                (!d.accepted && (d.commitment.value()!=0 || !d.result_id.empty()))) invalid("Delegation decision has invalid date or refusal state");
            if (!d.accepted) continue;
            ++runs;committed=checked_unit_add(committed,d.commitment);
            if (last && d.day<advance_days(*last,p.kind==DelegationKind::evaluation?p.interval:Days{1})) invalid("Delegation replay repeats within its interval");
            last=d.day;
            if (p.kind==DelegationKind::evaluation) {
                const auto& evidence=find(s.lab->evaluations,d.result_id);const auto& work=find(s.capacity->projects,evidence.project_id);
                if (d.result_id!=evaluation_id(p,runs) || evidence.model_id!=p.target_id || evidence.plan_id!=p.plan_id || work.starts_on!=d.day ||
                    work.source_id!=p.allocation.source_id || work.requested!=p.allocation.daily_compute || work.staff!=p.allocation.staff ||
                    d.commitment!=find(s.lab->catalog.evaluations,p.plan_id).cash) invalid("Delegated evaluation differs from its actual bounded project/payment");
            } else {
                const auto it=std::find_if(s.business_offers->renewals.begin(),s.business_offers->renewals.end(),[&](const auto& r){return r.offer_id==p.target_id && r.renewed_on==d.day;});
                if (it==s.business_offers->renewals.end() || d.result_id!=std::to_string(static_cast<std::size_t>(it-s.business_offers->renewals.begin())+1) ||
                    d.commitment!=lease_commitment_ceiling(find(s.business_offers->catalog,p.target_id))) invalid("Delegated renewal lacks its actual lease commitment");
            }
        }
        auto expected_review=advance_days(p.created_on,Days{1});
        if (p.kind==DelegationKind::evaluation && last) expected_review=advance_days(*last,p.interval);
        if (p.kind==DelegationKind::lease_renewal) {
            expected_review=lease_expiry(*s.business_offers,p.target_id);
            if (last) expected_review=advance_days(*last,find(s.business_offers->catalog,p.target_id).term);
            else {
                const auto acceptance=std::find_if(s.business_offers->accepted.begin(),s.business_offers->accepted.end(),[&](const auto& a){return a.offer_id==p.target_id;});
                expected_review=acceptance->expires_before;
                for (const auto& r:s.business_offers->renewals) if (r.offer_id==p.target_id && r.renewed_on<=p.created_on) expected_review=r.expires_before;
            }
        }
        if (p.next_review!=expected_review) invalid("Policy next-review cursor differs from actual authorized attempts");
        if (runs!=p.runs || committed!=p.committed || (p.runs==p.maximum_runs && p.enabled)) invalid("Delegation counters or authorization differ from decisions");
    }
    for (const auto& d:m.decisions) (void)find(m.policies,d.policy_id);
    std::set<std::string> notice_ids;
    for (const auto& n:m.notices) {
        if (n.id.empty() || n.id.size()>256 || !notice_ids.insert(n.id).second || n.day<s.accounts->starts_on || n.day>s.date ||
            (n.deadline && *n.deadline<n.day)) invalid("Management notices require unique actual dates/deadlines");
        if (n.kind=="setback_announced" || n.kind=="setback_started" || n.kind=="setback_ended") {
            const auto& setback=find(m.catalog.setbacks,n.subject);
            const auto expected_day=n.kind=="setback_announced"?setback.announced_on:n.kind=="setback_started"?setback.starts_on:setback.ends_before;
            const auto expected_deadline=n.kind=="setback_announced"?std::optional{setback.starts_on}:n.kind=="setback_started"?std::optional{setback.ends_before}:std::nullopt;
            if (n.day!=expected_day || n.deadline!=expected_deadline || n.required!=(n.kind=="setback_started")) invalid("Setback notice differs from its actual warning/work/recovery dates");
        } else if (n.kind=="policy_blocked") {
            (void)find(m.policies,n.subject);
            if (n.required || n.deadline || std::none_of(m.decisions.begin(),m.decisions.end(),[&](const auto& d){return d.policy_id==n.subject && d.day==n.day && !d.accepted;}))
                invalid("Policy alert lacks an actual refusal decision");
        } else if (n.kind=="financing_due") {
            const auto& debt=find(s.accounts->debts,"financing/"+n.subject);
            if (n.required || n.deadline!=debt.due || n.day!=OrdinalDay{checked_subtract(debt.due.value(),7)}) invalid("Financing notice invents a debt deadline");
        } else if (n.kind=="lease_due") {
            if (!s.business_offers || n.required || !n.deadline || n.day!=OrdinalDay{checked_subtract(n.deadline->value(),7)} ||
                (std::none_of(s.business_offers->accepted.begin(),s.business_offers->accepted.end(),[&](const auto& a){return a.offer_id==n.subject && a.expires_before==*n.deadline;}) &&
                 std::none_of(s.business_offers->renewals.begin(),s.business_offers->renewals.end(),[&](const auto& r){return r.offer_id==n.subject && r.expires_before==*n.deadline;})))
                invalid("Lease notice invents an expiry deadline");
        } else invalid("Unsupported management notice kind");
    }
    for (const auto& setback:m.catalog.setbacks) {
        const auto& source=find(s.capacity->sources,setback.source_id);
        if (setback.announced_on<s.accounts->starts_on || setback.ends_before>s.end_date || setback.reduced_capacity>=source.nominal)
            invalid("Setback must reduce a real source within the campaign");
        if (s.date>=setback.starts_on && s.date<setback.ends_before && source.effective>setback.reduced_capacity) invalid("Current effective capacity bypasses the active setback");
        for (const auto& day:s.capacity->days) if (day.day>=setback.starts_on && day.day<setback.ends_before)
            for (const auto& used:day.sources) if (used.source_id==setback.source_id && used.effective>setback.reduced_capacity)
                invalid("Actual capacity history bypassed the dated setback");
        for (const auto& [kind,day]:std::array<std::pair<std::string_view,OrdinalDay>,3>{{{"setback_announced",setback.announced_on},{"setback_started",setback.starts_on},{"setback_ended",setback.ends_before}}}) {
            const auto found=std::find_if(m.notices.begin(),m.notices.end(),[&](const auto& n){return n.kind==kind && n.subject==setback.id && n.day==day;});
            if (s.date>=day && found==m.notices.end()) invalid("Scheduled setback lacks its actual notice/transition boundary");
        }
    }
}
}
