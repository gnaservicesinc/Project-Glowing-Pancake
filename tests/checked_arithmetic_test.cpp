#include <pgp/core/checked_arithmetic.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace pgp::core;
constexpr auto umax = std::numeric_limits<std::uint64_t>::max();
constexpr auto imax = std::numeric_limits<std::int64_t>::max();
constexpr auto imin = std::numeric_limits<std::int64_t>::min();
int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << description << '\n';
        ++failures;
    }
}
template<class Exception, class Function>
void rejects(Function function, const char* description) {
    try {
        function();
        check(false, description);
    } catch (const Exception&) {
    } catch (...) {
        check(false, "Unexpected exception type");
    }
}

void signed_boundaries() {
    check(checked_add(imax, 0) == imax, "Signed maximum plus zero");
    check(checked_add(imin, 0) == imin, "Signed minimum plus zero");
    check(checked_add(imax, imin) == -1, "Opposite signed extremes sum");
    check(checked_subtract(imin, imin) == 0, "Signed minimum subtraction");
    check(checked_subtract(-1, imax) == imin, "Subtraction reaches signed minimum");
    check(checked_subtract(-1, imin) == imax, "Subtraction reaches signed maximum");
    rejects<std::overflow_error>([] { (void)checked_add(imax, 1); }, "Reject addition overflow");
    rejects<std::overflow_error>([] { (void)checked_add(imin, -1); }, "Reject addition underflow");
    rejects<std::overflow_error>([] { (void)checked_subtract(imax, -1); }, "Reject subtraction overflow");
    rejects<std::overflow_error>([] { (void)checked_subtract(imin, 1); }, "Reject subtraction underflow");
    rejects<std::overflow_error>([] { (void)checked_subtract(0, imin); }, "Reject negating signed minimum");

    const std::array<std::uint64_t, 0> none{};
    const std::uint64_t two[]{2};
    check(signed_mul_div_round_half_up(imin, none, 1) == imin, "Preserve most-negative posting");
    check(signed_mul_div_round_half_up(imin, two, 2) == imin, "Wide signed minimum product divides safely");
    check(signed_mul_div_round_half_up(-1, none, 2) == -1, "Negative half tie rounds magnitude up");
    check(signed_mul_div_round_half_up(1, none, 2) == 1, "Positive half tie rounds magnitude up");
    check(signed_mul_div_round_half_up(-1, none, 3) == 0, "Small negative magnitude rounds to zero");
    check(signed_mul_div_round_half_up(imin, none, umax) == -1, "Unsigned denominator above signed maximum");
    rejects<std::overflow_error>([&] { (void)signed_mul_div_round_half_up(imax, two, 1); }, "Positive final narrowing rejects");
    rejects<std::overflow_error>([&] { (void)signed_mul_div_round_half_up(imin, two, 1); }, "Negative final narrowing rejects");
    rejects<std::invalid_argument>([&] { (void)signed_mul_div_round_half_up(0, none, 0); }, "Zero posting still validates denominator");
    rejects<std::invalid_argument>([&] { (void)signed_mul_div_round_half_up(1, none, -1); }, "Signed negative denominator rejects");
}

