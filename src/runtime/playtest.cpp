#include "playtest.hpp"

#include <pgp/core/workplace_research.hpp>
#include <algorithm>
#include <set>
#include <ranges>

namespace pgp::runtime {
using namespace core;

void validate_playtest_morning(const Checkpoint& checkpoint) {
    const auto& world=checkpoint.world;
    if(!world.developer_playtest || !world.founding)
        throw std::invalid_argument("Open a founder campaign in God mode.");
    if(objective_time_stopped(world) || observing_world(world) || world.simulation.phase!=DayPhase::work_ready)
        throw std::invalid_argument("An operating company is required.");
    if(checkpoint.runtime.prepared_day!=world.simulation.date ||
       std::ranges::any_of(checkpoint.runtime.presentations,[](const auto& notice) {return !notice.choices.empty() && !notice.resolved;}))
        throw std::invalid_argument("Resolve the required decisions before using God mode.");
}

void refill_playtest_resources(WorldState& world) {
    constexpr Cents reserve{100'000'000'000'000};
    auto& state=world.simulation;
    const auto available=uncommitted_cash(state);
    if(available.value()<reserve.value()/2) {
        record_cash(*state.accounts,state.date,checked_unit_subtract(reserve,available),Account::equity,
            "God mode company funding","playtest.capital/"+std::to_string(state.revision));
        state.cash=accounts_report(*state.accounts).cash;
    }
    auto& founder=*state.succession;
    if(founder.personal_cash.value()<reserve.value()/2) {
        const auto amount=checked_unit_subtract(reserve,founder.personal_cash);
        founder.playtest_cash=checked_unit_add(founder.playtest_cash,amount);
        founder.personal_cash=reserve;
    }
    auto& capacity=*state.capacity;
    if(std::ranges::find(capacity.sources,"playtest.compute",&CapacitySource::id)==capacity.sources.end()) {
        const auto& owner=capacity.owner;
        constexpr MilliCUPerDay rate{1'000'000'000};
        add_capacity_source(capacity,state.date,{"playtest.compute","physical/"+owner+"/playtest.compute",owner,
            "playtest.provider",CapacitySourceKind::leased,rate,rate,1'000'000'000,"general",true,state.date});
    }
}

void edit_playtest_person(WorldState& world,std::string_view id,const std::vector<std::string>& traits,const std::array<int,5>& stats) {
    auto& state=world.simulation;
    if(!state.management || !state.management->people)
        throw std::invalid_argument("This company has no people register.");
    if(traits.size()>person_trait_limit || std::set<std::string_view>{traits.begin(),traits.end()}.size()!=traits.size() ||
        std::ranges::any_of(traits,[](const auto& trait) {
            return trait.empty() || trait.size()>80 || trait.find_first_of("\r\n\t")!=trait.npos || trait.find('\0')!=trait.npos;
        })) throw std::invalid_argument("Enter up to 32 distinct traits of up to 80 bytes each.");
    auto& people=*state.management->people;
    PersonProfile* profile=nullptr;
    Applicant* employment=nullptr;
    for(auto& worker:people.employees) if(worker.terms.profile.id==id) employment=&worker.terms;
    for(auto& applicant:people.applicants) if(applicant.profile.id==id) employment=&applicant;
    for(auto& applicant:people.queued_applicants) if(applicant.profile.id==id) employment=&applicant;
    if(employment) profile=&employment->profile;
    else for(auto& [offer,person]:people.known_people) {
        (void)offer;
        if(person.id==id) profile=&person;
    }
    if(!profile) throw std::invalid_argument("Choose a person in the company register.");
    const auto max_experience=from_ordinal(profile->applied_on).year-profile->birth_year-18;
    if(stats[0]<0 || stats[0]>max_experience || std::ranges::any_of(stats | std::views::drop(1),[](int stat){return stat<0 || stat>100;}))
        throw std::invalid_argument("Enter stats from 0 to 100 and experience within the person's working age.");
    if(state.lab && profile->traits!=traits) for(auto& run:state.lab->research)
        if(state.lab->catalog.creative_research.contains(run.research_id) &&
            std::ranges::find(run.researchers,id)!=run.researchers.end())
            run.original_traits.try_emplace(profile->id,profile->traits);
    profile->traits=traits;
    if(profile->role=="Founder / researcher" && people.social) people.social->founder_traits=traits;
    profile->experience=static_cast<unsigned>(stats[0]);
    profile->research=static_cast<unsigned>(stats[1]);
    profile->engineering=static_cast<unsigned>(stats[2]);
    if(employment && profile->evaluation!=static_cast<unsigned>(stats[3])) {
        const auto hire=std::ranges::find(state.capacity->staff_additions,id,&StaffAddition::id);
        if(hire!=state.capacity->staff_additions.end()) {
            if(!employment->playtest_original_evaluation) employment->playtest_original_evaluation=profile->evaluation;
        }
        else employment->evaluation_credit=BasisPoints{state.management->catalog.people->maximum_evaluation_credit.value()*stats[3]/100};
    }
    profile->evaluation=static_cast<unsigned>(stats[3]);
    profile->mission_fit=static_cast<unsigned>(stats[4]);
    reconcile_workplace_research(state);
}

void deliver_playtest_orders(WorldState& world) {
    auto& state=world.simulation;
    const auto deliver=[&](auto& order,OrdinalDay& day) {
        if(day>state.date) {day=state.date;order.playtest_delivery=true;}
    };
    if(state.infrastructure) {
        auto& infra=*state.infrastructure;
        for(auto& order:infra.hardware) if(!order.retired_on) deliver(order,order.delivered_on);
        if(infra.workplace) {
            if(infra.workplace->headquarters) for(auto& order:infra.workplace->headquarters->items) deliver(order,order.delivered_on);
            if(infra.workplace->devices) for(auto& device:infra.workplace->devices->records) deliver(device,device.ready_on);
        }
        prepare_infrastructure(state);
    }
    if(state.succession && state.succession->personal_life)
        for(auto& decor:state.succession->personal_life->decor) deliver(decor,decor.delivered_on);
    reconcile_workplace_research(state);
}

void complete_playtest_research(WorldState& world,std::string_view topic) {
    auto& state=world.simulation;
    if(!state.lab || !state.capacity) throw std::invalid_argument("Research is unavailable in this campaign.");
    for(auto& result:state.lab->research) {
        if(result.research_id!=topic || result.completed || result.failed || result.cooperation) continue;
        auto work=std::ranges::find(state.capacity->projects,result.project_id,&CapacityProject::id);
        if(work==state.capacity->projects.end() ||
            (work->status!=ProjectStatus::active && work->status!=ProjectStatus::paused)) continue;
        if(work->status==ProjectStatus::paused) work->pauses.back().ends_before=state.date;
        work->playtest_completed=true;
        work->status=ProjectStatus::completed;
        work->stopped_on=state.date;
        work->completed_work=work->total_work;
        work->completed_staff_days=work->required_staff_days;
        result.completed=true;
        return;
    }
    throw std::invalid_argument("Start a company research project before completing it in God mode.");
}
}
