#pragma once

#include <pgp/core/civic.hpp>
#include <pgp/core/random.hpp>

namespace pgp::core::civic_detail {
constexpr std::size_t politics_stream = static_cast<std::size_t>(RandomStream::politics);
[[nodiscard]] bool commenting(const CivicDocket&, OrdinalDay);
[[nodiscard]] bool has_evidence(const SimulationState&, const CivicPolicy&, std::string_view, OrdinalDay);
[[nodiscard]] std::string advocacy_key(std::uint64_t docket, std::size_t number);
[[nodiscard]] bool applies(const CivicDocket&, OrdinalDay);
[[nodiscard]] std::optional<OrdinalDay> boundary(const CivicState&, OrdinalDay from);
void calendar_day(CivicState&, OrdinalDay, BasisPoints adoption);
} // namespace pgp::core::civic_detail
