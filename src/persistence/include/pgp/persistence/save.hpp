#pragma once

#include <pgp/persistence/storage.hpp>
#include <pgp/runtime/runtime.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace pgp::persistence {
struct SaveIdentity {
    std::uint32_t save_format;
    std::uint32_t simulation_rules;
    std::uint32_t content_schema;
    std::string game_version;
    std::string content_version;
    // SHA256 of compiler canonical definition-collection bytes. This initial
    // identity covers no unimplemented shipped asset/localization catalog.
    std::string content_hash;
    auto operator<=>(const SaveIdentity&) const = default;
};
struct SaveLimits {
    std::size_t maximum_bytes = default_maximum_save_bytes;
    std::size_t maximum_json_events = 4'000'000;
};
struct LoadedSave {
    SaveIdentity identity;
    runtime::Checkpoint checkpoint;
    std::uint64_t autosave_sequence = 0;
};
[[nodiscard]] SaveIdentity save_identity(const runtime::CompiledProgram& program);
[[nodiscard]] std::string encode_save(const runtime::CompiledProgram& program,
                                      const runtime::Checkpoint& checkpoint,
                                      std::uint64_t autosave_sequence = 0,
                                      SaveLimits limits = {});
[[nodiscard]] LoadedSave decode_save(const runtime::CompiledProgram& program,
                                     std::string_view bytes, SaveLimits limits = {});
void save_file(const runtime::CompiledProgram& program, const runtime::Checkpoint& checkpoint,
               const std::filesystem::path& path, std::uint64_t autosave_sequence = 0,
               SaveLimits limits = {});
[[nodiscard]] LoadedSave load_file(const runtime::CompiledProgram& program,
                                   const std::filesystem::path& path, SaveLimits limits = {});

enum class AutosaveReason { monthly_boundary, before_irreversible_commitment, campaign_end };
class AutosaveRing {
public:
    explicit AutosaveRing(std::filesystem::path directory);
    // Disabled saves perform no reads/writes. Each committed save advances a
    // generation embedded in its envelope; no fragile separate cursor file.
    [[nodiscard]] std::optional<std::filesystem::path> write(
        const runtime::CompiledProgram& program, const runtime::Checkpoint& checkpoint,
        AutosaveReason reason, bool enabled = true, SaveLimits limits = {}) const;
    // Returns newest valid current or recovery copy. Corrupt candidates are
    // retained; if none validates, returns an explanatory error.
    [[nodiscard]] LoadedSave latest(const runtime::CompiledProgram& program, SaveLimits limits = {}) const;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
private:
    std::filesystem::path directory_;
};
}
