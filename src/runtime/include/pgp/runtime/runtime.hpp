#pragma once

#include <pgp/runtime/compiler.hpp>
#include <pgp/core/simulation.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pgp::runtime {

inline constexpr std::size_t maximum_activations = 4096;
inline constexpr std::size_t maximum_records = 65536;
inline constexpr std::size_t maximum_continuation_frames = 8;
inline constexpr std::size_t maximum_steps_per_operation = 4096;

// The core state is the sole owner of company cash, date, revisions and RNG.
// Other foundation facts have no implemented business owner yet. Their keys
// must be registered, and cash may never also appear in this map.
struct WorldState {
    core::SimulationState simulation;
    std::string company_id = "company.player";
    std::string world_id = "world.scenario";
    std::string state_id = "state.us_ca";
    core::OrdinalDay campaign_start{0};
    core::OrdinalDay scope_created{0};
    std::uint64_t root_seed = 0;
    // nullopt is the engineering-fixture collection; scenarios explicitly
    // enable their roots so reusable subflows do not get another trigger.
    std::optional<std::vector<std::string>> enabled_flows;
    std::map<std::string, content::Scalar, std::less<>> facts;
    // Explicit authorized counterparty for foundation cash-transfer fixtures.
    // Every cash effect moves an equal amount in the opposite direction here.
    core::Cents counterparty_cash{0};
    std::vector<std::string> decisions;
    std::vector<std::string> negotiations;
    auto operator<=>(const WorldState&) const = default;
};

enum class ActivationStatus { ready, delayed, awaiting_choice, continuous_active, completed };
enum class FrameKind { sequence, subflow };
struct ContinuationFrame {
    FrameKind kind = FrameKind::sequence;
    std::string flow_id;
    std::string sequence_node;
    std::size_t next_child = 0;
    std::string return_node;
    ScopeBinding binding;
    auto operator<=>(const ContinuationFrame&) const = default;
};
struct Activation {
    std::uint64_t id = 0;
    std::string definition_id;
    std::uint32_t definition_revision = 0;
    core::OrdinalDay started_on{0};
    ScopeBinding binding;
    std::string current_flow;
    std::string current_node;
    std::vector<ContinuationFrame> frames;
    ActivationStatus status = ActivationStatus::ready;
    std::optional<core::OrdinalDay> due;
    std::optional<core::OrdinalDay> wait_started;
    std::optional<std::uint64_t> presentation_id;
    std::uint32_t node_visits = 0;
    bool exiting = false;
    auto operator<=>(const Activation&) const = default;
};
struct LifecycleState {
    std::string definition_id;
    ScopeBinding binding;
    std::uint32_t occurrences = 0;
    bool was_eligible = false;
    std::optional<core::OrdinalDay> last_checked;
    std::optional<core::OrdinalDay> last_started;
    std::optional<core::OrdinalDay> first_due;
    unsigned calendar_day = 1;
    std::uint64_t interval_index = 0;
    std::optional<std::uint64_t> active_activation;
    auto operator<=>(const LifecycleState&) const = default;
};
struct OwnedModifier {
    std::uint64_t activation_id = 0;
    ScopeBinding binding;
    std::string key;
    std::string field;
    std::int64_t add_basis_points = 0;
    auto operator<=>(const OwnedModifier&) const = default;
};
struct EffectReceipt {
    std::uint64_t id = 0;
    std::uint64_t activation_id = 0;
    std::uint64_t revision = 0;
    core::OrdinalDay day{0};
    std::string flow_id;
    std::string node_id;
    std::size_t effect_index = 0;
    std::string operation;
    ScopeBinding target;
    content::Scalar before{std::int64_t{0}};
    content::Scalar after{std::int64_t{0}};
    core::Cents counterparty_delta{0};
    auto operator<=>(const EffectReceipt&) const = default;
};
enum class PresentationKind { news, dialogue, result };
struct PresentedChoice {
    std::string id;
    std::string label_key;
    auto operator<=>(const PresentedChoice&) const = default;
};
struct Presentation {
    std::uint64_t id = 0;
    std::uint64_t activation_id = 0;
    core::OrdinalDay day{0};
    PresentationKind kind = PresentationKind::news;
    std::string flow_id;
    std::string node_id;
    std::string title_key;
    std::string text_key;
    std::vector<PresentedChoice> choices;
    bool resolved = false;
    std::string selected_choice;
    auto operator<=>(const Presentation&) const = default;
};
// One committed narrative selection per executed node visit. The activation
// retains the bound scope; content identity retains the ordered branch weights.
// Keeping the sample and stream transition lets reload validate the selection
// without drawing from the authoritative stream again.
struct RandomResolution {
    std::uint64_t activation_id = 0;
    std::uint32_t node_visit = 0;
    core::OrdinalDay day{0};
    std::string flow_id;
    std::string node_id;
    std::string branch_id;
    std::uint64_t sample = 0;
    std::uint64_t stream_before = 0;
    std::uint64_t stream_after = 0;
    auto operator<=>(const RandomResolution&) const = default;
};
struct RuntimeState {
    std::uint64_t next_activation_id = 1;
    std::uint64_t next_receipt_id = 1;
    std::uint64_t next_presentation_id = 1;
    std::optional<core::OrdinalDay> prepared_day;
    std::vector<Activation> activations;
    std::vector<LifecycleState> lifecycles;
    std::vector<OwnedModifier> modifiers;
    std::vector<EffectReceipt> receipts;
    std::vector<Presentation> presentations;
    // Captured at the first branch, allowing already-advanced starting streams
    // while detecting truncated resolution history on restore.
    std::optional<std::uint64_t> narrative_origin;
    std::vector<RandomResolution> random_resolutions;
    auto operator<=>(const RuntimeState&) const = default;
};
struct Checkpoint {
    WorldState world;
    RuntimeState runtime;
    auto operator<=>(const Checkpoint&) const = default;
};
enum class RuntimeReason {
    accepted, stale_revision, invalid_state, missing_definition, invalid_choice,
    morning_not_ready, effect_rejected, budget_exceeded, arithmetic_overflow,
    campaign_finished
};
struct TraceEntry {
    std::uint64_t activation_id = 0;
    std::string flow_id;
    std::string node_id;
    std::string path;
    ScopeBinding binding;
    std::string detail;
    bool synthetic_cleanup = false;
    bool committed = false;
};
struct RuntimeResult {
    RuntimeReason reason = RuntimeReason::accepted;
    std::uint64_t revision = 0;
    std::string explanation;
    std::vector<TraceEntry> paths;
    std::vector<ClauseTrace> clauses;
    bool committed = false;
    [[nodiscard]] bool accepted() const noexcept { return reason == RuntimeReason::accepted; }
};

