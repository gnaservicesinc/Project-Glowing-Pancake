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
bool accepted(const auto& rows,std::string_view id) {
    return std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.offer_id==id;});
}
void notice(ManagementState& state,std::string id,std::string kind,std::string subject,OrdinalDay day,
            std::optional<OrdinalDay> deadline={},bool required=false) {
    if (std::any_of(state.notices.begin(),state.notices.end(),[&](const auto& row){return row.id==id;})) return;
    if (state.notices.size()>=256) invalid("Management notice history is full");
    state.notices.push_back({std::move(id),std::move(kind),std::move(subject),day,deadline,required,false});
}
std::string evaluation_id(const DelegationPolicy& policy,std::uint32_t run) { return "delegated."+policy.id+"."+std::to_string(run); }
std::string replacement_id(const DelegationPolicy& policy) { return "replacement."+policy.id; }
bool empty_allocation(const DelegationPolicy& policy) {
    return policy.allocation.source_id.empty() && policy.allocation.daily_compute.value()==0 && policy.allocation.staff==0;
}
void policy_shape(const SimulationState& state,const DelegationPolicy& p) {
    const auto& catalog=state.management->catalog;
    if (p.id.empty() || p.id.size()>32 || p.budget.value()<0 || p.reserve_floor.value()<0 ||
        p.monthly_ceiling.value()<0 || p.maximum_runs==0 || p.runs>p.maximum_runs ||
        p.committed.value()<0 || p.committed>p.budget || p.interval.value()>365 ||
        p.created_on<state.accounts->starts_on || p.created_on>state.date || p.next_review<p.created_on)
        invalid("Policy needs finite authorization and valid dated bounds");
    if (p.kind!=DelegationKind::pricing && p.pricing) invalid("Only a price policy can authorize pricing bounds");
    if (p.kind==DelegationKind::operating_reserve) {
        if (p.reserve_floor.value()==0 || p.budget.value()!=0 || p.monthly_ceiling.value()!=0 ||
            !p.target_id.empty() || !p.plan_id.empty() || !empty_allocation(p) ||
            p.interval.value()!=0 || p.maximum_runs!=1 || p.runs!=0 || p.next_review!=p.created_on)
            invalid("An operating reserve needs a positive cash target and no spending authority");
        return;
    }
    if (p.target_id.empty()) invalid("Choose a target for this policy");
    if (p.kind==DelegationKind::hardware_replacement) {
        if (!state.infrastructure || p.interval!=Days{1} || p.maximum_runs!=2 || !empty_allocation(p) ||
            !std::ranges::all_of(p.id,[](unsigned char ch) {
                return (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='.' || ch=='-' || ch=='_';
            })) invalid("Replacement needs owned hardware, a valid identity and two finite actions");
        const auto terms=hardware_replacement_terms(state,p.target_id,p.plan_id);
        const auto& old=find(state.infrastructure->hardware,p.target_id);
        if (*old.commissioned_on>p.created_on || terms.purchase>p.budget || terms.monthly_maintenance>p.monthly_ceiling)
            invalid("Replacement requires a commissioned original rack and explicit purchase and monthly maintenance ceilings");
        return;
    }
    if (p.kind==DelegationKind::evaluation || p.kind==DelegationKind::lease_renewal) {
        if (p.budget.value()==0 || p.maximum_runs>catalog.maximum_policy_runs || p.interval<catalog.minimum_evaluation_interval)
            invalid("Evaluation and lease policies need bounded runs, spending and review intervals");
    }
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
    } else if (p.kind==DelegationKind::pricing) {
        const auto& product=find(state.market->products,p.target_id);
        if (!p.pricing || p.budget.value()!=0 || p.maximum_runs>pricing_review_limit || p.interval<Days{7} ||
            product.channel==ProductChannel::open_weights || !p.plan_id.empty() ||
            !p.allocation.source_id.empty() || p.allocation.daily_compute.value()!=0 || p.allocation.staff!=0 || p.monthly_ceiling.value()!=0)
            invalid("Pricing needs explicit price bounds, a paid product and no spending or work allocation");
        const auto& b=*p.pricing;
        if (b.floor.value()<0 || b.ceiling.value()>market_price_limit || b.floor>=b.ceiling || b.step.value()<=0 ||
            b.step>checked_unit_subtract(b.ceiling,b.floor) || b.low_utilization>=b.high_utilization || b.high_utilization.value()>10000)
            invalid("Choose an ordered price range, a positive step within it, and ordered utilization thresholds from 0 to 10000");
    } else invalid("Unsupported delegation policy");
}
void pricing_ready(const SimulationState& s,const DelegationPolicy& p) {
    const auto& product=find(s.market->products,p.target_id);
    if (!product.available || find(s.capacity->projects,product.capacity_project_id).status!=ProjectStatus::active)
        invalid("Pricing delegation needs a currently operating product");
    if (product.price_per_task<p.pricing->floor || product.price_per_task>p.pricing->ceiling)
        invalid("Set the current price inside the authorized range before enabling this policy");
    if (std::any_of(s.management->policies.begin(),s.management->policies.end(),[&](const auto& other){
        return other.kind==DelegationKind::pricing && other.enabled && other.target_id==p.target_id && other.id!=p.id;
    })) invalid("Suspend the other enabled price policy for this product first");
}
bool replacement_pending(const SimulationState& s,const DelegationPolicy& policy) {
    return policy.runs==1 && !find(s.infrastructure->hardware,replacement_id(policy)).retired_on;
}
void replacement_ready(const SimulationState& s,const DelegationPolicy& policy) {
    const auto& hardware=s.infrastructure->hardware;
    if (find(hardware,policy.target_id).retired_on) invalid("The original rack was already retired");
    const auto id=replacement_id(policy);
    const auto order=std::ranges::find(hardware,id,&HardwareOrder::id);
    if (policy.runs==0 && order!=hardware.end()) invalid("The replacement order identity is already in use");
    if (policy.runs==1 && (order==hardware.end() || order->retired_on)) invalid("The replacement order is no longer available");
    if (std::ranges::any_of(s.management->policies,[&](const auto& other) {
        return other.kind==DelegationKind::hardware_replacement && other.target_id==policy.target_id &&
            other.id!=policy.id && (other.enabled || replacement_pending(s,other));
    })) invalid("An enabled policy or paid replacement already covers this rack");
}
bool decision_room(const SimulationState& state) {
    const auto reserved_slot=operating_reserve_target(state).value()>0?1U:0U;
    return state.management->decisions.size()<management_decision_limit-reserved_slot;
}
void record_reserve_control(SimulationState& state,const DelegationPolicy& policy) {
    state.management->decisions.push_back({policy.id,policy.enabled?"reserve_enabled":"reserve_suspended",
        policy.enabled?"Operating cash protected from new spending. Accepted bills and principal can still be paid.":"Operating reserve released for new spending.",
        state.date,Cents{0},true});
}
void check_enable_capacity(const ManagementState& state) {
    if (std::ranges::count_if(state.policies,[](const auto& p){return p.enabled;})>=static_cast<std::ptrdiff_t>(enabled_policy_limit))
        invalid("Suspend an enabled policy before enabling another");
}
}
Cents operating_reserve_target(const SimulationState& state) {
    if (state.management) for (const auto& policy:state.management->policies)
        if (policy.kind==DelegationKind::operating_reserve && policy.enabled) return policy.reserve_floor;
    return Cents{0};
}
Cents discretionary_cash(const SimulationState& state) {
    return std::max(Cents{0},checked_unit_subtract(uncommitted_cash(state),operating_reserve_target(state)));
}
void require_operating_reserve(Cents cash_before,const SimulationState& after) {
    const auto target=operating_reserve_target(after);
    if (target.value()==0) return;
    const auto available=uncommitted_cash(after);
    if (available<target && available<cash_before)
        invalid("New spending would use the operating reserve. Review the policy in Office or choose a smaller commitment.");
}
void validate_management_catalog(const ManagementCatalog& c) {
    if(c.staff_workload) validate_staff_workload(*c.staff_workload);
    validate_funding_offers(c.support_offers);
    if (c.service_monitoring) validate_service_monitoring(*c.service_monitoring);
    if (c.corporate_tax) validate_corporate_tax_policy(*c.corporate_tax);
    if(c.succession) {
        validate_succession_catalog(*c.succession);
        if(c.governance && std::ranges::none_of(c.governance->board,[&](const auto& b){return b.id==c.succession->founder_board && b.player;}))
            invalid("The succession charter must name the founder's actual board rights.");
        if(c.governance && std::ranges::any_of(c.governance->board,[](const auto& b){return b.id=="leadership.ceo";}))
            invalid("The leadership.ceo board identity is reserved for the current successor.");
    }
    if (c.governance) validate_governance_catalog(*c.governance);
    if (c.default_policy_budget.value()<=0 || c.default_reserve_floor.value()<0 || c.default_monthly_ceiling.value()<=0 ||
        c.default_policy_runs==0 || c.default_policy_runs>c.maximum_policy_runs || c.financing.size()>8 || c.specialists.size()>8 || c.setbacks.size()>8 || c.maximum_policy_runs==0 || c.maximum_policy_runs>12 ||
        c.minimum_evaluation_interval.value()<7 || c.minimum_evaluation_interval.value()>365 || c.scoring.task_target==0 ||
        c.scoring.task_target>market_task_limit*100 || c.scoring.founder.empty() || c.scoring.opening_founder_share.value()==0)
        invalid("Management catalog has invalid bounds or scoring targets");
    std::uint32_t weights=0;for (auto w:c.scoring.weights) {if (w>100) invalid("Slice scoring weight exceeds its bound");weights+=w;}
    if (weights!=100) invalid("Slice score weights must sum to 100");
    std::set<std::string> ids;
    for (const auto& offer:c.support_offers) ids.insert(offer.id);
    for (const auto& row:c.financing) {
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || !ids.insert(row.id).second ||
            row.proceeds.value()<=0 || row.proceeds.value()>1'000'000'000 || row.term.value()>1096 ||
            (row.term.value()==0 ? (row.acquired.value()==0 || row.acquired.value()>5000 || row.monthly_interest.value()!=0) : row.acquired.value()!=0))
            invalid("Financing offers need bounded disclosed cash, dilution or debt terms");
    }
    for (const auto& row:c.specialists)
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || !ids.insert(row.id).second || row.signing_cost.value()<0 ||
            row.signing_cost.value()>10'000'000 || row.monthly_salary.value()<=0 || row.monthly_salary.value()>10'000'000 ||
            row.start_lead>Days{90} || row.onboarding>Days{90} || row.evaluation_credit.value()>2500)
            invalid("Specialist offer needs distinct identity and bounded payroll, lead time and onboarding");
    std::map<std::string,OrdinalDay> last;
    for (const auto& row:c.setbacks) {
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || row.source_id.empty() || !ids.insert(row.id).second ||
            row.announced_on>=row.starts_on || row.starts_on>=row.ends_before || row.reduced_capacity.value()==0 ||
            (last.contains(row.source_id) && last.at(row.source_id)>row.announced_on)) invalid("Setbacks need a visible advance warning and nonoverlapping finite source intervals");
        (void)from_ordinal(row.announced_on);(void)from_ordinal(row.ends_before);last.insert_or_assign(row.source_id,row.ends_before);
    }
}
void take_financing(SimulationState& s,std::string_view id) {
    auto& m=management(s);
    const auto support=std::ranges::find(m.catalog.support_offers,id,&FundingSupportOffer::id);
    if (support!=m.catalog.support_offers.end()) {
        accept_funding_support(s,*support);
        return;
    }
    const auto& offer=find(m.catalog.financing,id);
    if (accepted(m.financing,id)) invalid("This financing offer has already been used; terms cannot be rerolled");
    if (offer.term.value()==0) company_equity(s,"investor/"+offer.id,offer.proceeds,offer.acquired,"financing/"+offer.id);
    else draw_debt(*s.accounts,s.date,{"financing/"+offer.id,offer.proceeds,offer.monthly_interest,s.date,advance_days(s.date,offer.term),0,std::nullopt},"Financing: "+offer.name);
    m.financing.push_back({offer.id,s.date});s.cash=accounts_report(*s.accounts).cash;
}
SpecialistSchedule specialist_schedule(const SpecialistOffer& offer,OrdinalDay signed_on) {
    const auto starts_on=advance_days(signed_on,offer.start_lead);
    return {starts_on,advance_days(starts_on,offer.onboarding)};
}
void hire_specialist(SimulationState& s,std::string_view id,Cents protected_cash) {
    auto& m=management(s);const auto& offer=find(m.catalog.specialists,id);
    if (accepted(m.specialists,id) || offer.signing_cost>checked_unit_subtract(s.cash,protected_cash)) invalid("Specialist was hired already or signing cost exceeds available cash");
    const auto schedule=specialist_schedule(offer,s.date);
    if (schedule.ready_on>=s.end_date) invalid("This specialist would finish onboarding after the last workday");
    if (offer.signing_cost.value()>0) record_cash(*s.accounts,s.date,Cents{-offer.signing_cost.value()},Account::expenses,"Specialist signing: "+offer.name,"specialist/"+offer.id);
    add_recurring(*s.accounts,{"specialist/"+offer.id,offer.monthly_salary,schedule.starts_on,std::nullopt,"Specialist salary: "+offer.name});
    if (schedule.ready_on==s.date) add_staff_capacity(*s.capacity,s.date,"specialist/"+offer.id,1,offer.evaluation_credit);
    m.specialists.push_back({offer.id,s.date});s.cash=accounts_report(*s.accounts).cash;
}
void withdraw_pending_specialists(SimulationState& s) {
    if (!s.management) return;
    auto& m=*s.management;
    for (auto& hire:m.specialists)
        if (!hire.withdrawn_on && s.date<specialist_schedule(find(m.catalog.specialists,hire.offer_id),hire.day).ready_on)
            hire.withdrawn_on=s.date;
}
void add_delegation_policy(SimulationState& s,DelegationPolicy p) {
    auto& m=management(s);
    if (m.policies.size()>=management_policy_limit || std::any_of(m.policies.begin(),m.policies.end(),[&](const auto& row){return row.id==p.id;}))
        invalid("Choose a distinct policy within the saved authorization limit");
    check_enable_capacity(m);
    p.created_on=s.date;p.next_review=advance_days(s.date,Days{1});p.runs=0;p.committed=Cents{0};p.blocked.clear();p.enabled=true;
    if (p.kind==DelegationKind::operating_reserve) p.next_review=s.date;
    policy_shape(s,p);
    if (p.kind==DelegationKind::pricing) pricing_ready(s,p);
    if (p.kind==DelegationKind::operating_reserve) {
        if (operating_reserve_target(s).value()>0) invalid("Suspend the current operating reserve before authorizing a new target");
        if (m.decisions.size()+2>management_decision_limit) invalid("The policy audit has no room for both reserve authorization and release");
        record_reserve_control(s,p);
    }
    if (p.kind==DelegationKind::hardware_replacement) {
        replacement_ready(s,p);
        p.next_review=std::max(p.next_review,hardware_replacement_terms(s,p.target_id,p.plan_id).eligible_on);
        if (p.next_review>=s.end_date) invalid("The original rack reaches its rated useful life after the campaign");
    }
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
    if (enabled==p.enabled) return;
    if (enabled) {
        check_enable_capacity(m);
        const bool spending=p.kind==DelegationKind::evaluation || p.kind==DelegationKind::lease_renewal;
        if (p.runs>=p.maximum_runs || (spending && p.committed>=p.budget))
            invalid("This policy exhausted its authorized runs or budget. Its counters cannot reset.");
        if (p.kind==DelegationKind::pricing) pricing_ready(s,p);
        if (p.kind==DelegationKind::hardware_replacement) replacement_ready(s,p);
        if (p.kind==DelegationKind::operating_reserve) {
            if (operating_reserve_target(s).value()>0) invalid("Suspend the other operating reserve before enabling this one");
            if (m.decisions.size()+2>management_decision_limit) invalid("The policy audit has no room for both reserve authorization and release");
        }
    }
    p.enabled=enabled;if (enabled) p.blocked.clear();
    if (p.kind==DelegationKind::operating_reserve) record_reserve_control(s,p);
}
void suspend_pricing_policies(SimulationState& s,std::string_view product) {
    if (!s.management) return;
    for (auto& p:s.management->policies) if (p.kind==DelegationKind::pricing && p.enabled && p.target_id==product) {
        if (!decision_room(s)) invalid("Policy audit is full. Suspend the price policy before changing the quote.");
        p.enabled=false;p.blocked="Player override: pricing policy suspended; bounds and counters retained.";
        s.management->decisions.push_back({p.id,"",p.blocked,s.date,Cents{0},false,std::nullopt,true});
    }
}
void review_delegation(SimulationState& s,Cents protected_cash) {
    if (!s.management || s.phase!=DayPhase::work_ready || required_management_notice(s) || required_succession_notice(s) ||
        (s.liquidity && liquidity_report(*s.liquidity).pending_warning) || s.management->reviewed_on==s.date) return;
    s.management->reviewed_on=s.date;
    for (std::size_t i=0;i<s.management->policies.size();++i) {
        auto& p=s.management->policies[i];
        if (!p.enabled || p.kind==DelegationKind::operating_reserve || p.runs>=p.maximum_runs || p.next_review>s.date) continue;
        if (!decision_room(s)) {
            p.enabled=false;
            p.blocked="The finite policy decision history is full.";
            continue;
        }
        if (p.kind==DelegationKind::evaluation) {
            const auto& model=find(s.lab->models,p.target_id);
            if (!model.project_id.empty()) {
                const auto status=find(s.capacity->projects,model.project_id).status;
                if(status==ProjectStatus::active || status==ProjectStatus::paused) continue;
            }
            if (p.runs>0) {
                const auto& last=find(s.lab->evaluations,evaluation_id(p,p.runs));
                const auto status=find(s.capacity->projects,last.project_id).status;
                if(status==ProjectStatus::active || status==ProjectStatus::paused) continue;
            }
        }
        try {
            const bool replacement=p.kind==DelegationKind::hardware_replacement;
            if (replacement) {
                replacement_ready(s,p);
                if (p.runs==1) {
                    p.blocked=hardware_retirement_wait(s,p.target_id,replacement_id(p));
                    if (!p.blocked.empty()) continue;
                }
            }
            Cents cost{0};
            std::string result_id;
            std::string explanation="Authorized action committed within cash, staff and compute bounds.";
            std::optional<std::uint64_t> price_cursor;
            if (p.kind==DelegationKind::evaluation) cost=find(s.lab->catalog.evaluations,p.plan_id).cash;
            else if (p.kind==DelegationKind::lease_renewal) {
                const auto& offer=find(s.business_offers->catalog,p.target_id);
                if (offer.monthly_amount>p.monthly_ceiling) invalid("Lease renewal exceeds the authorized monthly ceiling");
                cost=lease_commitment_ceiling(offer);
            } else if (replacement && p.runs==0) {
                cost=hardware_replacement_terms(s,p.target_id,p.plan_id).purchase;
            }
            const auto available=checked_unit_subtract(s.cash,protected_cash);
            const auto floor=cost.value()>0?std::max(p.reserve_floor,operating_reserve_target(s)):p.reserve_floor;
            if (cost>checked_unit_subtract(p.budget,p.committed) ||
                (!(replacement && p.runs==1) && (available<floor || cost>checked_unit_subtract(available,floor))))
                invalid("Delegated commitment exceeds the remaining total budget or cash reserve floor");
            // Waits only inspect current commitments. Copy a candidate for an actual action.
            auto candidate=s;
            auto& proposed=candidate.management->policies[i];
            proposed.next_review=advance_days(s.date,p.interval);
            if (p.kind==DelegationKind::evaluation) {
                result_id=evaluation_id(p,p.runs+1);
                start_evaluation(*candidate.lab,*candidate.accounts,*candidate.capacity,s.date,result_id,p.target_id,p.plan_id,p.allocation,protected_cash);
            } else if (p.kind==DelegationKind::lease_renewal) {
                renew_compute_lease(*candidate.business_offers,*candidate.accounts,*candidate.capacity,s.date,p.target_id,protected_cash);
                result_id=std::to_string(candidate.business_offers->renewals.size());
                proposed.next_review=lease_expiry(*candidate.business_offers,p.target_id);
            } else if (replacement) {
                if (p.runs==0) {
                    result_id=replacement_id(p);
                    order_hardware_replacement(candidate,p.target_id,p.plan_id,result_id,protected_cash);
                    const auto delivery=find(candidate.infrastructure->hardware,result_id).delivered_on;
                    proposed.next_review=std::max(proposed.next_review,delivery);
                    explanation="Replacement purchased. The old rack stays installed until its replacement is operational and its own commitments clear.";
                } else {
                    result_id=p.target_id;
                    retire_owned_hardware(candidate,p.target_id);
                    explanation="Obsolete rack retired after its replacement supplied the required capacity and all dependent work cleared.";
                }
                prepare_infrastructure(candidate);
            } else {
                pricing_ready(s,p);
                const auto review=pricing_review(s,p,s.date,find(s.market->products,p.target_id).price_per_task);
                set_market_price(*candidate.market,s.date,p.target_id,review.quote,p.id);
                price_cursor=candidate.market->price_events.size();explanation=review.explanation;
            }
            ++proposed.runs;proposed.committed=checked_unit_add(proposed.committed,cost);
            proposed.blocked.clear();
            if (proposed.runs>=proposed.maximum_runs) proposed.enabled=false;
            candidate.cash=accounts_report(*candidate.accounts).cash;
            candidate.management->decisions.push_back({p.id,result_id,std::move(explanation),s.date,cost,true,price_cursor});
            validate_simulation(candidate);s=std::move(candidate);
        } catch (const std::invalid_argument& error) {
            auto& refused=s.management->policies[i];refused.enabled=false;refused.blocked=error.what();
            s.management->decisions.push_back({refused.id,"",refused.blocked,s.date,Cents{0},false});
            if (s.management->notices.size()<128)
                notice(*s.management,"policy.blocked/"+refused.id+"/"+std::to_string(s.management->decisions.size()),"policy_blocked",refused.id,s.date);
        }
    }
}
void prepare_management(SimulationState& s) {
    if (!s.management) return;
    auto& m=*s.management;
    for (const auto& hire:m.specialists) {
        const auto& offer=find(m.catalog.specialists,hire.offer_id);
        if (hire.withdrawn_on || specialist_schedule(offer,hire.day).ready_on!=s.date) continue;
        const auto id="specialist/"+hire.offer_id;
        if (std::ranges::find(s.capacity->staff_additions,id,&StaffAddition::id)==s.capacity->staff_additions.end())
            add_staff_capacity(*s.capacity,s.date,id,1,offer.evaluation_credit);
    }
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
    prepare_service_monitoring(s);
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
    std::array<bool,introductory_lab_domains> covered{};
    if (s.deployments) for (const auto& d:s.deployments->records) {
        const auto& e=find(s.lab->evaluations,d.evidence_id);const auto& plan=find(s.lab->catalog.evaluations,e.plan_id);
        for (std::size_t i=0;i<covered.size();++i) if (plan.independent && plan.coverage[i].value()>0 && e.lower[i]>=plan.required[i]) covered[i]=true;
    }
    score.points[3]=static_cast<std::uint32_t>(std::count(covered.begin(),covered.end(),true))*rubric.weights[3]/static_cast<std::uint32_t>(covered.size());
    score.points[4]=static_cast<std::uint32_t>(mul_div_floor(static_cast<std::uint64_t>(std::min(score.founder_share,rubric.opening_founder_share).value()),rubric.weights[4],static_cast<std::uint64_t>(rubric.opening_founder_share.value())).quotient);
    for (const auto point:score.points) score.total+=point;
    return score;
}

