#pragma once

#include <pgp/core/society.hpp>
#include <type_traits>

namespace pgp::content::society_codec {
template<class T,class Field> void fields(T& row,Field f) {
    using Type=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<Type,core::SocietyCatalog>) {
        f("assumptions",row.assumptions);f("regions",row.regions);f("lag_months",row.lag_months);f("media",row.media);f("forum",row.forum);
    } else if constexpr(std::is_same_v<Type,core::SocietyRegion>) {
        f("id",row.id);f("name",row.name);f("population_weight",row.population_weight);f("output_weight",row.output_weight);
        f("initial_components_bp",row.initial);f("adaptation_bp",row.adaptation);f("forum_interest_bp",row.forum_interest);f("implementation_days",row.implementation);
    } else if constexpr(std::is_same_v<Type,core::ForumTerms>) {
        f("id",row.id);f("title",row.title);f("first_agenda_day",row.first_agenda);f("repeat_years",row.repeat_years);f("quorum",row.quorum);
        f("hearing_days",row.hearing);f("term_days",row.term);f("channel",row.channel);f("evidence_axis",row.evidence_axis);
        f("evidence_floor_bp",row.evidence_floor);f("support_threshold_bp",row.support_threshold);
        f("endorsement_shift_bp",row.endorsement_shift);f("endorsement_fee_cents",row.endorsement_fee);
    } else if constexpr(std::is_same_v<Type,core::MediaTerms>) {
        f("fee_cents",row.fee);f("duration_days",row.duration);f("awareness_bp",row.awareness);f("monthly_limit",row.monthly_limit);
    } else if constexpr(std::is_same_v<Type,core::RegionOutcomes>) {
        f("demand",row.demand);f("delivered",row.delivered);f("public_demand",row.public_demand);f("public_work",row.public_work);
        f("science_work",row.science_work);f("industrial_work",row.industrial_work);f("agent_work",row.agent_work);
        f("compute_milli",row.compute_milli);f("promised",row.promised);f("missed",row.missed);f("player_work",row.player_work);
        f("concentration_bp",row.concentration);f("quality_bp",row.quality);f("player_quality_bp",row.player_quality);
        f("player_public",row.player_public);f("player_science",row.player_science);
        f("player_promised",row.player_promised);f("player_missed",row.player_missed);
    } else if constexpr(std::is_same_v<Type,core::SocietyMonth>) {
        f("month_end",row.month_end);f("outcomes",row.outcomes);f("components_bp",row.components);f("audiences_bp",row.audiences);
    } else if constexpr(std::is_same_v<Type,core::MediaCampaign>) {
        f("id",row.id);f("region",row.region);f("service",row.service);f("evidence",row.evidence);f("audience",row.audience);
        f("starts_on",row.starts_on);f("ends_before",row.ends_before);f("paid_cents",row.paid);f("awareness_bp",row.awareness);
    } else if constexpr(std::is_same_v<Type,core::ForumEndorsement>) {
        f("day",row.day);f("region",row.region);f("evidence",row.evidence);f("support",row.support);f("paid_cents",row.paid);
    } else if constexpr(std::is_same_v<Type,core::ForumMember>) {
        f("region",row.region);f("score_bp",row.score);f("supports",row.supports);f("effective_from",row.effective_from);
    } else if constexpr(std::is_same_v<Type,core::ForumRound>) {
        f("id",row.id);f("opened_on",row.opened_on);f("ballot_on",row.ballot_on);f("expires_before",row.expires_before);
        f("endorsements",row.endorsements);f("members",row.members);
    } else if constexpr(std::is_same_v<Type,core::SocietyState>) {
        f("catalog",row.catalog);f("starts_on",row.starts_on);f("prepared_on",row.prepared_on);
        f("months",row.months);f("campaigns",row.campaigns);f("rounds",row.rounds);
    }
}
} // namespace pgp::content::society_codec
