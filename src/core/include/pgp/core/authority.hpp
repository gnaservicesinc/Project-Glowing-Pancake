#pragma once
#include <pgp/core/society.hpp>

namespace pgp::core {
inline constexpr std::size_t authority_record_limit=4096, authority_active_limit=32, authority_case_limit=4096;
inline constexpr std::int64_t authority_maximum_lead_days=90,authority_maximum_term_days=730;
inline constexpr std::string_view authority_hire_prefix="authority.hire/";
enum class AuthorityKind { standards,infrastructure,delegation,coercive };
enum class PublicPromiseKind { publication,public_work };
enum class PublicCaseKind { promise,delivery,resistance };
[[nodiscard]] std::string_view authority_key(AuthorityKind);
[[nodiscard]] std::string_view authority_key(PublicPromiseKind);
[[nodiscard]] std::string_view authority_key(PublicCaseKind);
struct AuthorityRegion {
    std::string id,issuer;
    BasisPoints consent{6500};
    auto operator<=>(const AuthorityRegion&) const = default;
};
struct AuthorityOffer {
    std::string id,name,rights;
    AuthorityKind kind=AuthorityKind::standards;
    Cents setup{0},monthly{0};
    MilliCUPerDay compute{0};
    std::uint32_t staff=1;
    Days lead{14},term{360};
    std::uint64_t public_work=10000;
    BasisPoints points{3000},evidence_floor{6000},consent_floor{6000};
    auto operator<=>(const AuthorityOffer&) const = default;
};
struct AuthorityCatalog {
    std::string assumptions;
    std::vector<AuthorityRegion> regions;
    std::vector<AuthorityOffer> offers;
    Days promise_window{30},concern_period{7},response_window{21},extension{30},sanctions{14},identity_memory{730};
    Days reapplication_delay{90},directive_spacing{7};
    Cents remedy_fee{0};
    MilliCUPerDay remedy_compute{0};
    std::uint32_t remedy_staff=1;
    std::uint64_t promise_tasks=10000;
    BasisPoints minimum_operation{8000},procurement_floor{7000};
    BasisPoints initial_resistance{2000},resistance_growth{750},resistance_decay{500},remedy_relief{1500},resistance_threshold{5000};
    // testimony, kept promise, broken promise, executive disclosure,
    // coercive application, resistance, revoked delegation.
    std::array<std::array<std::int32_t,society_audiences>,7> identity_effects{};
    auto operator<=>(const AuthorityCatalog&) const = default;
};
struct PublicTestimony {
    std::uint64_t id=0;
    std::string report;
    OrdinalDay day{0};
    auto operator<=>(const PublicTestimony&) const = default;
};
struct PublicPromise {
    std::uint64_t id=0,deployment=0;
    PublicPromiseKind kind=PublicPromiseKind::public_work;
    std::string region,subject;
    OrdinalDay made_on{0},due_on{0};
    std::uint64_t target=0,progress=0;
    std::optional<OrdinalDay> met_on;
    std::optional<OrdinalDay> closed_on;
    auto operator<=>(const PublicPromise&) const = default;
};
struct InstitutionalReview {
    OrdinalDay day{0};
    std::uint64_t work=0,required_work=0,public_work=0,required_public=0;
    BasisPoints operation{0},consent{0},score{0},evidence{0};
    bool approved=false;
    auto operator<=>(const InstitutionalReview&) const = default;
};
struct ProcurementDirective {
    OrdinalDay day{0};bool enabled=false;
    auto operator<=>(const ProcurementDirective&) const = default;
};
struct AuthorityMandate {
    std::uint64_t id=0,testimony=0,promise=0,forum_round=0,deployment=0;
    std::string offer,region,service,project,source,site;
    OrdinalDay applied_on{0},decision_on{0},ends_before{0};
    std::optional<OrdinalDay> withdrawn_on,staff_released_on,revoked_on;
    std::vector<InstitutionalReview> reviews;
    std::vector<ProcurementDirective> directives;
    auto operator<=>(const AuthorityMandate&) const = default;
};
struct PublicCase {
    std::uint64_t id=0,subject=0,testimony=0;
    PublicCaseKind kind=PublicCaseKind::promise;
    std::string region;
    OrdinalDay opened_on{0},deadline{0};
    std::optional<OrdinalDay> negotiated_on,resolved_on,disclosed_on,cited_on;
    std::string remedy_project;
    bool acknowledged=false;
    auto operator<=>(const PublicCase&) const = default;
};
struct ExecutiveNegotiation {
    std::string dispute,project;
    OrdinalDay day{0};
    auto operator<=>(const ExecutiveNegotiation&) const = default;
};
struct AuthorityMonth {
    OrdinalDay month_end{0};
    std::array<std::uint32_t,society_regions> soft{},hard{},resistance{};
    std::uint32_t population_coverage=0,output_coverage=0;
    auto operator<=>(const AuthorityMonth&) const = default;
};
struct AuthorityState {
    AuthorityCatalog catalog;
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> prepared_on;
    std::vector<PublicTestimony> testimonies;
    std::vector<PublicPromise> promises;
    std::vector<AuthorityMandate> mandates;
    std::vector<PublicCase> cases;
    std::vector<ExecutiveNegotiation> executive_negotiations;
    std::vector<AuthorityMonth> months;
    auto operator<=>(const AuthorityState&) const = default;
};
struct SubmitPublicTestimony {std::string report;std::uint64_t case_id=0;};
struct MakePublicPromise {PublicPromiseKind kind=PublicPromiseKind::public_work;std::string region,subject;};
struct RequestInstitutionalMandate {std::string offer,region,service,source,site;std::uint64_t testimony=0,promise=0;};
struct NegotiatePublicCase {std::uint64_t id=0;std::string source;};
struct NegotiateExecutiveDispute {std::string id,source;};
struct AcknowledgePublicCase {std::uint64_t id=0;};
struct RelinquishInstitutionalMandate {std::uint64_t id=0;};
struct DirectRegionalProcurement {std::uint64_t id=0;bool enabled=false;};
void validate_authority_catalog(const AuthorityCatalog&);
[[nodiscard]] const AuthorityOffer& authority_offer(const AuthorityCatalog&,std::string_view);
[[nodiscard]] std::string_view public_case_phase(const PublicCase&,OrdinalDay,const AuthorityCatalog&);
} // namespace pgp::core