void validate_management(const SimulationState& s,bool deep) {
    if (!s.management) {
        if (s.accounts && s.accounts->taxes) invalid("Corporate taxes require the scenario's management policy");
        if (s.market && std::any_of(s.market->price_events.begin(),s.market->price_events.end(),[](const auto& e){return !e.actor.empty();}))
            invalid("A delegated price event needs its management authorization");
        return;
    }
    if (!s.accounts || !s.capacity || !s.lab || !s.market || !s.deployments) invalid("Management requires the playable company modules");
    const auto& m=*s.management;
    if(m.catalog.staff_workload.has_value()!=s.capacity->workload.has_value() ||
        (s.capacity->workload && s.capacity->workload->rules!=*m.catalog.staff_workload))
        invalid("Staff workload differs from the authored company policy");
    if (m.catalog.corporate_tax.has_value()!=s.accounts->taxes.has_value() ||
        (s.accounts->taxes && s.accounts->taxes->policy!=*m.catalog.corporate_tax))
        invalid("Corporate taxes differ from the authored management policy");
    if (m.financing.size()>m.catalog.financing.size() || m.specialists.size()>m.catalog.specialists.size() || m.policies.size()>management_policy_limit ||
        m.decisions.size()>management_decision_limit || m.notices.size()>256 || (m.reviewed_on && (*m.reviewed_on<s.accounts->starts_on || *m.reviewed_on>s.date)))
        invalid("Management history exceeds its bounds or daily cursor");
    if (!deep) return;
    if (std::ranges::count_if(m.policies,[](const auto& p){return p.enabled;})>static_cast<std::ptrdiff_t>(enabled_policy_limit) ||
        (operating_reserve_target(s).value()>0 && m.decisions.size()==management_decision_limit))
        invalid("Active policies exceed the limit or the reserve has no remaining release slot");
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
    // Regional contract teams are reconciled with their fees, payroll and
    // operating agreements by the world actor. Management owns ordinary hires.
    std::size_t ready_specialists=0;
    for (const auto& row:m.specialists) {
        const auto& offer=find(m.catalog.specialists,row.offer_id);
        if (!ids.insert(row.offer_id).second || row.day<s.accounts->starts_on || row.day>s.date) invalid("Specialist acceptance repeats or has an invalid date");
        const auto schedule=specialist_schedule(offer,row.day);
        const auto id="specialist/"+offer.id;
        const auto added=std::ranges::find(s.capacity->staff_additions,id,&StaffAddition::id);
        const auto& salary=find(s.accounts->recurring,id);
        const bool ready=!row.withdrawn_on && schedule.ready_on<=s.date;
        if (ready) ++ready_specialists;
        if (schedule.ready_on>=s.end_date || ready!=(added!=s.capacity->staff_additions.end()) ||
            (ready && (added->day!=schedule.ready_on || added->people!=1 || added->evaluation_credit!=offer.evaluation_credit)) ||
            salary.starts_on!=schedule.starts_on ||
            salary.monthly_amount!=offer.monthly_salary ||
            (offer.signing_cost.value()>0 && !paid(id,row.day,offer.signing_cost,Account::expenses,Account::cash)))
            invalid("Specialist staffing and payroll differ from the signed hiring schedule");
        if (row.withdrawn_on) {
            const auto withdrawn=*row.withdrawn_on;
            if (withdrawn<row.day || withdrawn>s.date || withdrawn>=schedule.ready_on ||
                salary.ends_before!=std::max(schedule.starts_on,withdrawn) ||
                std::ranges::none_of(s.receipts,[&](const auto& receipt){return receipt.day==withdrawn && receipt.cause=="withdraw_operations";}))
                invalid("A pending specialist withdrawal needs a matching operations withdrawal and salary end");
        } else if (salary.ends_before &&
            (!ready || std::ranges::none_of(s.capacity->staff_departures,[&](const auto& departure) {
                return departure.day==*salary.ends_before && departure.day>=schedule.ready_on;
            }))) invalid("Specialist payroll ended without a dated staff departure");
        if (salary.ends_before) for (const auto& work:s.capacity->projects)
            if (work.specialist_id==id && (!work.stopped_on || *work.stopped_on>*salary.ends_before ||
                (work.status==ProjectStatus::completed && *work.stopped_on==*salary.ends_before)))
                invalid("An evaluation retained specialist expertise after the paid appointment ended");
    }
    const auto management_hires=std::ranges::count_if(s.capacity->staff_additions,
        [](const auto& row){return !row.id.starts_with(authority_hire_prefix);});
    if (static_cast<std::size_t>(management_hires)!=ready_specialists+(m.governance?m.governance->appointments.size():0))
        invalid("Team additions lack their actual onboarded specialists or executives");
    std::map<std::string_view,std::vector<const DelegationDecision*>> decisions_by_policy;
    std::optional<OrdinalDay> previous_decision_day;
    for (const auto& d:m.decisions) {
        if (previous_decision_day && d.day<*previous_decision_day) invalid("Policy decision history is out of order");
        previous_decision_day=d.day;
        decisions_by_policy[d.policy_id].push_back(&d);
    }
    std::set<std::string> enabled_prices,replacing_hardware;
    bool reserve_enabled=false;
    std::set<std::uint64_t> price_references;
    for (const auto& p:m.policies) {
        if (!ids.insert(p.id).second) invalid("Delegation policy identity is duplicated");
        policy_shape(s,p);
        if (p.kind==DelegationKind::pricing && p.enabled && !enabled_prices.insert(p.target_id).second)
            invalid("Multiple enabled policies cannot set the same product price");
        if (p.kind==DelegationKind::hardware_replacement && (p.enabled || replacement_pending(s,p)) &&
            !replacing_hardware.insert(p.target_id).second)
            invalid("Multiple live replacements target the same rack");
        std::uint32_t runs=0;Cents committed{0};std::optional<OrdinalDay> last;
        auto expected_review=advance_days(p.created_on,Days{1});
        if (p.kind==DelegationKind::hardware_replacement)
            expected_review=std::max(expected_review,hardware_replacement_terms(s,p.target_id,p.plan_id).eligible_on);
        const auto& decisions=decisions_by_policy[p.id];
        bool reserve_active=false;
        for (const auto* entry:decisions) {
            const auto& d=*entry;
            if (d.day<p.created_on || d.day>s.date || d.explanation.empty() || d.explanation.size()>1024 ||
                (!d.accepted && (d.commitment.value()!=0 || !d.result_id.empty() || d.price_event_cursor)) ||
                (d.overridden && (d.accepted || p.kind!=DelegationKind::pricing)) ||
                (p.kind!=DelegationKind::pricing && d.price_event_cursor)) invalid("Delegation decision has invalid date or refusal state");
            if (p.kind==DelegationKind::operating_reserve) {
                if (!d.accepted || d.commitment.value()!=0 ||
                    d.result_id!=(reserve_active?"reserve_suspended":"reserve_enabled") ||
                    (entry==decisions.front() && d.day!=p.created_on))
                    invalid("Reserve history must alternate actual authorization and release");
                reserve_active=!reserve_active;
                continue;
            }
            if (!d.accepted) continue;
            ++runs;committed=checked_unit_add(committed,d.commitment);
            if (last && d.day<advance_days(*last,p.kind==DelegationKind::lease_renewal?Days{1}:p.interval)) invalid("Delegation replay repeats within its interval");
            last=d.day;
            if (p.kind==DelegationKind::evaluation) {
                const auto& evidence=find(s.lab->evaluations,d.result_id);const auto& work=find(s.capacity->projects,evidence.project_id);
                if (d.result_id!=evaluation_id(p,runs) || evidence.model_id!=p.target_id || evidence.plan_id!=p.plan_id || work.starts_on!=d.day ||
                    work.source_id!=p.allocation.source_id || work.requested!=p.allocation.daily_compute || work.staff!=p.allocation.staff ||
                    d.commitment!=find(s.lab->catalog.evaluations,p.plan_id).cash) invalid("Delegated evaluation differs from its actual bounded project/payment");
            } else if (p.kind==DelegationKind::lease_renewal) {
                const auto it=std::find_if(s.business_offers->renewals.begin(),s.business_offers->renewals.end(),[&](const auto& r){return r.offer_id==p.target_id && r.renewed_on==d.day;});
                if (it==s.business_offers->renewals.end() || d.result_id!=std::to_string(static_cast<std::size_t>(it-s.business_offers->renewals.begin())+1) ||
                    d.commitment!=lease_commitment_ceiling(find(s.business_offers->catalog,p.target_id))) invalid("Delegated renewal lacks its actual lease commitment");
            } else if (p.kind==DelegationKind::hardware_replacement) {
                const auto& old=find(s.infrastructure->hardware,p.target_id);
                const auto& order=find(s.infrastructure->hardware,replacement_id(p));
                if (runs>2 || d.day<expected_review || order.site_id!=old.site_id || order.family_id!=p.plan_id)
                    invalid("Replacement decision lacks its authorized order or date");
                if (runs==1) {
                    if (d.result_id!=order.id || order.ordered_on!=d.day || d.commitment!=order.paid ||
                        (old.retired_on && *old.retired_on<d.day))
                        invalid("Replacement purchase differs from its paid hardware order");
                    expected_review=std::max(advance_days(d.day,Days{1}),order.delivered_on);
                } else {
                    if (d.result_id!=old.id || old.retired_on!=d.day || !order.commissioned_on ||
                        *order.commissioned_on>d.day || (order.retired_on && *order.retired_on<d.day) || d.commitment.value()!=0)
                        invalid("Replacement retirement lacks the commissioned successor or actual disposal");
                    expected_review=advance_days(d.day,Days{1});
                }
            } else {
                if (d.commitment.value()!=0 || !d.result_id.empty() || !d.price_event_cursor || *d.price_event_cursor==0 ||
                    *d.price_event_cursor>s.market->price_events.size() || !price_references.insert(*d.price_event_cursor).second ||
                    d.day<=p.created_on) invalid("Price decision lacks a distinct dated audit event");
                const auto& event=s.market->price_events[static_cast<std::size_t>(*d.price_event_cursor-1)];
                if (event.actor!=p.id || event.product_id!=p.target_id || event.day!=d.day ||
                    event.before<p.pricing->floor || event.before>p.pricing->ceiling || event.after<p.pricing->floor || event.after>p.pricing->ceiling)
                    invalid("Price decision differs from its product, authorization or bounds");
                const auto replay=pricing_review(s,p,d.day,event.before);
                if (replay.quote!=event.after || replay.explanation!=d.explanation) invalid("Price decision differs from settled operating evidence");
            }
        }
        if (p.kind==DelegationKind::operating_reserve) {
            if (decisions.empty() || reserve_active!=p.enabled || (reserve_enabled && p.enabled))
                invalid("The operating reserve differs from its authorization history");
            reserve_enabled=reserve_enabled || p.enabled;
            expected_review=p.created_on;
        }
        if ((p.kind==DelegationKind::pricing || p.kind==DelegationKind::evaluation) && last)
            expected_review=advance_days(*last,p.interval);
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
        decisions_by_policy.erase(p.id);
    }
    if (!decisions_by_policy.empty()) invalid("Policy decisions refer to an unavailable authorization");
    for (std::size_t i=0;i<s.market->price_events.size();++i)
        if (!s.market->price_events[i].actor.empty() && !price_references.contains(i+1)) invalid("Delegated price event lacks an accepted policy decision");
    std::set<std::string> notice_ids;
    for (const auto& n:m.notices) {
        if (n.id.empty() || n.id.size()>256 || !notice_ids.insert(n.id).second || n.day<s.accounts->starts_on || n.day>s.date ||
            (n.deadline && *n.deadline<n.day)) invalid("Management notices require unique actual dates/deadlines");
        if (n.kind=="setback_announced" || n.kind=="setback_started" || n.kind=="setback_ended") {
            const auto& setback=find(m.catalog.setbacks,n.subject);
            const auto expected_day=n.kind=="setback_announced"?setback.announced_on:n.kind=="setback_started"?setback.starts_on:setback.ends_before;
            const auto expected_deadline=n.kind=="setback_announced"?std::optional{setback.starts_on}:n.kind=="setback_started"?std::optional{setback.ends_before}:std::nullopt;
            if (n.day!=expected_day || n.deadline!=expected_deadline || n.required!=(n.kind=="setback_started")) invalid("Setback notice differs from its actual warning/work/recovery dates");
        } else if (n.kind==service_workload_notice) {
            if (!service_workload_alert(s,n)) invalid("Service warning lacks a settled same-release performance decline");
        } else if (n.kind==service_monitoring_limit_notice) {
            const auto prior_count=static_cast<std::size_t>(&n-m.notices.data());
            if (!m.catalog.service_monitoring || n.id!=service_monitoring_limit_id ||
                n.subject!=service_monitoring_limit_subject || !n.required || n.deadline ||
                prior_count<service_monitoring_notice_limit ||
                std::any_of(m.notices.begin(),m.notices.begin()+static_cast<std::ptrdiff_t>(prior_count),
                    [&](const auto& prior) { return prior.day>n.day; }))
                invalid("Monitoring suspension lacks an actual notice-history limit");
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
