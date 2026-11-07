#include <pgp/runtime/society.hpp>
#include <pgp/runtime/authority.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(std::string message) { throw std::invalid_argument(std::move(message)); }
template<class Rows> const auto* named(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    return it==rows.end()?nullptr:&*it;
}
bool in(const std::vector<std::string>& group,std::string_view id) {
    return std::find(group.begin(),group.end(),id)!=group.end();
}
const DeploymentRecord* deployed(const SimulationState& state,std::string_view service,OrdinalDay day) {
    if(!state.deployments) return nullptr;
    for(auto it=state.deployments->records.rbegin();it!=state.deployments->records.rend();++it)
        if(it->service_id==service && it->deployed_on<=day) return &*it;
    return nullptr;
}
bool qualified(const SocietyState& society,const SimulationState& state,std::string_view service,OrdinalDay day) {
    const auto* deployment=deployed(state,service,day);
    const auto* report=deployment?society_evidence(state,deployment->evidence_id,society.catalog.forum.evidence_axis,day):nullptr;
    return report && report->model_id==deployment->model_id &&
        report->lower[society.catalog.forum.evidence_axis]>=society.catalog.forum.evidence_floor;
}
std::string media_key(std::uint64_t id) { return "society.media/"+std::to_string(id); }
std::string forum_key(std::uint64_t id,std::string_view region) { return "society.forum/"+std::to_string(id)+"/"+std::string(region); }
bool same_month(OrdinalDay a,OrdinalDay b) {
    const auto x=from_ordinal(a),y=from_ordinal(b);return x.year==y.year && x.month==y.month;
}
void pay(SimulationState& state,Cents amount,std::string cause,std::string key) {
    if(!state.accounts) invalid("Public media actions require company accounts.");
    Cents reserved{0};for(const auto& row:state.reservations) reserved=checked_unit_add(reserved,row.reserved);
    if(amount>checked_unit_subtract(state.cash,reserved)) invalid("The disclosed fee exceeds unreserved company cash.");
    record_cash(*state.accounts,state.date,Cents{-amount.value()},Account::expenses,std::move(cause),std::move(key));
    state.cash=accounts_report(*state.accounts).cash;
}
MediaCampaign media_terms(const WorldState& world,const BuyMediaCampaign& request,OrdinalDay day,
    std::span<const MediaCampaign> prior,bool check_active=true,std::uint64_t saved_deployment=0) {
    const auto& society=*world.society;
    (void)society_region_index(society.catalog,request.region);
    const auto& state=world.simulation;
    const auto* deployment=saved_deployment && state.deployments && saved_deployment<=state.deployments->records.size()?
        &state.deployments->records[static_cast<std::size_t>(saved_deployment-1)]:deployed(state,request.service,day);
    const auto* product=state.market?named(state.market->products,request.service):nullptr;
    const auto* report=society_evidence(state,request.evidence,society.catalog.forum.evidence_axis,day);
    if(request.audience>=society_audiences || !deployment || !product || (check_active && !product->available) ||
        deployment->service_id!=request.service || deployment->deployed_on>day ||
        (!deployment->region_id.empty() && deployment->region_id!=request.region) ||
        deployment->evidence_id!=request.evidence || !report || report->model_id!=deployment->model_id)
        invalid("Choose a deployed service in this region, its completed independent report and a named audience.");
    if(product->channel==society.catalog.forum.channel && forum_enforced(society,request.region,day) &&
        report->lower[society.catalog.forum.evidence_axis]<society.catalog.forum.evidence_floor)
        invalid("Restore this service's regional eligibility with a qualifying independent report before advertising it.");
    std::size_t monthly=0,repeats=0;
    for(auto it=prior.rbegin();it!=prior.rend() && same_month(it->starts_on,day);++it) {
        ++monthly;
        if(it->region==request.region && it->service==request.service && it->audience==request.audience) ++repeats;
    }
    if(monthly>=society.catalog.media.monthly_limit || prior.size()>=society_campaign_limit)
        invalid("The finite monthly media campaign allowance is exhausted.");
    const auto deployment_index=static_cast<std::uint64_t>(deployment-state.deployments->records.data())+1;
    return {prior.size()+1,deployment_index,request.region,request.service,request.evidence,request.audience,day,
        advance_days(day,society.catalog.media.duration),society.catalog.media.fee,
        BasisPoints{society.catalog.media.awareness.value()/static_cast<std::int64_t>(repeats+1)}};
}
std::vector<const SimulationState*> sellers(const WorldState& world) {
    std::vector<const SimulationState*> result{&world.simulation};
    if(world.competition) for(const auto& rival:world.competition->companies)
        if(rival.simulation.market) result.push_back(&rival.simulation);
    return result;
}
// Visits only this calendar month's source receipts. Daily advancement never
// replays old months; full source reconciliation belongs to save/restore.
std::array<RegionOutcomes,society_regions> observations(const WorldState& world,OrdinalDay month_end) {
    const auto& society=*world.society;
    std::array<RegionOutcomes,society_regions> result{};
    std::array<std::uint64_t,society_regions> weighted_quality{},player_quality{};
    const auto civil=from_ordinal(month_end);
    const auto first=std::max(world.campaign_start,to_ordinal({civil.year,civil.month,1}));
    if(!world.simulation.market) invalid("Regional outcomes need settled customer markets.");
    const auto& shared=*world.simulation.market;
    for(auto day=first;day<=month_end;day=advance_days(day,Days{1})) for(const auto& segment:shared.segments) {
        if(segment.region_id.empty()) continue;
        auto& region=result[society_region_index(society.catalog,segment.region_id)];
        const auto demand=effective_market_demand(shared,segment,day);
        region.demand+=demand;
        if(in(society.catalog.public_services,segment.id)) region.public_demand+=demand;
    }
    std::map<std::string,std::array<std::uint64_t,society_regions>,std::less<>> supplier_work;
    for(const auto* source:sellers(world)) {
        const auto& market=*source->market;
        auto owner=market.owner;
        if(world.competition) for(const auto& acquisition:world.competition->acquisitions)
            if(acquisition.target==owner && acquisition.completed_on && *acquisition.completed_on<=month_end) owner=world.company_id;
        auto& supplier=supplier_work[owner];
        const bool player=source==&world.simulation;
        const auto delivery=[&](std::string_view segment_id,std::string_view product_id,std::uint64_t tasks,
            std::uint64_t promised,std::uint64_t missed,OrdinalDay day,const DeploymentRecord* version=nullptr) {
            const auto* segment=named(market.segments,segment_id);
            const auto* product=named(market.products,product_id);
            if(!segment || !product) invalid("A regional delivery lost its customer or product.");
            if(segment->region_id.empty()) return;
            const auto index=society_region_index(society.catalog,segment->region_id);
            auto& row=result[index];row.delivered+=tasks;row.promised+=promised;row.missed+=missed;supplier[index]+=tasks;
            if(tasks && !forum_allows_release(&society,*source,*product,*segment,version,day))
                invalid("A restricted release delivered without the required independent evidence.");
            if(tasks && !institutional_delivery_allowed(world,*source,product->id,segment->id,day,version))
                invalid("An actual delivery bypassed a binding regional procurement rule or sanction.");
            if(in(society.catalog.public_services,segment->id)) {row.public_work+=tasks;if(player) row.player_public+=tasks;}
            if(in(society.catalog.science,segment->id)) {row.science_work+=tasks;if(player) row.player_science+=tasks;}
            if(in(society.catalog.industry,segment->id)) row.industrial_work+=tasks;
            if(product->channel==ProductChannel::agent) row.agent_work+=tasks;
            const auto* deployment=version?version:deployed(*source,product_id,day);
            const auto* report=deployment?society_evidence(*source,deployment->evidence_id,society.catalog.forum.evidence_axis,day):nullptr;
            const auto quality=report?static_cast<std::uint64_t>(report->lower[society.catalog.forum.evidence_axis].value()):0;
            weighted_quality[index]+=tasks*quality;
            if(player) {row.player_work+=tasks;row.player_promised+=promised;row.player_missed+=missed;player_quality[index]+=tasks*quality;}
            std::string_view project_id=version?version->project_id:product->capacity_project_id;
            for(auto it=market.capacity_bindings.rbegin();!version && it!=market.capacity_bindings.rend();++it)
                if(it->product_id==product_id && it->starts_on<=day) {project_id=it->project_id;break;}
            const auto* project=source->capacity?named(source->capacity->projects,project_id):nullptr;
            if(tasks && !project) invalid("Delivered work lost its metered serving project.");
            if(project) row.compute_milli+=static_cast<std::uint64_t>(required_inference(tasks,project->inference_milli_CU_per_task).value());
        };
        auto it=std::lower_bound(market.days.begin(),market.days.end(),first,[](const auto& row,OrdinalDay date){return row.day<date;});
        for(;it!=market.days.end() && it->day<=month_end;++it) {
            for(const auto& row:it->discretionary) {
                delivery(row.segment_id,row.product_id,row.tasks-row.trial_tasks,0,0,it->day);
                if (row.trial_tasks>0) {
                    const auto* trial=market_trial(market,row.product_id,it->day);
                    const auto* version=trial && source->deployments?trial_release(*source->deployments,*trial):nullptr;
                    if (!version) invalid("A settled trial lost its qualified release");
                    delivery(row.segment_id,row.product_id,row.trial_tasks,0,0,it->day,version);
                }
            }
            for(const auto& row:it->contracts) {
                const auto* contract=named(market.contracts,row.contract_id);
                if(!contract) invalid("A regional contract receipt lost its signed terms.");
                delivery(contract->segment_id,contract->product_id,row.delivered_tasks,row.promised_tasks,
                    row.promised_tasks-row.delivered_tasks,it->day);
            }
        }
    }
    for(std::size_t i=0;i<result.size();++i) {
        auto& row=result[i];
        if(row.delivered) {
            std::uint64_t hhi=0;
            for(const auto& [owner,supplier]:supplier_work) {
                (void)owner;
                const auto share=mul_div_floor(supplier[i],10000,row.delivered).quotient;
                hhi+=share*share;
            }
            row.concentration=BasisPoints{static_cast<std::int64_t>(hhi/10000)};
            row.quality=BasisPoints{static_cast<std::int64_t>(weighted_quality[i]/row.delivered)};
        }
        if(row.player_work) row.player_quality=BasisPoints{static_cast<std::int64_t>(player_quality[i]/row.player_work)};
    }
    return result;
}
} // namespace
const core::EvaluationEvidence* society_evidence(const core::SimulationState& state,std::string_view id,
    std::size_t axis,core::OrdinalDay day) {
    if(!state.lab || !state.capacity || axis>=lab_domain_count) return nullptr;
    const auto* report=named(state.lab->evaluations,id);
    if(!report || !report->completed) return nullptr;
    const auto* plan=named(state.lab->catalog.evaluations,report->plan_id);
    const auto* work=named(state.capacity->projects,report->project_id);
    return plan && plan->independent && plan->coverage[axis].value()>0 && work &&
        work->status==ProjectStatus::completed && work->stopped_on && *work->stopped_on<=day?report:nullptr;
}
std::string society_launch_block(const core::SocietyState* society,const core::SimulationState& state,
    const core::DeployModel& request,core::OrdinalDay day) {
    if(!society || !state.deployments || !state.market) return {};
    const auto service=std::find_if(state.deployments->catalog.begin(),state.deployments->catalog.end(),
        [&](const auto& row){return row.product.id==request.service_id;});
    if(service==state.deployments->catalog.end() || service->product.channel!=society->catalog.forum.channel) return {};
    const auto* report=society_evidence(state,request.evidence_id,society->catalog.forum.evidence_axis,day);
    if(report && report->model_id==request.model_id && report->lower[society->catalog.forum.evidence_axis]>=society->catalog.forum.evidence_floor) return {};
    auto territory=request.region_id;
    if(territory.empty()) if(const auto* existing=named(state.market->products,request.service_id)) territory=existing->region_id;
    for(const auto& preference:service->preferences) {
        if(!preference.preference.eligible) continue;
        const auto* segment=named(state.market->segments,preference.segment_id);
        if(segment && !segment->region_id.empty() && (territory.empty() || territory==segment->region_id) &&
            forum_enforced(*society,segment->region_id,day))
            return "The standards forum rule is effective in "+segment->region_id+". Complete an independent "+
                std::string(lab_domain_names[society->catalog.forum.evidence_axis])+" report with lower bound at least "+
                std::to_string(society->catalog.forum.evidence_floor.value()/100)+"/100, then redeploy with that report.";
    }
    return {};
}
bool forum_allows_release(const SocietyState* society,const SimulationState& state,const MarketProduct& product,
                          const MarketSegment& segment,const DeploymentRecord* version,OrdinalDay day) {
    if (!society || segment.region_id.empty() || product.channel!=society->catalog.forum.channel ||
        !forum_enforced(*society,segment.region_id,day)) return true;
    if (!version) return qualified(*society,state,product.id,day);
    const auto* report=society_evidence(state,version->evidence_id,society->catalog.forum.evidence_axis,day);
    return report && report->model_id==version->model_id &&
        report->lower[society->catalog.forum.evidence_axis]>=society->catalog.forum.evidence_floor;
}
core::MarketInfluences society_market_conditions(const WorldState& world,const core::SimulationState& source,core::OrdinalDay day) {
    if(!world.society || !source.market) return {};
    std::map<std::string,bool,std::less<>> known;
    return society_market_influences(*world.society,*source.market,day,[&](std::string_view id) {
        auto [it,inserted]=known.try_emplace(std::string(id),false);
        if(inserted) it->second=qualified(*world.society,source,id,day);
        return it->second;
    });
}
core::MediaCampaign preview_media_campaign(const WorldState& world,const core::BuyMediaCampaign& request) {
    if(!world.society) invalid("This scenario has no regional media system.");
    return media_terms(world,request,world.simulation.date,world.society->campaigns);
}
void buy_media_campaign(WorldState& world,const core::BuyMediaCampaign& request) {
    const auto campaign=preview_media_campaign(world,request);
    pay(world.simulation,campaign.paid,"Disclosed media campaign: "+request.region+" / "+std::string(society_audience_name(request.audience)),media_key(campaign.id));
    world.society->campaigns.push_back(campaign);
}
void endorse_forum_standard(WorldState& world,const core::EndorseForumStandard& request) {
    if(!world.society) invalid("This scenario has no standards forum.");
    auto& society=*world.society;
    (void)society_region_index(society.catalog,request.region);
    if(request.round==0 || request.round>society.rounds.size()) invalid("Choose a current standards forum round.");
    auto& round=society.rounds[static_cast<std::size_t>(request.round-1)];
    const auto day=world.simulation.date;
    if(day<round.opened_on || day>=round.ballot_on ||
        std::any_of(round.endorsements.begin(),round.endorsements.end(),[&](const auto& row){return row.region==request.region;}) ||
        !society_evidence(world.simulation,request.evidence,society.catalog.forum.evidence_axis,day))
        invalid("Disclose one completed independent report per member during the open hearing.");
    pay(world.simulation,society.catalog.forum.endorsement_fee,"Standards forum disclosure: "+request.region,forum_key(round.id,request.region));
    round.endorsements.push_back({day,request.region,request.evidence,request.support,society.catalog.forum.endorsement_fee});
}
void advance_society(WorldState& world,core::OrdinalDay settled) {
    if(!world.society) return;
    const auto civil=from_ordinal(settled);
    if(civil.day==days_in_month(civil.year,civil.month))
        close_society_month(*world.society,settled,observations(world,settled));
    prepare_society(*world.society,campaign_day(world));
}
void validate_society_world(const WorldState& world) {
    if(!world.society) return;
    const auto& society=*world.society;
    validate_society(society,campaign_day(world));
    if(society.starts_on!=world.campaign_start || !world.simulation.accounts || !world.simulation.market)
        invalid("Regional society lost its source campaign, accounts or market.");
    for(const auto& month:society.months)
        if(observations(world,month.month_end)!=month.outcomes) invalid("Regional outcomes differ from actual settled company delivery receipts.");
    if(from_ordinal(campaign_day(world)).day!=1 && campaign_day(world)>world.campaign_start)
        (void)observations(world,OrdinalDay{checked_subtract(campaign_day(world).value(),1)});
    std::map<std::string,const Posting*,std::less<>> payments;
    for(const auto& posting:world.simulation.accounts->postings)
        if(posting.key.starts_with("society.")) payments.emplace(posting.key,&posting);
    const auto payment=[&](std::string key,Cents amount,OrdinalDay day) {
        const auto it=payments.find(key);
        if(it==payments.end() || it->second->debit!=Account::expenses || it->second->credit!=Account::cash ||
            it->second->day!=day || it->second->amount!=amount) invalid("A media or forum disclosure lost its exact paid expense.");
        payments.erase(it);
    };
    for(std::size_t i=0;i<society.campaigns.size();++i) {
        const auto& saved=society.campaigns[i];
        if(saved.starts_on>world.simulation.date || (i && society.campaigns[i-1].starts_on>saved.starts_on) ||
            media_terms(world,{saved.region,saved.service,saved.evidence,saved.audience},saved.starts_on,
                std::span{society.campaigns}.first(i),false,saved.deployment)!=saved) invalid("A saved media campaign differs from its deployed evidence, diminishing returns or monthly allowance.");
        payment(media_key(saved.id),saved.paid,saved.starts_on);
    }
    for(const auto& round:society.rounds) for(const auto& row:round.endorsements) {
        if(!society_evidence(world.simulation,row.evidence,society.catalog.forum.evidence_axis,row.day))
            invalid("A standards forum endorsement used unavailable or non-independent evidence.");
        payment(forum_key(round.id,row.region),row.paid,row.day);
    }
    if(!payments.empty()) invalid("The company has an orphaned regional media or forum payment.");
    for(const auto* source:sellers(world)) if(source->deployments) {
        const auto verify=[&](const DeploymentRecord& row) {
            if(!society_launch_block(&society,*source,{row.service_id,row.model_id,row.evidence_id,{},row.human_review,row.region_id},row.deployed_on).empty())
                invalid("A saved release bypassed its region's effective standards rule.");
        };
        for (const auto& row:source->deployments->records) verify(row);
        for (const auto& rollout:source->deployments->rollouts) verify(rollout.candidate);
    }
}
} // namespace pgp::runtime
