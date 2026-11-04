#pragma once

#include <cstdint>
#include <string_view>

namespace pgp::core {

// Parse one complete JSON integer token. Fractional/exponent notation is refused
// even when mathematically integral; a future document reader must retain tokens
// rather than first converting them through a binary floating-point value.
std::int64_t parse_json_i64(std::string_view token);
std::uint64_t parse_json_u64(std::string_view token);

} // namespace pgp::core
