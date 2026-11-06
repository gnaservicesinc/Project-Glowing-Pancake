#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgp::core {

// A checkpoint copy shares the page tree. A write copies only the path to
// its page, so retaining a rollback checkpoint does not copy older records.
// Reads never detach. Iterators belong to their history and require it to
// remain unchanged while in use.
struct UncountedHistory {
    template<class T> void add(const T&) {}
};

template<class T, std::size_t PageSize, class Totals = UncountedHistory>
class PagedHistory {
    static_assert(PageSize > 0);
    static constexpr std::size_t fanout = 16;
    struct Node {
        Node() = default;
        Node(const Node& other) : children(other.children) {
            if (!other.rows.empty()) {
                rows.reserve(PageSize);
                rows.insert(rows.end(), other.rows.begin(), other.rows.end());
            }
        }
        std::array<std::shared_ptr<Node>, fanout> children{};
        std::vector<T> rows;
    };

public:
    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() = default;
        reference operator*() const { return (*history_)[static_cast<std::size_t>(row_)]; }
        pointer operator->() const { return std::addressof(**this); }
        reference operator[](difference_type n) const { return *(*this + n); }
        const_iterator& operator++() { ++row_; return *this; }
        const_iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        const_iterator& operator--() { --row_; return *this; }
        const_iterator operator--(int) { auto previous = *this; --*this; return previous; }
        const_iterator& operator+=(difference_type n) { row_ += n; return *this; }
        const_iterator& operator-=(difference_type n) { row_ -= n; return *this; }
        friend const_iterator operator+(const_iterator it, difference_type n) { return it += n; }
        friend const_iterator operator+(difference_type n, const_iterator it) { return it += n; }
        friend const_iterator operator-(const_iterator it, difference_type n) { return it -= n; }
        friend difference_type operator-(const_iterator a, const_iterator b) { return a.row_ - b.row_; }
        auto operator<=>(const const_iterator&) const = default;

    private:
        friend class PagedHistory;
        const_iterator(const PagedHistory& history, std::size_t row)
            : history_(&history), row_(static_cast<difference_type>(row)) {}
        const PagedHistory* history_ = nullptr;
        difference_type row_ = 0;
    };

    PagedHistory() = default;
    PagedHistory(const PagedHistory&) = default;
    PagedHistory& operator=(const PagedHistory&) = default;
    PagedHistory(PagedHistory&& other) noexcept { *this = std::move(other); }
    PagedHistory& operator=(PagedHistory&& other) noexcept {
        if (this != &other) {
            root_ = std::move(other.root_);
            pages_ = std::exchange(other.pages_, 1);
            size_ = std::exchange(other.size_, 0);
            totals_ = std::exchange(other.totals_, Totals{});
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] const Totals& totals() const { return totals_; }
    const T& operator[](std::size_t row) const {
        const Node* node = root_.get();
        std::size_t page = row / PageSize;
        for (auto span = pages_; span > 1;) {
            span /= fanout;
            node = node->children[page / span].get();
            page %= span;
        }
        return node->rows[row % PageSize];
    }
    const T& at(std::size_t row) const {
        if (row >= size_) throw std::out_of_range("History row is outside its range");
        return (*this)[row];
    }
    const T& front() const { return (*this)[0]; }
    const T& back() const { return (*this)[size_ - 1]; }
    const_iterator begin() const { return {*this, 0}; }
    const_iterator end() const { return {*this, size_}; }
    auto rbegin() const { return std::make_reverse_iterator(end()); }
    auto rend() const { return std::make_reverse_iterator(begin()); }

    void push_back(T row) {
        auto totals = totals_;
        totals.add(row);
        const auto page = size_ / PageSize;
        if (page == pages_) {
            auto parent = std::make_shared<Node>();
            parent->children[0] = std::move(root_);
            root_ = std::move(parent);
            pages_ *= fanout;
        }
        writable_page(page).push_back(std::move(row));
        ++size_;
        totals_ = std::move(totals);
    }

    void replace(std::size_t row, T value) requires std::is_same_v<Totals, UncountedHistory> {
        if (row >= size_) throw std::out_of_range("History row is outside its range");
        writable_page(row / PageSize)[row % PageSize] = std::move(value);
    }

    bool operator==(const PagedHistory& other) const {
        return size_ == other.size_ && (root_ == other.root_ || std::equal(begin(), end(), other.begin()));
    }
    auto operator<=>(const PagedHistory& other) const {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
    }

private:
    std::vector<T>& writable_page(std::size_t page) {
        auto* slot = &root_;
        auto span = pages_;
        for (;;) {
            if (!*slot) *slot = std::make_shared<Node>();
            else if (slot->use_count() > 1) *slot = std::make_shared<Node>(**slot);
            auto& node = **slot;
            if (span == 1) {
                if (node.rows.capacity() == 0) node.rows.reserve(PageSize);
                return node.rows;
            }
            span /= fanout;
            slot = &node.children[page / span];
            page %= span;
        }
    }

    std::shared_ptr<Node> root_;
    std::size_t pages_ = 1;
    std::size_t size_ = 0;
    Totals totals_{};
};

} // namespace pgp::core
