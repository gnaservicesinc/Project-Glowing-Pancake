#pragma once
#include <pgp/core/accounts.hpp>

namespace pgp::core {
struct SimulationState;
struct CorporateFinanceTerms {
    std::string id,name,negotiation;
    Cents proceeds{0},covenant_cash_floor{0},waiver_fee{0};
    BasisPoints acquired{0},monthly_interest{0};
    Days term{0};
    bool board_seat=false;
    bool donation=false;
    auto operator<=>(const CorporateFinanceTerms&) const = default;
};
struct ExecutiveCandidate {
    std::string id,name,role,priorities,publication_research;
    Cents signing_cost{0},cash_salary{0},hybrid_salary{0};
    BasisPoints hybrid_grant{0};
    Days cliff{0},vesting{0},promise_term{0};
    std::uint32_t technical=0,commercial=0,risk_tolerance=0;
    auto operator<=>(const ExecutiveCandidate&) const = default;
};
struct BoardMember {
    std::string id,name;
    std::uint32_t seats=1;
    bool player=false;
    Cents cash_floor{0};
    BasisPoints maximum_dilution{0};
    auto operator<=>(const BoardMember&) const = default;
};
struct OrganizationalCharter {
    std::string id,name,priorities,employee_expectations;
    Cents cash_floor{0},monthly_employee_budget{0};
    BasisPoints maximum_dilution{0};
    auto operator<=>(const OrganizationalCharter&) const = default;
};
struct PublicListingRules {
    std::string offer_id;
    Days minimum_age{3650},performance_days{365};
    Cents minimum_revenue{0},minimum_profit{0},director_cash_floor{0};
    BasisPoints director_maximum_dilution{0};
    auto operator<=>(const PublicListingRules&) const = default;
};
struct ContactFinanceOffer {
    std::string person,terms;
    std::uint32_t minimum_reputation=0,minimum_publications=0;
    std::uint32_t minimum_evaluations=0;
    auto operator<=>(const ContactFinanceOffer&) const = default;
};
struct ContactFinanceRules {
    Days retry{30},validity{30};
    std::vector<ContactFinanceOffer> offers;
    auto operator<=>(const ContactFinanceRules&) const = default;
};
struct GovernanceCatalog {
    std::vector<CorporateFinanceTerms> financing;
    std::vector<ExecutiveCandidate> executives;
    std::vector<BoardMember> board;
    Days vote_window{14},concern_period{7},response_window{30};
    BasisPoints maximum_pool_expansion{1000},acquisition_equity{1000};
    Cents acquisition_premium{10000000};
    Cents subsidiary_cash_floor{10000000};
    Days subsidiary_reserve_days{30};
    std::vector<OrganizationalCharter> charters{};
    std::optional<PublicListingRules> public_listing{};
    std::optional<ContactFinanceRules> contact_finance{};
    auto operator<=>(const GovernanceCatalog&) const = default;
};
struct ExecutiveAppointment {
    std::string candidate;
    bool hybrid=false;
    OrdinalDay hired_on{0};
    std::optional<OrdinalDay> departed_on;
    auto operator<=>(const ExecutiveAppointment&) const = default;
};
// Unvested claims remain inside the one fully diluted reserved pool. Vested
// claims have distinct holdings; a financing round dilutes both exactly once.
struct OptionGrant {
    std::string candidate;
    BasisPoints unvested{0};
    bool departed=false;
    auto operator<=>(const OptionGrant&) const = default;
};
struct CapitalEvent {
    std::string kind,subject,key;
    OrdinalDay day{0};
    BasisPoints shares{0};
    Cents consideration{0};
    auto operator<=>(const CapitalEvent&) const = default;
};
enum class BoardMotionKind { financing,pool_expansion,acquisition,charter };
enum class BoardMotionStatus { pending,approved,rejected,expired };
struct BoardBallot {
    std::string member;
    std::uint32_t seats=0;
    bool support=false;
    auto operator<=>(const BoardBallot&) const = default;
};
struct BoardMotion {
    std::string id,subject;
    BoardMotionKind kind=BoardMotionKind::financing;
    BoardMotionStatus status=BoardMotionStatus::pending;
    OrdinalDay opened_on{0},deadline{0};
    std::optional<OrdinalDay> resolved_on;
    Cents cash{0},asset_value{0},reviewed_cash{0};
    BasisPoints shares{0};
    std::uint64_t posting_cursor=0;
    std::optional<bool> player_vote;
    std::vector<BoardBallot> ballots;
    bool executed=false;
    std::uint64_t leadership_cursor=0;
    std::uint64_t charter_cursor=0;
    auto operator<=>(const BoardMotion&) const = default;
};
enum class CorporateDisputeStatus { concern,formal,resolved,departed,accelerated,waived };
struct CorporateDispute {
    std::string id,subject,kind;
    OrdinalDay opened_on{0},deadline{0};
    CorporateDisputeStatus status=CorporateDisputeStatus::concern;
    std::optional<OrdinalDay> resolved_on;
    bool acknowledged=false;
    Cents opening_cash{0};
    std::uint64_t posting_cursor=0,publication_cursor=0;
    std::optional<OrdinalDay> negotiated_on;
    std::string review_project;
    auto operator<=>(const CorporateDispute&) const = default;
};
struct CharterChange {
    std::string charter,motion;
    OrdinalDay day{0};
    auto operator<=>(const CharterChange&) const = default;
};
inline constexpr std::size_t public_filing_limit=retained_campaign_days/89+2;
struct PublicFiling {
    OrdinalDay starts_on{0},ends_before{0};
    Cents revenue{0},expenses{0},ending_cash{0};
    [[nodiscard]] Cents profit() const {return checked_unit_subtract(revenue,expenses);}
    auto operator<=>(const PublicFiling&) const = default;
};
struct ListingQualification {
    OrdinalDay eligible_on{0};
    Cents revenue{0},profit{0};
    bool old_enough=false,qualified=false;
};
struct GovernanceState {
    std::vector<EquityHolding> opening_holdings;
    std::vector<ExecutiveAppointment> appointments;
    std::vector<OptionGrant> grants;
    std::vector<CapitalEvent> capital;
    std::vector<BoardMotion> motions;
    std::vector<CorporateDispute> disputes;
    std::vector<CharterChange> charters{};
    PagedHistory<PublicFiling,16> public_filings;
    auto operator<=>(const GovernanceState&) const = default;
};
struct NegotiateCorporateFinance { std::string terms_id; };
struct AppointExecutive { std::string candidate;bool hybrid=false; };
struct ProposePoolExpansion { BasisPoints shares{0}; };
struct ProposeCharter { std::string id; };
struct VoteBoardMotion { std::string id;bool support=false; };
struct RespondCorporateDispute { std::string id;bool buy_waiver=false; };
struct TransferCompanyOwnership { std::string parent; };
struct TransferGroupFunds { std::string parent,target;std::uint32_t sequence=0;Cents amount{0};bool funding=false,parent_leg=false; };

void validate_governance_catalog(const GovernanceCatalog&);
[[nodiscard]] bool nonprofit_organization(const SimulationState&);
void validate_governance(const SimulationState&,bool deep=true);
void prepare_governance(SimulationState&);
void close_public_reporting_day(SimulationState&);
[[nodiscard]] ListingQualification listing_qualification(const SimulationState&,OrdinalDay);
[[nodiscard]] std::optional<OrdinalDay> public_listing_date(const SimulationState&);
[[nodiscard]] const PublicFiling* latest_public_filing(const SimulationState&,OrdinalDay);
[[nodiscard]] bool required_governance_notice(const SimulationState&);
[[nodiscard]] BasisPoints unallocated_option_pool(const SimulationState&);
[[nodiscard]] std::vector<BoardMember> current_board(const SimulationState&);
[[nodiscard]] std::vector<EquityHolding> corporate_holdings_before(const SimulationState&,OrdinalDay);
void company_equity(SimulationState&,std::string investor,Cents proceeds,BasisPoints acquired,std::string key,bool assets=false);
void negotiate_corporate_finance(SimulationState&,std::string_view,bool from_contact=false);
void appoint_executive(SimulationState&,std::string_view,bool hybrid,Cents protected_cash);
void propose_pool_expansion(SimulationState&,BasisPoints);
void initialize_charter(SimulationState&,std::string_view);
void propose_charter(SimulationState&,std::string_view);
[[nodiscard]] const OrganizationalCharter* active_charter(const SimulationState&);
void propose_acquisition_board(SimulationState&,std::string target,Cents cash,Cents asset_value,BasisPoints shares);
void vote_board_motion(SimulationState&,std::string_view,bool,Cents protected_cash);
void respond_corporate_dispute(SimulationState&,std::string_view,bool,Cents protected_cash);
void extend_executive_response(SimulationState&,std::string_view dispute,std::string project);
}
