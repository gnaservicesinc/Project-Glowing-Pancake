#include <pgp/core/integer_codec.hpp>

#include <charconv>
#include <stdexcept>
#include <system_error>

namespace pgp::core {
namespace {
void validate_integer_token(std::string_view token, bool signed_value) {
    if (token.empty()) {
        throw std::invalid_argument("empty JSON integer token");
    }
    if (token.front() == '-') {
        if (!signed_value) {
            throw std::invalid_argument("negative unsigned JSON integer");
        }
        token.remove_prefix(1);
    }
    if (token.empty() || (token.size() > 1 && token.front() == '0')) {
        throw std::invalid_argument("invalid JSON integer spelling");
    }
    for (const char digit : token) {
        if (digit < '0' || digit > '9') {
            throw std::invalid_argument("authoritative JSON requires integer tokens");
        }
    }
}

template<class Integer>
Integer parse(std::string_view token, bool signed_value) {
    validate_integer_token(token, signed_value);
    Integer value{};
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error == std::errc::result_out_of_range) {
        throw std::out_of_range("JSON integer outside destination range");
    }
    if (error != std::errc{} || end != token.data() + token.size()) {
        throw std::invalid_argument("invalid JSON integer token");
    }
    return value;
}
} // namespace

std::int64_t parse_json_i64(std::string_view token) {
    return parse<std::int64_t>(token, true);
}

std::uint64_t parse_json_u64(std::string_view token) {
    return parse<std::uint64_t>(token, false);
}
} // namespace pgp::core
