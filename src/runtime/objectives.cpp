#include "objectives_internal.hpp"

#include <algorithm>
#include <stdexcept>

namespace pgp::runtime {
using namespace core;
namespace {
using namespace objective_detail;
constexpr std::array<std::string_view,7> end_keys{"deadline","asi_race","insolvency","institutional_control","final_order","catastrophe","postmortem"};
constexpr std::array<std::string_view,4> mode_keys{"scored","sandbox","observer","freeplay"};
std::size_t index(ObjectiveKind kind) {
    const auto i=static_cast<std::size_t>(kind);
    if(i>=objective_kinds.size()) throw std::invalid_argument("Unknown campaign objective.");
    return i;
}
bool selected(const ObjectiveSelection& selection,ObjectiveKind kind) {
    return selection.primary==kind || std::find(selection.secondary.begin(),selection.secondary.end(),kind)!=selection.secondary.end();
}
bool lost_race(const WorldState& world,const AsiRaceResult& race) {
    return std::find(race.winners.begin(),race.winners.end(),world.company_id)==race.winners.end();
}
bool new_frontier_evidence(const WorldState& world) {
    const auto today=world.simulation.date;
    const auto recorded=[today](const SimulationState& actor) {
        return actor.frontier && std::any_of(actor.frontier->trials.begin(),actor.frontier->trials.end(),
            [today](const auto& trial){return trial.result && trial.result->observed_on==today;});
    };
    if(recorded(world.simulation)) return true;
    return std::any_of(world.competition->companies.begin(),world.competition->companies.end(),
        [&](const auto& rival){return recorded(rival.simulation);});
}
void breach(ObjectiveBreach& row,OrdinalDay date) {
    if(!row.first) row.first=date;
    ++row.failed_checks;
}
void review_month(const WorldState& world,std::span<const ObjectiveMonth> months,
    std::array<ObjectiveBreach,objective_kinds.size()>& breaches) {
    const auto& m=months.back();
    if(!utopia_met(m)) breach(breaches[index(ObjectiveKind::utopia)],m.month_end);
    const auto first=objective_window_start(world.objectives->deadline,ObjectiveKind::monopoly);
    if(m.month_end<first) return;
    if(!domination_met(m)) breach(breaches[index(ObjectiveKind::domination)],m.month_end);
    auto& monopoly=breaches[index(ObjectiveKind::monopoly)];
    if(!monopoly.first && !monopoly_still_possible(aggregate_delivery(months,first),remaining_delivery(world,m.month_end)))
        breach(monopoly,m.month_end);
}
std::string score(std::uint32_t bp) {
    const auto fraction=bp%100;
    return std::to_string(bp/100)+"."+ (fraction<10?"0":"")+std::to_string(fraction);
}
bool actual_failure(CampaignEndCause cause) {
    return cause==CampaignEndCause::insolvency || cause==CampaignEndCause::institutional_control ||
        cause==CampaignEndCause::final_order || cause==CampaignEndCause::catastrophe;
}
std::optional<std::string> final_closure(const SimulationState& state,OrdinalDay day) {
    if(state.civic) for(const auto& docket:state.civic->dockets)
        if(docket.effective_from && docket.expires_before && *docket.effective_from<=day && day<*docket.expires_before &&
           civic_policy(state.civic->catalog,docket.policy_id).rule==CivicRule::final_closure)
            return docket.policy_id+"/"+std::to_string(docket.id);
    return {};
}
OrdinalDay sandbox_limit() {return to_ordinal({9999,1,1});}
bool wins_profit(const WorldState& world,const CampaignEnd& end) {
    const auto player=std::find_if(end.profit.begin(),end.profit.end(),[&](const auto& row){return row.institution==world.company_id;});
    return player!=end.profit.end() && std::none_of(end.profit.begin(),end.profit.end(),[&](const auto& row){return row.legacy_wealth>player->legacy_wealth;});
}
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
}

std::string_view campaign_end_key(CampaignEndCause cause) noexcept {
    const auto i=static_cast<std::size_t>(cause);return i<end_keys.size()?end_keys[i]:std::string_view{};
}
std::string_view objective_mode_key(ObjectiveMode mode) noexcept {
    const auto i=static_cast<std::size_t>(mode);return i<mode_keys.size()?mode_keys[i]:std::string_view{};
}
bool objective_is_custom(const ObjectivesState& objectives) noexcept {
    return objectives.mode!=ObjectiveMode::scored || (!objectives.selections.empty() &&
        std::any_of(objectives.selections.begin(),objectives.selections.end(),[&](const auto& event) {
            return event.selection.primary!=objectives.selections.front().selection.primary;
        }));
}
bool objective_time_stopped(const WorldState& world) noexcept {
    return world.objectives && world.objectives->end &&
        (world.objectives->mode==ObjectiveMode::scored || world.objectives->mode==ObjectiveMode::freeplay);
}
bool observing_world(const WorldState& world) noexcept {
    return world.objectives && world.objectives->mode==ObjectiveMode::observer;
}
OrdinalDay campaign_day(const WorldState& world) noexcept {
    return world.objectives?world.objectives->observer_day.value_or(world.simulation.date):world.simulation.date;
}
OrdinalDay campaign_calendar_limit(const WorldState& world) {
    return observing_world(world)?sandbox_limit():world.simulation.end_date;
}
bool campaign_paused(const WorldState& world) noexcept {
    return observing_world(world)?world.objectives->observer_paused:world.simulation.paused;
}
bool has_asi_objective(const WorldState& world) noexcept {
    return world.simulation.frontier && world.simulation.frontier->catalog.asi_route;
}
std::optional<ObjectiveKind> required_objective_review(const WorldState& world) {
    if(!world.objectives || world.objectives->end || world.objectives->mode==ObjectiveMode::freeplay) return {};
    const auto& o=*world.objectives;
    const auto& selection=o.selections.back().selection;
    const auto pending=[&](ObjectiveKind kind){const auto& b=o.breaches[index(kind)];return b.first && !b.acknowledged_on;};
    if(pending(selection.primary)) return selection.primary;
    for(auto kind:selection.secondary) if(pending(kind)) return kind;
    return {};
}
std::optional<ObjectiveKind> objective_postmortem_candidate(const WorldState& world) {
    if(const auto required=required_objective_review(world)) return required;
    if(!world.objectives || world.objectives->end) return {};
    const auto& o=*world.objectives;const auto& selection=o.selections.back().selection;
    if(o.breaches[index(selection.primary)].first) return selection.primary;
    for(auto kind:selection.secondary) if(o.breaches[index(kind)].first) return kind;
    return {};
}
std::optional<OrdinalDay> next_objective_boundary(const WorldState& world) {
    if(!world.objectives || world.objectives->end || world.objectives->mode==ObjectiveMode::freeplay) return {};
    const auto& o=*world.objectives;
    const auto today=world.simulation.date;
    const auto start=objective_window_start(o.deadline,ObjectiveKind::utopia);
    const auto civil=from_ordinal(std::max(today,start));
    const auto month_end=to_ordinal({civil.year,civil.month,days_in_month(civil.year,civil.month)});
    return std::min(o.deadline,advance_days(month_end,Days{1}));
}
ObjectiveResult objective_result(const WorldState& world,ObjectiveKind kind) {
    if(kind==ObjectiveKind::profit && world.founding && world.founding->legal_form=="nonprofit") return ObjectiveResult::unavailable;
    if(!world.objectives || (kind==ObjectiveKind::asi && !has_asi_objective(world))) return ObjectiveResult::unavailable;
    const auto& o=*world.objectives;
    if(o.mode==ObjectiveMode::freeplay) return o.end?ObjectiveResult::lost:ObjectiveResult::unfinished;
    if(kind==ObjectiveKind::asi && o.race) return lost_race(world,*o.race)?ObjectiveResult::lost:ObjectiveResult::won;
    if(o.breaches[index(kind)].first) return ObjectiveResult::lost;
    if(!o.end) return ObjectiveResult::pending;
    if(o.end->cause!=CampaignEndCause::deadline)
        return actual_failure(o.end->cause)?ObjectiveResult::lost:ObjectiveResult::unfinished;
    switch(kind) {
    case ObjectiveKind::monopoly:case ObjectiveKind::domination:case ObjectiveKind::utopia: return ObjectiveResult::won;
    case ObjectiveKind::profit: return wins_profit(world,*o.end)?ObjectiveResult::won:ObjectiveResult::lost;
    case ObjectiveKind::survival: return survival_met(*o.end->survival)?ObjectiveResult::won:ObjectiveResult::lost;
    case ObjectiveKind::asi: return ObjectiveResult::lost;
    }
    return ObjectiveResult::unavailable;
}
std::string objective_breach_explanation(const WorldState& world,ObjectiveKind kind) {
    const auto& o=*world.objectives;
    const auto& b=o.breaches[index(kind)];
    if(!b.first) return {};
    const auto day=iso_date(*b.first);
    if(kind==ObjectiveKind::asi) return "The first independently validated ASI was recorded on "+day+
        " by "+o.race->winners.front()+(o.race->winners.size()>1?" and other same-day qualifiers.":".");
    const auto found=std::lower_bound(o.months.begin(),o.months.end(),*b.first,
        [](const auto& m,OrdinalDay date){return m.month_end<date;});
    if(found==o.months.end() || found->month_end!=*b.first) return {};
    if(kind==ObjectiveKind::domination) {
        auto result=day+" had qualifying authority over "+score(found->population_coverage)+
            "% of population and "+score(found->output_coverage)+"% of output. Both had to reach 80.00% on every final check.";
        const auto& history=world.authority->months;
        const auto evidence=std::lower_bound(history.begin(),history.end(),*b.first,
            [](const auto& row,OrdinalDay date){return row.month_end<date;});
        if(evidence!=history.end() && evidence->month_end==*b.first) for(std::size_t i=0;i<society_regions;++i) {
            const auto authority=std::max(evidence->soft[i],evidence->hard[i]);
            if(authority<authority_region_floor)
                result+=" "+world.society->catalog.regions[i].name+": "+score(authority)+" / 60.00 authority.";
        }
        return result;
    }
    if(kind==ObjectiveKind::utopia) {
        std::string detail=day+" missed a required welfare check: ";
        bool comma=false;
        const auto append=[&](std::string part){if(comma) detail+="; ";detail+=part;comma=true;};
        for(std::size_t i=0;i<utopia_global_floors.size();++i) if(found->global_welfare[i]<utopia_global_floors[i])
            append(std::string(welfare_index_name(i))+" "+score(found->global_welfare[i])+" / "+score(utopia_global_floors[i])+" globally");
        for(std::size_t region=0;region<society_regions;++region) {
            const auto& indices=found->regional_welfare[region];
            const auto low=std::min_element(indices.begin(),indices.end());
            if(*low<utopia_region_floor)
                append(world.society->catalog.regions[region].name+" "+std::string(welfare_index_name(static_cast<std::size_t>(low-indices.begin())))+
                    " "+score(*low)+" / 70.00");
        }
        return detail+". Every required month must pass.";
    }
    const auto first=objective_window_start(o.deadline,ObjectiveKind::monopoly);
    const auto totals=aggregate_delivery(std::span{o.months.begin(),std::next(found)},first);
    const auto global=global_delivery(totals);
    auto result=day+" closed with "+std::to_string(global.controlled)+" of "+std::to_string(global.total)+
        " delivered tasks credited to your institution. ";
    result+=advance_days(*b.first,Days{1})==o.deadline?
        "The final aggregate missed the 85% global threshold or at least one 60% regional threshold.":
        "Even crediting all remaining physically possible customer work to your institution cannot reach both thresholds.";
    for(std::size_t i=0;i<totals.size();++i) if(!totals[i].total ||
        mul_div_floor(totals[i].controlled,10000,totals[i].total).quotient<monopoly_region_floor)
        result+=" "+world.society->catalog.regions[i].name+": "+std::to_string(totals[i].controlled)+" / "+std::to_string(totals[i].total)+".";
    return result;
}
void initialize_objectives(WorldState& world,const ObjectiveCharter& charter,std::optional<ObjectiveSelection> selection) {
    validate_objective_charter(charter);
    auto opening=selection.value_or(charter.opening);
    validate_objective_selection(opening,has_asi_objective(world));
    if(!world.society || !world.authority || !world.simulation.succession || !world.competition || !world.competition->setup ||
       world.campaign_start>objective_window_start(world.simulation.end_date,ObjectiveKind::utopia))
        invalid("Campaign objectives need their complete world and institutional sources.");
    auto& o=world.objectives.emplace();o.deadline=world.simulation.end_date;
    o.selections.push_back({world.campaign_start,std::move(opening)});
}
void objective_detail::finish(WorldState& world,CampaignEndCause cause,std::string source) {
    auto& o=*world.objectives;
    if(o.end) return;
    auto& end=o.end.emplace();end.day=world.simulation.date;end.cause=cause;end.source=std::move(source);
    const auto& accounts=*world.simulation.accounts;
    end.posting_cursor=accounts.postings.size();
    const auto settled=accounts.closed_through;
    if(settled) end.obligations=recorded_exposure(accounts,accounts.postings.size(),*settled);
    if(actual_failure(cause)) world.facts.at("company.is_operating")=false;
    world.simulation.paused=true;
}
void settle_objectives(WorldState& world,std::optional<OrdinalDay> settled,const CompiledProgram& program) {
    if(!world.objectives || world.objectives->end) return;
    auto& o=*world.objectives;auto& state=world.simulation;
    if(o.mode==ObjectiveMode::freeplay) {
        if(state.phase==DayPhase::closed) finish(world,CampaignEndCause::insolvency,std::to_string(state.liquidity->workouts.back().id));
        else if(!retains_control(world)) finish(world,CampaignEndCause::institutional_control);
        else if(const auto order=final_closure(state,state.date)) finish(world,CampaignEndCause::final_order,*order);
        return;
    }
    if(settled && *settled>=objective_window_start(o.deadline,ObjectiveKind::utopia) &&
        *settled<o.deadline && objective_month_end(*settled)) {
        o.months.push_back(month(world,*settled));
        review_month(world,o.months,o.breaches);
    }
    if(!o.race && settled && has_asi_objective(world) && new_frontier_evidence(world)) {
        o.race=race_through(world,state.date);
        if(o.race && lost_race(world,*o.race)) breach(o.breaches[index(ObjectiveKind::asi)],o.race->day);
    }
    if(state.phase==DayPhase::closed) {
        finish(world,CampaignEndCause::insolvency,std::to_string(state.liquidity->workouts.back().id));return;
    }
    if(state.date==o.deadline) {
        finish(world,CampaignEndCause::deadline);
        o.end->profit=terminal_profit(world,charter(world,program));o.end->survival=terminal_survival(world);return;
    }
    if(!retains_control(world)) {
        finish(world,CampaignEndCause::institutional_control);return;
    }
    if(const auto order=final_closure(state,state.date)) {
        finish(world,CampaignEndCause::final_order,*order);return;
    }
    if(o.race && o.selections.back().selection.primary==ObjectiveKind::asi) {
        finish(world,CampaignEndCause::asi_race);return;
    }
    if(required_objective_review(world)) state.paused=true;
}
void select_objectives(WorldState& world,const ObjectiveSelection& selection) {
    if(!world.objectives || world.objectives->end) invalid("Choose objectives during a scored campaign.");
    auto& o=*world.objectives;
    if(world.founding && world.founding->legal_form=="nonprofit" &&
        (selection.primary==ObjectiveKind::profit || std::ranges::find(selection.secondary,ObjectiveKind::profit)!=selection.secondary.end()))
        invalid("Nonprofit assets belong to the institution and cannot be scored as founder wealth.");
    validate_objective_selection(selection,has_asi_objective(world));
    if(selection==o.selections.back().selection) invalid("These are already the current campaign objectives.");
    if(o.selections.size()>=objective_selection_limit) invalid("The objective selection history is full.");
    o.selections.push_back({world.simulation.date,selection});
    world.simulation.paused=true;
}
void acknowledge_objective(WorldState& world,ObjectiveKind kind) {
    if(required_objective_review(world)!=kind) invalid("The objective review is no longer the current required decision.");
    world.objectives->breaches[index(kind)].acknowledged_on=world.simulation.date;
}
void open_campaign_postmortem(WorldState& world) {
    if(!world.objectives || world.objectives->end) invalid("There is no ongoing scored campaign to end.");
    const auto kind=objective_postmortem_candidate(world);
    if(!kind) invalid("An early postmortem needs an irreversible loss of a selected objective.");
    finish(world,CampaignEndCause::postmortem,std::string(objective_key(*kind)));
}
void continue_campaign(WorldState& world) {
    if(!world.objectives || world.objectives->mode!=ObjectiveMode::scored)
        invalid("This campaign has already continued or has no scored ending.");
    auto& o=*world.objectives;
    if(!o.end) open_campaign_postmortem(world);
    const bool observer=actual_failure(o.end->cause);
    o.mode=observer?ObjectiveMode::observer:ObjectiveMode::sandbox;
    if(observer) o.observer_day=world.simulation.date;
    else {
        const auto installed=industry_adoption_snapshot(world);
        extend_campaign(world.simulation,sandbox_limit(),&installed);
    }
    for(auto& actor:world.competition->companies) extend_campaign(actor.simulation,sandbox_limit());
}

void validate_objectives(const WorldState& world,const CompiledProgram& program,bool deep) {
    const auto* def=program.collection.find(world.scenario_id);
    const auto* scenario=def?std::get_if<content::ScenarioDefinition>(&def->data):nullptr;
    if((scenario && scenario->objectives.has_value())!=world.objectives.has_value())
        invalid("The save and source scenario disagree about campaign objectives.");
    if(!world.objectives) return;
    const auto& o=*world.objectives;const auto& state=world.simulation;
    if(o.deadline!=scenario->ends_before || o.selections.empty() || o.selections.size()>objective_selection_limit ||
        o.selections.front().day!=world.campaign_start || o.months.size()>utopia_months || objective_mode_key(o.mode).empty() ||
        !state.accounts || !state.liquidity || !state.succession || !state.market || !world.society || !world.authority ||
        !world.competition || !world.competition->setup) invalid("Saved objective rules lack their campaign sources.");
    const auto through=o.end?o.end->day:state.date;
    if(o.mode==ObjectiveMode::freeplay) {
        if(!world.founding || world.founding->mode!="sandbox" || state.end_date!=sandbox_limit() || o.observer_day ||
            !o.observer_paused || !o.months.empty() || o.race ||
            std::ranges::any_of(o.breaches,[](const auto& b){return b.first || b.failed_checks!=0 || b.acknowledged_on;}) ||
            (o.end && (!actual_failure(o.end->cause) || o.end->day!=state.date || !state.paused)) ||
            (!o.end && (state.phase==DayPhase::finished || state.phase==DayPhase::closed)))
            invalid("Standalone sandbox cannot invent a completed campaign or change its clock contract.");
        for(const auto& row:o.selections) {
            validate_objective_selection(row.selection,has_asi_objective(world));
            if(row.day<world.campaign_start || row.day>state.date) invalid("Sandbox mission changes need their actual dates.");
        }
        if(o.end && o.end->cause==CampaignEndCause::insolvency && state.phase!=DayPhase::closed)
            invalid("Sandbox insolvency needs the actual closed workout.");
        return;
    }
    const auto first=objective_window_start(o.deadline,ObjectiveKind::utopia);
    const auto month_end_count=[&](OrdinalDay day) {
        if(day<=first) return std::size_t{0};
        const auto date=from_ordinal(day),start=from_ordinal(first);
        return static_cast<std::size_t>((date.year-start.year)*12+static_cast<int>(date.month)-static_cast<int>(start.month));
    };
    if(through<world.campaign_start || through>o.deadline || o.months.size()!=month_end_count(through))
        invalid("Saved objective window is missing or contains an unsettled calendar month.");
    if(o.mode==ObjectiveMode::scored) {
        if(state.end_date!=o.deadline || o.observer_day || !o.observer_paused || (o.end && (!state.paused || o.end->day!=state.date)))
            invalid("Scored campaign clock differs from its recorded ending.");
    } else if(!o.end || o.end->day>state.date ||
        (o.mode==ObjectiveMode::observer && (!actual_failure(o.end->cause) || !o.observer_day || *o.observer_day<state.date ||
            *o.observer_day>sandbox_limit() || state.date!=o.end->day || state.end_date!=o.deadline || !state.paused)) ||
        (o.mode==ObjectiveMode::sandbox && (actual_failure(o.end->cause) || o.observer_day || !o.observer_paused ||
            state.end_date!=sandbox_limit()))) invalid("Saved continuation does not preserve its scored ending.");
    if(required_objective_review(world) && !state.paused) invalid("An unreviewed objective breach must keep the campaign paused.");
    if(!o.end && (state.phase==DayPhase::finished || state.phase==DayPhase::closed || !retains_control(world)))
        invalid("The scored company has an unrecorded ending.");
    if(!deep) return;
    if(o.observer_day) (void)from_ordinal(*o.observer_day);
    for(std::size_t i=0;i<o.selections.size();++i) {
        const auto& row=o.selections[i];
        validate_objective_selection(row.selection,has_asi_objective(world));
        if(row.day>through || (i && (row.day<o.selections[i-1].day || row.selection==o.selections[i-1].selection)))
            invalid("Objective selections do not follow the dated campaign.");
    }
    std::array<ObjectiveBreach,objective_kinds.size()> expected{};
    auto start=first;
    for(std::size_t i=0;i<o.months.size();++i) {
        const auto date=from_ordinal(start);
        const auto end=to_ordinal({date.year,date.month,days_in_month(date.year,date.month)});
        if(o.months[i]!=month(world,end)) invalid("Saved objective check differs from its settled delivery, authority or welfare.");
        review_month(world,std::span{o.months}.first(i+1),expected);
        start=advance_days(end,Days{1});
    }
    if(o.race!=race_through(world,through)) invalid("The ASI race does not match independent qualification dates and same-day ties.");
    if(o.race && lost_race(world,*o.race)) breach(expected[index(ObjectiveKind::asi)],o.race->day);
    for(auto kind:objective_kinds) {
        const auto& got=o.breaches[index(kind)];auto& wanted=expected[index(kind)];
        if(got.acknowledged_on && (!wanted.first || *got.acknowledged_on>through || *got.acknowledged_on<
            (kind==ObjectiveKind::asi?*wanted.first:advance_days(*wanted.first,Days{1})) ||
            std::none_of(o.selections.begin(),o.selections.end(),[&](const auto& row){return row.day<=*got.acknowledged_on && selected(row.selection,kind); })))
            invalid("Objective review predates an actual selected and irreversible breach.");
        wanted.acknowledged_on=got.acknowledged_on;
        if(got!=wanted) invalid("Saved objective breaches differ from the settled checks.");
    }
    if(!o.end) return;
    const auto& end=*o.end;
    if(actual_failure(end.cause) && world.facts.at("company.is_operating")!=content::Scalar{false})
        invalid("A failed campaign cannot accept further operating work.");
    if(campaign_end_key(end.cause).empty() || end.posting_cursor>state.accounts->postings.size() ||
        (o.mode!=ObjectiveMode::sandbox && end.posting_cursor!=state.accounts->postings.size()) ||
        (end.posting_cursor<state.accounts->postings.size() &&
            state.accounts->postings[static_cast<std::size_t>(end.posting_cursor)].day<end.day) ||
        (end.posting_cursor && state.accounts->postings[static_cast<std::size_t>(end.posting_cursor)-1].day>end.day) ||
        end.obligations!=recorded_exposure(*state.accounts,static_cast<std::size_t>(end.posting_cursor),OrdinalDay{checked_subtract(end.day.value(),1)}))
        invalid("The postmortem does not match the recorded obligations.");
    if(end.cause==CampaignEndCause::deadline) {
        if(end.day!=o.deadline || end.profit!=terminal_profit(world,charter(world,program)) ||
            end.survival!=terminal_survival(world) || !end.source.empty())
            invalid("Terminal founder valuation or survival activity differs from the settled century.");
    } else if(!end.profit.empty() || end.survival) invalid("An early ending cannot award century scores.");
    if(end.cause==CampaignEndCause::asi_race && (!o.race || o.race->day>end.day || !end.source.empty() ||
        o.selections.back().selection.primary!=ObjectiveKind::asi)) invalid("An early ASI ending has no independently settled winner.");
    if(end.cause==CampaignEndCause::postmortem) {
        const auto kind=objective_kind(end.source);
        if(!kind || !selected(o.selections.back().selection,*kind) || !o.breaches[index(*kind)].first)
            invalid("Early postmortem lacks a selected irreversible loss.");
    }
    if(end.cause==CampaignEndCause::insolvency &&
       (state.phase!=DayPhase::closed || state.liquidity->workouts.back().closed_on!=end.day ||
        end.source!=std::to_string(state.liquidity->workouts.back().id) || end.obligations!=state.liquidity->exposure))
        invalid("Insolvency ending lacks an actual closed, warned workout.");
    if(end.cause==CampaignEndCause::institutional_control && (retains_control(world) || !end.source.empty()))
        invalid("Institutional failure requires loss of executive and governing rights and majority ownership.");
    if(end.cause==CampaignEndCause::final_order && final_closure(state,end.day)!=end.source)
        invalid("An enforceable closure requires a final effective federal docket.");
}
} // namespace pgp::runtime
