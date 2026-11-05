#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pgp::runtime {
using namespace core;
std::string_view to_string(HeavyweightPhase phase) noexcept {
    switch(phase) {case HeavyweightPhase::watch:return "watch";case HeavyweightPhase::mobilize:return "mobilize";case HeavyweightPhase::compete:return "compete";case HeavyweightPhase::consolidate:return "consolidate";}return "invalid";
}
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
template<class Function> void public_days(const SimulationState& state,OrdinalDay begin,OrdinalDay end,Function action) {
    if (!state.market) return;const auto first=std::max(begin,state.market->starts_on);
    for (auto day=first;day<end;day=advance_days(day,Days{1})) {
        const auto index=static_cast<std::uint64_t>(day.value()-state.market->starts_on.value());
        if (index>=state.market->days.size()) break;action(state.market->days[static_cast<std::size_t>(index)]);
    }
}
void transition(WorldState& world,RivalCompany& rival,HeavyweightPhase phase,std::string cause,std::optional<OrdinalDay> due={}) {
    auto& response=*rival.response;if (response.transitions.size()>=512) invalid("Heavyweight transition history exceeds its bound");
    response.phase=phase;response.transitions.push_back({world.simulation.date,phase,std::move(cause),due});
    world.competition->notices.push_back({rival.definition_id,response.transitions.back().cause=="warning"?"warning":"response",world.simulation.date,due,"response."+std::to_string(response.transitions.size())});
}
bool run(RivalCompany& rival,CommandAction action) {return competition_detail::submit_rival(rival,std::move(action));}
bool live(const RivalCompany& rival) {return rival.planning->entered_on && rival.simulation.phase!=DayPhase::closed && rival.simulation.phase!=DayPhase::finished;}
void retire(WorldState& world,RivalCompany& rival,const content::HeavyweightPolicy& policy) {
    if (rival.response->retired_on || !live(rival)) return;
    competition_detail::cancel_pending_competitive_offers(world,rival);
    for (const auto& offer:world.competition->offers) if (offer.company_id==rival.definition_id && offer.status==CompetitiveOfferStatus::active) return;
    if (!run(rival,WithdrawOperations{})) invalid("A commitment-free heavyweight could not withdraw its actual operations");
    rival.response->retired_on=world.simulation.date;
    transition(world,rival,HeavyweightPhase::consolidate,"withdrawn",advance_days(world.simulation.date,policy.cooldown));
}
void mobilize(WorldState& world,const CompiledProgram& program,RivalCompany& rival,const content::RivalDefinition& definition) {
    const auto& policy=*definition.response;const auto day=world.simulation.date;
    rival.response->mobilized_on=day;rival.response->ready_on=advance_days(day,policy.delivery_lead);
    rival.simulation=competition_detail::opening_rival(world,program,definition,day);
    rival.planning->entered_on=day;rival.planning->next_review=*rival.response->ready_on;
    transition(world,rival,HeavyweightPhase::mobilize,"mobilize",rival.response->ready_on);
    world.competition->notices.push_back({rival.definition_id,"entry",day,{}});
    // Capital is a single finite response allocation, not a recurring subsidy.
    // The paid team/lease accrue costs throughout the real delivery lead time.
    const auto lease=policy.lease_offer_id+"."+definition.id;
    if (!run(rival,HireSpecialist{policy.specialist_id}) || !run(rival,AcceptBusinessOffer{lease}) || !run(rival,SetEffectiveCapacity{"lease/"+lease,MilliCUPerDay{0}})) {
        transition(world,rival,HeavyweightPhase::consolidate,"resources_unaffordable");retire(world,rival,policy);
    }
}
}
PressureReview observe_heavyweight_pressure(const WorldState& world,const CompiledProgram& program,std::string_view id,OrdinalDay day) {
    const auto& policy=*rival_definition(program,id).response;PressureReview result;result.day=day;
    const auto begin=OrdinalDay{std::max(world.campaign_start.value(),day.value()-policy.review_interval.value())};
    const auto finance_begin=OrdinalDay{std::max(world.campaign_start.value(),day.value()-6*policy.review_interval.value())};
    std::vector<const SimulationState*> public_sources{&world.simulation};
    for (const auto& rival:world.competition->companies) if (rival.planning->entered_on && rival_definition(program,rival.definition_id).planning->group!="heavyweight") public_sources.push_back(&rival.simulation);
    public_days(world.simulation,begin,day,[&](const auto& record) {
        for (const auto& observed:record.segments) {
            const auto& segments=world.simulation.market->segments;
            const auto segment=std::find_if(segments.begin(),segments.end(),[&](const auto& row){return row.id==observed.segment_id;});
            if (segment==segments.end()) invalid("Public demand observation lost its authored segment");
            result.customer_demand+=public_market_demand(*world.simulation.market,*segment,record.day);
        }
    });
    std::set<std::string> services;
    for (const auto* source:public_sources) {
        public_days(*source,begin,day,[&](const auto& record) {
            const auto delivered=[&](std::string_view product,std::uint64_t tasks) {
                result.delivered_tasks+=tasks;
                const auto price=market_price_on(*source->market,product,record.day);
                result.substituted_revenue=checked_unit_add(result.substituted_revenue,Cents{mul_div_floor(static_cast<std::uint64_t>(price.value()),tasks,1).quotient});
                services.insert(source->market->owner+":"+std::string(product));
            };
            for (const auto& row:record.discretionary) if (row.tasks>0) delivered(row.product_id,row.tasks);
            for (const auto& row:record.contracts) if (row.delivered_tasks>0) {
                const auto contract=std::find_if(source->market->contracts.begin(),source->market->contracts.end(),[&](const auto& c){return c.id==row.contract_id;});
                if (contract==source->market->contracts.end()) invalid("Public customer record lost its contract");delivered(contract->product_id,row.delivered_tasks);
            }
        });
        if (source->management) for (const auto& accepted:source->management->financing) if (accepted.day>=finance_begin && accepted.day<day) {
            const auto& catalog=source->management->catalog.financing;
            const auto offer=std::find_if(catalog.begin(),catalog.end(),[&](const auto& row){return row.id==accepted.offer_id;});
            if (offer!=catalog.end() && offer->public_announcement) ++result.disclosed_rounds;
        }
    }
    result.public_services=static_cast<std::uint32_t>(services.size());
    const auto window=static_cast<std::uint64_t>(std::max<std::int64_t>(1,day.value()-begin.value()));
    const auto monthly=mul_div_floor(static_cast<std::uint64_t>(result.substituted_revenue.value()),30,window).quotient;
    result.components[0]=BasisPoints{std::min<std::uint64_t>(10000,mul_div_floor(monthly,10000,static_cast<std::uint64_t>(policy.revenue_at_risk.value())).quotient)};
    result.components[1]=BasisPoints{std::min<std::uint64_t>(10000,mul_div_floor(result.public_services,10000,policy.capability_target).quotient)};
    result.components[2]=BasisPoints{result.customer_demand==0?0:std::min<std::uint64_t>(10000,mul_div_floor(result.delivered_tasks,10000,result.customer_demand).quotient)};
    result.components[3]=BasisPoints{std::min<std::uint64_t>(10000,mul_div_floor(result.disclosed_rounds,10000,policy.financed_target).quotient)};
    std::uint64_t total=0;for (std::size_t i=0;i<4;++i) total+=mul_div_floor(static_cast<std::uint64_t>(result.components[i].value()),policy.weights[i],10000).quotient;
    result.score=BasisPoints{total};return result;
}
void update_heavyweight_responses(WorldState& world,const CompiledProgram& program) {
    if (!world.competition || !world.competition->setup || world.simulation.date>=world.simulation.end_date) return;
    const auto day=world.simulation.date;
    for (auto& rival:world.competition->companies) if (rival.response) {
        auto& response=*rival.response;const auto& definition=rival_definition(program,rival.definition_id);const auto& policy=*definition.response;
        if (rival.planning->entered_on && !response.retired_on && rival.simulation.phase==DayPhase::closed && response.phase!=HeavyweightPhase::consolidate)
            transition(world,rival,HeavyweightPhase::consolidate,"insolvency");
        if (day>=response.next_review) {
            if (response.reviews.size()>=8192) invalid("Heavyweight pressure reviews exceed their century bound");
            const auto observation=observe_heavyweight_pressure(world,program,rival.definition_id,day);response.reviews.push_back(observation);
            response.next_review=advance_days(response.next_review,policy.review_interval);
            response.high_reviews=observation.score>=policy.entry_pressure?response.high_reviews+1:0;
            response.low_reviews=observation.score<=policy.exit_pressure?response.low_reviews+1:0;
            if (response.phase==HeavyweightPhase::watch && !rival.planning->entered_on && day>=advance_days(world.campaign_start,definition.start_after)) {
                if (!response.warned_on && response.high_reviews>=2) {
                    response.warned_on=day;if (!rival.warned_on) rival.warned_on=day;
                    transition(world,rival,HeavyweightPhase::watch,"warning",advance_days(day,policy.warning_lead));
                } else if (response.warned_on && response.low_reviews>=2) {response.warned_on.reset();transition(world,rival,HeavyweightPhase::watch,"warning_withdrawn");}
            } else if ((response.phase==HeavyweightPhase::mobilize || response.phase==HeavyweightPhase::compete) && response.low_reviews>=2) {
                transition(world,rival,HeavyweightPhase::consolidate,"low_pressure");
            }
        }
        if (response.phase==HeavyweightPhase::watch && response.warned_on && !rival.planning->entered_on && day>=advance_days(*response.warned_on,policy.warning_lead)) mobilize(world,program,rival,definition);
        if (response.phase==HeavyweightPhase::mobilize && live(rival) && response.ready_on && day>=*response.ready_on) {
            std::vector<std::pair<std::string,MilliCUPerDay>> sources;for (const auto& source:rival.simulation.capacity->sources) sources.emplace_back(source.id,source.nominal);
            for (const auto& [id,capacity]:sources) if (!run(rival,SetEffectiveCapacity{id,capacity})) invalid("Paid delivered heavyweight capacity could not enter service");
            transition(world,rival,HeavyweightPhase::compete,"compete");
        }
        if (response.phase==HeavyweightPhase::compete && live(rival)) competition_detail::issue_competitive_offers(world,program,rival);
        if (response.phase==HeavyweightPhase::consolidate) {
            retire(world,rival,policy);
            if (response.retired_on && day>=advance_days(*response.retired_on,policy.cooldown)) transition(world,rival,HeavyweightPhase::watch,"program_ended");
        }
    }
}
}
