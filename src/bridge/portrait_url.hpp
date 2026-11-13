#pragma once

#include <pgp/core/calendar.hpp>
#include <pgp/core/people.hpp>
#include <pgp/core/portrait.hpp>
#include <pgp/runtime/family.hpp>

#include <QString>

#include <algorithm>

namespace pgp::bridge {

/// Compute a person's chronological age in whole years at `now`.
[[nodiscard]] inline int portrait_person_age(const core::PersonProfile& profile,
                                             core::OrdinalDay now) noexcept {
    return profile.romantic
        ? runtime::family_age(profile.romantic->born_on, now)
        : std::max(0, core::from_ordinal(now).year - profile.birth_year);
}

/// Format an `image://portraits/...` URL encoding `recipe` and `stage`.
[[nodiscard]] inline QString portrait_url(const core::PortraitRecipe& recipe,
                                          core::LifeStage stage) {
    QString url = QStringLiteral("image://portraits/%1").arg(recipe.kit);
    for (const auto part : recipe.parts) url += '/' + QString::number(part);
    for (const auto part : recipe.style) url += '/' + QString::number(part);
    const auto key = core::life_stage_key(stage);
    url += '/';
    url += QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
    return url;
}

/// Format an `image://portraits/...` URL from `recipe` and whole-year `age`.
[[nodiscard]] inline QString portrait_url(const core::PortraitRecipe& recipe, int age = 25) {
    return portrait_url(recipe, core::life_stage_for_age(age));
}

/// Format a life-stage-aware `image://portraits/...` URL for `profile` at `now`.
[[nodiscard]] inline QString portrait_url(const core::PersonProfile& profile,
                                          core::OrdinalDay now) {
    return portrait_url(profile.portrait, portrait_person_age(profile, now));
}

} // namespace pgp::bridge
