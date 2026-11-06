#pragma once

#include <pgp/core/market.hpp>

#include <array>
#include <span>

namespace pgp::core {
struct SimulationState;
struct DeployModel;
inline constexpr std::size_t civic_blocs = 3, civic_issues = 7, civic_jurisdictions = 51;
inline constexpr std::size_t civic_election_limit = (retained_campaign_months + 23) / 24;
inline constexpr std::size_t civic_docket_limit = 2048;
inline constexpr std::size_t civic_advocacy_limit = 3;
enum class CivicIssue { privacy, innovation, labor, competition, investment, security, access };
enum class CivicRule { launch_evidence, site_power, final_closure };
enum class CivicStage { agenda, committee, house, senate, executive, review, agency, effective, rejected, expired };
[[nodiscard]] std::string_view civic_key(CivicIssue);
[[nodiscard]] std::string_view civic_key(CivicRule);
[[nodiscard]] std::string_view civic_key(CivicStage);
struct CivicBloc {
    std::string id, name;
    std::array<std::int32_t, civic_issues> positions{};
    BasisPoints campaign_effort{0};
    auto operator<=>(const CivicBloc&) const = default;
};
struct CivicConstituency {
    std::string id, name;
    std::uint32_t house_seats = 0;
    std::array<bool, 3> senate_classes{};
    std::uint32_t governor_cycle = 0;
    std::array<std::uint32_t, civic_blocs> preferences{};
    CivicIssue priority = CivicIssue::access;
    BasisPoints sector_exposure{0};
    auto operator<=>(const CivicConstituency&) const = default;
};
struct CivicPolicy {
    std::string id, title, explanation, jurisdiction;
    CivicIssue issue = CivicIssue::access;
    CivicRule rule = CivicRule::launch_evidence;
    OrdinalDay first_agenda{0};
    std::uint32_t repeat_years = 0;
    BasisPointDelta case_strength{0};
    Days hearing{1}, vote_spacing{1}, implementation{1}, term{1}, review{0};
    BasisPoints review_floor{0};
    Cents advocacy_fee{0};
    BasisPoints advocacy_shift{0}, evidence_shift{0};
    ProductChannel channel = ProductChannel::legacy;
    std::uint32_t evidence_axis = 0;
    BasisPoints evidence_floor{0};
    bool exempt_reviewed_service = false;
    std::uint64_t permit_watts = 0;
    auto operator<=>(const CivicPolicy&) const = default;
};
struct CivicCatalog {
    std::string assumptions;
    std::array<CivicBloc, civic_blocs> blocs;
    std::vector<CivicConstituency> constituencies;
    std::vector<CivicPolicy> policies;
    std::uint32_t opening_president = 0;
    BasisPoints adoption_reference{0}, election_uncertainty{0};
    auto operator<=>(const CivicCatalog&) const = default;
};
using CivicSeats = std::array<std::uint32_t, civic_blocs>;
struct CivicInstitutions {
    CivicSeats house{};
    std::array<CivicSeats, 3> senate_classes{};
    std::vector<CivicSeats> assemblies;
    std::vector<std::uint32_t> governors;
    std::uint32_t president = 0;
    auto operator<=>(const CivicInstitutions&) const = default;
};
struct CivicElection {
    OrdinalDay day{0};
    BasisPoints adoption{0};
    BasisPointDelta performance{0};
    std::uint64_t stream_before = 0, stream_after = 0;
    CivicSeats electors{};
    CivicInstitutions result;
    auto operator<=>(const CivicElection&) const = default;
};
struct CivicSubmission {
    OrdinalDay day{0};
    bool support = true;
    std::string evidence;
    Cents fee{0};
    BasisPoints influence{0};
    auto operator<=>(const CivicSubmission&) const = default;
};
struct CivicDecision {
    CivicStage stage = CivicStage::committee;
    OrdinalDay day{0};
    std::uint32_t yes = 0, seats = 0;
    bool passed = false;
    auto operator<=>(const CivicDecision&) const = default;
};
struct CivicDocket {
    std::uint64_t id = 0;
    std::string policy_id;
    OrdinalDay opened_on{0};
    CivicStage stage = CivicStage::agenda;
    std::optional<OrdinalDay> next_on, effective_from, expires_before;
    std::vector<CivicSubmission> submissions;
    std::vector<CivicDecision> decisions;
    auto operator<=>(const CivicDocket&) const = default;
};
struct CivicState {
    CivicCatalog catalog;
    OrdinalDay starts_on{0};
    std::uint64_t politics_origin = 0;
    std::optional<OrdinalDay> prepared_on, next_boundary, last_effective_on;
    std::vector<CivicElection> elections;
    std::vector<CivicDocket> dockets;
    auto operator<=>(const CivicState&) const = default;
};
struct FundPolicyAdvocacy { std::uint64_t docket; bool support; };
struct SubmitPolicyEvidence { std::uint64_t docket; std::string evidence; bool support; };
struct CivicAdvocacyTerms {
    Cents fee{0};
    BasisPoints influence{0};
    std::size_t remaining = 0;
};
[[nodiscard]] CivicState opening_civic(CivicCatalog catalog, OrdinalDay start, std::uint64_t stream_origin);
void validate_civic_catalog(const CivicCatalog&);
void validate_civic(const SimulationState&, bool deep);
void prepare_civic(SimulationState&, const std::vector<MarketAdoption>* industry_adoptions = nullptr);
[[nodiscard]] BasisPoints civic_adoption(const MarketState&, std::span<const MarketAdoption>, OrdinalDay);
[[nodiscard]] BasisPoints civic_adoption(const MarketState&, const MarketAdoptionHistory&, OrdinalDay);
[[nodiscard]] CivicInstitutions civic_institutions(const CivicState&, OrdinalDay);
[[nodiscard]] CivicSeats civic_senate(const CivicInstitutions&);
[[nodiscard]] std::optional<OrdinalDay> next_civic_election(OrdinalDay from);
[[nodiscard]] OrdinalDay civic_seating(OrdinalDay election, bool executive);
[[nodiscard]] const CivicPolicy& civic_policy(const CivicCatalog&, std::string_view id);
[[nodiscard]] std::optional<CivicAdvocacyTerms> civic_advocacy_terms(const CivicState&, const CivicDocket&, OrdinalDay);
[[nodiscard]] std::vector<std::string> civic_evidence_options(const SimulationState&, const CivicDocket&);
void fund_policy_advocacy(SimulationState&, const FundPolicyAdvocacy&, Cents protected_cash);
void submit_policy_evidence(SimulationState&, const SubmitPolicyEvidence&);
[[nodiscard]] std::string civic_launch_block(const CivicState*, const SimulationState&, const DeployModel&, OrdinalDay);
[[nodiscard]] std::string civic_site_block(const CivicState*, std::string_view state, std::uint64_t watts, OrdinalDay);
[[nodiscard]] bool civic_rule_effective_today(const CivicState&, OrdinalDay);
void validate_civic_deployments(const CivicState&, const SimulationState&);
} // namespace pgp::core
