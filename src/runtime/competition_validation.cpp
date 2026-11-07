#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::runtime::competition_detail {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (it==rows.end()) invalid("Saved rival strategy has an absent reference");return *it;
}
void resources(const WorldState& world,const CompiledProgram& program,const RivalCompany& rival,const content::RivalDefinition& definition) {
    const auto& state=rival.simulation;const auto initial=opening_rival(world,program,definition,*rival.planning->entered_on);
    if (state.accounts->starts_on!=*rival.planning->entered_on || state.accounts->postings.empty() || state.accounts->postings.front()!=initial.accounts->postings.front() ||
        opening_staff_count(*state.capacity)!=definition.staff || state.management->catalog!=initial.management->catalog ||
        state.deployments->catalog!=initial.deployments->catalog || state.business_offers.has_value()!=initial.business_offers.has_value() ||
        (state.business_offers && state.business_offers->catalog!=initial.business_offers->catalog) || !state.opening_actions.empty())
        invalid("Rival opening capital, staff, catalogs or private commitments differ from authored terms");
    std::set<std::string> allowed_sources{"lease.initial"};
    if (state.business_offers) for (const auto& row:state.business_offers->accepted) {
        const auto& offer=find(state.business_offers->catalog,row.offer_id);allowed_sources.insert("lease/"+offer.id);
        const auto& source=find(state.capacity->sources,"lease/"+offer.id);
        const bool suspended=rival.response && (rival.response->retired_on || std::none_of(rival.response->transitions.begin(),rival.response->transitions.end(),[](const auto& row){return row.cause=="compete";}));
        const auto expected=!suspended && state.date<lease_expiry(*state.business_offers,offer.id)?offer.capacity:MilliCUPerDay{0};
        if (source.effective!=expected || source.nominal!=offer.capacity) invalid("Rival leased capacity bypassed paid availability or expiry");
    }
    const auto ordinary=std::count_if(state.capacity->sources.begin(),state.capacity->sources.end(),[](const auto& row){return row.kind!=CapacitySourceKind::imported;});
    if (static_cast<std::size_t>(ordinary)!=allowed_sources.size()) invalid("Rival invented unbudgeted capacity");
    for (const auto& source:state.capacity->sources) if (source.kind!=CapacitySourceKind::imported && !allowed_sources.contains(source.id)) invalid("Rival uses an unowned capacity source");
    auto initial_source=initial.capacity->sources.front();
    if (rival.response && !rival.response->retired_on && std::any_of(rival.response->transitions.begin(),rival.response->transitions.end(),[](const auto& row){return row.cause=="compete";})) initial_source.effective=initial_source.nominal;
    if (find(state.capacity->sources,"lease.initial")!=initial_source) invalid("Rival initial compute capacity differs from its finite opening/delivery terms");
    for (auto obligation:initial.accounts->recurring) {
        // Finite leadership engagements reconcile their salary and end dates
        // against the actual term history in the owning core validator.
        if(obligation.id.starts_with("leadership/")) continue;
        if (rival.response && rival.response->retired_on) obligation.ends_before=rival.response->retired_on;
        if (find(state.accounts->recurring,obligation.id)!=obligation) invalid("Rival changed its authored recurring costs without a real withdrawal");
    }
    for (const auto& posting:state.accounts->postings) if (posting.debit==Account::cash && posting.key!="opening") {
        const bool equity=posting.key.starts_with("financing/");
        const bool debt=posting.key.starts_with("debt/financing/");
        if (equity || debt) {
            const auto id=posting.key.substr(equity?10:15);const auto& offer=find(state.management->catalog.financing,id);
            if (posting.amount!=offer.proceeds || posting.credit!=(equity?Account::equity:Account::debt) ||
                std::none_of(state.management->financing.begin(),state.management->financing.end(),[&](const auto& row){return row.offer_id==id && row.day==posting.day;}))
                invalid("Rival cash bypassed its finite authorized financing offer");
        } else if (posting.key.starts_with("group.funding/") && rival.parent_company==world.company_id && posting.credit==Account::equity) {
            // The cross-company acquisition validator reconciles both actual
            // legs; this is paid owner capital, not a replacement subsidy.
        } else if (posting.credit!=Account::receivables || !posting.document_id.starts_with("market.")) invalid("Rival invented cash, a hidden subsidy or non-customer revenue");
    }
    std::set<std::string> projects;
    for (const auto& row:state.lab->research) projects.insert(row.project_id);
    for (const auto& row:state.lab->models) if (!row.project_id.empty()) projects.insert(row.project_id);
    for (const auto& row:state.lab->evaluations) projects.insert(row.project_id);
    for (const auto& row:state.lab->generated) projects.insert(row.project_id);
    for (const auto& row:state.deployments->records) projects.insert(row.project_id);
    if (state.frontier) for (const auto& row:state.frontier->trials) projects.insert(row.project);
    if (state.succession) for (const auto& row:state.succession->plans) if(!row.project.empty()) projects.insert(row.project);
    if (state.capacity->projects.size()!=projects.size()) invalid("Rival work is not owned by actual research, training, evaluation or qualified serving");
    for (const auto& project:state.capacity->projects) if (!projects.contains(project.id)) invalid("Rival has an unowned work project");
    for (const auto& change:state.market->demand_changes) {
        const auto source=std::find_if(world.simulation.market->demand_changes.begin(),world.simulation.market->demand_changes.end(),[&](const auto& row){return row.id==change.id;});
        if (source==world.simulation.market->demand_changes.end()) invalid("Rival invented external customer demand");
        auto expected=*source;expected.starts_on=std::max(expected.starts_on,state.market->starts_on);
        if (change!=expected) invalid("Rival demand incident differs from the shared public event");
    }
}
}
void validate_planning(const WorldState& world,const CompiledProgram& program,bool deep) {
    const auto& state=*world.competition;
    if (state.companies.size()>20 || state.notices.size()>4096 || state.releases.size()>core::allocation_product_limit || !world.simulation.lab || !world.simulation.market || world.scenario_id.empty())
        invalid("Planned competition is missing its source/resources or exceeds a bound");
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if (!scenario || content::scenario_competition(*scenario,world.difficulty)!=state.setup) invalid("Saved rival settings differ from their authored setup");
    std::vector<std::string> expected,actual;
    for (const auto& row:scenario->rivals) if (content::selected_rival(row,*state.setup)) expected.push_back(row.id);
    for (const auto& row:state.companies) actual.push_back(row.definition_id);
    if (expected!=actual) invalid("Saved planned roster differs from its configured slots");
    std::set<std::tuple<std::string,std::string,std::string>> notice_ids;
    for (const auto& rival:state.companies) {
        const auto& authored=rival_definition(program,rival.definition_id);
        const auto risk_origin=RandomStreams{rival_seed(world,authored)}.snapshot()[static_cast<std::size_t>(RandomStream::risk)];
        if (rival.simulation.deployments && rival.simulation.deployments->risk) {
            if (rival.simulation.deployments->risk->stream_origin!=risk_origin)
                invalid("Rival deployment risk stream origin differs from its private seed");
        } else if (rival.simulation.random[static_cast<std::size_t>(RandomStream::risk)]!=risk_origin)
            invalid("A rival without deployment risk advanced its unowned stream");
        if (!rival.planning || !authored.planning) invalid("A planned company lost its author profile or continuation");
        const auto& plan=*rival.planning;const auto earliest=advance_days(world.campaign_start,authored.start_after);
        if (plan.decisions.size()>rival_decision_limit) invalid("Rival strategy decisions exceed their retention bound");
        if (rival.response.has_value()!=authored.response.has_value()) invalid("Rival response state differs from its authored policy");
        if (rival.warned_on && !rival.response) {
            std::uint32_t released=0;for (const auto& row:state.releases) if (row.company_id==world.company_id && row.day<*rival.warned_on) ++released;
            if (authored.player_deployment_threshold==0 || *rival.warned_on<earliest || *rival.warned_on>campaign_day(world) || released<authored.player_deployment_threshold)
                invalid("Heavyweight warning lacks actual prior public customer traction");
        }
        if (!plan.entered_on) {
            if (rival.simulation!=dormant_company(world,authored) || rival.attempted || rival.published_on || !plan.decisions.empty() || plan.next_review!=earliest ||
                (!rival.response && campaign_day(world)>=earliest && campaign_day(world)<campaign_calendar_limit(world) && (authored.player_deployment_threshold==0 ||
                    (rival.warned_on && campaign_day(world)>=advance_days(*rival.warned_on,authored.warning_lead)))))
                invalid("A scheduled rival spent before entry or skipped its actual entry date");
            continue;
        }
        const auto entry=rival.response?rival.response->mobilized_on.value_or(campaign_calendar_limit(world)):authored.player_deployment_threshold>0?(rival.warned_on?advance_days(*rival.warned_on,authored.warning_lead):campaign_calendar_limit(world)):earliest;
        const auto& simulation=rival.simulation;
        if (*plan.entered_on!=entry || *plan.entered_on>campaign_day(world) || !simulation.accounts || !simulation.lab || !simulation.capacity || !simulation.market || !simulation.deployments || !simulation.management || !simulation.liquidity ||
            simulation.accounts->owner!=authored.id || simulation.lab->catalog!=world.simulation.lab->catalog || simulation.lab->starts_on!=entry ||
            simulation.end_date!=campaign_calendar_limit(world) || simulation.date>campaign_day(world) ||
            (simulation.date!=campaign_day(world) && simulation.phase!=DayPhase::closed) ||
            plan.next_review!=advance_days(rival.response?rival.response->ready_on.value_or(entry):entry,Days{static_cast<std::int64_t>(plan.decisions.size())*state.setup->review_interval.value()})) invalid("Rival entry, resource ownership or strategy/daily cursor is invalid");
        if (simulation.lab->research_stream_origin!=RandomStreams{rival_seed(world,authored)}.snapshot()[static_cast<std::size_t>(RandomStream::research)]) invalid("Rival research stream origin differs from its private seed");
        if (simulation.frontier.has_value()!=world.simulation.frontier.has_value() ||
            (simulation.frontier && (simulation.frontier->seed!=world.simulation.frontier->seed ||
                (deep && simulation.frontier->catalog!=world.simulation.frontier->catalog))))
            invalid("Rival frontier work differs from the shared future conditions and scenario.");
        if (simulation.lab->models.size()>authored.planning->recipes.size()) invalid("Rival trained outside its bounded strategy portfolio");
        for (const auto& insight:simulation.lab->insights) validate_public_insight(world,insight);
        if (!deep) continue;
        validate_simulation(simulation);resources(world,program,rival,authored);
        std::uint64_t stream=RandomStreams{rival_seed(world,authored)}.snapshot()[static_cast<std::size_t>(RandomStream::rivals)];
        for (std::size_t i=0;i<plan.decisions.size();++i) {
            const auto& decision=plan.decisions[i];SplitMix64 next{stream};const auto draw=next.next();
            const std::set<std::string> actions{"wait","data","research","train","evaluate","deploy","price","hire","repay","finance","expand","publish","frontier_system","frontier_trial"};
            if (decision.day!=advance_days(rival.response?rival.response->ready_on.value_or(entry):entry,Days{static_cast<std::int64_t>(i)*state.setup->review_interval.value()}) || decision.day>simulation.date || decision.stream_before!=stream || decision.stream_after!=next.state() ||
                !actions.contains(decision.action) || decision.explanation.empty() || decision.explanation.size()>1024 || decision.subject.size()>256 ||
                decision.observation!=public_observation(world,decision.day,draw))
                invalid("Rival decision lost its scheduled date, bounded observation or independent RNG progression");
            stream=next.state();
        }
        if (simulation.random[static_cast<std::size_t>(RandomStream::rivals)]!=stream) invalid("Rival strategy consumed an unrecorded or rerolled random draw");
    }
    for (const auto& notice:state.notices) {
        const auto found=std::find_if(state.companies.begin(),state.companies.end(),[&](const auto& row){return row.definition_id==notice.company_id;});
        if (found==state.companies.end() || !notice_ids.emplace(notice.company_id,notice.kind,notice.subject).second || notice.day<world.campaign_start || notice.day>campaign_day(world))
            invalid("Competitive notice has a missing source, duplicate identity or invalid date");
        if (notice.kind=="entry") {if (found->planning->entered_on!=notice.day || notice.response_due || !notice.subject.empty()) invalid("Rival entry notice is not an actual company opening");}
        else if (notice.kind=="warning" && found->response) { /* Checked against the response transition below. */ }
        else if (notice.kind=="response" || notice.kind=="offer") {if (!found->response) invalid("Competitive response/offer has no heavyweight policy");}
        else if (notice.kind=="warning") {if (found->warned_on!=notice.day || notice.response_due!=advance_days(notice.day,rival_definition(program,notice.company_id).warning_lead) || !notice.subject.empty()) invalid("Heavyweight notice lost its real response window");}
        else if (notice.kind=="closure") {if (found->simulation.phase!=DayPhase::closed || found->simulation.date!=notice.day || notice.response_due || !notice.subject.empty()) invalid("Rival closure has no actual insolvency closure");}
        else if (notice.kind=="publication") {if (!found->planning->entered_on || notice.response_due || std::none_of(found->simulation.lab->publications.begin(),found->simulation.lab->publications.end(),[&](const auto& row){return row.research_id==notice.subject && row.day==notice.day;})) invalid("Competitive publication lacks an actual completed research result");}
        else invalid("Unknown planned competitive notice");
    }
    for (const auto& rival:state.companies) {
        if ((rival.planning->entered_on && !notice_ids.contains({rival.definition_id,"entry",""})) || (rival.warned_on && !rival.response && !notice_ids.contains({rival.definition_id,"warning",""})) ||
            (rival.simulation.phase==DayPhase::closed && !notice_ids.contains({rival.definition_id,"closure",""}))) invalid("Planned competition lost a required public transition");
        std::optional<OrdinalDay> first;
        if (rival.planning->entered_on) for (const auto& publication:rival.simulation.lab->publications) {
            if (!notice_ids.contains({rival.definition_id,"publication",publication.research_id})) invalid("Planned competition lost a real publication notice");
            if (!first || publication.day<*first) first=publication.day;
        }
        if (first!=rival.published_on) invalid("Rival public record disagrees with its actual publication history");
    }
    for (const auto& insight:world.simulation.lab->insights) validate_public_insight(world,insight);
    validate_heavyweight_responses(world,program,deep);
    validate_industry(world,deep);
}
}