[[nodiscard]] WorldState foundation_world(core::Cents cash = core::Cents{0});
[[nodiscard]] std::vector<std::string> supported_effects();
[[nodiscard]] content::Scalar read_fact(const WorldState& world,
                                      const content::FactMetadata& metadata,
                                      const ScopeBinding& binding);
[[nodiscard]] std::int64_t modifier_total(const RuntimeState& state,
                                         const ScopeBinding& binding, std::string_view field);
// Strict reload validation, with no due work, RNG draws or state mutation.
void validate_checkpoint(const CompiledProgram& program, const Checkpoint& checkpoint);
[[nodiscard]] std::string_view to_string(RuntimeReason reason) noexcept;

class Runtime {
public:
    Runtime(CompiledProgram program, Checkpoint checkpoint);
    [[nodiscard]] Checkpoint checkpoint() const { return checkpoint_; }
    [[nodiscard]] const CompiledProgram& program() const noexcept { return program_; }
    [[nodiscard]] RuntimeResult prepare_opening(std::uint64_t expected_revision);
    [[nodiscard]] RuntimeResult choose(std::uint64_t expected_revision,
                                       std::uint64_t presentation_id, std::string_view choice_id);
    [[nodiscard]] RuntimeResult submit(const core::Command& command);
    // Explicit author-fixture operation on this copied Runtime. It does not
    // claim that a natural continuous exit condition became true.
    [[nodiscard]] RuntimeResult synthetic_cleanup(std::uint64_t expected_revision,
                                                  std::uint64_t activation_id);
private:
    CompiledProgram program_;
    Checkpoint checkpoint_;
};
} // namespace pgp::runtime
