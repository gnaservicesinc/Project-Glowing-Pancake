#pragma once

#include <pgp/core/checked_arithmetic.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pgp::core {
namespace detail {

template <typename Tag, std::int64_t Minimum, std::int64_t Maximum>
class Quantity {
public:
    template <std::integral Integer>
        requires (!std::same_as<Integer, bool>)
    explicit constexpr Quantity(Integer value) {
        if (!std::in_range<std::int64_t>(value)) {
            throw std::out_of_range("Unit does not fit its signed 64-bit storage");
        }
        const auto narrowed = static_cast<std::int64_t>(value);
        if (narrowed < Minimum || narrowed > Maximum) {
            throw std::out_of_range("Unit is outside its permitted range");
        }
        value_ = narrowed;
    }

    template <std::floating_point Float>
    Quantity(Float) = delete;
    Quantity(bool) = delete;

    [[nodiscard]] constexpr std::int64_t value() const noexcept { return value_; }
    auto operator<=>(const Quantity&) const = default;

private:
    std::int64_t value_;
};

struct CentsTag;
struct MilliCUTag;
struct MilliCUPerDayTag;
struct BasisPointsTag;
struct BasisPointDeltaTag;
struct PartsPerMillionTag;
struct PartsPerMillionDeltaTag;
struct OrdinalDayTag;
struct DaysTag;
inline constexpr auto signed_min = std::numeric_limits<std::int64_t>::min();
inline constexpr auto signed_max = std::numeric_limits<std::int64_t>::max();
} // namespace detail

// Every Cents value is constant-2014 purchasing power. No nominal index is stored.
using Cents = detail::Quantity<detail::CentsTag, detail::signed_min, detail::signed_max>;
using MilliCU = detail::Quantity<detail::MilliCUTag, 0, detail::signed_max>;
using MilliCUPerDay = detail::Quantity<detail::MilliCUPerDayTag, 0, detail::signed_max>;
using BasisPoints = detail::Quantity<detail::BasisPointsTag, 0, 10'000>;
using BasisPointDelta = detail::Quantity<detail::BasisPointDeltaTag, -10'000, 10'000>;
using PartsPerMillion = detail::Quantity<detail::PartsPerMillionTag, 0, 1'000'000>;
using PartsPerMillionDelta = detail::Quantity<detail::PartsPerMillionDeltaTag, -1'000'000, 1'000'000>;
using OrdinalDay = detail::Quantity<detail::OrdinalDayTag, detail::signed_min, detail::signed_max>;
using Days = detail::Quantity<detail::DaysTag, 0, detail::signed_max>;

// Same-unit operations preserve both storage and semantic range checks. There
// are deliberately no arithmetic operators that could hide a unit conversion.
template <typename Tag, std::int64_t Minimum, std::int64_t Maximum>
[[nodiscard]] auto checked_unit_add(detail::Quantity<Tag, Minimum, Maximum> left,
                                  detail::Quantity<Tag, Minimum, Maximum> right) {
    return detail::Quantity<Tag, Minimum, Maximum>{checked_add(left.value(), right.value())};
}

template <typename Tag, std::int64_t Minimum, std::int64_t Maximum>
[[nodiscard]] auto checked_unit_subtract(detail::Quantity<Tag, Minimum, Maximum> left,
                                       detail::Quantity<Tag, Minimum, Maximum> right) {
    return detail::Quantity<Tag, Minimum, Maximum>{checked_subtract(left.value(), right.value())};
}

[[nodiscard]] inline MilliCU integrate(MilliCUPerDay rate, Days duration) {
    const std::array factors{static_cast<std::uint64_t>(rate.value()),
                             static_cast<std::uint64_t>(duration.value())};
    return MilliCU{mul_div_floor(factors, 1).quotient};
}

// Distinct entity domains instantiate different Tag types. Zero is reserved
// for absent references; callers represent absence separately, e.g. optional.
template <typename Tag>
class EntityId {
public:
    template <std::integral Integer>
        requires (!std::same_as<Integer, bool>)
    explicit constexpr EntityId(Integer value) {
        if (!std::in_range<std::uint64_t>(value) || value == 0) {
            throw std::out_of_range("Entity ID must be a nonzero unsigned 64-bit integer");
        }
        value_ = static_cast<std::uint64_t>(value);
    }

    template <std::floating_point Float>
    EntityId(Float) = delete;
    EntityId(bool) = delete;

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    auto operator<=>(const EntityId&) const = default;

private:
    std::uint64_t value_;
};

} // namespace pgp::core
