#include "history_blocks.hpp"

#include <array>
#include <cstdint>

namespace pgp::persistence::detail {
namespace {
constexpr auto block_bytes = history_block_bytes;
constexpr std::size_t literal_limit = 128;
constexpr std::size_t match_minimum = 4;
constexpr std::size_t match_limit = 131;
constexpr std::size_t maximum_distance = 65535;
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::string_view path = "history/block";

unsigned byte(std::string_view input, std::size_t index) {
    return static_cast<unsigned char>(input[index]);
}
void literals(std::string& output, std::string_view input) {
    while (!input.empty()) {
        const auto size = std::min(input.size(), literal_limit);
        output.push_back(static_cast<char>(size - 1));
        output.append(input.substr(0, size));
        input.remove_prefix(size);
    }
}
// A byte below 128 starts a literal run. A byte at or above 128 starts a
// backreference with a two-byte little-endian distance. Blocks have no
// cross-page references and the decoder checks every run before copying.
std::string pack(std::string_view input) {
    std::string output;
    std::array<std::size_t, 4096> recent{};
    std::size_t start = 0, at = 0;
    while (at + match_minimum <= input.size()) {
        const std::uint32_t word = byte(input, at) | (byte(input, at + 1) << 8U) |
                                   (byte(input, at + 2) << 16U) | (byte(input, at + 3) << 24U);
        const auto bucket = (word * 2654435761U) >> 20U;
        const auto prior = std::exchange(recent[bucket], at + 1);
        const auto distance = at + 1 - prior;
        if (prior == 0 || distance > maximum_distance ||
            input.substr(prior - 1, match_minimum) != input.substr(at, match_minimum)) {
            ++at;
            continue;
        }
        const auto limit = std::min(match_limit, input.size() - at);
        std::size_t length = match_minimum;
        while (length < limit && input[at + length] == input[at + length - distance]) ++length;
        literals(output, input.substr(start, at - start));
        output.push_back(static_cast<char>(128 + length - match_minimum));
        output.push_back(static_cast<char>(distance & 255));
        output.push_back(static_cast<char>(distance >> 8U));
        at += length;
        start = at;
    }
    literals(output, input.substr(start));
    return output;
}
std::string unpack(std::string_view input) {
    std::string output;
    output.reserve(std::min(block_bytes, input.size() * 4));
    std::size_t at = 0;
    while (at < input.size()) {
        const auto tag = byte(input, at++);
        if (tag < 128) {
            const auto length = static_cast<std::size_t>(tag + 1);
            if (length > input.size() - at || length > block_bytes - output.size())
                invalid(path, "Truncated or oversized history literal");
            output.append(input.substr(at, length));
            at += length;
        } else {
            const auto length = tag - 128 + match_minimum;
            if (input.size() - at < 2 || length > block_bytes - output.size())
                invalid(path, "Truncated or oversized history reference");
            const auto distance = byte(input, at) | (byte(input, at + 1) << 8U);
            at += 2;
            if (distance == 0 || distance > output.size())
                invalid(path, "History reference points outside its block");
            if (distance >= length) output.append(output, output.size() - distance, length);
            else for (std::size_t i = 0; i < length; ++i) output.push_back(output[output.size() - distance]);
        }
    }
    return output;
}
std::string base64(std::string_view bytes) {
    std::string result;
    result.reserve((bytes.size() * 4 + 2) / 3);
    std::uint32_t bits = 0;
    unsigned pending = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bits = (bits << 8U) | byte(bytes, i);
        pending += 8;
        while (pending >= 6) {
            pending -= 6;
            result.push_back(alphabet[(bits >> pending) & 63]);
        }
    }
    if (pending != 0) result.push_back(alphabet[(bits << (6U - pending)) & 63]);
    return result;
}
std::string from_base64(std::string_view input) {
    constexpr auto maximum_packed = block_bytes + (block_bytes + literal_limit - 1) / literal_limit;
    if (input.size() > (maximum_packed * 4 + 2) / 3 || input.size() % 4 == 1)
        invalid(path, "History block exceeds its encoded byte bound or has a truncated digit");
    static constexpr auto digits = [] {
        std::array<unsigned, 256> table{};
        table.fill(64);
        for (std::size_t i = 0; i < alphabet.size(); ++i)
            table[static_cast<unsigned char>(alphabet[i])] = static_cast<unsigned>(i);
        return table;
    }();
    std::string result;
    result.reserve(input.size() * 3 / 4);
    std::uint32_t bits = 0;
    unsigned pending = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto digit = digits[byte(input, i)];
        if (digit == 64) invalid(path, "Invalid history block digit");
        bits = (bits << 6U) | digit;
        pending += 6;
        if (pending >= 8) {
            pending -= 8;
            result.push_back(static_cast<char>((bits >> pending) & 255));
        }
    }
    if ((bits & ((1U << pending) - 1)) != 0) invalid(path, "Noncanonical history block suffix");
    return result;
}
}

void HistoryRows::push(Json row) {
    if (!packed_) {
        rows_.push_back(std::move(row));
        return;
    }
    if (!row.is_array() && !row.is_object()) invalid(path, "Expected a history record");
    push_packed(row.dump());
}
void HistoryRows::repeat() {
    if (!packed_ || previous_.empty()) invalid(path, "No packed history row to repeat");
    push_packed(previous_);
}
void HistoryRows::push_packed(std::string encoded) {
    if (encoded.size() > block_bytes - 2) invalid(path, "History row exceeds its expanded byte bound");
    // Budget the expanded rows and leave room for bounded repeat markers.
    constexpr auto repeat_overhead = 4 * history_block_rows;
    if (page_rows_ != 0 && (page_rows_ == history_block_rows ||
        page_bytes_ + encoded.size() + 1 > block_bytes - repeat_overhead)) flush();
    page_bytes_ += encoded.size() + (page_rows_ == 0 ? 0 : 1);
    if (page_rows_ != 0 && encoded == previous_) ++repeats_;
    else {
        flush_repeats();
        if (page_rows_ != 0) page_ += ',';
        page_ += encoded;
        previous_ = std::move(encoded);
    }
    ++page_rows_;
    ++count_;
}
void HistoryRows::flush_repeats() {
    if (repeats_ == 0) return;
    page_ += ',';
    page_ += std::to_string(repeats_);
    repeats_ = 0;
}
void HistoryRows::flush() {
    if (page_rows_ == 0) return;
    flush_repeats();
    page_ += ']';
    rows_.push_back(base64(pack(page_)));
    page_ = "[";
    previous_.clear();
    page_rows_ = 0;
    page_bytes_ = 2;
}
Json HistoryRows::finish() {
    if (!packed_) return std::move(rows_);
    flush();
    return {{"count", count_}, {"blocks", std::move(rows_)}};
}
DecodedHistoryBlock read_history_block(const Json& encoded) {
    if (!encoded.is_string()) invalid(path, "Expected a packed history string");
    const auto raw = unpack(from_base64(encoded.get_ref<const std::string&>()));
    auto rows = parse(raw, block_bytes, 1'000'000);
    array(rows, path, history_block_rows);
    return {std::move(rows), raw.size()};
}

} // namespace pgp::persistence::detail
