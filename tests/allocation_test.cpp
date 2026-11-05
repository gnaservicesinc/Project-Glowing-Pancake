#include <pgp/core/allocation.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

using namespace pgp::core;
namespace {
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Operation> void refuses(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Invalid allocation input was accepted");
}
const AllocationSegmentResult& segment(const AllocationResult& result, std::string_view id) {
    const auto found = std::find_if(result.segments.begin(), result.segments.end(), [&](const auto& row) { return row.id == id; });
    if (found == result.segments.end()) throw std::runtime_error("Missing segment result");
    return *found;
}
std::uint64_t delivered(const AllocationResult& result, std::string_view segment_id, std::string_view product_id) {
    const auto& row = segment(result, segment_id);
    const auto found = std::find_if(row.deliveries.begin(), row.deliveries.end(), [&](const auto& value) { return value.product_id == product_id; });
    return found == row.deliveries.end() ? 0 : found->tasks;
}
void conserved(const std::vector<AllocationProduct>& products, const std::vector<AllocationSegment>& segments,
               const AllocationResult& result) {
    check(result.rounds <= result.round_bound, "Allocation exceeded its catalog-sized round bound");
    check(result.segments.size() == segments.size(), "Allocation dropped a segment");
    std::map<std::string, std::uint64_t> product_tasks;
    for (const auto& input : segments) {
        const auto& row = segment(result, input.id);
        std::uint64_t tasks = row.outside_tasks;
        std::int64_t spend = 0;
        std::int64_t customer_work = 0;
        for (const auto& value : row.deliveries) {
            const auto product = std::find_if(products.begin(), products.end(), [&](const auto& item) { return item.id == value.product_id; });
            const auto edge = std::find_if(input.edges.begin(), input.edges.end(), [&](const auto& item) { return item.product_id == value.product_id; });
            check(product != products.end() && edge != input.edges.end(), "Allocation delivered an ineligible product");
            check(value.cost.value() == static_cast<std::int64_t>(value.tasks) * product->price.value(),
                  "Bill must equal useful delivered tasks times the product price");
            check(!edge->task_limit || value.tasks <= *edge->task_limit,
                  "Allocation exceeded the segment-product migration limit");
            tasks += value.tasks;
            spend = checked_add(spend, value.cost.value());
            customer_work = checked_add(customer_work, product->customer_compute_per_task.value()*static_cast<std::int64_t>(value.tasks));
            product_tasks[value.product_id] += value.tasks;
        }
        check(tasks == input.tasks, "Allocation lost demand instead of delivering it or recording outside work");
        check(checked_add(spend, row.remaining_budget.value()) == input.budget.value(),
              "Allocation spent the customer budget twice or lost its remainder");
        check(customer_work <= input.customer_compute.value(), "Vendors used the same customer hardware twice");
        check(std::accumulate(row.outside.begin(), row.outside.end(), std::uint64_t{0},
            [](auto total, const auto& value) { return total + value.tasks; }) == row.outside_tasks,
            "Outside tasks lack complete reason records");
    }
    for (const auto& product : products)
        check(product_tasks[product.id] <= product.capacity, "Competing segments exceeded shared product capacity");
}
void hand_cases() {
    std::vector<AllocationProduct> products{{"A", Cents{3}, 10}, {"B", Cents{7}, 10}};
    std::vector<AllocationSegment> segments{{"one", 7, Cents{30}, 1, {{"A", 1, {}}, {"B", 2, {}}}}};
    auto result = allocate_demand(products, segments);
    check(delivered(result, "one", "A") == 2 && delivered(result, "one", "B") == 3 &&
          segment(result, "one").outside_tasks == 2 && segment(result, "one").remaining_budget == Cents{3},
          "Unequal-price hand case must apportion 2A, 3B, 2 outside for 27 cents");
    conserved(products, segments, result);

    products = {{"A", Cents{10}, 10}, {"B", Cents{20}, 10}};
    segments = {{"one", 10, Cents{100}, 0, {{"A", 1, {}}, {"B", 1, {}}}}};
    result = allocate_demand(products, segments);
    check(delivered(result, "one", "A") == 4 && delivered(result, "one", "B") == 3 &&
          segment(result, "one").outside_tasks == 3 && segment(result, "one").remaining_budget == Cents{0},
          "100/150 rational scaling must preserve 4A, 3B, 3 outside instead of truncating to zero");
    check(segment(result, "one").outside.size() == 1 &&
          segment(result, "one").outside.front().reason == AllocationOutsideReason::budget,
          "Budget-refused terminal work needs its actual reason");
    conserved(products, segments, result);

    products = {{"free", Cents{0}, 10}, {"paid", Cents{20}, 10}};
    segments = {{"one", 10, Cents{40}, 0, {{"free", 1, {}}, {"paid", 1, {}}}}};
    result = allocate_demand(products, segments);
    check(delivered(result, "one", "free") == 8 && delivered(result, "one", "paid") == 2 &&
          segment(result, "one").outside_tasks == 0, "Budget refusals must retry feasible free work");
    conserved(products, segments, result);
    products.front().capacity = 5;
    result = allocate_demand(products, segments);
    check(delivered(result, "one", "free") == 5 && delivered(result, "one", "paid") == 2 &&
          segment(result, "one").outside_tasks == 3, "Free work must still consume finite task capacity");
    conserved(products, segments, result);

    products = {{"A", Cents{10}, 10}, {"B", Cents{20}, 10}};
    segments = {{"one", 10, Cents{200}, 0, {{"A", 3, {}}, {"B", 1, {}}}},
                {"two", 10, Cents{200}, 0, {{"A", 3, {}}, {"B", 1, {}}}}};
    result = allocate_demand(products, segments);
    for (const auto* id : {"one", "two"})
        check(delivered(result, id, "A") == 5 && delivered(result, id, "B") == 5 &&
              segment(result, id).remaining_budget == Cents{50},
              "Shared capacity must ration both rows together then redistribute refused tasks");
    conserved(products, segments, result);
    auto reversed_products = products;
    auto reversed_segments = segments;
    std::reverse(reversed_products.begin(), reversed_products.end());
    std::reverse(reversed_segments.begin(), reversed_segments.end());
    for (auto& row : reversed_segments) std::reverse(row.edges.begin(), row.edges.end());
    check(allocate_demand(reversed_products, reversed_segments) == result,
          "Input entity order changed allocation, reasons or round count");
}
void edge_limits_and_empty_outcomes() {
    std::vector<AllocationProduct> products{{"A", Cents{0}, 20}, {"B", Cents{1}, 20}};
    std::vector<AllocationSegment> segments{{"one", 10, Cents{100}, 0, {{"A", 100, 3}, {"B", 1, {}}}}};
    auto result = allocate_demand(products, segments);
    check(delivered(result, "one", "A") == 3 && delivered(result, "one", "B") == 7,
          "Exhausted segment-product limits must return refused work to remaining eligible products");
    conserved(products, segments, result);
    segments = {{"one", 10, Cents{0}, 0, {{"A", 1, 0}, {"B", 1, {}}}}};
    result = allocate_demand(products, segments);
    check(segment(result, "one").outside_tasks == 10, "Unaffordable and limited products must leave explicit outside demand");
    conserved(products, segments, result);
    segments = {{"one", 10, Cents{100}, 0, {{"A", 0, {}}, {"B", 0, {}}}}};
    result = allocate_demand(products, segments);
    check(segment(result, "one").outside.front().reason == AllocationOutsideReason::zero_utility,
          "A zero utility denominator must finish outside with a reason");
    conserved(products, segments, result);
    segments = {{"one", 10, Cents{100}, 1, {}}};
    result = allocate_demand({}, segments);
    check(segment(result, "one").outside_tasks == 10, "Absent products cannot drop all customer tasks");
    conserved({}, segments, result);
    segments = {{"one", 0, Cents{100}, 0, {}}};
    result = allocate_demand({}, segments);
    check(result.rounds == 0 && segment(result, "one").outside_tasks == 0 &&
          segment(result, "one").remaining_budget == Cents{100}, "No-demand row must preserve its budget without execution");
}
void bounds_and_apportionment() {
    const std::vector<WeightedQuota> weights{{"B", 1}, {"A", 1}};
    const auto quota = apportion_units(std::numeric_limits<std::uint64_t>::max(), weights);
    check(quota.size() == 2 && quota[0].id == "A" && quota[0].units == (std::numeric_limits<std::uint64_t>::max() / 2) + 1 &&
          quota[1].units == std::numeric_limits<std::uint64_t>::max() / 2,
          "Wide Hamilton products and ID residue ties must remain exact near uint64 bounds");
    const std::vector<WeightedQuota> zero{{"A", 0}};
    refuses([&] { (void)apportion_units(1, zero); });
    check(apportion_units(0, zero).front().units == 0, "Zero apportionment may preserve a zero-weight row");
    const std::vector<WeightedQuota> duplicate{{"A", 1}, {"A", 2}};
    refuses([&] { (void)apportion_units(1, duplicate); });
    std::vector<AllocationProduct> products{{"A", Cents{allocation_price_limit}, allocation_task_limit}};
    std::vector<AllocationSegment> segments{{"one", allocation_task_limit, Cents{100'000'000'000'000'000}, 0,
        {{"A", allocation_weight_limit, {}}}}};
    auto result = allocate_demand(products, segments);
    check(delivered(result, "one", "A") == allocation_task_limit && segment(result, "one").remaining_budget == Cents{0},
          "Catalog boundary allocation must use checked wide products without per-task work");
    conserved(products, segments, result);
    auto malformed_products = products;
    malformed_products.front().price = Cents{-1};
    refuses([&] { (void)allocate_demand(malformed_products, segments); });
    malformed_products = products;
    malformed_products.front().capacity = allocation_task_limit + 1;
    refuses([&] { (void)allocate_demand(malformed_products, segments); });
    malformed_products = products;
    malformed_products.front().id = "outside";
    refuses([&] { (void)allocate_demand(malformed_products, segments); });
    auto malformed_segments = segments;
    malformed_segments.front().edges.front().product_id = "missing";
    refuses([&] { (void)allocate_demand(products, malformed_segments); });
    malformed_segments = segments;
    malformed_segments.front().edges.push_back(malformed_segments.front().edges.front());
    refuses([&] { (void)allocate_demand(products, malformed_segments); });
    malformed_segments = segments;
    malformed_segments.front().edges.front().weight = allocation_weight_limit + 1;
    refuses([&] { (void)allocate_demand(products, malformed_segments); });
    malformed_segments = segments;
    malformed_segments.front().budget = Cents{-1};
    refuses([&] { (void)allocate_demand(products, malformed_segments); });
    malformed_segments.assign(allocation_segment_limit + 1, segments.front());
    refuses([&] { (void)allocate_demand(products, malformed_segments); });
}
void customer_hardware_and_incumbents() {
    std::vector<AllocationProduct> products{
        {"vendor.open", Cents{0}, 10, MilliCU{2}},
        {"vendor.licensed", Cents{4}, 10, MilliCU{2}}
    };
    std::vector<AllocationSegment> segments{
        {"public", 10, Cents{100}, 0, {{"vendor.open", 1, {}}, {"vendor.licensed", 1, {}}}, MilliCUPerDay{12}}
    };
    auto result=allocate_demand(products,segments);
    check(delivered(result,"public","vendor.open")==3 && delivered(result,"public","vendor.licensed")==3 &&
        segment(result,"public").outside_tasks==4 && segment(result,"public").outside.front().reason==AllocationOutsideReason::customer_compute,
        "Both vendors must share one 12-milli-CU hardware budget");
    conserved(products,segments,result);
    segments.push_back({"science",10,Cents{100},0,{{"vendor.licensed",1,{}}},MilliCUPerDay{20}});
    result=allocate_demand(products,segments);
    check(delivered(result,"public","vendor.licensed")+delivered(result,"science","vendor.licensed")==10 &&
        delivered(result,"public","vendor.open")+delivered(result,"public","vendor.licensed")==6,
        "A local vendor's finite supported service must contend across both segments");
    conserved(products,segments,result);
    segments.pop_back();
    segments.front().edges={{"vendor.open",10,{}},{"vendor.licensed",1,5,5}};
    result=allocate_demand(products,segments);
    check(delivered(result,"public","vendor.licensed")==5 && delivered(result,"public","vendor.open")==1,
        "A feasible incumbent must retain its installed workload before discretionary switching");
    conserved(products,segments,result);
    products.back().capacity=0;
    result=allocate_demand(products,segments);
    check(delivered(result,"public","vendor.licensed")==0 && delivered(result,"public","vendor.open")==6,
        "An unavailable incumbent must release its work to eligible alternatives");
    conserved(products,segments,result);
}
void small_constrained_cases() {
    // Small independently varied budgets, free prices, edge limits and shared
    // supplies exercise conservation and the catalog-sized progress argument.
    for (std::uint64_t sample = 0; sample < 48; ++sample) {
        std::vector<AllocationProduct> products;
        std::vector<AllocationSegment> segments;
        for (std::uint64_t p = 0; p < 3; ++p)
            products.push_back({"product." + std::to_string(p), Cents{(sample + p * 3) % 12},
                (sample * 5 + p * 7) % 31,MilliCU{(sample+p)%3}});
        for (std::uint64_t s = 0; s < 3; ++s) {
            AllocationSegment row{"segment." + std::to_string(s), (sample * 3 + s * 7) % 21,
                                  Cents{(sample * 7 + s * 11) % 61}, (sample + s) % 4, {},MilliCUPerDay{(sample*9+s*13)%40}};
            for (std::uint64_t p = 0; p < 3; ++p)
                row.edges.push_back({products[static_cast<std::size_t>(p)].id, (sample * 2 + s * 3 + p) % 9,
                    (sample + s + p) % 2 == 0 ? std::optional{(sample + s * 5 + p) % 16} : std::nullopt,
                    (sample+s+p)%4});
            segments.push_back(std::move(row));
        }
        const auto result = allocate_demand(products, segments);
        conserved(products, segments, result);
        std::reverse(products.begin(), products.end());
        std::reverse(segments.begin(), segments.end());
        for (auto& row : segments) std::reverse(row.edges.begin(), row.edges.end());
        check(allocate_demand(products, segments) == result, "Small constrained case depended on input order");
    }
}
} // namespace
int main() {
    try {
        hand_cases();
        edge_limits_and_empty_outcomes();
        bounds_and_apportionment();
        customer_hardware_and_incumbents();
        small_constrained_cases();
        std::cout << checks << " allocation checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
