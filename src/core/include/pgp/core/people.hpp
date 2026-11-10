#pragma once
#include <pgp/core/units.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/core/portrait.hpp>
#include <pgp/core/talent.hpp>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <utility>
#include <span>
namespace pgp::core {
struct SimulationState;
struct SalaryRateTag;
using SalaryRate = detail::Quantity<SalaryRateTag,7500,15000>;
struct RecruitmentRules {
    Days response{1},validity{14};
    std::uint32_t maximum_rounds=3;
    std::uint32_t minimum_ask=8500,maximum_ask=12000,minimum_acceptance=8000,maximum_acceptance=10000;
    std::uint32_t minimum_offer=5000,maximum_offer=20000;
    std::uint32_t maximum_signing_multiple=5;
    auto operator<=>(const RecruitmentRules&) const = default;
};
struct WorkplaceMoodRules {
    std::uint32_t pay=25, company_prestige=12, ceo_prestige=10, role=10, coworkers=12, amenity_cap=14;
    std::map<std::string, int> temperaments;
    std::map<std::string, std::uint32_t> amenities;
    auto operator<=>(const WorkplaceMoodRules&) const = default;
};
struct DismissalRules {
    std::uint32_t severance_months=1;
    auto operator<=>(const DismissalRules&) const = default;
};
struct FounderAudienceRule {
    std::string founder, audience;
    int salary_bp=0, mood=0;
    auto operator<=>(const FounderAudienceRule&) const = default;
};
struct PeopleRules {
    std::vector<std::string> first_names,last_names;
    std::uint32_t maximum_hq=128,candidates=4;
    Days recruitment_interval{30},onboarding{7};
    Cents monthly_salary{500000},signing_cost{100000};
    std::uint32_t base_skill=50,skill_jitter=15,pay_skill_weight=20,prestige_skill_weight=2,mission_skill_weight=8;
    BasisPoints maximum_evaluation_credit{2000};
    std::optional<RecruitmentRules> recruitment{};
    std::vector<std::vector<std::string>> trait_groups{};
    std::optional<WorkplaceMoodRules> mood{};
    std::optional<DismissalRules> dismissal{};
    std::vector<FounderAudienceRule> founder_audiences{};
    std::optional<TalentRules> talent{};
    auto operator<=>(const PeopleRules&) const = default;
};
struct PersonProfile {
    std::string id,name,gender,pronouns,education,role,bio,preferred_mission;
    int birth_year=1990;
    std::uint32_t experience=0,research=0,engineering=0,evaluation=0,mission_fit=0;
    OrdinalDay applied_on{0};
    PortraitRecipe portrait{};
    std::vector<std::string> traits{};
    std::vector<std::string> skills{};
    std::string orientation="unrecorded";
    auto operator<=>(const PersonProfile&) const = default;
};
struct TeamDynamics {
    std::uint32_t creativity=0,compatibility_bp=0;
    std::vector<std::string> traits;
};
[[nodiscard]] TeamDynamics team_dynamics(std::span<const PersonProfile* const>);
[[nodiscard]] TeamDynamics research_team(const SimulationState&,std::span<const std::string>);
[[nodiscard]] std::vector<std::string> distinct_person_traits(const PersonProfile&);
[[nodiscard]] std::uint32_t shared_trait_compatibility(std::uint64_t matches,std::uint64_t pairs);
struct EmployeeMood {
    int score=50, pay=0, company_prestige=0, ceo_prestige=0, role=0, coworkers=0, amenities=0, temperament_effect=0, gatherings=0;
    std::string temperament;
    std::vector<std::string> accessible_amenities;
    int founder_audience=0;
};
[[nodiscard]] std::map<std::string,EmployeeMood> workplace_moods(const SimulationState&);
struct EmploymentOffer {
    Cents salary{0},signing{0},counter_salary{0},counter_signing{0};
    OrdinalDay submitted_on{0},respond_on{0},expires_on{0};
    std::string reply="pending";
    auto operator<=>(const EmploymentOffer&) const = default;
};
struct Recruitment {
    // Private preferences are drawn once when the applicant is generated.
    Cents ask_salary{0},ask_signing{0},minimum_salary{0},minimum_signing{0};
    std::vector<EmploymentOffer> offers;
    std::optional<OrdinalDay> agreed_on,withdrawn_on,hired_on;
    auto operator<=>(const Recruitment&) const = default;
};
struct Applicant {
    PersonProfile profile;
    Cents salary{0},signing{0};
    BasisPoints evaluation_credit{0};
    std::optional<Recruitment> recruitment{};
    auto operator<=>(const Applicant&) const = default;
};
struct Employee {
    Applicant terms;
    OrdinalDay joined_on{0},ready_on{0};
    bool opening=false;
    std::optional<OrdinalDay> departed_on;
    std::optional<Cents> dismissal_severance{};
    auto operator<=>(const Employee&) const = default;
};
struct ContactMessage {
    OrdinalDay day{0};
    std::string topic, reply;
    int mood=-1;
    auto operator<=>(const ContactMessage&) const = default;
};
inline constexpr std::string_view opening_contact_prefix="opening/";
inline constexpr std::size_t opening_contact_limit=17;
struct OpeningConversation {
    std::string source, outcome;
    auto operator<=>(const OpeningConversation&) const = default;
};
inline constexpr std::size_t contact_funding_limit=128;
struct ContactFundingDiscussion {
    std::uint32_t id=0,reputation=0,publications=0;
    std::uint32_t evaluations=0;
    OrdinalDay requested_on{0},expires_on{0};
    std::vector<std::string> offers;
    std::string status="offered",selected,motion;
    std::optional<OrdinalDay> responded_on;
    auto operator<=>(const ContactFundingDiscussion&) const = default;
};
struct PeopleContact {
    std::string person;
    OrdinalDay exchanged_on{0};
    std::vector<ContactMessage> messages;
    std::optional<OpeningConversation> opening{};
    std::vector<ContactFundingDiscussion> funding{};
    auto operator<=>(const PeopleContact&) const = default;
};
struct PeopleState {
    std::uint64_t stream=0,next_id=1;
    OrdinalDay starts_on{0},next_recruitment{0};
    SalaryRate offered_pay{10000};
    std::string mission="research";
    std::uint32_t founding_prestige=0;
    std::uint32_t community_prestige=0, community_buzz=0;
    std::vector<Applicant> applicants;
    std::vector<Employee> employees;
    // Authored specialist/executive offers also have stable, saved personal profiles.
    std::map<std::string,PersonProfile> known_people;
    std::vector<PeopleContact> contacts{};
    std::string founder_gender{};
    std::optional<TalentState> talent{};
    auto operator<=>(const PeopleState&) const = default;
};
struct SetRecruitingTerms {SalaryRate pay{10000};std::string mission;};
struct HireEmployee {std::string id;std::uint32_t desk=0;};
struct ProposeEmployment {std::string id;Cents salary{0},signing{0};};
struct AcceptEmploymentCounter {std::string id;};
struct DeclineApplicant {std::string id;};
using RecruitmentAction=std::variant<ProposeEmployment,AcceptEmploymentCounter,DeclineApplicant>;
struct ManageRecruitment {RecruitmentAction action;};
struct ExchangePeopleContact {std::string person;std::uint32_t room=0;};
struct MessagePeopleContact {std::string person,topic,device;std::uint32_t room=0;};
struct DismissPeopleContact {std::string person,device;std::uint32_t room=0;};
struct RequestContactFunding {std::string person,device;std::uint32_t room=0;};
struct RespondContactFunding {std::string person,device;std::uint32_t discussion=0;std::string reply,terms;std::uint32_t room=0;};
using ContactAction=std::variant<ExchangePeopleContact,MessagePeopleContact,DismissPeopleContact,RequestContactFunding,RespondContactFunding>;
struct ManagePeopleContact {ContactAction action;};
struct DismissalQuote {
    Cents monthly_salary{0},severance{0};
    std::vector<std::string> projects;
    std::uint32_t returned_items=0;
    bool operations_manager=false;
};
[[nodiscard]] Cents employee_monthly_salary(const SimulationState&,const Employee&);
[[nodiscard]] DismissalQuote dismissal_quote(const SimulationState&,std::string_view);
void dismiss_employee(SimulationState&,std::string_view,Cents protected_cash);
void initialize_individual_payroll(SimulationState&);
void validate_employee_departures(const SimulationState&);
void manage_people_contact(SimulationState&,const ContactAction&,Cents protected_cash=Cents{0});
void validate_people_contacts(const SimulationState&);
void request_contact_funding(SimulationState&,PeopleContact&);
void respond_contact_funding(SimulationState&,PeopleContact&,const RespondContactFunding&);
void validate_contact_funding(const SimulationState&);
[[nodiscard]] std::uint32_t contact_funding_reputation(const SimulationState&,OrdinalDay);
[[nodiscard]] std::uint32_t contact_funding_evaluations(const SimulationState&,OrdinalDay);
[[nodiscard]] bool contact_funding_available(const SimulationState&,std::string_view);
[[nodiscard]] std::uint32_t employee_contact_room(const SimulationState&,std::string_view person);
void validate_people_rules(const PeopleRules&);
void initialize_people(SimulationState&,std::uint64_t seed,std::string founder_name={},std::string founder_pronouns={},bool founder_researcher=false,std::uint32_t prestige=0,std::string mission="research");
void prepare_people(SimulationState&);
void validate_people(const SimulationState&,bool deep=true);
void hire_employee(SimulationState&,std::string_view,Cents protected_cash,std::uint32_t desk=0);
void set_recruiting_terms(SimulationState&,const SetRecruitingTerms&);
void withdraw_people(SimulationState&);
void validate_recruitment_rules(const RecruitmentRules&);
void initialize_recruitment(Applicant&,const RecruitmentRules&,class SplitMix64&);
void manage_recruitment(SimulationState&,const RecruitmentAction&);
void prepare_recruitment(SimulationState&);
void validate_recruitment(const Applicant&,const SimulationState&,bool hired);
[[nodiscard]] const FounderAudienceRule* founder_audience_on(const SimulationState&,std::string_view audience,OrdinalDay);
std::string recruitment_status(const Applicant&,OrdinalDay);
std::pair<Cents,Cents> agreed_employment(const Applicant&);
}
