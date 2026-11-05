#include <pgp/core/calendar.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::int64_t days_before_year(int year) {
    const auto preceding = static_cast<std::int64_t>(year) - 1;
    return preceding * 365 + preceding / 4 - preceding / 100 + preceding / 400;
}
constexpr auto epoch_days = days_before_year(2014);
constexpr auto minimum_ordinal = -epoch_days;
constexpr auto maximum_ordinal = days_before_year(10'000) - 1 - epoch_days;
constexpr std::array<unsigned, 12> month_lengths{31, 28, 31, 30, 31, 30,
                                               31, 31, 30, 31, 30, 31};

void check_year(int year) {
    if (year < 1 || year > 9999) {
        throw std::out_of_range("Gregorian year must be in 1..9999");
    }
}
} // namespace

bool is_leap_year(int year) {
    check_year(year);
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned days_in_month(int year, unsigned month) {
    check_year(year);
    if (month < 1 || month > 12) {
        throw std::out_of_range("Gregorian month must be in 1..12");
    }
    return month_lengths[month - 1] + ((month == 2 && is_leap_year(year)) ? 1U : 0U);
}

OrdinalDay to_ordinal(CivilDate date) {
    const auto last_day = days_in_month(date.year, date.month);
    if (date.day < 1 || date.day > last_day) {
        throw std::out_of_range("Invalid Gregorian day of month");
    }
    auto absolute = days_before_year(date.year);
    for (unsigned month = 1; month < date.month; ++month) {
        absolute += days_in_month(date.year, month);
    }
    absolute += static_cast<std::int64_t>(date.day) - 1;
    return OrdinalDay{absolute - epoch_days};
}

CivilDate from_ordinal(OrdinalDay date) {
    if (date.value() < minimum_ordinal || date.value() > maximum_ordinal) {
        throw std::out_of_range("Ordinal is outside Gregorian years 1..9999");
    }
    const auto absolute = date.value() + epoch_days;
    int lower = static_cast<int>((absolute * 400) / 146097) + 1;
    if (lower < 1) lower = 1;
    if (lower > 9999) lower = 9999;
    while (lower > 1 && days_before_year(lower) > absolute) {
        --lower;
    }
    while (lower < 9999 && days_before_year(lower + 1) <= absolute) {
        ++lower;
    }
    auto remaining = absolute - days_before_year(lower);
    unsigned month = 1;
    while (remaining >= static_cast<std::int64_t>(days_in_month(lower, month))) {
        remaining -= days_in_month(lower, month);
        ++month;
    }
    return CivilDate{lower, month, static_cast<unsigned>(remaining + 1)};
}

OrdinalDay advance_days(OrdinalDay date, Days duration) {
    // Validate the origin even for zero duration; never turn an invalid saved
    // ordinal into an apparently accepted date without checking its range.
    (void)from_ordinal(date);
    const auto result = OrdinalDay{checked_add(date.value(), duration.value())};
    (void)from_ordinal(result);
    return result;
}

std::string iso_date(OrdinalDay date) {
    const auto civil = from_ordinal(date);
    // Fixed ASCII construction avoids locale-sensitive formatting in hashes.
    std::string result = "0000-00-00";
    auto year = civil.year;
    for (int position = 3; position >= 0; --position) {
        result[static_cast<std::size_t>(position)] = static_cast<char>('0' + year % 10);
        year /= 10;
    }
    result[5] = static_cast<char>('0' + civil.month / 10);
    result[6] = static_cast<char>('0' + civil.month % 10);
    result[8] = static_cast<char>('0' + civil.day / 10);
    result[9] = static_cast<char>('0' + civil.day % 10);
    return result;
}

OrdinalDay parse_iso_date(std::string_view date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-')
        throw std::invalid_argument("Expected YYYY-MM-DD");
    const auto digits = [](std::string_view part) {
        unsigned value = 0;
        for (const auto digit : part) {
            if (digit < '0' || digit > '9') throw std::invalid_argument("Non-numeric date");
            value = value * 10 + static_cast<unsigned>(digit - '0');
        }
        return value;
    };
    return to_ordinal({static_cast<int>(digits(date.substr(0, 4))), digits(date.substr(5, 2)), digits(date.substr(8, 2))});
}

} // namespace pgp::core
