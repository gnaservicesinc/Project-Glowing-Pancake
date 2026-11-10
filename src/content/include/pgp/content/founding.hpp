#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <pgp/core/portrait.hpp>

namespace pgp::content {
inline constexpr std::string_view startup_debt_id="founding/startup_loan";
// Authored setup fields, not executable scripts or another event language.
// All resource values are converted to strong units by scenario initialization.
struct FoundingOption {
    std::string id, name, detail;
    std::int64_t capital_cents=0, monthly_cents=0, fee_cents=0;
    std::uint32_t shares_bp=0, prestige=0, buzz=0;
    std::string family, leg, legal_form;
    auto operator<=>(const FoundingOption&) const = default;
};
struct FormativeScene {
    std::string id, title, date, text;
    std::vector<FoundingOption> choices;
    std::string pool,requires_choice;
    auto operator<=>(const FormativeScene&) const = default;
};
struct FoundingOpeningRules {
    std::uint32_t inherited_staff=3, cofounder_staff=1, ceo_equity_bp=500;
    auto operator<=>(const FoundingOpeningRules&) const = default;
};
struct FoundingLoan {
    std::string lender;
    std::int64_t principal_cents=0, minimum_investment_cents=0;
    std::uint32_t monthly_interest_bp=0, term_days=0;
    auto operator<=>(const FoundingLoan&) const = default;
};
struct FoundingPartner {
    std::string id, name, pronouns, gender, letter, access, objective;
    std::vector<std::string> traits;
    std::int64_t capital_cents=0, monthly_salary_cents=0, cash_floor_cents=0;
    std::uint32_t shares_bp=0, seats=1, maximum_dilution_bp=0;
    auto operator<=>(const FoundingPartner&) const = default;
};
struct FoundingCatalog {
    std::vector<FoundingOption> families, legs, routes, backers, offices, researchers;
    std::vector<FormativeScene> scenes;
    std::int64_t formation_cents=0, compute_monthly_cents=0, founder_salary_cents=0;
    std::uint32_t prestige_finance_bp=0, buzz_finance_bp=0, maximum_finance_bonus_bp=0;
    std::optional<FoundingOpeningRules> opening;
    std::optional<FoundingLoan> startup_loan;
    std::vector<FoundingPartner> partners;
    std::uint32_t queer_community_buzz=0, queer_community_prestige=0;
    std::map<std::string,std::vector<std::string>> skill_rewards,trait_rewards;
    auto operator<=>(const FoundingCatalog&) const = default;
};
struct FoundingSelection {
    std::string mode="campaign", first_name="Alex", last_name="Vale", gender="nonbinary";
    std::string sociability="introverted", activity="sedentary", disposition="deliberate";
    std::string family="middle", leg="genius", route="partnership", legal_form="corporation";
    std::string office="shared", researcher="founder";
    std::map<std::string,std::string,std::less<>> choices;
    std::vector<std::string> backers;
    std::string partner;
    core::PortraitRecipe appearance;
    std::int64_t personal_investment_cents=-1;
    std::string birth_sex="indeterminate", orientation="questioning";
    std::uint64_t story_seed=19972015;
    auto operator<=>(const FoundingSelection&) const = default;
};
inline constexpr std::array<std::string_view,8> founder_orientations{
    "straight","gay_or_lesbian","bisexual","pansexual","asexual","queer","questioning","demisexual"};
[[nodiscard]] std::vector<const FormativeScene*> formative_scenes(const FoundingCatalog&,const FoundingSelection&);
[[nodiscard]] std::vector<std::string> formative_skills(const FoundingCatalog&,const FoundingSelection&);
[[nodiscard]] std::vector<std::string> formative_traits(const FoundingCatalog&,const FoundingSelection&);
[[nodiscard]] bool transgender_founder(const FoundingSelection&);
[[nodiscard]] bool queer_founder(const FoundingSelection&);
struct FoundingTerms {
    std::int64_t contributed_cents=0, paid_setup_cents=0, payroll_cents=0, office_cents=0;
    std::int64_t personal_contribution_cents=0,personal_remaining_cents=0;
    std::uint32_t founder_bp=0, partner_bp=0, prestige=0, buzz=0;
    std::map<std::string,std::uint32_t,std::less<>> backer_shares;
    bool nonprofit=false, founder_researcher=false;
    std::uint32_t opening_staff=0;
    std::optional<FoundingLoan> startup_loan;
    auto operator<=>(const FoundingTerms&) const = default;
};
[[nodiscard]] FoundingCatalog load_founding_catalog(std::string_view);
[[nodiscard]] std::string canonical_founding_catalog(const FoundingCatalog&);
[[nodiscard]] FoundingSelection load_founding_selection(std::string_view);
[[nodiscard]] std::string canonical_founding_selection(const FoundingSelection&);
[[nodiscard]] const FoundingOption& founding_option(const std::vector<FoundingOption>&,std::string_view);
[[nodiscard]] std::string founding_ineligibility(const FoundingOption&,const FoundingSelection&);
[[nodiscard]] const FoundingPartner* founding_partner(const FoundingCatalog&,std::string_view id);
[[nodiscard]] std::string partner_ineligibility(const FoundingPartner&,const FoundingSelection&);
// A complete validation/preview is pure: changing an answer cannot stack rewards.
[[nodiscard]] FoundingTerms founding_terms(const FoundingCatalog&,const FoundingSelection&,bool complete=true);
}
