#pragma once

#include "json_support.hpp"
#include <pgp/core/accounts.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json accounts_json(const core::AccountsState& state, bool packed = false);
[[nodiscard]] core::AccountsState parse_accounts(const Json& value, bool packed = false);
}
