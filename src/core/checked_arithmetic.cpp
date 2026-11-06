#include <pgp/core/checked_arithmetic.hpp>

#include <array>
#include <limits>

namespace pgp::core {
namespace {
struct Wide {
    std::uint64_t high{};
    std::uint64_t low{1};
};

Wide multiply(Wide value, std::uint64_t factor) {
    // Base-2^32 schoolbook multiplication. Each accumulation is bounded by
    // (2^32-1)^2 + 2*(2^32-1), exactly UINT64_MAX.
    constexpr std::uint64_t mask = 0xffffffffULL;
    if (value.high == 0 && (value.low | factor) <= mask)
        return {0, value.low * factor};
    const std::array<std::uint32_t, 4> left{
        static_cast<std::uint32_t>(value.low & mask),
        static_cast<std::uint32_t>(value.low >> 32),
        static_cast<std::uint32_t>(value.high & mask),
        static_cast<std::uint32_t>(value.high >> 32)};
    const std::array<std::uint32_t, 2> right{
        static_cast<std::uint32_t>(factor & mask),
        static_cast<std::uint32_t>(factor >> 32)};
    std::array<std::uint32_t, 6> product{};
    for (std::size_t i = 0; i < left.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < right.size(); ++j) {
            const auto sum = static_cast<std::uint64_t>(left[i]) * right[j]
                + product[i + j] + carry;
            product[i + j] = static_cast<std::uint32_t>(sum & mask);
            carry = sum >> 32;
        }
        product[i + right.size()] = static_cast<std::uint32_t>(carry);
    }
    if (product[4] != 0 || product[5] != 0) {
        throw std::overflow_error("arithmetic product exceeds 128 bits");
    }
    return {
        static_cast<std::uint64_t>(product[2])
            | (static_cast<std::uint64_t>(product[3]) << 32),
        static_cast<std::uint64_t>(product[0])
            | (static_cast<std::uint64_t>(product[1]) << 32)};
}

Wide exact_product(std::span<const std::uint64_t> factors) {
    for (const auto factor : factors) {
        if (factor == 0) {
            return {0, 0};
        }
    }
    Wide product;
    for (const auto factor : factors) {
        product = multiply(product, factor);
    }
    return product;
}

QuotientRemainder divide(Wide numerator, std::uint64_t denominator) {
    if (numerator.high == 0)
        return {numerator.low / denominator, numerator.low % denominator};
    if (numerator.high >= denominator) {
        throw std::overflow_error("arithmetic quotient exceeds 64 bits");
    }
    std::uint64_t quotient = 0;
    auto remainder = numerator.high;
    for (int bit = 63; bit >= 0; --bit) {
        const bool carry = (remainder >> 63) != 0;
        remainder = (remainder << 1)
            | ((numerator.low >> static_cast<unsigned>(bit)) & 1U);
        if (carry || remainder >= denominator) {
            // In the carry case the true remainder has a 65th bit. Unsigned
            // subtraction wraps to the exact nonnegative value after division.
            remainder -= denominator;
            quotient |= std::uint64_t{1} << static_cast<unsigned>(bit);
        }
    }
    return {quotient, remainder};
}

std::uint64_t rounded(QuotientRemainder result, std::uint64_t denominator) {
    const auto half_up_threshold = denominator / 2 + denominator % 2;
    if (result.remainder >= half_up_threshold) {
        if (result.quotient == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("rounded arithmetic result exceeds 64 bits");
        }
        ++result.quotient;
    }
    return result.quotient;
}
} // namespace

std::int64_t checked_add(std::int64_t left, std::int64_t right) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if ((right > 0 && left > maximum - right)
        || (right < 0 && left < minimum - right)) {
        throw std::overflow_error("signed arithmetic addition overflow");
    }
    return left + right;
}

std::int64_t checked_subtract(std::int64_t left, std::int64_t right) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if ((right > 0 && left < minimum + right)
        || (right < 0 && left > maximum + right)) {
        throw std::overflow_error("signed arithmetic subtraction overflow");
    }
    return left - right;
}

QuotientRemainder mul_div_floor(
    std::span<const std::uint64_t> factors, std::uint64_t denominator) {
    if (denominator == 0) {
        throw std::invalid_argument("arithmetic denominator must be positive");
    }
    return divide(exact_product(factors), denominator);
}

std::uint64_t mul_div_round_half_up(
    std::span<const std::uint64_t> factors, std::uint64_t denominator) {
    const auto result = mul_div_floor(factors, denominator);
    return rounded(result, denominator);
}

std::int64_t signed_mul_div_round_half_up(
    std::int64_t amount, std::span<const std::uint64_t> factors,
    std::uint64_t denominator) {
    if (denominator == 0) {
        throw std::invalid_argument("arithmetic denominator must be positive");
    }
    if (amount == 0) {
        return 0;
    }
    const bool negative = amount < 0;
    const auto unsigned_amount = static_cast<std::uint64_t>(amount);
    const auto magnitude = negative ? std::uint64_t{0} - unsigned_amount : unsigned_amount;
    auto product = exact_product(factors);
    product = multiply(product, magnitude);
    const auto result = rounded(divide(product, denominator), denominator);
    constexpr auto positive_limit = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    const auto limit = negative ? positive_limit + 1 : positive_limit;
    if (result > limit) {
        throw std::overflow_error("signed rounded arithmetic result exceeds 64 bits");
    }
    if (!negative) {
        return static_cast<std::int64_t>(result);
    }
    if (result == positive_limit + 1) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(result);
}
} // namespace pgp::core
