#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace pgp::core {

// Settled days can share unchanged detail without changing their own date.
// All writes are explicit so reading a checkpoint never detaches a payload.
template<class Row>
class DayRows {
    using Rows = std::vector<Row>;

public:
    [[nodiscard]] std::size_t size() const { return read().size(); }
    [[nodiscard]] bool empty() const { return read().empty(); }
    const Row& operator[](std::size_t index) const { return read()[index]; }
    const Row& front() const { return read().front(); }
    const Row& back() const { return read().back(); }
    auto begin() const { return read().begin(); }
    auto end() const { return read().end(); }

    void push_back(Row row) { write().push_back(std::move(row)); }
    void replace(std::size_t index, Row row) { write().at(index) = std::move(row); }
    template<class Less> void sort(Less less) {
        if (size() < 2) return;
        auto& rows = write();
        std::sort(rows.begin(), rows.end(), less);
    }
    void reuse_equal(const DayRows& previous) {
        if (*this == previous) rows_ = previous.rows_;
    }
    bool operator==(const DayRows& other) const { return rows_ == other.rows_ || read() == other.read(); }
    auto operator<=>(const DayRows& other) const { return read() <=> other.read(); }

private:
    const Rows& read() const {
        static const Rows empty;
        return rows_ ? *rows_ : empty;
    }
    Rows& write() {
        if (!rows_) rows_ = std::make_shared<Rows>();
        else if (rows_.use_count() > 1) rows_ = std::make_shared<Rows>(*rows_);
        return *rows_;
    }
    std::shared_ptr<Rows> rows_;
};

} // namespace pgp::core
