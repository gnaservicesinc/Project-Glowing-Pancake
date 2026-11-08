#pragma once

#include <pgp/core/civic.hpp>
#include "society_codec.hpp"
#include "authority_codec.hpp"
#include "frontier_codec.hpp"
#include "succession_codec.hpp"
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace pgp::content::civic_codec {
using Json = nlohmann::json;
[[noreturn]] inline void invalid() { throw std::invalid_argument("Civic data has invalid, unsupported or missing typed fields."); }
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type {};
template<class T> struct Extension {};
template<class T, class Field> void fields(T& row, Field field) {
    using Type = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<Type, core::CivicCatalog>) {
        field("assumptions", row.assumptions); field("blocs", row.blocs); field("constituencies", row.constituencies);
        field("policies", row.policies); field("opening_president", row.opening_president);
        field("adoption_reference_bp", row.adoption_reference); field("election_uncertainty_bp", row.election_uncertainty);
    } else if constexpr (std::is_same_v<Type, core::CivicBloc>) {
        field("id", row.id); field("name", row.name); field("positions_bp", row.positions); field("campaign_effort_bp", row.campaign_effort);
    } else if constexpr (std::is_same_v<Type, core::CivicConstituency>) {
        field("id", row.id); field("name", row.name); field("house_seats", row.house_seats); field("senate_classes", row.senate_classes);
        field("governor_cycle", row.governor_cycle); field("preferences_bp", row.preferences); field("priority", row.priority);
        field("sector_exposure_bp", row.sector_exposure);
    } else if constexpr (std::is_same_v<Type, core::CivicPolicy>) {
        field("id", row.id); field("title", row.title); field("explanation", row.explanation); field("jurisdiction", row.jurisdiction);
        field("issue", row.issue); field("rule", row.rule); field("first_agenda_day", row.first_agenda); field("repeat_years", row.repeat_years);
        field("case_strength_bp", row.case_strength); field("hearing_days", row.hearing); field("vote_spacing_days", row.vote_spacing);
        field("implementation_days", row.implementation); field("term_days", row.term); field("review_days", row.review);
        field("review_floor_bp", row.review_floor); field("advocacy_fee_cents", row.advocacy_fee);
        field("advocacy_shift_bp", row.advocacy_shift); field("evidence_shift_bp", row.evidence_shift);
        field("channel", row.channel); field("evidence_axis", row.evidence_axis); field("evidence_floor_bp", row.evidence_floor);
        field("exempt_reviewed_service", row.exempt_reviewed_service); field("permit_watts", row.permit_watts);
    } else if constexpr (std::is_same_v<Type, core::CivicState>) {
        field("catalog", row.catalog); field("starts_on", row.starts_on); field("politics_origin", row.politics_origin);
        field("prepared_on", row.prepared_on); field("next_boundary", row.next_boundary); field("last_effective_on", row.last_effective_on);
        field("elections", row.elections); field("dockets", row.dockets);
    } else if constexpr (std::is_same_v<Type, core::CivicInstitutions>) {
        field("house", row.house); field("senate_classes", row.senate_classes); field("assemblies", row.assemblies);
        field("governors", row.governors); field("president", row.president);
    } else if constexpr (std::is_same_v<Type, core::CivicElection>) {
        field("day", row.day); field("adoption_bp", row.adoption); field("performance_bp", row.performance);
        field("stream_before", row.stream_before); field("stream_after", row.stream_after);
        field("electors", row.electors); field("result", row.result);
    } else if constexpr (std::is_same_v<Type, core::CivicDocket>) {
        field("id", row.id); field("policy", row.policy_id); field("opened_on", row.opened_on);
        field("stage", row.stage); field("next_on", row.next_on); field("effective_from", row.effective_from);
        field("expires_before", row.expires_before); field("submissions", row.submissions); field("decisions", row.decisions);
    } else if constexpr (std::is_same_v<Type, core::CivicSubmission>) {
        field("day", row.day); field("support", row.support); field("evidence", row.evidence); field("fee_cents", row.fee);
        field("influence_bp", row.influence);
    } else if constexpr (std::is_same_v<Type, core::CivicDecision>) {
        field("stage", row.stage); field("day", row.day); field("yes", row.yes); field("seats", row.seats); field("passed", row.passed);
    } else if constexpr (requires { Extension<Type>::fields(row,field); }) {
        Extension<Type>::fields(row,field);
    } else {
        society_codec::fields(row,field);
        authority_codec::fields(row,field);
        frontier_codec::fields(row,field);
        succession_codec::fields(row,field);
    }
}
template<class T> constexpr std::size_t maximum_rows() {
    if constexpr (requires { Extension<T>::maximum_rows; }) return Extension<T>::maximum_rows;
    else if constexpr (std::is_same_v<T,core::LeadershipTerm>) return core::succession_term_limit;
    else if constexpr (std::is_same_v<T,core::LeadershipControl> || std::is_same_v<T,core::SuccessionNotice>) return core::succession_notice_limit;
    else if constexpr (std::is_same_v<T,core::DepartureMessage>) return 6;
    else if constexpr (std::is_same_v<T,core::DepartureRelease>) return core::succession_plan_limit;
    else if constexpr (std::is_same_v<T,core::SuccessionPlan>) return core::succession_plan_limit;
    else if constexpr (std::is_same_v<T,core::LeadershipTrack>) return 4;
    else if constexpr (std::is_same_v<T,core::BoardBallot>) return 17;
    else if constexpr (std::is_same_v<T,core::SocietyMonth>) return core::society_month_limit;
    else if constexpr (std::is_same_v<T,core::AuthorityMonth>) return core::society_month_limit;
    else if constexpr (std::is_same_v<T,core::FrontierTrial>) return core::frontier_trial_limit;
    else if constexpr (std::is_same_v<T,core::FrontierSystem>) return core::frontier_system_limit;
    else if constexpr (std::is_same_v<T,core::FrontierGate>) return 64;
    else if constexpr (std::is_same_v<T,core::FrontierResearchProgram> || std::is_same_v<T,core::FrontierRefreshProgram>) return core::frontier_policy_limit;
    else if constexpr (std::is_same_v<T,core::FrontierResearchDecision> || std::is_same_v<T,core::FrontierRefreshRun>) return core::frontier_policy_run_limit;
    else if constexpr (std::is_same_v<T,std::string>) return 64;
    else if constexpr (std::is_same_v<T,core::AuthorityMandate> || std::is_same_v<T,core::PublicTestimony> ||
        std::is_same_v<T,core::PublicPromise> || std::is_same_v<T,core::ExecutiveNegotiation>) return core::authority_record_limit;
    else if constexpr (std::is_same_v<T,core::PublicCase>) return core::authority_case_limit;
    else if constexpr (std::is_same_v<T,core::InstitutionalReview> || std::is_same_v<T,core::ProcurementDirective>) return 64;
    else if constexpr (std::is_same_v<T,core::MediaCampaign>) return core::society_campaign_limit;
    else if constexpr (std::is_same_v<T,core::ForumRound>) return core::society_forum_limit;
    else if constexpr (std::is_same_v<T, core::CivicElection>) return core::civic_election_limit;
    else if constexpr (std::is_same_v<T, core::CivicDocket>) return core::civic_docket_limit;
    else if constexpr (std::is_same_v<T, core::CivicSubmission>) return core::civic_advocacy_limit + 1;
    else if constexpr (std::is_same_v<T, core::CivicDecision>) return 5;
    else return core::civic_jurisdictions;
}
template<class T> T decode(const Json& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (!value.is_string() || value.get_ref<const std::string&>().size() > 4096) invalid();
        return value.get<std::string>();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!value.is_boolean()) invalid();
        return value.get<bool>();
    } else if constexpr (succession_codec::enum_count<T> != 0) {
        const auto number=decode<unsigned>(value);
        if(number>=succession_codec::enum_count<T>) invalid();
        return static_cast<T>(number);
    } else if constexpr (std::is_same_v<T, core::ProductChannel>) {
        return core::parse_channel(decode<std::string>(value));
    } else if constexpr (authority_codec::is_enum<T>) {
        const auto name=decode<std::string>(value);
        constexpr auto count=std::is_same_v<T,core::AuthorityKind>?4U:std::is_same_v<T,core::PublicPromiseKind>?2U:3U;
        for(std::size_t i=0;i<count;++i) if(core::authority_key(static_cast<T>(i))==name) return static_cast<T>(i);
        invalid();
    } else if constexpr (frontier_codec::enum_count<T> != 0) {
        const auto name = decode<std::string>(value);
        for (std::size_t i=0; i<frontier_codec::enum_count<T>; ++i)
            if (core::frontier_key(static_cast<T>(i))==name) return static_cast<T>(i);
        invalid();
    } else if constexpr (requires { Extension<T>::key(T{}); }) {
        const auto name=decode<std::string>(value);
        for(std::size_t i=0;i<Extension<T>::count;++i) if(Extension<T>::key(static_cast<T>(i))==name) return static_cast<T>(i);
        invalid();
    } else if constexpr (std::is_enum_v<T>) {
        const auto name = decode<std::string>(value);
        constexpr auto count = std::is_same_v<T, core::CivicIssue> ? core::civic_issues : std::is_same_v<T, core::CivicRule> ? 3U : 10U;
        for (std::size_t i = 0; i < count; ++i) if (core::civic_key(static_cast<T>(i)) == name) return static_cast<T>(i);
        invalid();
    } else if constexpr (std::is_integral_v<T>) {
        if (!value.is_number_integer()) invalid();
        if (value.is_number_unsigned()) {
            const auto number = value.get<std::uint64_t>();
            if (!std::in_range<T>(number)) invalid();
            return static_cast<T>(number);
        }
        const auto number = value.get<std::int64_t>();
        if (!std::in_range<T>(number)) invalid();
        return static_cast<T>(number);
    } else if constexpr (Optional<T>::value) {
        return value.is_null() ? T{} : T{decode<typename T::value_type>(value)};
    } else if constexpr (requires(T row) { row.value(); }) {
        return T{decode<std::int64_t>(value)};
    } else if constexpr (requires { std::tuple_size<T>::value; }) {
        if (!value.is_array() || value.size() != std::tuple_size_v<T>) invalid();
        T row{};
        for (std::size_t i = 0; i < row.size(); ++i) row[i] = decode<typename T::value_type>(value.at(i));
        return row;
    } else if constexpr (requires(T row) { row.push_back(typename T::value_type{}); }) {
        if (!value.is_array() || value.size() > maximum_rows<typename T::value_type>()) invalid();
        T rows;
        rows.reserve(value.size());
        for (const auto& item : value) rows.push_back(decode<typename T::value_type>(item));
        return rows;
    } else {
        if (!value.is_object()) invalid();
        T row;
        std::size_t count = 0;
        fields(row, [&](const char* name, auto& field,bool optional_key=false) {
            if (!value.contains(name)) {if(optional_key) return;invalid();}
            field = decode<std::remove_cvref_t<decltype(field)>>(value.at(name));
            ++count;
        });
        if (value.size() != count) invalid();
        return row;
    }
}
template<class T> Json encode(const T& value) {
    if constexpr (std::is_same_v<T, std::string> || std::is_integral_v<T>) return value;
    else if constexpr (succession_codec::enum_count<T> != 0) return static_cast<unsigned>(value);
    else if constexpr (std::is_same_v<T, core::ProductChannel>) return core::channel_key(value);
    else if constexpr (authority_codec::is_enum<T>) return core::authority_key(value);
    else if constexpr (frontier_codec::enum_count<T> != 0) return core::frontier_key(value);
    else if constexpr (requires { Extension<T>::key(value); }) return Extension<T>::key(value);
    else if constexpr (std::is_enum_v<T>) return core::civic_key(value);
    else if constexpr (Optional<T>::value) return value ? encode(*value) : Json(nullptr);
    else if constexpr (requires { value.value(); }) return value.value();
    else if constexpr (requires { value.begin(); value.end(); }) {
        auto result = Json::array();
        for (const auto& row : value) result.push_back(encode(row));
        return result;
    } else {
        auto result = Json::object();
        fields(value, [&](const char* name, const auto& field,bool optional_key=false) {
            if constexpr (Optional<std::remove_cvref_t<decltype(field)>>::value) if(optional_key && !field) return;
            result[name] = encode(field);
        });
        return result;
    }
}
} // namespace pgp::content::civic_codec
