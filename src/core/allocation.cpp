#include <pgp/core/allocation.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace pgp::core {
namespace {
constexpr std::string_view outside_id = "outside";
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void text_valid(std::string_view text) {
    if (text.empty() || text.size() > 128 || std::any_of(text.begin(), text.end(),
        [](unsigned char c) { return c < 32 || c == 127; }))
        invalid("Allocation identities must be nonempty bounded text without control characters");
}
std::uint64_t add(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
        throw std::overflow_error("Allocation unsigned sum overflow");
    return left + right;
}
std::uint64_t cost(std::uint64_t tasks, std::uint64_t rate) {
    return mul_div_floor(tasks, rate, 1).quotient;
}
struct Product {
    std::string id;
    Cents price{0};
    std::uint64_t remaining = 0;
    std::uint64_t local_rate = 0;
};
struct Edge {
    std::size_t product = 0;
    std::uint64_t weight = 0;
    std::uint64_t remaining = 0;
    std::uint64_t retained = 0;
    std::optional<std::uint64_t> interest_limit{};
};
struct Segment {
    std::string id;
    std::uint64_t remaining = 0;
    std::uint64_t budget = 0;
    std::uint64_t local_compute = 0;
    std::uint64_t outside_weight = 0;
    std::vector<Edge> edges;
    std::vector<std::uint64_t> delivered;
    std::map<AllocationOutsideReason, std::uint64_t> outside;
    std::vector<AllocationInterest> waitlist_interest{};
};
bool eligible(const Segment& segment, const Edge& edge, const std::vector<Product>& products) {
    const auto& product = products[edge.product];
    return edge.weight != 0 && edge.remaining != 0 && product.remaining != 0 &&
           static_cast<std::uint64_t>(product.price.value()) <= segment.budget &&
           product.local_rate <= segment.local_compute;
}
AllocationOutsideReason refusal_reason(const Segment& segment, const std::vector<Product>& products) {
    bool positive_weight = false, available_product = false, available_edge = false, affordable = false;
    for (const auto& edge : segment.edges) {
        if (edge.weight == 0) continue;
        positive_weight = true;
        const auto& product = products[edge.product];
        if (product.remaining == 0) continue;
        available_product = true;
        if (edge.remaining == 0) continue;
        available_edge = true;
        if (static_cast<std::uint64_t>(product.price.value()) <= segment.budget) affordable = true;
    }
    if (!positive_weight) return segment.edges.empty() ? AllocationOutsideReason::no_eligible_product : AllocationOutsideReason::zero_utility;
    if (!available_product) return AllocationOutsideReason::capacity;
    if (!available_edge) return AllocationOutsideReason::edge_limit;
    if (!affordable) return AllocationOutsideReason::budget;
    return AllocationOutsideReason::customer_compute;
}
void send_outside(Segment& segment, std::uint64_t tasks, AllocationOutsideReason reason) {
    if (tasks > segment.remaining) invalid("Allocation outside work exceeds remaining demand");
    segment.remaining -= tasks;
    if (tasks != 0) segment.outside[reason] = add(segment.outside[reason], tasks);
}
using Claims = std::vector<std::vector<std::uint64_t>>;
template<class Rate>
void constrain_claims(std::vector<std::uint64_t>& claims, const std::vector<Product>& products,
                      std::uint64_t budget, Rate rate, bool keep_free) {
    std::uint64_t total = 0;
    for (std::size_t p = 0; p < products.size(); ++p) total = add(total, cost(claims[p], rate(products[p])));
    if (total <= budget) return;

    struct Remainder { std::size_t product; std::uint64_t fraction; std::uint64_t maximum; };
    std::vector<Remainder> remainders;
    std::uint64_t spent = 0;
    for (std::size_t p = 0; p < products.size(); ++p) {
        const auto unit = rate(products[p]);
        if (keep_free && unit == 0) continue;
        const auto maximum = claims[p];
        const auto scaled = mul_div_floor(maximum, budget, total);
        claims[p] = scaled.quotient;
        spent = add(spent, cost(scaled.quotient, unit));
        if (scaled.remainder != 0) remainders.push_back({p, scaled.remainder, maximum});
    }
    std::sort(remainders.begin(), remainders.end(), [&](const auto& left, const auto& right) {
        if (left.fraction != right.fraction) return left.fraction > right.fraction;
        return products[left.product].id < products[right.product].id;
    });
    for (const auto& remainder : remainders) {
        const auto p = remainder.product;
        const auto unit = rate(products[p]);
        if (claims[p] < remainder.maximum && unit <= budget - spent) {
            ++claims[p];
            spent = add(spent, unit);
        }
    }
}
void conserve_segment(std::vector<std::uint64_t>& claims, const Segment& segment, const std::vector<Product>& products) {
    constrain_claims(claims, products, segment.local_compute, [](const auto& product) { return product.local_rate; }, true);
    constrain_claims(claims, products, segment.budget, [](const auto& product) {
        return static_cast<std::uint64_t>(product.price.value());
    }, false);
}
void record_waitlist_interest(Segment& segment,const std::vector<Product>& products,
                              const std::map<std::string,std::size_t,std::less<>>& product_indices) {
    if (segment.remaining==0 || std::ranges::none_of(segment.edges,[](const auto& edge){return edge.interest_limit.has_value();})) return;
    std::vector<WeightedQuota> weights;
    for (const auto& edge:segment.edges) {
        const auto& product=products[edge.product];
        const bool accepting_interest=edge.interest_limit.value_or(0)>0 && edge.weight>0 &&
            product.local_rate<=segment.local_compute && static_cast<std::uint64_t>(product.price.value())<=segment.budget;
        if (accepting_interest || eligible(segment,edge,products)) weights.push_back({product.id,edge.weight});
    }
    if (weights.empty()) return;
    weights.push_back({std::string(outside_id),segment.outside_weight});
    std::vector<std::uint64_t> claims(products.size(),0);
    // A bounded first-choice pass shares each buyer's task, money and hardware
    // budget. These are expressions of interest and commit no allocation.
    for (const auto& choice:apportion_units(segment.remaining,weights)) {
        if (choice.id==outside_id) continue;
        const auto index=product_indices.at(choice.id);
        const auto edge=std::ranges::find(segment.edges,index,&Edge::product);
        claims[index]=std::min(choice.units,edge->interest_limit.value_or(edge->remaining));
    }
    conserve_segment(claims,segment,products);
    for (const auto& edge:segment.edges)
        if (edge.interest_limit && claims[edge.product]>0)
            segment.waitlist_interest.push_back({products[edge.product].id,claims[edge.product]});
}
void commit_claims(Claims& claims, std::vector<Segment>& segments, std::vector<Product>& products) {
    for (std::size_t p = 0; p < products.size(); ++p) {
        std::uint64_t total = 0;
        std::vector<WeightedQuota> weights;
        for (std::size_t s = 0; s < segments.size(); ++s) {
            total = add(total, claims[s][p]);
            if (claims[s][p] != 0) weights.push_back({segments[s].id, claims[s][p]});
        }
        if (total <= products[p].remaining) continue;
        for (const auto& quota : apportion_units(products[p].remaining, weights)) {
            const auto found = std::lower_bound(segments.begin(), segments.end(), quota.id,
                [](const auto& segment, const auto& id) { return segment.id < id; });
            claims[static_cast<std::size_t>(found - segments.begin())][p] = quota.units;
        }
    }
    for (std::size_t s = 0; s < segments.size(); ++s) {
        auto& segment = segments[s];
        for (auto& edge : segment.edges) {
            const auto p = edge.product;
            const auto delivered = claims[s][p];
            const auto charged = cost(delivered, static_cast<std::uint64_t>(products[p].price.value()));
            const auto used = cost(delivered, products[p].local_rate);
            if (delivered > segment.remaining || delivered > edge.remaining || delivered > products[p].remaining ||
                charged > segment.budget || used > segment.local_compute)
                invalid("Allocation exceeded demand, budget, buyer compute or product supply");
            segment.remaining -= delivered;
            segment.budget -= charged;
            segment.local_compute -= used;
            edge.remaining -= delivered;
            products[p].remaining -= delivered;
            segment.delivered[p] = add(segment.delivered[p], delivered);
        }
    }
}
} // namespace

