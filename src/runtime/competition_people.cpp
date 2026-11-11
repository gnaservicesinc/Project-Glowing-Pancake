#include "competition_people.hpp"
#include <algorithm>
namespace pgp::runtime::competition_detail {
std::optional<PeoplePlan> plan_individual_hire(const core::SimulationState& s,core::Cents usable,core::Cents runway) {
    using namespace core;
    if(!s.management || !s.management->people || !s.management->catalog.people->recruitment || !s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->headquarters)return {};
    const auto& rules=*s.management->catalog.people;const auto& people=*s.management->people;const auto& hq=*s.infrastructure->workplace->headquarters;
    const auto current=std::ranges::count_if(people.employees,[](const auto& e){return !e.departed_on;});
    if(static_cast<std::uint32_t>(current)>=rules.industry_team_target || people.employees.size()>=rules.maximum_hq ||
       std::ranges::any_of(people.employees,[&](const auto& e){return !e.departed_on && e.ready_on>s.date;}))return {};
    const Applicant* candidate=nullptr;int priority=-1;
    for(const auto& applicant:people.applicants) {
        const auto status=recruitment_status(applicant,s.date);
        if(status!="available" && status!="waiting" && status!="counter" && status!="agreed")continue;
        const int rank=(status=="agreed"?1000:status=="counter"?900:status=="waiting"?800:0)+static_cast<int>(applicant.profile.research+applicant.profile.engineering);
        if(rank>priority){candidate=&applicant;priority=rank;}
    }
    if(!candidate)return {};
    const auto status=recruitment_status(*candidate,s.date);const auto workload=capacity_report(*s.capacity);
    if(status=="available" && workload.available_staff>workload.assigned_staff+1)return {};
    if(status=="waiting")return {};
    const auto affordable=[&](Cents immediate,Cents monthly=Cents{0}){
        return usable>=checked_unit_add(runway,checked_unit_add(immediate,Cents{mul_div_round_half_up(static_cast<std::uint64_t>(monthly.value()),2,1)}));
    };
    Cents salary=candidate->salary,signing=candidate->signing;
    if(status=="counter") {const auto& counter=candidate->recruitment->offers.back();salary=counter.counter_salary;signing=counter.counter_signing;}
    if(status=="agreed") {const auto terms=agreed_employment(*candidate);salary=terms.first;signing=terms.second;}
    if(!affordable(signing,salary))return {};
    const auto& offers=s.infrastructure->catalog.workplace->headquarters->equipment;
    const auto kind=[&](const HQEquipment& item)->std::string_view {const auto offer=std::ranges::find(offers,item.offer,&HQEquipmentOffer::id);return offer==offers.end()?std::string_view{}:std::string_view{offer->kind};};
    const HQEquipment* desk=nullptr;
    for(const auto& item:hq.items)if(kind(item)=="desk" && item.person.empty() && item.premises==current_headquarters(s).id && item.delivered_on<=s.date) {
        if(hq_desk_ready(s,item.id)){desk=&item;break;}
        if(!desk)desk=&item;
    }
    if(desk && hq_desk_ready(s,desk->id)) {
        if(status=="agreed")return PeoplePlan{"hire_individual",candidate->profile.id,HireEmployee{candidate->profile.id,desk->id}};
        if(status=="counter")return PeoplePlan{"accept_counter",candidate->profile.id,ManageRecruitment{AcceptEmploymentCounter{candidate->profile.id}}};
        return PeoplePlan{"offer_employment",candidate->profile.id,ManageRecruitment{ProposeEmployment{candidate->profile.id,salary,signing}}};
    }
    if(desk) {
        for(const auto& item:hq.items)if(item.offer=="desktop" && item.person.empty() && !item.premises && !item.mount) {
            if(item.delivered_on>s.date)return {};
            PlaceHQEquipment place{item.id,desk->id,0,0,false};if(hq_placement_blocker(s,place).empty())return PeoplePlan{"equip_workstation",std::to_string(item.id),ManageHeadquarters{place}};
        }
        const auto computer=std::ranges::find(offers,std::string{"desktop"},&HQEquipmentOffer::id);
        if(computer!=offers.end() && affordable(computer->price, salary))return PeoplePlan{"buy_workstation","desktop",ManageHeadquarters{BuyHQEquipment{"desktop"}}};
        return {};
    }
    for(const auto& item:hq.items)if(kind(item)=="desk" && !item.premises && item.person.empty()) {
        if(item.delivered_on>s.date)return {};
        const auto& premises=current_headquarters(s);
        for(int y=0;y<premises.depth;++y)for(int x=0;x<premises.width;++x) {
            PlaceHQEquipment place{item.id,0,x,y,false};if(hq_placement_blocker(s,place).empty())return PeoplePlan{"equip_workstation",std::to_string(item.id),ManageHeadquarters{place}};
        }
        return {};
    }
    const auto model=std::ranges::find(offers,std::string{"desk"},&HQEquipmentOffer::id);
    if(model!=offers.end() && affordable(model->price,salary))return PeoplePlan{"buy_workstation","desk",ManageHeadquarters{BuyHQEquipment{"desk"}}};
    return {};
}
}
