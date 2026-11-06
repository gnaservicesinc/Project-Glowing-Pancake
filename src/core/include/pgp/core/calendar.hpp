#pragma once

#include <pgp/core/units.hpp>

#include <compare>
#include <cstddef>
#include <string>
#include <string_view>

namespace pgp::core {

// Storage budgets cover the scored century and five more sandbox centuries.
// These budgets bound retained records independently of the calendar itself.
inline constexpr std::size_t retained_campaign_days = 220'000;
inline constexpr std::size_t retained_campaign_months = (retained_campaign_days + 27) / 28;

struct CivilDate {
    int year;
    unsigned month;
    unsigned day;
    auto operator<=>(const CivilDate&) const = default;
};

// Proleptic Gregorian dates, years 1..9999, with 2014-01-01 == OrdinalDay{0}.
// Invalid dates or ordinals outside this calendar range throw out_of_range.
[[nodiscard]] bool is_leap_year(int year);
[[nodiscard]] unsigned days_in_month(int year, unsigned month);
[[nodiscard]] OrdinalDay to_ordinal(CivilDate date);
[[nodiscard]] CivilDate from_ordinal(OrdinalDay date);
[[nodiscard]] OrdinalDay advance_days(OrdinalDay date, Days duration);
[[nodiscard]] std::string iso_date(OrdinalDay date);
[[nodiscard]] OrdinalDay parse_iso_date(std::string_view date);

} // namespace pgp::core