std::vector<ApportionedQuota> apportion_units(std::uint64_t total, std::span<const WeightedQuota> weights) {
    if (weights.size() > allocation_edge_limit + 1) invalid("Apportionment count exceeds its bound");
    struct Part { std::string id; std::uint64_t units; std::uint64_t remainder; };
    std::vector<Part> parts;
    std::set<std::string> ids;
    std::uint64_t weight_sum = 0;
    for (const auto& weight : weights) {
        text_valid(weight.id);
        if (!ids.insert(weight.id).second || weight.weight > allocation_weight_limit)
            invalid("Apportionment has duplicate identities or an excessive weight");
        weight_sum = add(weight_sum, weight.weight);
    }
    if (weight_sum == 0 && total != 0) invalid("Positive apportionment needs a positive total weight");
    std::uint64_t used = 0;
    for (const auto& weight : weights) {
        const auto portion = weight_sum == 0 ? QuotientRemainder{0, 0} : mul_div_floor(total, weight.weight, weight_sum);
        parts.push_back({weight.id, portion.quotient, portion.remainder});
        used = add(used, portion.quotient);
    }
    std::sort(parts.begin(), parts.end(), [](const auto& left, const auto& right) {
        if (left.remainder != right.remainder) return left.remainder > right.remainder;
        return left.id < right.id;
    });
    const auto residue = total - used;
    if (residue > parts.size()) invalid("Apportionment residue exceeded its proved count bound");
    for (std::size_t i = 0; i < static_cast<std::size_t>(residue); ++i) ++parts[i].units;
    std::sort(parts.begin(), parts.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    std::vector<ApportionedQuota> result;
    for (const auto& part : parts) result.push_back({part.id, part.units});
    return result;
}

AllocationResult allocate_demand(std::span<const AllocationProduct> input_products,
                                 std::span<const AllocationSegment> input_segments) {
    if (input_products.size() > allocation_product_limit || input_segments.size() > allocation_segment_limit)
        invalid("Allocation catalog exceeds its segment or product bound");
    std::vector<Product> products;
    std::vector<Segment> segments;
    for (const auto& product : input_products) {
        text_valid(product.id);
        if (product.id == outside_id || product.price.value() < 0 || product.price.value() > allocation_price_limit ||
            product.capacity > allocation_task_limit || product.customer_compute_per_task.value() > 1'000'000'000)
            invalid("Product has a reserved identity, invalid price or excessive capacity");
        products.push_back({product.id, product.price, product.capacity, static_cast<std::uint64_t>(product.customer_compute_per_task.value())});
    }
    std::sort(products.begin(), products.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    std::map<std::string, std::size_t, std::less<>> product_indices;
    for (std::size_t i = 0; i < products.size(); ++i)
        if (!product_indices.emplace(products[i].id, i).second) invalid("Duplicate allocation product identity");
    std::size_t edge_count = 0;
    bool has_waitlists = false;
    for (const auto& segment : input_segments) {
        text_valid(segment.id);
        if (segment.tasks > allocation_task_limit || segment.budget.value() < 0 ||
            segment.outside_weight > allocation_weight_limit || segment.edges.size() > allocation_product_limit)
            invalid("Segment demand, budget, outside weight or edge count is invalid");
        edge_count += segment.edges.size();
        if (edge_count > allocation_edge_limit) invalid("Allocation edge count exceeds its bound");
        Segment row{segment.id, segment.tasks, static_cast<std::uint64_t>(segment.budget.value()),
                    static_cast<std::uint64_t>(segment.customer_compute.value()),
                    segment.outside_weight, {}, std::vector<std::uint64_t>(products.size(), 0), {}};
        std::set<std::string> edge_ids;
        for (const auto& edge : segment.edges) {
            const auto found = product_indices.find(edge.product_id);
            if (found == product_indices.end() || !edge_ids.insert(edge.product_id).second || edge.weight > allocation_weight_limit ||
                (edge.task_limit && *edge.task_limit > allocation_task_limit) || edge.retained_tasks > allocation_task_limit ||
                (edge.waitlist_interest_limit && (!edge.task_limit || *edge.waitlist_interest_limit<*edge.task_limit ||
                    *edge.waitlist_interest_limit>allocation_task_limit)))
                invalid("Allocation edge has an unknown/duplicate product, excessive weight or task limit");
            const auto limit = edge.task_limit.value_or(allocation_task_limit);
            has_waitlists=has_waitlists || edge.waitlist_interest_limit.has_value();
            row.edges.push_back({found->second, edge.weight, limit, std::min(edge.retained_tasks, limit),edge.waitlist_interest_limit});
        }
        std::sort(row.edges.begin(), row.edges.end(), [](const auto& left, const auto& right) { return left.product < right.product; });
        segments.push_back(std::move(row));
    }
    std::sort(segments.begin(), segments.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    for (std::size_t i = 1; i < segments.size(); ++i)
        if (segments[i - 1].id == segments[i].id) invalid("Duplicate allocation segment identity");

    AllocationResult result;
    result.round_bound = products.size() + edge_count + 1;
    if (has_waitlists) for (auto& segment:segments) record_waitlist_interest(segment,products,product_indices);
    const bool has_retention = std::any_of(segments.begin(), segments.end(), [&](const auto& segment) {
        return std::any_of(segment.edges.begin(), segment.edges.end(), [&](const auto& edge) {
            return edge.retained != 0 && eligible(segment, edge, products);
        });
    });
    if (has_retention) {
        Claims claims(segments.size(), std::vector<std::uint64_t>(products.size(), 0));
        for (std::size_t s = 0; s < segments.size(); ++s) {
            const auto& segment = segments[s];
            std::vector<WeightedQuota> incumbents;
            std::uint64_t retained = 0;
            for (const auto& edge : segment.edges) if (edge.retained != 0 && eligible(segment, edge, products)) {
                incumbents.push_back({products[edge.product].id, edge.retained});
                retained = add(retained, edge.retained);
            }
            for (const auto& quota : apportion_units(std::min(retained, segment.remaining), incumbents))
                claims[s][product_indices.at(quota.id)] = quota.units;
            conserve_segment(claims[s], segment, products);
        }
        commit_claims(claims, segments, products);
    }
    while (std::any_of(segments.begin(), segments.end(), [](const auto& segment) { return segment.remaining != 0; })) {
        if (++result.rounds > result.round_bound) invalid("Allocation exhausted its proved finite round bound");
        Claims claims(segments.size(), std::vector<std::uint64_t>(products.size(), 0));
        std::vector<std::vector<bool>> eligible_before(segments.size());
        std::vector<bool> supplied_before;
        for (const auto& product : products) supplied_before.push_back(product.remaining != 0);
        for (std::size_t s = 0; s < segments.size(); ++s) {
            auto& segment = segments[s];
            for (const auto& edge : segment.edges) eligible_before[s].push_back(eligible(segment, edge, products));
            if (segment.remaining == 0) continue;
            std::vector<WeightedQuota> weights;
            for (const auto& edge : segment.edges)
                if (eligible(segment, edge, products)) weights.push_back({products[edge.product].id, edge.weight});
            if (weights.empty()) {
                send_outside(segment, segment.remaining, refusal_reason(segment, products));
                continue;
            }
            weights.push_back({std::string(outside_id), segment.outside_weight});
            for (const auto& quota : apportion_units(segment.remaining, weights)) {
                if (quota.id == outside_id) send_outside(segment, quota.units, AllocationOutsideReason::preference);
                else {
                    const auto p = product_indices.at(quota.id);
                    const auto edge = std::find_if(segment.edges.begin(), segment.edges.end(), [p](const auto& value) { return value.product == p; });
                    claims[s][p] = std::min(quota.units, edge->remaining);
                }
            }
            conserve_segment(claims[s], segment, products);
        }
        commit_claims(claims, segments, products);
        if (std::any_of(segments.begin(), segments.end(), [](const auto& segment) { return segment.remaining != 0; })) {
            bool progress = false;
            for (std::size_t p = 0; p < products.size(); ++p)
                if (supplied_before[p] && products[p].remaining == 0) progress = true;
            for (std::size_t s = 0; s < segments.size(); ++s)
                for (std::size_t e = 0; e < segments[s].edges.size(); ++e)
                    if (eligible_before[s][e] && !eligible(segments[s], segments[s].edges[e], products)) progress = true;
            if (!progress) invalid("Allocation did not exhaust a supply/edge or finish residual demand");
        }
    }
    for (auto& segment : segments) {
        AllocationSegmentResult row;
        row.id = segment.id;
        row.remaining_budget = Cents{segment.budget};
        row.waitlist_interest=std::move(segment.waitlist_interest);
        for (std::size_t p = 0; p < products.size(); ++p)
            if (segment.delivered[p] != 0) row.deliveries.push_back({products[p].id, segment.delivered[p], Cents{cost(segment.delivered[p], static_cast<std::uint64_t>(products[p].price.value()))}});
        for (const auto& [reason, tasks] : segment.outside) {
            row.outside.push_back({reason, tasks});
            row.outside_tasks = add(row.outside_tasks, tasks);
        }
        result.segments.push_back(std::move(row));
    }
    return result;
}
} // namespace pgp::core
