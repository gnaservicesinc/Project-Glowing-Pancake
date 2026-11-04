#include <pgp/core/calendar.hpp>
#include <pgp/core/random.hpp>
#include <pgp/core/units.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace pgp::core;

namespace {
struct CompanyTag;
struct ProjectTag;
using CompanyId = EntityId<CompanyTag>;
using ProjectId = EntityId<ProjectTag>;

static_assert(!std::is_convertible_v<std::int64_t, Cents>);
static_assert(!std::is_convertible_v<Cents, std::int64_t>);
static_assert(!std::is_constructible_v<Cents, double>);
static_assert(!std::is_constructible_v<Cents, bool>);
static_assert(!std::is_constructible_v<Cents, MilliCU>);
static_assert(!std::is_convertible_v<MilliCUPerDay, MilliCU>);
static_assert(!std::is_constructible_v<CompanyId, ProjectId>);
static_assert(!std::is_constructible_v<CompanyId, double>);
static_assert(!std::is_constructible_v<CompanyId, bool>);
static_assert(std::totally_ordered<CompanyId>);
static_assert(std::totally_ordered<OrdinalDay>);
static_assert(Cents{-4}.value() == -4);

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

template <typename Exception, typename Operation>
void rejects(Operation operation, std::string_view message) {
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error{std::string{message}};
}

void units_and_ids() {
    require(checked_unit_add(Cents{25}, Cents{-40}) == Cents{-15}, "Signed cents addition");
    require(checked_unit_subtract(Cents{-25}, Cents{40}) == Cents{-65}, "Signed cents subtraction");
    require(integrate(MilliCUPerDay{60'000}, Days{60}) == MilliCU{3'600'000},
            "Capacity integrates to total work, not a rate");
    require(integrate(MilliCUPerDay{0}, Days{std::numeric_limits<std::int64_t>::max()}) == MilliCU{0},
            "Zero-rate integration");
    require(BasisPoints{10'000}.value() == 10'000 && PartsPerMillion{1'000'000}.value() == 1'000'000,
            "Inclusive probability/rate limits");
    require(BasisPointDelta{-10'000}.value() == -10'000 &&
                PartsPerMillionDelta{-1'000'000}.value() == -1'000'000,
            "Signed deltas are separate types");
    rejects<std::out_of_range>([] { (void)MilliCU{-1}; }, "Negative work accepted");
    rejects<std::out_of_range>([] { (void)MilliCUPerDay{-1}; }, "Negative capacity accepted");
    rejects<std::out_of_range>([] { (void)Days{-1}; }, "Negative duration accepted");
    rejects<std::out_of_range>([] { (void)BasisPoints{10'001}; }, "Oversized basis points accepted");
    rejects<std::out_of_range>([] { (void)PartsPerMillion{-1}; }, "Negative probability accepted");
    rejects<std::out_of_range>([] { (void)PartsPerMillion{1'000'001}; }, "Oversized probability accepted");
    rejects<std::out_of_range>([] { (void)Cents{std::numeric_limits<std::uint64_t>::max()}; },
                               "Unsigned-to-signed narrowing accepted");
    rejects<std::overflow_error>([] {
        (void)checked_unit_add(Cents{std::numeric_limits<std::int64_t>::max()}, Cents{1});
    }, "Cents overflow accepted");
    rejects<std::out_of_range>([] { (void)checked_unit_subtract(MilliCU{0}, MilliCU{1}); },
                               "Semantic work underflow accepted");
    rejects<std::out_of_range>([] {
        (void)integrate(MilliCUPerDay{std::numeric_limits<std::int64_t>::max()}, Days{2});
    }, "Integrated work narrowing accepted");
    rejects<std::out_of_range>([] { (void)CompanyId{0}; }, "Zero ID accepted");
    rejects<std::out_of_range>([] { (void)CompanyId{-1}; }, "Negative ID accepted");
    require(CompanyId{std::numeric_limits<std::uint64_t>::max()}.value() ==
                std::numeric_limits<std::uint64_t>::max(), "Full ID range not preserved");

    struct RankedCompany {
        std::int64_t score;
        CompanyId id;
    };
    const auto rank = [](const RankedCompany& left, const RankedCompany& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.id < right.id;
    };
    std::vector<RankedCompany> companies{{10, CompanyId{9}}, {20, CompanyId{7}}, {10, CompanyId{3}}};
    std::sort(companies.begin(), companies.end(), rank);
    require(companies[0].id == CompanyId{7} && companies[1].id == CompanyId{3} &&
                companies[2].id == CompanyId{9}, "Total sort key must end in unique entity ID");
}

void calendar() {
    require(to_ordinal({2014, 1, 1}) == OrdinalDay{0}, "Campaign epoch");
    require(from_ordinal(OrdinalDay{-1}) == CivilDate{2013, 12, 31}, "Pre-epoch date");
    require(is_leap_year(2000) && !is_leap_year(1900) && !is_leap_year(2100) && is_leap_year(2400),
            "Gregorian century leap rule");
    require(advance_days(to_ordinal({2000, 2, 28}), Days{1}) == to_ordinal({2000, 2, 29}),
            "Leap-day stepping");
    require(advance_days(to_ordinal({2100, 2, 28}), Days{1}) == to_ordinal({2100, 3, 1}),
            "Nonleap-century stepping");
    require(iso_date(to_ordinal({1, 2, 3})) == "0001-02-03" &&
                iso_date(to_ordinal({9999, 12, 31})) == "9999-12-31", "Fixed ASCII ISO date");
    require(to_ordinal({2114, 1, 1}).value() == 36'524, "Exact 100-year campaign interval");
    auto cursor = to_ordinal({2014, 1, 1});
    for (std::int64_t day = 0; day < 36'524; ++day) {
        require(to_ordinal(from_ordinal(cursor)) == cursor, "Campaign round trip");
        cursor = advance_days(cursor, Days{1});
    }
    require(from_ordinal(cursor) == CivilDate{2114, 1, 1}, "Campaign stepping terminates at boundary");
    // Check every supported year's endpoints independently of the campaign.
    for (int year = 1; year <= 9999; ++year) {
        for (const auto date : {CivilDate{year, 1, 1}, CivilDate{year, 12, 31}}) {
            require(from_ordinal(to_ordinal(date)) == date, "Calendar range round trip");
        }
    }
    rejects<std::out_of_range>([] { (void)to_ordinal({0, 1, 1}); }, "Year zero accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({10'000, 1, 1}); }, "Year 10000 accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({2014, 0, 1}); }, "Month zero accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({2014, 13, 1}); }, "Month 13 accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({2014, 1, 0}); }, "Day zero accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({2100, 2, 29}); }, "Invalid leap day accepted");
    rejects<std::out_of_range>([] { (void)to_ordinal({2014, 4, 31}); }, "Invalid month end accepted");
    rejects<std::out_of_range>([] { (void)advance_days(to_ordinal({9999, 12, 31}), Days{1}); },
                               "Date above supported range accepted");
    rejects<std::out_of_range>([] {
        (void)from_ordinal(OrdinalDay{to_ordinal({1, 1, 1}).value() - 1});
    }, "Date below supported range accepted");
    rejects<std::out_of_range>([] {
        (void)advance_days(OrdinalDay{std::numeric_limits<std::int64_t>::max()}, Days{0});
    }, "Invalid origin accepted for zero-day step");
    rejects<std::overflow_error>([] {
        (void)advance_days(OrdinalDay{1}, Days{std::numeric_limits<std::int64_t>::max()});
    }, "Date addition wraps");
}

void random_streams() {
    constexpr std::array<std::uint64_t, 5> expected{
        0xe220a8397b1dcdafULL, 0x6e789e6aa1b965f4ULL, 0x06c45d188009454fULL,
        0xf88bb8a8724c81ecULL, 0x1b39896a51a8749bULL
    };
    SplitMix64 generator{0};
    for (const auto value : expected) {
        require(generator.next() == value, "SplitMix64 known vector");
    }
    require(generator.state() == 0x1715609f7c746c69ULL, "Saved pre-next-draw accumulator");
    auto restored = SplitMix64{generator.state()};
    const auto original_next = generator.next();
    const auto restored_next = restored.next();
    require(original_next == restored_next, "Single-stream reload continuation");
    SplitMix64 rejection{3};
    require(rejection.bounded(0x8000000000000001ULL) == 3'694'763'184'872'335'752ULL,
            "Uniform bounded draw with rejected short prefix");
    require(rejection.state() == 0x3c6ef372fe94f82dULL, "Bounded draw must consume both rejected/accepted samples");
    require(rejection.bounded(1) == 0, "Unit bound");
    rejects<std::invalid_argument>([&] { (void)rejection.bounded(0); }, "Zero random bound accepted");

    RandomStreams streams{0};
    const auto initial = streams.snapshot();
    require(initial[0] == expected[0] && initial[1] == expected[1] &&
                initial[6] == 0x2c829abe1f4532e1ULL, "Fixed domain seed derivation");
    (void)streams.stream(RandomStream::research).next();
    const auto research_drawn = streams.snapshot();
    for (std::size_t index = 0; index < stream_count; ++index) {
        require((research_drawn[index] != initial[index]) == (index == 0), "Research draw changed another stream");
    }
    auto preview = streams;
    (void)preview.stream(RandomStream::markets).next();
    require(streams.snapshot() == research_drawn, "Preview copy consumed authoritative randomness");
    auto reloaded = RandomStreams::restore(streams.snapshot());
    for (std::size_t index = 0; index < stream_count; ++index) {
        const auto domain = static_cast<RandomStream>(index);
        const auto next_original = streams.stream(domain).next();
        const auto next_reloaded = reloaded.stream(domain).next();
        require(next_original == next_reloaded, "Every named stream survives reload");
    }
    require(streams.snapshot() == reloaded.snapshot(), "All-stream snapshot continuation");
    rejects<std::out_of_range>([&] { (void)streams.stream(RandomStream::count); }, "Invalid stream domain accepted");
    rejects<std::out_of_range>([&] {
        (void)static_cast<const RandomStreams&>(streams).stream(static_cast<RandomStream>(99));
    }, "Invalid const stream domain accepted");
}
} // namespace

int main() {
    try {
        units_and_ids();
        calendar();
        random_streams();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
