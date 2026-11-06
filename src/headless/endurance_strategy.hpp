#pragma once

#include <pgp/runtime/runtime.hpp>
#include <iosfwd>
#include <string_view>

enum class EnduranceStrategy { specialist, open, frontier, infrastructure };

[[nodiscard]] EnduranceStrategy parse_endurance_strategy(std::string_view name);
[[nodiscard]] pgp::runtime::Checkpoint begin_endurance_strategy(
    const pgp::runtime::CompiledProgram&, std::string_view scenario,
    pgp::core::OrdinalDay through, EnduranceStrategy);
void review_endurance_strategy(pgp::runtime::Runtime&, EnduranceStrategy, std::ostream&);
