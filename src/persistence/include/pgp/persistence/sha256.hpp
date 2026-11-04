#pragma once
#include <string>
#include <string_view>

namespace pgp::persistence {
// Integrity/content identity, not authentication or encryption.
[[nodiscard]] std::string sha256(std::string_view bytes);
}
