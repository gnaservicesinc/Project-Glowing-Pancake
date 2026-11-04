#include <pgp/core/checked_arithmetic.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
struct Recipient {
    std::uint64_t id;
    std::uint64_t requested;
    std::uint64_t price;
    std::uint64_t capacity;
    std::uint64_t allocated{0};
    std::uint64_t remainder{0};
};

// One bounded local rationing fixture, not the M2-03 market optimizer. Each
// rational quota is floored once, then each fractional residue is considered
// once against demand, remaining money and recipient capacity.
std::array<Recipient, 2> ration(std::array<Recipient, 2> recipients,
                              std::uint64_t budget, std::uint64_t demand) {
    constexpr std::uint64_t tentative_cost = 150;
    std::uint64_t spent = 0;
    std::uint64_t assigned = 0;
    for (auto& recipient : recipients) {
        const auto quota = pgp::core::mul_div_floor(recipient.requested, budget, tentative_cost);
        recipient.allocated = std::min({quota.quotient, recipient.capacity, demand - assigned});
        recipient.allocated = std::min(recipient.allocated, (budget - spent) / recipient.price);
        recipient.remainder = quota.remainder;
        assigned += recipient.allocated;
        spent += recipient.allocated * recipient.price;
    }
    std::sort(recipients.begin(), recipients.end(), [](const Recipient& a, const Recipient& b) {
        if (a.remainder != b.remainder) { return a.remainder > b.remainder; }
        return a.id < b.id;
    });
    for (auto& recipient : recipients) {
        if (recipient.remainder != 0 && recipient.allocated < recipient.requested
            && recipient.allocated < recipient.capacity && assigned < demand
            && recipient.price <= budget - spent) {
            ++recipient.allocated;
            ++assigned;
            spent += recipient.price;
        }
    }
    std::sort(recipients.begin(), recipients.end(), [](const Recipient& a, const Recipient& b) {
        return a.id < b.id;
    });
    if (spent > budget || assigned > demand) { throw std::runtime_error("rationing constraint violated"); }
    return recipients;
}
} // namespace

int main() {
    try {
        const auto usual = ration({{{1, 5, 10, 5}, {2, 5, 20, 5}}}, 100, 10);
        const auto reversed = ration({{{2, 5, 20, 5}, {1, 5, 10, 5}}}, 100, 10);
        if (usual[0].allocated != 4 || usual[1].allocated != 3
            || reversed[0].allocated != 4 || reversed[1].allocated != 3) {
            throw std::runtime_error("rational scaling / unique-ID residue tie failed");
        }
        const auto capped = ration({{{1, 5, 10, 3}, {2, 5, 20, 5}}}, 100, 10);
        if (capped[0].allocated != 3 || capped[1].allocated != 3) {
            throw std::runtime_error("capacity / budget residue recheck failed");
        }
        const auto demand = ration({{{1, 5, 10, 5}, {2, 5, 20, 5}}}, 100, 6);
        if (demand[0].allocated != 3 || demand[1].allocated != 3) {
            throw std::runtime_error("demand residue recheck failed");
        }
        std::cout << "Bounded constrained-residue examples pass\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
