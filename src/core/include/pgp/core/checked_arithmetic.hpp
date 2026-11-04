#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace pgp::core {

// Throws overflow_error rather than wrapping. Money uses signed constant-2014
// cents; callers must retain their unit types around these scalar operations.
[[nodiscard]] std::int64_t checked_add(std::int64_t left, std::int64_t right);
[[nodiscard]] std::int64_t checked_subtract(std::int64_t left, std::int64_t right);

struct QuotientRemainder {
    std::uint64_t quotient;
    std::uint64_t remainder;
};

// Products are exact up to 2^128-1 on every compiler, using the same portable
// multiword implementation. A larger intermediate is an overflow even when a
// later division could fit. An empty factor list denotes the product 1. Any
// zero factor makes the product zero without overflowing earlier factors.
// A zero denominator is invalid; quotient overflow is reported separately.
[[nodiscard]] QuotientRemainder mul_div_floor(
    std::span<const std::uint64_t> factors, std::uint64_t denominator);
[[nodiscard]] std::uint64_t mul_div_round_half_up(
    std::span<const std::uint64_t> factors, std::uint64_t denominator);

// Round the unsigned magnitude once, then restore the sign. Negative half ties
// go away from zero. INT64_MIN is accepted without negating a signed minimum.
[[nodiscard]] std::int64_t signed_mul_div_round_half_up(
    std::int64_t amount, std::span<const std::uint64_t> factors,
    std::uint64_t denominator);

[[nodiscard]] inline QuotientRemainder mul_div_floor(
    std::uint64_t left, std::uint64_t right, std::uint64_t denominator) {
    const std::uint64_t factors[]{left, right};
    return mul_div_floor(factors, denominator);
}
[[nodiscard]] inline std::uint64_t mul_div_round_half_up(
    std::uint64_t left, std::uint64_t right, std::uint64_t denominator) {
    const std::uint64_t factors[]{left, right};
    return mul_div_round_half_up(factors, denominator);
}

// Prevent signed negative denominators from being silently converted to a
// large positive unsigned number at an ordinary integer-literal call site.
template<std::signed_integral Integer>
[[nodiscard]] inline std::uint64_t positive_denominator(Integer denominator) {
    if (denominator <= 0) {
        throw std::invalid_argument("arithmetic denominator must be positive");
    }
    return static_cast<std::uint64_t>(denominator);
}
template<std::signed_integral Integer>
[[nodiscard]] inline QuotientRemainder mul_div_floor(
    std::span<const std::uint64_t> factors, Integer denominator) {
    return mul_div_floor(factors, positive_denominator(denominator));
}
template<std::signed_integral Integer>
[[nodiscard]] inline std::uint64_t mul_div_round_half_up(
    std::span<const std::uint64_t> factors, Integer denominator) {
    return mul_div_round_half_up(factors, positive_denominator(denominator));
}
template<std::signed_integral Integer>
[[nodiscard]] inline std::int64_t signed_mul_div_round_half_up(
    std::int64_t amount, std::span<const std::uint64_t> factors, Integer denominator) {
    return signed_mul_div_round_half_up(amount, factors, positive_denominator(denominator));
}
template<std::signed_integral Integer>
[[nodiscard]] inline QuotientRemainder mul_div_floor(
    std::uint64_t left, std::uint64_t right, Integer denominator) {
    return mul_div_floor(left, right, positive_denominator(denominator));
}
template<std::signed_integral Integer>
[[nodiscard]] inline std::uint64_t mul_div_round_half_up(
    std::uint64_t left, std::uint64_t right, Integer denominator) {
    return mul_div_round_half_up(left, right, positive_denominator(denominator));
}

} // namespace pgp::core
