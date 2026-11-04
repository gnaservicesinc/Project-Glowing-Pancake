#include <pgp/core/integer_codec.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

int main() {
    using namespace pgp::core;
    try {
        if (parse_json_i64("-9223372036854775808") != std::numeric_limits<std::int64_t>::min()
            || parse_json_i64("9223372036854775807") != std::numeric_limits<std::int64_t>::max()
            || parse_json_i64("-0") != 0
            || parse_json_u64("18446744073709551615") != std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error("integer boundary parsing failed");
        }
        for (const auto token : {"", "1.0", "1e3", "1E0", "+1", "01", "-01", " 1", "1 ", "true", "null", "\"1\"", "--1"}) {
            bool rejected = false;
            try { (void)parse_json_i64(token); }
            catch (const std::invalid_argument&) { rejected = true; }
            if (!rejected) { throw std::runtime_error("invalid integer token accepted"); }
        }
        for (const auto token : {"9223372036854775808", "-9223372036854775809"}) {
            bool rejected = false;
            try { (void)parse_json_i64(token); }
            catch (const std::out_of_range&) { rejected = true; }
            if (!rejected) { throw std::runtime_error("signed integer overflow accepted"); }
        }
        bool rejected = false;
        try { (void)parse_json_u64("18446744073709551616"); }
        catch (const std::out_of_range&) { rejected = true; }
        if (!rejected) { throw std::runtime_error("unsigned integer overflow accepted"); }
        rejected = false;
        try { (void)parse_json_u64("-1"); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) { throw std::runtime_error("negative unsigned token accepted"); }
        std::cout << "Strict authoritative integer-token checks pass\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
