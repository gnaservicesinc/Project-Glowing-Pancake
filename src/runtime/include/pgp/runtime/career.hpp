#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime {
struct ResearchJobOffer {
    std::string employer,name;
    core::Cents salary{0};
    bool fellowship=false;
};
struct CareerResearchOption {
    std::string topic;
    core::Cents fee{0};
    core::LabAllocation allocation;
    core::Days estimated_duration{0};
};
[[nodiscard]] bool can_continue_career(const WorldState&);
[[nodiscard]] const content::RivalDefinition* career_fellowship(const WorldState&,const CompiledProgram&);
[[nodiscard]] const ResearchAppointment* career_appointment(const WorldState&,std::string_view employer);
[[nodiscard]] bool career_open(const WorldState&);
[[nodiscard]] const RivalCompany* career_employer(const WorldState&);
[[nodiscard]] std::vector<ResearchJobOffer> research_job_offers(const WorldState&,const CompiledProgram&);
[[nodiscard]] std::vector<CareerResearchOption> career_research_options(const WorldState&,const CompiledProgram&);
[[nodiscard]] core::Cents career_salary_received(const WorldState&);
[[nodiscard]] const core::RecurringObligation* career_payroll(const RivalCompany&);
void accept_research_appointment(WorldState&,const CompiledProgram&,std::string_view employer);
void direct_career_research(WorldState&,const CompiledProgram&,std::string_view topic);
void publish_career_research(WorldState&,std::string_view topic);
void validate_career(const WorldState&,const CompiledProgram&,bool deep);
}