void exact_rounding() {
    check(mul_div_round_half_up(1, 1, 2) == 1, "Exact even-denominator half tie");
    check(mul_div_round_half_up(1, 1, 3) == 0, "Odd denominator rounds below half");
    check(mul_div_round_half_up(1, 2, 3) == 1, "Odd denominator rounds above half");
    check(mul_div_round_half_up(umax, umax, umax) == umax, "Full 128-bit product with representable quotient");
    const auto exact = mul_div_floor(umax, umax - 1, umax);
    check(exact.quotient == umax - 1 && exact.remainder == 0, "Exact wide quotient/remainder");
    const std::uint64_t small[]{1, 5000, 5000, 5000};
    check(mul_div_round_half_up(small, 1000000000000ULL) == 0,
        "Multifactor expression rounds only once (staged half-up would yield one)");
    const std::uint64_t training[]{9001, 5001, 5001};
    check(mul_div_round_half_up(training, 100000000ULL) == 2251, "Multifactor basis-point expression");
    const std::array<std::uint64_t, 0> empty{};
    check(mul_div_round_half_up(empty, 1) == 1, "Empty product is identity");
    const std::uint64_t zero_late[]{umax, umax, umax, 0};
    check(mul_div_round_half_up(zero_late, 1) == 0, "Zero product avoids meaningless intermediate overflow");
    const std::uint64_t overflowing[]{std::uint64_t{1} << 63, std::uint64_t{1} << 63, 4};
    check(signed_mul_div_round_half_up(0, overflowing, 1) == 0, "Zero signed amount avoids meaningless intermediate overflow");
    rejects<std::overflow_error>([&] { (void)mul_div_round_half_up(overflowing, umax); }, "Reject intermediate above 128 bits");
    rejects<std::overflow_error>([] { (void)mul_div_floor(umax, umax, umax - 1); }, "Reject floor quotient above 64 bits");
    const std::uint64_t round_overflow[]{8191, 4504149450301441ULL}; // 2^65-1
    const auto floor_max = mul_div_floor(round_overflow, 2);
    check(floor_max.quotient == umax && floor_max.remainder == 1, "Floor can reach unsigned maximum with residue");
    rejects<std::overflow_error>([&] { (void)mul_div_round_half_up(round_overflow, 2); }, "Reject rounding past unsigned maximum");
    rejects<std::invalid_argument>([] { (void)mul_div_round_half_up(0, 0, 0); }, "Zero factors still validate denominator");
    rejects<std::invalid_argument>([] { (void)mul_div_floor(1, 1, -1); }, "Negative literal denominator rejects");
    rejects<std::invalid_argument>([] { (void)mul_div_round_half_up(1, 1, -2); }, "Negative rounding denominator rejects");

    // Independent Python arbitrary-precision oracle vectors, generated with
    // random.Random(143765); high divisors exercise a 65-bit shifted remainder.
    struct Oracle { std::uint64_t a, b, divisor, quotient, remainder, rounded; };
    constexpr Oracle vectors[]{
        {16777398618902902180ULL, 12897591843467717429ULL, 16875958091739381984ULL, 12822266943628431255ULL, 5524194083138585300ULL, 12822266943628431255ULL},
        {16741951150597777243ULL, 15685416028724411092ULL, 17738796681790929010ULL, 14803961826749717690ULL, 14973110449043192456ULL, 14803961826749717691ULL},
        {13690293095089402232ULL, 17645031618704421896ULL, 18356244705900495076ULL, 13159862401187843276ULL, 646313384674362896ULL, 13159862401187843276ULL},
        {15777487637071865701ULL, 16410127421450199520ULL, 17444414409356055921ULL, 14842033469225636013ULL, 14860704960675180547ULL, 14842033469225636014ULL},
        {14485724964070724759ULL, 10369821613805515857ULL, 18252616045783134150ULL, 8229745448393764013ULL, 17080532499235659513ULL, 8229745448393764014ULL},
        {14041779801553680871ULL, 15879553811886803672ULL, 16060478091249098466ULL, 13883596534709067278ULL, 8876134169278362764ULL, 13883596534709067279ULL}};
    for (const auto& vector : vectors) {
        const auto result = mul_div_floor(vector.a, vector.b, vector.divisor);
        check(result.quotient == vector.quotient && result.remainder == vector.remainder,
            "Wide quotient/remainder differs from arbitrary-precision oracle");
        check(mul_div_round_half_up(vector.a, vector.b, vector.divisor) == vector.rounded,
            "Wide rounding differs from arbitrary-precision oracle");
    }

    // Exhaustive tiny formulas use an independent native-width oracle whose
    // bounded products cannot overflow; this catches bit/division boundaries.
    for (std::uint64_t a = 0; a < 30; ++a) {
        for (std::uint64_t b = 0; b < 30; ++b) {
            for (std::uint64_t divisor = 1; divisor < 30; ++divisor) {
                const auto product = a * b;
                const auto result = mul_div_floor(a, b, divisor);
                check(result.quotient == product / divisor && result.remainder == product % divisor,
                    "Small quotient/remainder mismatch");
                check(mul_div_round_half_up(a, b, divisor) == (product + divisor / 2) / divisor,
                    "Small half-up mismatch");
            }
        }
    }
}
} // namespace

int main() {
    signed_boundaries();
    exact_rounding();
    return failures == 0 ? 0 : 1;
}
