#pragma once

#include <pgp/runtime/rival_policy.hpp>

#include <span>
#include <vector>

namespace pgp::runtime::detail {
struct PreparedPolicyBatch {
    std::size_t count=0;
    std::vector<float> x;
    std::vector<float> mask;
    std::vector<float> one_hot;
    std::vector<float> norm_weights;
    std::vector<std::uint16_t> teachers;
    std::vector<std::uint32_t> legals;
};

[[nodiscard]] bool all_finite(std::span<const float> values) noexcept;
[[nodiscard]] bool optimizer_state_valid(const RivalPolicyOptimizerState& opt) noexcept;
[[nodiscard]] PreparedPolicyBatch prepare_policy_batch(
    const RivalPolicyWeights& weights,
    const RivalPolicyOptimizerState& opt,
    std::span<const RivalTrainingSample> samples,
    const RivalPolicyTrainingConfig& config);
[[nodiscard]] float compute_masked_accuracy(
    std::span<const float> masked_logits,
    const PreparedPolicyBatch& batch) noexcept;
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step_metal_impl(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    const PreparedPolicyBatch& batch,
    const RivalPolicyTrainingConfig& config);
}
