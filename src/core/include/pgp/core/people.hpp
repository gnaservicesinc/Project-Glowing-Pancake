#pragma once
#include <pgp/core/units.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/core/portrait.hpp>
#include <optional>
#include <map>
#include <string>
#include <vector>
namespace pgp::core {
struct SimulationState;
struct SalaryRateTag;
using SalaryRate = detail::Quantity<SalaryRateTag,7500,15000>;
struct PeopleRules {
    std::vector<std::string> first_names,last_names;
    std::uint32_t maximum_hq=128,candidates=4;
    Days recruitment_interval{30},onboarding{7};
    Cents monthly_salary{500000},signing_cost{100000};
    std::uint32_t base_skill=50,skill_jitter=15,pay_skill_weight=20,prestige_skill_weight=2,mission_skill_weight=8;
    BasisPoints maximum_evaluation_credit{2000};
    auto operator<=>(const PeopleRules&) const = default;
};
struct PersonProfile {
    std::string id,name,gender,pronouns,education,role,bio,preferred_mission;
    int birth_year=1990;
    std::uint32_t experience=0,research=0,engineering=0,evaluation=0,mission_fit=0;
    OrdinalDay applied_on{0};
    PortraitRecipe portrait{};
    auto operator<=>(const PersonProfile&) const = default;
};
struct Applicant {
    PersonProfile profile;
    Cents salary{0},signing{0};
    BasisPoints evaluation_credit{0};
    auto operator<=>(const Applicant&) const = default;
};
struct Employee {
    Applicant terms;
    OrdinalDay joined_on{0},ready_on{0};
    bool opening=false;
    std::optional<OrdinalDay> departed_on;
    auto operator<=>(const Employee&) const = default;
};
struct PeopleState {
    std::uint64_t stream=0,next_id=1;
    OrdinalDay starts_on{0},next_recruitment{0};
    SalaryRate offered_pay{10000};
    std::string mission="research";
    std::uint32_t founding_prestige=0;
    std::vector<Applicant> applicants;
    std::vector<Employee> employees;
    // Authored specialist/executive offers also have stable, saved personal profiles.
    std::map<std::string,PersonProfile> known_people;
    auto operator<=>(const PeopleState&) const = default;
};
struct SetRecruitingTerms {SalaryRate pay{10000};std::string mission;};
struct HireEmployee {std::string id;};
void validate_people_rules(const PeopleRules&);
void initialize_people(SimulationState&,std::uint64_t seed,std::string founder_name={},std::string founder_pronouns={},bool founder_researcher=false,std::uint32_t prestige=0);
void prepare_people(SimulationState&);
void validate_people(const SimulationState&);
void hire_employee(SimulationState&,std::string_view,Cents protected_cash);
void set_recruiting_terms(SimulationState&,const SetRecruitingTerms&);
void withdraw_people(SimulationState&);
}
