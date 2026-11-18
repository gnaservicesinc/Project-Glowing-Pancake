#pragma once

#include <pgp/core/objectives.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace pgp::runtime {
struct RivalCompany;
struct RivalObservation;
struct WorldState;

inline constexpr std::uint32_t rival_policy_schema=1;
inline constexpr std::size_t rival_policy_features=32;
inline constexpr std::size_t rival_policy_hidden=64;
inline constexpr std::size_t rival_policy_actions=32;
inline constexpr std::size_t rival_policy_param_count=
    rival_policy_features*rival_policy_hidden+rival_policy_hidden+
    rival_policy_hidden*rival_policy_actions+rival_policy_actions;
using RivalPolicyFeatures=std::array<float,rival_policy_features>;
struct RivalLearningSample {
    RivalPolicyFeatures features{};
    std::uint16_t teacher=0;
    std::uint32_t legal=0;
};
struct RivalPolicyWeights {
    std::array<float,rival_policy_features*rival_policy_hidden> w1{};
    std::array<float,rival_policy_hidden> b1{};
    std::array<float,rival_policy_hidden*rival_policy_actions> w2{};
    std::array<float,rival_policy_actions> b2{};
    bool operator==(const RivalPolicyWeights&) const = default;
};
struct RivalTrainingSample {
    RivalPolicyFeatures features{};
    std::uint16_t teacher=0;
    std::uint32_t legal=0;
    float weight=1.0F;
};
struct RivalPolicyOptimizerState {
    std::uint64_t step=0;
    RivalPolicyWeights m{};
    RivalPolicyWeights v{};
    bool operator==(const RivalPolicyOptimizerState&) const = default;
};
struct RivalPolicyTrainingConfig {
    float learning_rate=3e-3F;
    float beta1=0.9F;
    float beta2=0.999F;
    float epsilon=1e-8F;
    float weight_decay=1e-4F;
    float max_grad_norm=1.0F;
    bool prefer_metal=true;
};
enum class RivalPolicyBackend { cpu, metal_mpsgraph };
struct RivalPolicyStepMetrics {
    float loss=0.0F;
    float accuracy=0.0F;
    float grad_norm=0.0F;
    std::size_t valid_samples=0;
    RivalPolicyBackend backend=RivalPolicyBackend::cpu;
};
struct RivalAmbition {
    bool asi=false,controlled=false,secondary=false;
    float progress=0;
    [[nodiscard]] bool achieved() const {return asi && controlled && secondary;}
    [[nodiscard]] float reward() const;
};
[[nodiscard]] core::ObjectiveKind rival_secondary(std::string_view id);
[[nodiscard]] RivalAmbition rival_ambition(const WorldState&,const RivalCompany&);
[[nodiscard]] std::size_t rival_action_slot(std::string_view);
[[nodiscard]] RivalPolicyFeatures rival_policy_observation(const WorldState&,const RivalCompany&,const RivalObservation&);
using RivalPolicyRecorder=void(*)(const RivalLearningSample&,void*);
void set_rival_policy_recorder(RivalPolicyRecorder,void*);
[[nodiscard]] bool rival_policy_enabled();
using RivalPolicySelector=std::size_t(*)(const RivalLearningSample&,void*);
void set_rival_policy_selector(RivalPolicySelector,void*);
void set_rival_policy_weights(const RivalPolicyWeights*);
[[nodiscard]] std::size_t select_rival_policy_action(const RivalLearningSample&);
[[nodiscard]] RivalPolicyWeights initialize_rival_policy_weights(std::uint64_t seed=1);
[[nodiscard]] bool rival_policy_weights_valid(const RivalPolicyWeights&) noexcept;
[[nodiscard]] std::array<float,rival_policy_actions> evaluate_rival_policy_logits(const RivalPolicyWeights&,const RivalPolicyFeatures&);
[[nodiscard]] std::size_t select_rival_policy_with_weights(const RivalPolicyWeights&,const RivalLearningSample&);
[[nodiscard]] bool rival_policy_metal_available() noexcept;
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step_cpu(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalTrainingSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step_cpu(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalLearningSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step_metal(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalTrainingSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step_metal(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalLearningSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalTrainingSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] RivalPolicyStepMetrics train_rival_policy_step(
    RivalPolicyWeights&,RivalPolicyOptimizerState&,std::span<const RivalLearningSample>,const RivalPolicyTrainingConfig& ={});
[[nodiscard]] std::vector<std::uint8_t> serialize_rival_policy_checkpoint(const RivalPolicyWeights&,const RivalPolicyOptimizerState* =nullptr);
void deserialize_rival_policy_checkpoint(std::span<const std::uint8_t>,RivalPolicyWeights&,RivalPolicyOptimizerState* =nullptr);
}
