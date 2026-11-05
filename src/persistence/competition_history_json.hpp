#pragma once
#include "json_support.hpp"
namespace pgp::persistence::detail {
struct CompetitionHistoryBudget {std::size_t cells=0,string_bytes=0;};
// Lossless fixed-column history rows for newly introduced planned companies.
// Catalogs/current state stay named JSON; no ledger/work history is discarded.
[[nodiscard]] Json compact_competition_history(Json simulation,CompetitionHistoryBudget& budget);
[[nodiscard]] Json expand_competition_history(Json simulation,CompetitionHistoryBudget& budget,bool dictionary);
}
