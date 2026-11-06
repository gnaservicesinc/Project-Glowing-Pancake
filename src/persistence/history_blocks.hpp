#pragma once

#include "json_support.hpp"

namespace pgp::persistence::detail {

// The envelope retains its byte limit. Each block also has independent row,
// byte and parse budgets so long histories never expand into a single DOM.
inline constexpr std::size_t history_block_rows = 256;
inline constexpr std::size_t history_block_bytes = 2U * 1024U * 1024U;

class HistoryRows {
public:
    explicit HistoryRows(bool packed) : packed_(packed) {}
    void push(Json row);
    [[nodiscard]] Json finish();

private:
    void flush();
    void flush_repeats();
    bool packed_;
    Json rows_ = Json::array();
    std::string page_ = "[";
    std::string previous_;
    std::size_t page_rows_ = 0;
    std::size_t page_bytes_ = 2;
    std::size_t repeats_ = 0;
    std::size_t count_ = 0;
};

struct DecodedHistoryBlock {
    Json rows;
    std::size_t bytes;
};
[[nodiscard]] DecodedHistoryBlock read_history_block(const Json& encoded);

template<class Consume>
void read_history_rows(const Json& value, std::string_view path, std::size_t limit,
                       bool packed, Consume consume) {
    if (!packed) {
        array(value, path, limit);
        for (const auto& row : value) consume(row);
        return;
    }
    fields(value, path, {"count", "blocks"});
    auto remaining = static_cast<std::size_t>(u64(value.at("count"), path, limit));
    const auto& blocks = value.at("blocks");
    array(blocks, path, remaining);
    for (const auto& encoded : blocks) {
        const auto block = read_history_block(encoded);
        if (block.rows.empty()) invalid(path, "Empty history block");
        auto available = std::min(remaining, history_block_rows);
        auto bytes_left = history_block_bytes - block.bytes;
        const Json* previous = nullptr;
        for (const auto& row : block.rows) {
            if (row.is_number_integer()) {
                const auto repeats = u64(row, path, available);
                if (!previous || repeats == 0) invalid(path, "History repeats need a preceding row and positive count");
                const auto row_bytes = previous->dump().size() + 1;
                if (repeats > bytes_left / row_bytes) invalid(path, "Repeated history exceeds its expanded byte bound");
                bytes_left -= static_cast<std::size_t>(repeats) * row_bytes;
                for (std::uint64_t i = 0; i < repeats; ++i) consume(*previous);
                available -= static_cast<std::size_t>(repeats);
                remaining -= static_cast<std::size_t>(repeats);
                previous = nullptr;
            } else {
                if (available == 0) invalid(path, "History blocks exceed their declared row count");
                consume(row);
                --available;
                --remaining;
                previous = &row;
            }
        }
    }
    if (remaining != 0) invalid(path, "History blocks are missing rows");
}

} // namespace pgp::persistence::detail
